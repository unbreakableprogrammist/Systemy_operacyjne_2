// Włącza GNU-specyficzne funkcje systemowe
#define _GNU_SOURCE
// Dodatkowe nagłówki do obsługi błędów
#include <errno.h>
// Nagłówek do obsługi flag otwierania plików (O_WRONLY, O_RDONLY, itp.)
#include <fcntl.h>
// Nagłówek z limitami (PIPE_BUF)
#include <limits.h>
// Nagłówek do obsługi wejścia/wyjścia
#include <stdio.h>
// Nagłówek do obsługi alokacji pamięci (malloc, exit)
#include <stdlib.h>
// Nagłówek do obsługi stringów (memset)
#include <string.h>
// Nagłówek do operacji na plikach (S_IRUSR, S_IWUSR, itd.)
#include <sys/stat.h>
// Nagłówek z typami systemowymi (pid_t)
#include <sys/types.h>
// Nagłówek z funkcjami POSIX (read, write, close, unlink)
#include <unistd.h>

// Definiuje rozmiar wiadomości: cały bufor PIPE_BUF minus miejsce na PID
#define MSG_SIZE (PIPE_BUF - sizeof(pid_t))
// Makro do obsługi błędów: wypisuje informację o błędzie, plik i linię, a następnie kończy program
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// Funkcja wyświetlająca instrukcję użytkowania programu
void usage(char *name)
{
    // Wypisuje komunikat z poprawną składnią uruchomienia
    fprintf(stderr, "USAGE: %s fifo_file file\n", name);
    // Zakańcza program z kodem błędu
    exit(EXIT_FAILURE);
}

// Funkcja do wysyłania zawartości pliku do FIFO w paczkach atomowych
void write_to_fifo(int fifo, int file) {
    // Licznik bajtów przeczytanych z pliku
    ssize_t count;
    // Bufor o rozmiarze PIPE_BUF na całą wiadomość (PID + tekst)
    char buffer[PIPE_BUF];
    // Wskaźnik do części bufora zawierającej dane (bez PID)
    char *buf;

    // Zapisuje PID bieżącego procesu na początek bufora
    *((pid_t *)buffer) = getpid();

    // Ustawia wskaźnik buf zaraz za zapisanym PID-em (przesunięcie o rozmiar pid_t)
    buf = buffer + sizeof(pid_t); 

    // Pętla czytająca plik chunk po chunk
    do {
        // Czyta dane z pliku do bufora (od pozycji buf), z obsługą przerwań sygnałami
        count = TEMP_FAILURE_RETRY(read(file, buf, MSG_SIZE));
        // Jeśli read zwrócił -1, oznacza to błąd
        if (count < 0) ERR("read");
        
        // Jeśli przeczytaliśmy jakieś dane (count > 0)
        if (count > 0) {
            // Jeśli przeczytano mniej niż MSG_SIZE (np. koniec pliku)
            if (count < MSG_SIZE) {
                // Dopełnia resztę bufora zerami, by zawsze wysłać dokładnie PIPE_BUF bajtów
                memset(buf + count, 0, MSG_SIZE - count);
            }
            // Wysyła całą paczkę PIPE_BUF bajtów do FIFO (zawsze atomowo)
            if (write(fifo, buffer, PIPE_BUF) < 0) {
                // Jeśli write zwrócił -1, oznacza to błąd
                ERR("Write");
            }
        }
    // Pętla trwa, dopóki read zwraca > 0 (koniec pliku zwraca 0)
    } while (count > 0);
}

// Główna funkcja programu klienta
int main(int argc, char** argv) { 
    // Deskryptor pliku dla FIFO
    int fifo;
    // Deskryptor pliku dla pliku wejściowego
    int file;
    
    // Sprawdzenie, czy podano dokładnie 3 argumenty (nazwa programu + FIFO + plik)
    if (argc != 3) usage(argv[0]);

    // Próbuje utworzyć FIFO z odpowiednimi uprawnieniami
    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0) {
        // Jeśli błąd nie dotyczy już istniejącego FIFO (EEXIST), wypisz błąd
        if (errno != EEXIST) ERR("fifo create"); 
    }
    
    // Otwiera FIFO w trybie pisania (do wysłania danych)
    fifo = open(argv[1], O_WRONLY); 
    // Jeśli otwieranie się nie powiodło
    if (fifo < 0) ERR("open fifo");
    
    // Otwiera plik podany jako argument do odczytu
    file = open(argv[2], O_RDONLY); 
    // Jeśli otwieranie się nie powiodło
    if (file < 0) ERR("open file");

    // Wysyła zawartość pliku do FIFO
    write_to_fifo(fifo, file);

    // Zamyka deskryptor pliku wejściowego
    if (close(file) < 0) ERR("close file");
    // Zamyka deskryptor FIFO (ale nie usuwa pliku FIFO - robi to serwer)
    if (close(fifo) < 0) ERR("close fifo");
    
    // Zwraca sukces
    return EXIT_SUCCESS;
}