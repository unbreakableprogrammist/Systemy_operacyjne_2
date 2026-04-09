CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11
LDFLAGS = -pthread

.PHONY: all clean

all: zadanie1 semafor

zadanie1: zadanie1.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

semafor: semafor.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f zadanie1 semafor
