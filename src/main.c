#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <pthread.h>
#include "filas/fila_comprimir.h"
#include "filas/fila_log.h"
#include "miniz/miniz.h"

#define NUM_THREAD_COMPRESSOR 10
#define TAMANHO_EVENTO (sizeof(struct inotify_event))
#define BUFF_MAX (10 * (TAMANHO_EVENTO + NOME_MAX + 1))
#define NOME_ARQUIVO_LOG "log.txt"
#define TRUE 1
#define FALSE 0

typedef struct ParametrosCompressor {
  char origem[CAMINHO_MAX];
  char destino[CAMINHO_MAX];
  int remover_arquivo;
  fila_comprimir_t fila_comprimir;
  fila_log_t fila_log;
} parametros_compressor_t;

void adicionar_arquivo_comprimir(parametros_compressor_t* parametros_compressor, struct inotify_event* evento) {
  arquivo_comprimir_t* arquivo = malloc(sizeof(arquivo_comprimir_t));
  snprintf(arquivo->nome_arquivo, sizeof(arquivo->nome_arquivo), "%s", evento->name);
  snprintf(arquivo->caminho_arquivo, sizeof(arquivo->caminho_arquivo), "%s/%s", parametros_compressor->origem, evento->name);
  arquivo->prox = NULL;

  pthread_mutex_lock(&parametros_compressor->fila_comprimir.mutex);
  adicionar_fila_comprimir(&parametros_compressor->fila_comprimir, arquivo);
  pthread_mutex_unlock(&parametros_compressor->fila_comprimir.mutex);
  pthread_cond_signal(&parametros_compressor->fila_comprimir.var_cond);
}

void* monitorar(void* args) {
  parametros_compressor_t* parametros_compressor = (parametros_compressor_t*) args;
  int fd, wd, tamanho_buffer, i;
  char buffer[BUFF_MAX];

  fd = inotify_init();
  if (fd < 0) {
    printf("[MONITOR] Erro ao inicializar monitor...\n");
    exit(3);
  }

  wd = inotify_add_watch(fd, parametros_compressor->origem, IN_CLOSE_WRITE | IN_MOVED_TO);
  if (wd < 0) {
    printf("[MONITOR] Erro ao criar monitor para diretório '%s'...\n", parametros_compressor->origem);
    exit(3);
  }

  printf("[MONITOR] Monitorando diretório '%s'\n", parametros_compressor->origem);
  while (TRUE) {
    i = 0;
    tamanho_buffer = read(fd, buffer, BUFF_MAX);
    if (tamanho_buffer < 0) {
      printf("[MONITOR] Erro ao ler conteúdo do monitor...\n");
      exit(3);
    }
  
    while (i < tamanho_buffer) {
      struct inotify_event* evento = (struct inotify_event*) &buffer[i];

      if (evento->len) {
        printf("[MONITOR] Arquivo '%s' enviado para compressão\n", evento->name);
        adicionar_arquivo_comprimir(parametros_compressor, evento);
      }
  
      i += TAMANHO_EVENTO + evento->len;
    }
  }

  printf("[MONITOR] Monitoramento finalizado\n");
  inotify_rm_watch(fd, wd);
  close(fd);
}

void adicionar_arquivo_log(parametros_compressor_t* parametros_compressor, arquivo_comprimir_t* arquivo, char* caminho_zip) {
  arquivo_log_t* arquivo_log = malloc(sizeof(arquivo_log_t));
  snprintf(arquivo_log->nome_arquivo, sizeof(arquivo_log->nome_arquivo), "%s", arquivo->nome_arquivo);
  arquivo_log->prox = NULL;
  
  struct stat infos_arquivo;
  stat(arquivo->caminho_arquivo, &infos_arquivo);
  arquivo_log->tamanhoOriginal = infos_arquivo.st_size;  
  stat(caminho_zip, &infos_arquivo);
  arquivo_log->tamanhoCompressao = infos_arquivo.st_size;

  pthread_mutex_lock(&parametros_compressor->fila_log.mutex);
  adicionar_fila_log(&parametros_compressor->fila_log, arquivo_log);
  pthread_mutex_unlock(&parametros_compressor->fila_log.mutex);
  pthread_cond_signal(&parametros_compressor->fila_log.var_cond);
}

