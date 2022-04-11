all: processor generator
CC=gcc
CFLAGS= -std=gnu99 -Wall -lrt

processor: processor.c
	${CC} -o processor processor.c ${CFLAGS}

generator: generator.c
	${CC} -o generator generator.c ${CFLAGS}

_PHONY: clean all

clean:
	rm processor generator