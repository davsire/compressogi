#ifndef FILA_ARQUIVO_H
#define FILA_ARQUIVO_H

#include <pthread.h>

#define NOME_MAX 255
#define CAMINHO_MAX 4096

typedef struct Arquivo {
  char nome_arquivo[NOME_MAX];
  char caminho_arquivo[CAMINHO_MAX];
  char caminho_zip[CAMINHO_MAX];
  struct Arquivo* prox;
} arquivo_t;

typedef struct FilaArquivo {
  arquivo_t* cabeca;
  int vazia;
  pthread_mutex_t mutex;
  pthread_cond_t var_cond;
} fila_arquivo_t;

void inicializar_fila(fila_arquivo_t* fila);

void adicionar_fila(fila_arquivo_t* fila, arquivo_t* arquivo);

arquivo_t* remover_fila(fila_arquivo_t* fila);

#endif