void* comprimir(void* args) {
  parametros_compressor_t* parametros_compressor = (parametros_compressor_t*) args;

  while (TRUE) {
    pthread_mutex_lock(&parametros_compressor->fila_comprimir.mutex);
    while (parametros_compressor->fila_comprimir.vazia) {
      pthread_cond_wait(&parametros_compressor->fila_comprimir.var_cond, &parametros_compressor->fila_comprimir.mutex);
    }
    arquivo_comprimir_t* arquivo = remover_fila_comprimir(&parametros_compressor->fila_comprimir);
    pthread_mutex_unlock(&parametros_compressor->fila_comprimir.mutex);

    printf("[COMPRESSOR %lu] Comprimindo arquivo '%s'\n", (unsigned long)pthread_self(), arquivo->nome_arquivo);
    char destino_zip[CAMINHO_MAX];
    snprintf(destino_zip, sizeof(destino_zip), "%s/%s.zip", parametros_compressor->destino, arquivo->nome_arquivo);

    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(mz_zip_archive));

    mz_bool iniciado = mz_zip_writer_init_file(&zip_archive, destino_zip, 0);
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
    adicionar_arquivo_log(parametros_compressor, arquivo, destino_zip);
    if (parametros_compressor->remover_arquivo) {
      remove(arquivo->caminho_arquivo);
    }
    free(arquivo);
  }
}

void* registrar_log(void* args) {
  parametros_compressor_t* parametros_compressor = (parametros_compressor_t*) args;
  FILE* log = fopen(NOME_ARQUIVO_LOG, "w");

  if (!log) {
    printf("[LOGGER] Não foi possível criar o arquivo de log...\n");
    exit(4);
  }

  while (TRUE) {
    pthread_mutex_lock(&parametros_compressor->fila_log.mutex);
    while (parametros_compressor->fila_log.vazia) {
      pthread_cond_wait(&parametros_compressor->fila_log.var_cond, &parametros_compressor->fila_log.mutex);
    }
    arquivo_log_t* arquivo_log = remover_fila_log(&parametros_compressor->fila_log);
    pthread_mutex_unlock(&parametros_compressor->fila_log.mutex);

    fprintf(log, "%s | %lld bytes --> %lld bytes\n", arquivo_log->nome_arquivo, arquivo_log->tamanhoOriginal, arquivo_log->tamanhoCompressao);
    fflush(log);
    free(arquivo_log);
  }

  fclose(log);
}

void validar_diretorios(char* origem, char* destino) {
  DIR* diretorio = opendir(origem);
  if (!diretorio) {
    printf("O diretório origem '%s' não existe ou foi informado incorretamente!\n", origem);
    exit(2);
  }
  closedir(diretorio);

  diretorio = opendir(destino);
  if (!diretorio) {
    printf("O diretório destino '%s' não existe ou foi informado incorretamente!\n", destino);
    exit(2);
  }
  closedir(diretorio);
}

void inicializar_parametros_compressor(parametros_compressor_t* parametros_compressor, int argc, char** argv) {
  snprintf(parametros_compressor->origem, sizeof(parametros_compressor->origem), "%s", argv[1]);
  snprintf(parametros_compressor->destino, sizeof(parametros_compressor->destino), "%s", argv[2]);
  parametros_compressor->remover_arquivo = (argc > 3 && !strcmp(argv[3], "s")) ? TRUE : FALSE;
  inicializar_fila_comprimir(&parametros_compressor->fila_comprimir);
  inicializar_fila_log(&parametros_compressor->fila_log);
}

int main(int argc, char** argv) {
  if (argc < 3) {
    printf("Informe o caminho do diretório origem e destino:\n");
    printf("%s <diretorio_origem> <diretorio_destino> <remover_arquivo [s/N]>\n", argv[0]);
    exit(1);
  }

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

  pthread_join(monitor, NULL);
  for (int i = 0; i < NUM_THREAD_COMPRESSOR; i++) {
    pthread_join(compressores[i], NULL);
  }

  exit(0);
}
