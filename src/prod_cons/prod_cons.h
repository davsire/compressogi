#ifndef PROD_CONS_H
#define PROD_CONS_H

#include <semaphore.h>

typedef struct FilaProdCons {
  sem_t semaforo_ocupado;
  sem_t semaforo_vazio;
  int indice_atual;
  void* buffer[];
} fila_prod_cons_t;

void inicializar_fila_prod_cons(fila_prod_cons_t* fila_prod_cons, int maximo_buffer, int tamanho_valor);

void produzir(fila_prod_cons_t* fila_prod_cons, void* valor);

void* consumir(fila_prod_cons_t* fila_prod_cons);

#endif
