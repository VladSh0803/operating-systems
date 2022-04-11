all: program
CC=gcc
CFLAGS= -std=gnu99 -Wall -lrt -pthread
program: program.c
	${CC} -o program program.c ${CFLAGS}

_PHONY: clean all

clean:
	rm program