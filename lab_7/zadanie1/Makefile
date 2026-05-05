# Kompilator i flagi (włączamy wszystkie ostrzeżenia i standard POSIX)
CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99

# Lista plików, które chcemy stworzyć
TARGETS = serwer client_tcp client_local

# Domyślny cel, odpala się po wpisaniu 'make'
all: $(TARGETS)

# Zasada kompilacji serwera
serwer: serwer.c biblioteka.h
	$(CC) $(CFLAGS) serwer.c -o serwer

# Zasada kompilacji klienta TCP
client_tcp: client_tcp.c biblioteka.h
	$(CC) $(CFLAGS) client_tcp.c -o client_tcp

# Zasada kompilacji klienta lokalnego (UNIX)
client_local: client_local.c biblioteka.h
	$(CC) $(CFLAGS) client_local.c -o client_local

# Cel "clean" służy do posprzątania plików po pracy (komenda: make clean)
clean:
	rm -f $(TARGETS) *.o *~