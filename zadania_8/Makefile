CC = gcc
CFLAGS = -Wall -Wextra -pthread -O2

.PHONY: all clean

all: server

server: zadanie1.c
	$(CC) $(CFLAGS) zadanie1.c -o server

clean:
	rm -f server