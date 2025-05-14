#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/inotify.h>
#include <pthread.h>
#include "fila/fila_arquivo.h"
#include "miniz/miniz.h"

#define NUM_THREAD_COMPRESSOR 10
#define TAMANHO_EVENTO (sizeof(struct inotify_event))
#define BUFF_MAX (10 * (TAMANHO_EVENTO + NOME_MAX + 1))
#define TRUE 1
#define FALSE 0

typedef struct ParametrosCompressor {
  char origem[CAMINHO_MAX];
  char destino[CAMINHO_MAX];
  int remover_arquivo;
  fila_arquivo_t* fila;
  pthread_mutex_t mutex;
  pthread_cond_t var_cond;
} parametros_compressor_t;

void adicionar_arquivo_fila(parametros_compressor_t* parametros_compressor, struct inotify_event* evento) {
  fila_arquivo_t* arquivo = malloc(sizeof(fila_arquivo_t));
  snprintf(arquivo->nome_arquivo, sizeof(arquivo->nome_arquivo), "%s", evento->name);
  snprintf(arquivo->caminho_arquivo, sizeof(arquivo->caminho_arquivo), "%s/%s", parametros_compressor->origem, evento->name);
  arquivo->prox = NULL;

  pthread_mutex_lock(&parametros_compressor->mutex);
  adicionar_fila(&parametros_compressor->fila, arquivo);
  pthread_mutex_unlock(&parametros_compressor->mutex);
  pthread_cond_signal(&parametros_compressor->var_cond);
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
        adicionar_arquivo_fila(parametros_compressor, evento);
      }
  
      i += TAMANHO_EVENTO + evento->len;
    }
  }

  printf("[MONITOR] Monitoramento finalizado\n");
  inotify_rm_watch(fd, wd);
  close(fd);
}

void* comprimir(void* args) {
  parametros_compressor_t* parametros_compressor = (parametros_compressor_t*) args;

  while (TRUE) {
    pthread_mutex_lock(&parametros_compressor->mutex);
    while (parametros_compressor->fila == NULL) {
      pthread_cond_wait(&parametros_compressor->var_cond, &parametros_compressor->mutex);
    }
    fila_arquivo_t* arquivo = remover_fila(&parametros_compressor->fila);
    pthread_mutex_unlock(&parametros_compressor->mutex);

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
    if (parametros_compressor->remover_arquivo) {
      remove(arquivo->caminho_arquivo);
    }
    free(arquivo);
  }
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
  parametros_compressor->fila = NULL;
  pthread_mutex_init(&parametros_compressor->mutex, NULL);
  pthread_cond_init(&parametros_compressor->var_cond, NULL);
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

  pthread_join(monitor, NULL);
  for (int i = 0; i < NUM_THREAD_COMPRESSOR; i++) {
    pthread_join(compressores[i], NULL);
  }

  exit(0);
}
