#include "fila_arquivo.h"

void inicializar_fila(fila_arquivo_t* fila) {
  fila->cabeca = NULL;
  fila->vazia = TRUE;
  pthread_mutex_init(&fila->mutex, NULL);
  pthread_cond_init(&fila->var_cond, NULL);
}

void adicionar_fila(fila_arquivo_t* fila, arquivo_t* arquivo) {
  if (fila->vazia) {
    fila->cabeca = arquivo;
    fila->vazia = FALSE;
    return;
  }

  arquivo_t* no_atual = fila->cabeca;
  while (no_atual->prox != NULL) {
    no_atual = no_atual->prox;
  }
  no_atual->prox = arquivo;
}

arquivo_t* remover_fila(fila_arquivo_t* fila) {
  if (fila->vazia) {
    return NULL;
  }

  arquivo_t* remover = fila->cabeca;
  fila->cabeca = remover->prox;
  if (fila->cabeca == NULL) {
    fila->vazia = TRUE;
  }

  return remover;
}
