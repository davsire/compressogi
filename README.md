# Compressogi

Programa que monitora e comprime arquivos em tempo real e paralelamente, a partir de pastas origem e destino.

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
./compressogi <pasta_origem> <pasta_destino> <remover_arquivo>
```

- `<pasta_origem>`: Caminho da pasta a ser monitorada.
- `<pasta_destino>`: Caminho onde os arquivos zipados serão salvos.
- `<remover_arquivo>`: Parâmetro opcional, indica se é para excluir arquivos da pasta origem após compressão (aceita valores `s` e `n`, sendo `n` o padrão).

### Exemplo:

```bash
./compressogi /home/arquivos/textos /home/arquivos/textos_zip
```

## Observações

- Aplicação desenvolvida para sistemas baseados em Linux.
- Certificar-se que os diretórios fornecidos existem e têm permissões adequadas.
