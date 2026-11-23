# Makefile — base del repositorio: libmonitor.a (G1, machado-a-g1-libmonitor.md)

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -g

OBJS = bd.o episodios.o formato.o

all: libmonitor.a

libmonitor.a: $(OBJS)
	ar rcs $@ $(OBJS)

%.o: %.c monitor.h
	$(CC) $(CFLAGS) -c $< -o $@

check: all
	$(CC) $(CFLAGS) -I. -o tests/lib_check tests/lib_check.c libmonitor.a
	./tests/lib_check

clean:
	rm -f *.o libmonitor.a server client tests/lib_check

.PHONY: all check clean
