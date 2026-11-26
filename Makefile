# Makefile — biblioteca estática, servidor y cliente
# Uso: make | make check | make clean

CC = gcc
AR = ar
ARFLAGS = rcs
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2 -g
INCLUDES = -I.

BUILD = build
LIB = libmonitor.a
BINARIOS = server client

LIB_OBJS = $(BUILD)/bd.o $(BUILD)/episodios.o $(BUILD)/formato.o
COMUNES_OBJS = $(BUILD)/protocolo.o
SERVER_OBJS = $(BUILD)/server.o $(COMUNES_OBJS)
CLIENT_OBJS = $(BUILD)/client.o $(COMUNES_OBJS)
DEPS = $(sort $(LIB_OBJS:.o=.d) $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d))

.PHONY: all check clean

all: $(LIB) $(BINARIOS)

$(LIB): $(LIB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

server: $(SERVER_OBJS) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(LIB)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS)

$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(INCLUDES) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD):
	mkdir -p $@

check: $(LIB) $(BUILD)/lib_check
	./$(BUILD)/lib_check

$(BUILD)/lib_check: tests/lib_check.c $(LIB) | $(BUILD)
	$(CC) $(INCLUDES) $(CFLAGS) -o $@ $< $(LIB)

clean:
	rm -rf $(BUILD) $(LIB) $(BINARIOS)

-include $(DEPS)
