# Makefile — libmonitor.a (G1) y binarios server/client (G2, felix-b-g2-protocolo.md)

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g

OBJS = bd.o episodios.o formato.o

all: libmonitor.a server client

libmonitor.a: $(OBJS)
	ar rcs $@ $(OBJS)

server: server.o libmonitor.a
	$(CC) $(CFLAGS) -o $@ server.o -L. -lmonitor

client: client.o
	$(CC) $(CFLAGS) -o $@ client.o

%.o: %.c monitor.h
	$(CC) $(CFLAGS) -c $< -o $@

check: all
	$(CC) $(CFLAGS) -I. -o tests/lib_check tests/lib_check.c libmonitor.a
	./tests/lib_check

clean:
	rm -f *.o libmonitor.a server client tests/lib_check

.PHONY: all check clean
