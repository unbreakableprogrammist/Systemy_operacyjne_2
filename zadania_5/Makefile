# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE

# Nazwa pliku wynikowego i źródłowego
TARGET = zadanie_1
SRC = zadanie_1.c

# Zabezpieczenie przed konfliktami nazw plików
.PHONY: all clean

# Główna reguła (wywoływana po wpisaniu 'make')
all: $(TARGET)

# Reguła kompilacji
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Reguła czyszczenia (wywoływana po wpisaniu 'make clean')
clean:
	rm -f $(TARGET)