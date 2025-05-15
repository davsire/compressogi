#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "fila_comprimir.h"

#define TRUE 1
#define FALSE 0

void inicializar_fila_comprimir(fila_comprimir_t* fila) {
  fila->cabeca = NULL;
  fila->vazia = TRUE;
  pthread_mutex_init(&fila->mutex, NULL);
  pthread_cond_init(&fila->var_cond, NULL);
}

void adicionar_fila_comprimir(fila_comprimir_t* fila, arquivo_comprimir_t* arquivo) {
  if (fila->vazia) {
    fila->cabeca = arquivo;
    fila->vazia = FALSE;
    return;
  }

  arquivo_comprimir_t* no_atual = fila->cabeca;
  while (no_atual->prox != NULL) {
    no_atual = no_atual->prox;
  }
  no_atual->prox = arquivo;
}

arquivo_comprimir_t* remover_fila_comprimir(fila_comprimir_t* fila) {
  if (fila->vazia) {
    return NULL;
  }

  arquivo_comprimir_t* remover = fila->cabeca;
  fila->cabeca = remover->prox;
  if (fila->cabeca == NULL) {
    fila->vazia = TRUE;
  }

  return remover;
}
