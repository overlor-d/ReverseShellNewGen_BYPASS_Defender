# Variables
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lws2_32 -liphlpapi
SRCDIR = src
BUILDDIR = build
BINDIR = bin
TARGET = $(BINDIR)/mon_prog

# Liste des fichiers source et objets
SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(BUILDDIR)/%.o, $(SOURCES))

# Règle par défaut : compiler le programme
all: $(TARGET)

# Règle pour l'exécutable final
$(TARGET): $(OBJECTS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "[INFO] Compilation terminée : $(TARGET)"

# Règle pour compiler les fichiers objets
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "[INFO] Compilation de $< en $@"

# Nettoyage des fichiers générés
clean:
	@rm -rf $(BUILDDIR) $(BINDIR)
	@echo "[INFO] Nettoyage terminé"

# Test rapide (par exemple : exécuter le mode serveur)
run-server: $(TARGET)
	@$(TARGET) -s

# Test rapide (par exemple : exécuter le mode client)
run-client: $(TARGET)
	@$(TARGET) -c 127.0.0.1
