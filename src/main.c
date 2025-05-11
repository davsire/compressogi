#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/inotify.h>
#include <pthread.h>
#include "fila/fila_arquivo.h"
#include "miniz/miniz.h"

#define NUM_THREAD_POOL 10
#define TAMANHO_EVENTO (sizeof(struct inotify_event))
#define BUFF_MAX (10 * (TAMANHO_EVENTO + NOME_MAX + 1))
#define TRUE 1
#define FALSE 0

typedef struct Compressor {
  char origem[CAMINHO_MAX];
  char destino[CAMINHO_MAX];
  int remover_arquivo;
  fila_arquivo_t* fila;
  pthread_mutex_t mutex;
  pthread_cond_t var_cond;
} compressor_t;

void adicionar_arquivo_fila(compressor_t* compressor, struct inotify_event* evento) {
  fila_arquivo_t* arquivo = malloc(sizeof(fila_arquivo_t));
  snprintf(arquivo->nome_arquivo, sizeof(arquivo->nome_arquivo), "%s", evento->name);
  snprintf(arquivo->caminho_arquivo, sizeof(arquivo->caminho_arquivo), "%s/%s", compressor->origem, evento->name);
  arquivo->prox = NULL;

  pthread_mutex_lock(&compressor->mutex);
  adicionar_fila(&compressor->fila, arquivo);
  pthread_mutex_unlock(&compressor->mutex);
  pthread_cond_signal(&compressor->var_cond);
}

void* monitorar(void* args) {
  compressor_t* compressor = (compressor_t*) args;
  int fd, wd, tamanho_buffer, i;
  char buffer[BUFF_MAX];

  fd = inotify_init();
  if (fd < 0) {
    printf("[MONITOR] Erro ao inicializar monitor...\n");
    exit(2);
  }

  wd = inotify_add_watch(fd, compressor->origem, IN_CLOSE_WRITE | IN_MOVED_TO);
  if (wd < 0) {
    printf("[MONITOR] Erro ao criar monitor para pasta '%s'...\n", compressor->origem);
    exit(2);
  }

  printf("[MONITOR] Monitorando pasta '%s'\n", compressor->origem);
  while (TRUE) {
    i = 0;
    tamanho_buffer = read(fd, buffer, BUFF_MAX);
    if (tamanho_buffer < 0) {
      printf("[MONITOR] Erro ao ler conteúdo do monitor...\n");
      exit(2);
    }
  
    while (i < tamanho_buffer) {
      struct inotify_event* evento = (struct inotify_event*) &buffer[i];

      if (evento->len) {
        printf("[MONITOR] Arquivo '%s' enviado para compressão\n", evento->name);
        adicionar_arquivo_fila(compressor, evento);
      }
  
      i += TAMANHO_EVENTO + evento->len;
    }
  }

  printf("[MONITOR] Monitoramento finalizado\n");
  inotify_rm_watch(fd, wd);
  close(fd);
}

void* comprimir(void* args) {
  compressor_t* compressor = (compressor_t*) args;

  while (TRUE) {
    pthread_mutex_lock(&compressor->mutex);
    while (compressor->fila == NULL) {
      pthread_cond_wait(&compressor->var_cond, &compressor->mutex);
    }
    fila_arquivo_t* arquivo = remover_fila(&compressor->fila);
    pthread_mutex_unlock(&compressor->mutex);
  
    printf("[COMPRESSOR %lu] Comprimindo arquivo '%s'\n", (unsigned long)pthread_self(), arquivo->nome_arquivo);
    char destino_zip[CAMINHO_MAX];
    snprintf(destino_zip, sizeof(destino_zip), "%s/%s.zip", compressor->destino, arquivo->nome_arquivo);
  
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
    if (compressor->remover_arquivo) {
      remove(arquivo->caminho_arquivo);
    }
    free(arquivo);
  }
}

int main(int argc, char** argv) {
  if (argc < 3) {
    printf("Informe o caminho da pasta origem e destino:\n");
    printf("%s <pasta_origem> <pasta_destino> <remover_arquivo [s/N]>\n", argv[0]);
    exit(1);
  }

  compressor_t compressor;
  snprintf(compressor.origem, sizeof(compressor.origem), "%s", argv[1]);
  snprintf(compressor.destino, sizeof(compressor.destino), "%s", argv[2]);
  compressor.remover_arquivo = (argc > 3 && !strcmp(argv[3], "s")) ? TRUE : FALSE;
  compressor.fila = NULL;
  pthread_mutex_init(&compressor.mutex, NULL);
  pthread_cond_init(&compressor.var_cond, NULL);

  pthread_t monitor;
  pthread_create(&monitor, NULL, monitorar, &compressor);

  pthread_t pool_compressor[NUM_THREAD_POOL];
  for (int i = 0; i < NUM_THREAD_POOL; i++) {
    pthread_create(&pool_compressor[i], NULL, comprimir, &compressor);
  }

  pthread_join(monitor, NULL);
  for (int i = 0; i < NUM_THREAD_POOL; i++) {
    pthread_join(pool_compressor[i], NULL);
  }

  exit(0);
}
