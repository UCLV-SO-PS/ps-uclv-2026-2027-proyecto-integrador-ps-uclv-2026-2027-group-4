# Makefile — biblioteca estática, servidor y cliente
# Uso: make | make check | make clean

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g

BUILD = build
LIB = libmonitor.a
BINARIOS = server client

LIB_OBJS = $(BUILD)/bd.o $(BUILD)/episodios.o $(BUILD)/formato.o
SERVER_OBJS = $(BUILD)/server.o
CLIENT_OBJS = $(BUILD)/client.o

.PHONY: all check clean

all: $(LIB) $(BINARIOS)

$(LIB): $(LIB_OBJS)
	ar rcs $@ $^

server: $(SERVER_OBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(LIB)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS)

$(BUILD)/%.o: %.c monitor.h | $(BUILD)
	$(CC) $(CFLAGS) -I. -c $< -o $@

$(BUILD):
	mkdir -p $@

check: $(LIB) $(BUILD)/lib_check
	./$(BUILD)/lib_check

$(BUILD)/lib_check: tests/lib_check.c $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) -I. -o $@ $< $(LIB)

clean:
	rm -rf $(BUILD) $(LIB) $(BINARIOS)
