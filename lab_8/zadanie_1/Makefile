CC = gcc
CFLAGS = -Wall -Wextra -O2

.PHONY: all clean

all: server client

# Kompilacja serwera
server: serwer.c biblioteka.h
	$(CC) $(CFLAGS) serwer.c -o server

# Kompilacja klienta
client: client.c biblioteka.h
	$(CC) $(CFLAGS) client.c -o client

# Czyszczenie skompilowanych plików
clean:
	rm -f server client