TARGET = compressogi
SRC = src/main.c src/fila/fila_arquivo.c src/prod_cons/prod_cons.c src/miniz/miniz.c
CC = gcc
FLAGS = -pthread -Wformat-truncation=0

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) $(FLAGS) -o $(TARGET)

clean:
	rm $(TARGET)
