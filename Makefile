OS := $(shell uname -s)

CC = gcc
CFLAGS = -Wall -Wextra -I./src
LDFLAGS = -lws2_32
SRC = src/main.c
OBJ = main.o
BIN = myProgram

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

main.o: $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

clean:
	rm -f $(OBJ) $(BIN)

exec-serv: all
	@echo "=== Lancement du serveur ==="
	./$(BIN) -s

exec-client: all
	@echo "=== Lancement du client ==="
	./$(BIN) -c 192.168.1.174
