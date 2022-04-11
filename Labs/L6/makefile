all: server client
CC=gcc
CFLAGS= -std=gnu99 -Wall

server: server.c
	${CC} -o server server.c ${CFLAGS}
client: client.c
	${CC} -o client client.c ${CFLAGS}	

_PHONY: clean all

clean:
	rm server client