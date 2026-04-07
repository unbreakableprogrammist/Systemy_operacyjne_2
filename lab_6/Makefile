CC = gcc
CFLAGS = -Wall -Wextra -std=c99
TARGET = zad1
SRC = zad1.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) log.txt

.PHONY: all run clean
