##########################################################
# 🏗️  Makefile — Projet AwaleGame
# Auteur : Ines Chebbi - Malek Aouadi
# Objectif : compiler proprement le serveur, le client,
#            et les modules partagés.
##########################################################

# --- Compilateur et options ---
CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -pthread -Iinclude
LDFLAGS := -pthread

# --- Répertoires ---
SRC_DIR := src
BIN_DIR := bin
INCLUDE := include
TEST_DIR := tests

# --- Fichiers sources ---
SERVER_SRC := $(SRC_DIR)/server.c $(SRC_DIR)/users.c $(SRC_DIR)/awale.c $(SRC_DIR)/games.c $(SRC_DIR)/commandes.c
CLIENT_SRC := $(SRC_DIR)/client.c $(SRC_DIR)/awale.c

# --- Exécutables ---
SERVER_BIN := $(BIN_DIR)/server
CLIENT_BIN := $(BIN_DIR)/client

# --- Règle par défaut ---
all: dirs $(SERVER_BIN) $(CLIENT_BIN)

##########################################################
# 🔹 Compilation du serveur
##########################################################
$(SERVER_BIN): $(SERVER_SRC)
	@echo "🔧 Compilation du serveur..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Serveur compilé → $(SERVER_BIN)"

##########################################################
# 🔹 Compilation du client
##########################################################
$(CLIENT_BIN): $(CLIENT_SRC)
	@echo "🔧 Compilation du client..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Client compilé → $(CLIENT_BIN)"

##########################################################
# 📁 Création du répertoire bin si nécessaire
##########################################################
dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p data

##########################################################
# 🧪 Tests unitaires (si tu as des fichiers dans tests/)
##########################################################
test:
	@echo "🧪 Lancement des tests unitaires..."
	@$(MAKE) -s -C $(TEST_DIR)

##########################################################
# 🧹 Nettoyage
##########################################################
clean:
	@echo "🧹 Nettoyage..."
	rm -rf $(BIN_DIR) *.o *.out
	@echo "🗑️  Fichiers objets et exécutables supprimés."

re: clean all

##########################################################
# 📜 Aide
##########################################################
help:
	@echo ""
	@echo "Commandes disponibles :"
	@echo "  make              → compile tout (client + serveur)"
	@echo "  make server       → compile uniquement le serveur"
	@echo "  make client       → compile uniquement le client"
	@echo "  make test         → exécute les tests (si présents)"
	@echo "  make clean        → supprime les exécutables"
	@echo "  make re           → clean + build complet"
	@echo ""

##########################################################
# Raccourcis explicites
##########################################################
server: $(SERVER_BIN)
client: $(CLIENT_BIN)

.PHONY: all clean re help dirs test server client
