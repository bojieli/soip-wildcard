CFLAGS=-Wall -g -O2

default:
	gcc ${CFLAGS} -o wildcard wildcard.c server.c -lpthread
