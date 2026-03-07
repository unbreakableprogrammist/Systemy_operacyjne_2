# Kompilator i flagi
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Nazwa pliku wykonywalnego
TARGET = server
# Plik źródłowy
SRC = server.c

# Główna reguła (wywoływana po wpisaniu 'make')
all: $(TARGET)

# Reguła kompilacji
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Reguła czyszczenia (wywoływana po wpisaniu 'make clean')
clean:
	rm -f $(TARGET)