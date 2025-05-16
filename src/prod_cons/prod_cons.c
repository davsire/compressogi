#include <stdlib.h>
#include "prod_cons.h"

void inicializar_fila_prod_cons(fila_prod_cons_t* fila_prod_cons, int maximo_buffer, int tamanho_valor) {
  *fila_prod_cons->buffer = malloc(tamanho_valor * maximo_buffer);
  sem_init(&fila_prod_cons->semaforo_ocupado, 0, 0);
  sem_init(&fila_prod_cons->semaforo_vazio, 0, maximo_buffer);
  fila_prod_cons->indice_atual = -1;
}

void produzir(fila_prod_cons_t* fila_prod_cons, void* valor) {
  sem_wait(&fila_prod_cons->semaforo_vazio);
  fila_prod_cons->indice_atual++;
  fila_prod_cons->buffer[fila_prod_cons->indice_atual] = valor;
  sem_post(&fila_prod_cons->semaforo_ocupado);
}

void* consumir(fila_prod_cons_t* fila_prod_cons) {
  sem_wait(&fila_prod_cons->semaforo_ocupado);
  void* valor = fila_prod_cons->buffer[fila_prod_cons->indice_atual];
  fila_prod_cons->indice_atual--;
  sem_post(&fila_prod_cons->semaforo_vazio);
  return valor;
}
