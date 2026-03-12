# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE

# Nazwy plików wynikowych
TARGETS = zadanie_1 zadanie_2 gemini_code

# Zabezpieczenie przed konfliktami nazw
.PHONY: all clean

# Główna reguła (wywoływana po wpisaniu 'make') - kompiluje wszystko
all: $(TARGETS)

# Reguły dla poszczególnych programów
zadanie_1: zadanie_1.c
	$(CC) $(CFLAGS) -o zadanie_1 zadanie_1.c

zadanie_2: zadanie_2.c
	$(CC) $(CFLAGS) -o zadanie_2 zadanie_2.c

gemini_code: gemini_code.c
	$(CC) $(CFLAGS) -o gemini_code gemini_code.c

# Reguła czyszczenia (wywoływana po wpisaniu 'make clean')
clean:
	rm -f $(TARGETS)
	rm -f *.fifo