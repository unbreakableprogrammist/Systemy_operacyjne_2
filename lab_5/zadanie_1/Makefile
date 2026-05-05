# Ustawienie kompilator na gcc
CC = gcc
# Flagi kompilacji: -Wall (wszystkie ostrzeżenia), -Wextra (dodatkowe ostrzeżenia), 
# -O2 (optymalizacja poziom 2), -D_GNU_SOURCE (włączenie GNU-specyficznych funkcji)
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE

# Oznaczenie, że te reguły są fikcyjne i nie tworzą plików o tych nazwach
# .PHONY zapobiega konfliktom, jeśli będą pliki o nazwach all lub clean
.PHONY: all clean

# Domyślna reguła - buduje oba programy (serwer i client)
all: serwer client

# Reguła kompilacji programu serwer z pliku serwer.c
# $(CC) używa zmiennej CC (gcc), $(CFLAGS) używa zmiennej CFLAGS
# -o serwer określa nazwę pliku wyjściowego
serwer: serwer.c
	$(CC) $(CFLAGS) -o serwer serwer.c

# Reguła kompilacji programu client z pliku client.c
# $(CC) używa zmiennej CC (gcc), $(CFLAGS) używa zmiennej CFLAGS
# -o client określa nazwę pliku wyjściowego
client: client.c
	$(CC) $(CFLAGS) -o client client.c

# Reguła do czyszczenia - usuwa skompilowane programy (serwer i client)
# rm -f wymusza usunięcie bez pytania (-f = force)
clean:
	rm -f serwer client