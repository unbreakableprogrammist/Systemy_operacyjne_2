# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE

# Reguły specjalne (zabezpieczenie)
.PHONY: all clean

# Główna reguła buduje OBA programy
all: serwer client

# Reguła kompilacji serwera
serwer: serwer.c
	$(CC) $(CFLAGS) -o serwer serwer.c

# Reguła kompilacji klienta
client: client.c
	$(CC) $(CFLAGS) -o client client.c

# Reguła czyszczenia (usuwa oba programy)
clean:
	rm -f serwer client