# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Nazwa pliku wykonywalnego
TARGET = serwer
# Plik źródłowy
SRC = serwer.c

# Główna reguła (wywoływana po wpisaniu 'make')
all: $(TARGET)

# Reguła kompilacji
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Reguła czyszczenia (wywoływana po wpisaniu 'make clean')
clean:
	rm $(TARGET)