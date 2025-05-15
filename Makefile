TARGET = compressogi
SRC = src/main.c src/filas/fila_comprimir.c src/miniz/miniz.c
CC = gcc
FLAGS = -pthread -Wformat-truncation=0

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) $(FLAGS) -o $(TARGET)

clean:
	rm $(TARGET)
