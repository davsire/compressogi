#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <signal.h>
#include <pthread.h>
#include "fila/fila_arquivo.h"
#include "prod_cons/prod_cons.h"
#include "miniz/miniz.h"

#define NUM_THREAD_COMPRESSOR 10
#define TAM_BUFFER_PROD_CONS 5
#define TAM_EVENTO (sizeof(struct inotify_event))
#define TAM_BUFFER_EVENTO (10 * (TAM_EVENTO + NOME_MAX + 1))
#define NOME_ARQUIVO_LOG "log.txt"
#define TRUE 1
#define FALSE 0

typedef struct ParametrosCompressor {
  char origem[CAMINHO_MAX];
  char destino[CAMINHO_MAX];
  FILE* log;
  int monitor_fd;
  int monitor_wd;
  fila_arquivo_t fila_comprimir;
  fila_prod_cons_t fila_prod_cons;
} parametros_compressor_t;

arquivo_t* criar_arquivo(parametros_compressor_t* parametros_compressor, struct inotify_event* evento) {
  arquivo_t* arquivo = malloc(sizeof(arquivo_t));
  snprintf(arquivo->nome_arquivo, sizeof(arquivo->nome_arquivo), "%s", evento->name);
  snprintf(arquivo->caminho_arquivo, sizeof(arquivo->caminho_arquivo), "%s/%s", parametros_compressor->origem, evento->name);
  snprintf(arquivo->caminho_zip, sizeof(arquivo->caminho_zip), "%s/%s.zip", parametros_compressor->destino, evento->name);
  arquivo->prox = NULL;
  return arquivo;
}

void adicionar_arquivo_fila(fila_arquivo_t* fila, arquivo_t* arquivo) {
  pthread_mutex_lock(&fila->mutex);
  adicionar_fila(fila, arquivo);
  pthread_mutex_unlock(&fila->mutex);
  pthread_cond_signal(&fila->var_cond);
}

void* monitorar(void* args) {
  parametros_compressor_t* parametros_compressor = (parametros_compressor_t*) args;
  int tamanho_buffer, i;
  char buffer[TAM_BUFFER_EVENTO];

  parametros_compressor->monitor_fd = inotify_init();
  if (parametros_compressor->monitor_fd < 0) {
    printf("[MONITOR] Erro ao inicializar monitor...\n");
    exit(3);
  }

  parametros_compressor->monitor_wd = inotify_add_watch(parametros_compressor->monitor_fd, parametros_compressor->origem, IN_CLOSE_WRITE | IN_MOVED_TO);
  if (parametros_compressor->monitor_wd < 0) {
    printf("[MONITOR] Erro ao criar monitor para diretório '%s'...\n", parametros_compressor->origem);
    exit(3);
  }

  printf("[MONITOR] Monitorando diretório '%s'\n", parametros_compressor->origem);
  while (TRUE) {
    i = 0;
    tamanho_buffer = read(parametros_compressor->monitor_fd, buffer, TAM_BUFFER_EVENTO);
    if (tamanho_buffer < 0) {
      printf("[MONITOR] Erro ao ler conteúdo do monitor...\n");
      exit(3);
    }
  
    while (i < tamanho_buffer) {
      struct inotify_event* evento = (struct inotify_event*) &buffer[i];

      if (evento->len) {
        printf("[MONITOR] Arquivo '%s' enviado para compressão\n", evento->name);
        arquivo_t* arquivo = criar_arquivo(parametros_compressor, evento);
        adicionar_arquivo_fila(&parametros_compressor->fila_comprimir, arquivo);
      }
  
      i += TAM_EVENTO + evento->len;
    }
  }
}

void* comprimir(void* args) {
  parametros_compressor_t* parametros_compressor = (parametros_compressor_t*) args;

  while (TRUE) {
    pthread_mutex_lock(&parametros_compressor->fila_comprimir.mutex);
    while (parametros_compressor->fila_comprimir.vazia) {
      pthread_cond_wait(&parametros_compressor->fila_comprimir.var_cond, &parametros_compressor->fila_comprimir.mutex);
    }
    arquivo_t* arquivo = remover_fila(&parametros_compressor->fila_comprimir);
    pthread_mutex_unlock(&parametros_compressor->fila_comprimir.mutex);

    printf("[COMPRESSOR %lu] Comprimindo arquivo '%s'\n", (unsigned long)pthread_self(), arquivo->nome_arquivo);

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(mz_zip_archive));

    mz_bool iniciado = mz_zip_writer_init_file(&zip_archive, arquivo->caminho_zip, 0);
    if (!iniciado) {
      printf("[COMPRESSOR %lu] Erro ao inicializar arquivo zip\n", (unsigned long)pthread_self());
      mz_zip_writer_end(&zip_archive);
      free(arquivo);
      continue;
    }

    mz_bool adicionado = mz_zip_writer_add_file(&zip_archive, arquivo->nome_arquivo, arquivo->caminho_arquivo, NULL, 0, MZ_BEST_COMPRESSION);
    if (!adicionado) {
      printf("[COMPRESSOR %lu] Erro ao adicionar arquivo '%s' no zip\n", (unsigned long)pthread_self(), arquivo->nome_arquivo);
      mz_zip_writer_end(&zip_archive);
      free(arquivo);
      continue;
    }

    mz_bool finalizado = mz_zip_writer_finalize_archive(&zip_archive) && mz_zip_writer_end(&zip_archive);
    if (!finalizado) {
      printf("[COMPRESSOR %lu] Erro ao finalizar compressão\n", (unsigned long)pthread_self());
      free(arquivo);
      continue;
    }

    printf("[COMPRESSOR %lu] Arquivo '%s' comprimido com sucesso\n", (unsigned long)pthread_self(), arquivo->nome_arquivo);
    produzir(&parametros_compressor->fila_prod_cons, (void*) arquivo);
  }
}

