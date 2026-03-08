# Kompilator i flagi (D_GNU_SOURCE jest tu kluczowe dla TEMP_FAILURE_RETRY!)
CC = gcc
CFLAGS = -Wall -Wextra -O2 -D_GNU_SOURCE

# Nazwa pliku wynikowego i źródłowego
TARGET = prog
SRC = zad_2_1.c

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