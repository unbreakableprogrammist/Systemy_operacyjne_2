// Włącza GNU-specyficzne funkcje systemowe
#define _GNU_SOURCE
// Nagłówek do obsługi funkcji znaków (isalnum)
#include <ctype.h>
// Nagłówek do obsługi błędów (errno)
#include <errno.h>
// Nagłówek do obsługi flag otwierania plików (O_RDONLY)
#include <fcntl.h>
// Nagłówek z limitami (PIPE_BUF)
#include <limits.h>
// Nagłówek do obsługi wejścia/wyjścia (printf)
#include <stdio.h>
// Nagłówek do obsługi alokacji pamięci i exit
#include <stdlib.h>
// Nagłówek do operacji na plikach (S_IRUSR, S_IWUSR, itd.)
#include <sys/stat.h>
// Nagłówek z typami systemowymi (pid_t)
#include <sys/types.h>
// Nagłówek z funkcjami POSIX (read, write, close, unlink)
#include <unistd.h>

// Makro do obsługi błędów: wypisuje informację o błędzie, plik i linię, a następnie kończy program
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// Funkcja wyświetlająca instrukcję użytkowania programu
void usage(char *name)
{
    // Wypisuje komunikat z poprawną składnią uruchomienia
    fprintf(stderr, "USAGE: %s fifo_file\n", name);
    // Kończy program z kodem błędu
    exit(EXIT_FAILURE);
}

// Funkcja do odczytywania i przetwarzania danych z FIFO
void read_from_fifo(int fifo)
{
    // Licznik bajtów przeczytanych z FIFO
    ssize_t count;
    // Bufor o rozmiarze PIPE_BUF na całą wiadomość (PID + tekst)
    char buffer[PIPE_BUF];

    // Pętla czytająca dane z FIFO
    do
    {
        // Czyta paczkę PIPE_BUF bajtów z FIFO, z obsługą przerwań sygnałami
        count = TEMP_FAILURE_RETRY(read(fifo, buffer, PIPE_BUF));
        // Jeśli read zwrócił -1, oznacza to błąd
        if (count < 0)
            ERR("read");
        // Jeśli count == 0, oznacza koniec transmisji
        
        // Jeśli przeczytaliśmy jakieś dane (count > 0)
        if(count>0){ 
            // Wypisuje PID procesu klienta, który wysłał wiadomość (zapisany na początku bufora)
            printf("\nPID :%d : \n", *((pid_t *)buffer));
            // Pętla iterująca przez wszystkie bajty wiadomości (od pozycji pid_t do końca)
            for(int i=sizeof(pid_t);i<count;i++){
                // Jeśli bajt jest znakiem alfanumerycznym, wypisz go
                if(isalnum(buffer[i])) printf("%c",buffer[i]);
            }
        }
    // Pętla trwa, dopóki read zwraca > 0
    } while (count > 0);
}

// Główna funkcja programu serwera
int main(int argc, char **argv)
{
    // Deskryptor pliku dla FIFO
    int fifo;
    // Sprawdzenie, czy podano dokładnie 2 argumenty (nazwa programu + nazwa FIFO)
    if (argc != 2)
        usage(argv[0]);
    // S_IRUSR  -  uprawnienie dla właściciela do odczytu
    // S_IWUSR -  uprawnienie dla właściciela do pisania
    // S_IRGRP -  uprawnienie dla grupy do odczytu
    // S_IWGRP -  uprawnienie dla grupy do pisania
    
    // Próbuje utworzyć FIFO z odpowiednimi uprawnieniami
    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0)
    {
        // Jeśli błąd nie dotyczy już istniejącego FIFO (EEXIST), wypisz błąd
        if (errno != EEXIST)
            ERR("mkfifo create");
    }
    
    // Otwiera FIFO w trybie odczytu (do odebrania danych)
    if ((fifo = open(argv[1], O_RDONLY)) < 0)
        ERR("open");

    // Odczytuje i przetwarza dane z FIFO
    read_from_fifo(fifo);

    // Zamyka deskryptor FIFO
    if(close(fifo)<0) ERR("close fifo");

    // Usuwa plik FIFO z systemu plików po zakończeniu pracy
    if(unlink(argv[1]) < 0) ERR("unlink");

    // Zwraca sukces
    return EXIT_SUCCESS;
}