
# Simple Makefile for Awalé client/server
# Usage:
#   make            # builds both server and client
#   make server     # builds the server only
#   make client     # builds the client only
#   make run-server # runs the server
#   make run-client # runs the client
#   make clean      # removes build artifacts
#
# Notes:
# - Uses -pthread because the server uses POSIX threads.
# - Should work on Linux and macOS. If you use clang, it also understands -pthread.

# --- Configuration ---
CC      ?= gcc
CSTD    ?= c11
CFLAGS  ?= -std=$(CSTD) -Wall -Wextra -Wconversion -Wshadow -Wpedantic -O2
# You can add -g for debugging by running: make CFLAGS+=' -g'
LDFLAGS ?=
LDLIBS  ?= -pthread

# TCP port is defined in sources (PORT 8080); override at compile-time if needed:
# make CFLAGS+=' -DPORT=9090'
# Or set server address for client: make CFLAGS+=' -DSERVER_ADDR=\"192.168.1.10\"'

# --- Files ---
CLIENT_SRC := client.c
SERVER_SRC := server.c users.c
CLIENT_BIN := client
SERVER_BIN := server

CLIENT_OBJ := $(CLIENT_SRC:.c=.o)
SERVER_OBJ := $(SERVER_SRC:.c=.o)

# --- Default target ---
.PHONY: all
all: $(SERVER_BIN) $(CLIENT_BIN)

# --- Build rules ---
$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

# Pattern rule for .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# --- Convenience targets ---
.PHONY: client server run-server run-client clean help

client: $(CLIENT_BIN)
server: $(SERVER_BIN)

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(CLIENT_BIN) $(SERVER_BIN)

help:
	@echo "Targets:"
	@echo "  all         - build server and client (default)"
	@echo "  server      - build the server"
	@echo "  client      - build the client"
	@echo "  run-server  - run the server"
	@echo "  run-client  - run the client"
	@echo "  clean       - remove build artifacts"
	@echo ""
	@echo "Variables you can override, e.g. 'make CC=clang CFLAGS+=\" -g\"'"
	@echo "  CC, CFLAGS, LDFLAGS, LDLIBS"
	@echo ""
	@echo "Tips:"
	@echo "  - Add -g to CFLAGS for debugging."
	@echo "  - You can change the PORT at compile time with CFLAGS+=' -DPORT=9090'."
