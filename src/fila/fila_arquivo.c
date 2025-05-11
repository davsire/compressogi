#include <stdio.h>
#include <stdlib.h>
#include "fila_arquivo.h"

void adicionar_fila(fila_arquivo_t** fila, fila_arquivo_t* arquivo) {
  if (*fila == NULL) {
    *fila = arquivo;
    return;
  }

  fila_arquivo_t* noAtual = *fila;
  while (noAtual->prox != NULL) {
    noAtual = noAtual->prox;
  }
  noAtual->prox = arquivo;
}

fila_arquivo_t* remover_fila(fila_arquivo_t** fila) {
  if (*fila == NULL) {
    return NULL;
  }

  fila_arquivo_t* remover = *fila;
  *fila = remover->prox;
  return remover;
}
