#ifndef FILA_LOG_H
#define FILA_LOG_H

#include <pthread.h>
#include <sys/stat.h>

#define NOME_MAX 255

typedef struct ArquivoLog {
  char nome_arquivo[NOME_MAX];
  long long tamanhoOriginal;
  long long tamanhoCompressao;
  struct ArquivoLog* prox;
} arquivo_log_t;

typedef struct FilaLog {
  arquivo_log_t* cabeca;
  int vazia;
  pthread_mutex_t mutex;
  pthread_cond_t var_cond;
} fila_log_t;

void inicializar_fila_log(fila_log_t* fila);

void adicionar_fila_log(fila_log_t* fila, arquivo_log_t* arquivo);

arquivo_log_t* remover_fila_log(fila_log_t* fila);

#endif
