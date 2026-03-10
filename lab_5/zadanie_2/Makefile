# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE

# Zabezpieczenie przed konfliktami nazw
.PHONY: all clean

# Główna reguła - buduje oba programy naraz!
all: prog1 prog2

# Reguła dla Etapu 1
prog1: zad_2_1.c
	$(CC) $(CFLAGS) -o prog1 zad_2_1.c

# Reguła dla Etapu 2
prog2: zad_2_2.c
	$(CC) $(CFLAGS) -o prog2 zad_2_2.c

# Reguła czyszczenia - usuwa oba pliki wynikowe
clean:
	rm -f prog1 prog2