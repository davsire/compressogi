#ifndef FILA_H
#define FILA_H

#define NOME_MAX 255
#define CAMINHO_MAX 4096

typedef struct FilaArquivo {
  char nomeArquivo[NOME_MAX];
  char caminhoArquivo[CAMINHO_MAX];
  struct FilaArquivo* prox;
} fila_arquivo_t;

void adicionar_fila(fila_arquivo_t** fila, fila_arquivo_t* arquivo);

fila_arquivo_t* remover_fila(fila_arquivo_t** fila);

#endif
