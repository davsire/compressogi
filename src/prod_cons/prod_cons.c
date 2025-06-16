#include <stdlib.h>
#include "prod_cons.h"

void inicializar_fila_prod_cons(fila_prod_cons_t* fila_prod_cons, int maximo_buffer, int tamanho_valor) {
  *fila_prod_cons->buffer = malloc(tamanho_valor * maximo_buffer);
  sem_init(&fila_prod_cons->semaforo_ocupado, 0, 0);
  sem_init(&fila_prod_cons->semaforo_vazio, 0, maximo_buffer);
  pthread_mutex_init(&fila_prod_cons->mutex, NULL);
  fila_prod_cons->indice_atual = -1;
}

void produzir(fila_prod_cons_t* fila_prod_cons, void* valor) {
  sem_wait(&fila_prod_cons->semaforo_vazio);
  pthread_mutex_lock(&fila_prod_cons->mutex);
  fila_prod_cons->indice_atual++;
  fila_prod_cons->buffer[fila_prod_cons->indice_atual] = valor;
  pthread_mutex_unlock(&fila_prod_cons->mutex);
  sem_post(&fila_prod_cons->semaforo_ocupado);
}

void* consumir(fila_prod_cons_t* fila_prod_cons) {
  sem_wait(&fila_prod_cons->semaforo_ocupado);
  pthread_mutex_lock(&fila_prod_cons->mutex);
  void* valor = fila_prod_cons->buffer[fila_prod_cons->indice_atual];
  fila_prod_cons->indice_atual--;
  pthread_mutex_unlock(&fila_prod_cons->mutex);
  sem_post(&fila_prod_cons->semaforo_vazio);
  return valor;
}