void* registrar_log(void* args) {
  parametros_compressor_t* parametros_compressor = (parametros_compressor_t*) args;
  parametros_compressor->log = fopen(NOME_ARQUIVO_LOG, "w");

  if (!parametros_compressor->log) {
    printf("[LOGGER] Não foi possível criar o arquivo de log...\n");
    exit(4);
  }

  while (TRUE) {
    arquivo_t* arquivo = (arquivo_t*) consumir(&parametros_compressor->fila_prod_cons);

    struct stat infos_arquivo;
    stat(arquivo->caminho_arquivo, &infos_arquivo);
    long long tamanhoOriginal = infos_arquivo.st_size;
    stat(arquivo->caminho_zip, &infos_arquivo);
    long long tamanhoCompressao = infos_arquivo.st_size;

    fprintf(parametros_compressor->log, "%s | %lld bytes --> %lld bytes\n", arquivo->nome_arquivo, tamanhoOriginal, tamanhoCompressao);
    fflush(parametros_compressor->log);
    free(arquivo);
  }
}

void finalizar_programa() {
  printf("[COMPRESSOGI] Finalizando programa...\n");
}

void validar_diretorios(char* origem, char* destino) {
  DIR* diretorio = opendir(origem);
  if (!diretorio) {
    printf("[COMPRESSOGI] O diretório origem '%s' não existe ou foi informado incorretamente!\n", origem);
    exit(2);
  }
  closedir(diretorio);

  diretorio = opendir(destino);
  if (!diretorio) {
    printf("[COMPRESSOGI] O diretório destino '%s' não existe ou foi informado incorretamente!\n", destino);
    exit(2);
  }
  closedir(diretorio);
}

void inicializar_parametros_compressor(parametros_compressor_t* parametros_compressor, int argc, char** argv) {
  snprintf(parametros_compressor->origem, sizeof(parametros_compressor->origem), "%s", argv[1]);
  snprintf(parametros_compressor->destino, sizeof(parametros_compressor->destino), "%s", argv[2]);
  inicializar_fila(&parametros_compressor->fila_comprimir);
  inicializar_fila_prod_cons(&parametros_compressor->fila_prod_cons, TAM_BUFFER_PROD_CONS, sizeof(arquivo_t*));
}

void limpar_parametros_compressor(parametros_compressor_t* parametros_compressor) {
  fclose(parametros_compressor->log);
  inotify_rm_watch(parametros_compressor->monitor_fd, parametros_compressor->monitor_wd);
  close(parametros_compressor->monitor_fd);
}

int main(int argc, char** argv) {
  if (argc < 3) {
    printf("[COMPRESSOGI] Informe o caminho do diretório origem e destino:\n");
    printf("%s <diretorio_origem> <diretorio_destino>\n", argv[0]);
    exit(1);
  }

  signal(SIGINT, finalizar_programa);
  validar_diretorios(argv[1], argv[2]);

  parametros_compressor_t parametros_compressor;
  inicializar_parametros_compressor(&parametros_compressor, argc, argv);

  pthread_t monitor;
  pthread_create(&monitor, NULL, monitorar, &parametros_compressor);

  pthread_t compressores[NUM_THREAD_COMPRESSOR];
  for (int i = 0; i < NUM_THREAD_COMPRESSOR; i++) {
    pthread_create(&compressores[i], NULL, comprimir, &parametros_compressor);
  }

  pthread_t logger;
  pthread_create(&logger, NULL, registrar_log, &parametros_compressor);

  pause();
  pthread_cancel(monitor);
  pthread_cancel(logger);
  for (int i = 0; i < NUM_THREAD_COMPRESSOR; i++) {
    pthread_cancel(compressores[i]);
  }
  limpar_parametros_compressor(&parametros_compressor);

  exit(0);
}
