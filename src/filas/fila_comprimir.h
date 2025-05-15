#ifndef FILA_COMPRIMIR_H
#define FILA_COMPRIMIR_H

#include <pthread.h>

#define NOME_MAX 255
#define CAMINHO_MAX 4096

typedef struct ArquivoComprimir {
  char nome_arquivo[NOME_MAX];
  char caminho_arquivo[CAMINHO_MAX];
  struct ArquivoComprimir* prox;
} arquivo_comprimir_t;

typedef struct FilaComprimir {
  arquivo_comprimir_t* cabeca;
  int vazia;
  pthread_mutex_t mutex;
  pthread_cond_t var_cond;
} fila_comprimir_t;

void inicializar_fila_comprimir(fila_comprimir_t* fila);

void adicionar_fila_comprimir(fila_comprimir_t* fila, arquivo_comprimir_t* arquivo);

arquivo_comprimir_t* remover_fila_comprimir(fila_comprimir_t* fila);

#endif
