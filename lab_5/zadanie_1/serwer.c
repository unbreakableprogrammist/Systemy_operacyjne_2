#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
void usage(char *name)
{
    fprintf(stderr, "USAGE: %s fifo_file\n", name);
    exit(EXIT_FAILURE);
}

void read_from_fifo(int fifo)
{
    ssize_t count; // zmienna do zapisywania ile wczytamy z fifo
    char buffer[PIPE_BUF]; // bufor na cala wiadomosc (PID + tekst)

    do
    {
        count = TEMP_FAILURE_RETRY(read(fifo, buffer, PIPE_BUF)); // czytamy sobie cala wiadomosc z fifo
        if (count < 0)
            ERR("read"); // jesli read zwrocil -1 to oznacza ze byl jakis blad
        // jesli count == 0 to oznacza ze koniec wczytywania
        if(count>0){ 
            printf("\nPID :%d : \n", *((pid_t *)buffer));
            for(int i=sizeof(pid_t);i<count;i++){
                if(isalnum(buffer[i])) printf("%c",buffer[i]);
            }
        }
    } while (count > 0);
}

int main(int argc, char **argv)
{
    int fifo; // file descritpor na fifo
    if (argc != 2)
        usage(argv[0]);
    // S_IRUSR  -  permission for owner to read
    // S_IWUSR -  permission for owner to write
    // S_IRGRP -  permission for group to read
    // S_IWGRP -  permission for group to write
    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0)
    {
        if (errno != EEXIST)
            ERR("mkfifo create"); // jesli fifo nie istnieje to blad tworzenia
    }
    if ((fifo = open(argv[1], O_RDONLY)) < 0)
        ERR("open"); // otwieramy fifo do odczytu

    read_from_fifo(fifo); // funkcja do odczytu danych z fifo

    if(close(fifo)<0) ERR("close fifo");

    if(unlink(argv[1]) < 0) ERR("unlink"); // usuwamy fifo po zakonczeniu pracy programu

    return EXIT_SUCCESS;
}