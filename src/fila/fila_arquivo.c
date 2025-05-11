#include <stdio.h>
#include <stdlib.h>
#include "fila_arquivo.h"

void adicionar_fila(fila_arquivo_t** fila, fila_arquivo_t* arquivo) {
  if (*fila == NULL) {
    *fila = arquivo;
    return;
  }

  fila_arquivo_t* no_atual = *fila;
  while (no_atual->prox != NULL) {
    no_atual = no_atual->prox;
  }
  no_atual->prox = arquivo;
}

fila_arquivo_t* remover_fila(fila_arquivo_t** fila) {
  if (*fila == NULL) {
    return NULL;
  }

  fila_arquivo_t* remover = *fila;
  *fila = remover->prox;
  return remover;
}
