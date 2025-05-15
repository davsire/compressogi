# Compressogi

Programa que monitora e comprime arquivos em tempo real e paralelamente, a partir de diretórios origem e destino.

## Pré-requisitos

- Necessário usar Linux (compatibilidade com biblioteca `inotify`)

## Compilação

Para compilar a aplicação, utilize o seguinte comando na raiz do projeto:

```bash
make all
```

## Como Executar

Execute o programa da seguinte forma:

```bash
./compressogi <diretorio_origem> <diretorio_destino>
```

- `<diretorio_origem>`: Caminho da diretório a ser monitorada.
- `<diretorio_destino>`: Caminho onde os arquivos zipados serão salvos.

### Exemplo:

```bash
./compressogi /home/arquivos/textos /home/arquivos/textos_zip
```

## Observações

- Aplicação desenvolvida para sistemas baseados em Linux.
- Certificar-se que os diretórios fornecidos existem e têm permissões adequadas.
- Programa utiliza [Miniz](https://github.com/richgel999/miniz) para realizar a compressão em formato zip.
