#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MSG_SIZE (PIPE_BUF - sizeof(pid_t))
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *name)
{
    fprintf(stderr, "USAGE: %s fifo_file file\n", name);
    exit(EXIT_FAILURE);
}

void write_to_fifo(int fifo, int file) {
    ssize_t count;
    char buffer[PIPE_BUF]; // bufor na cala wiadomosc (PID + tekst)
    char *buf;

    // Zapisujemy PID binarnie na samym początku bufora
    *((pid_t *)buffer) = getpid();

    // Ustawiamy wskaźnik czytania zaraz za zapisanym PID-em
    buf = buffer + sizeof(pid_t); 

    do {
        count = TEMP_FAILURE_RETRY(read(file, buf, MSG_SIZE)); // czytamy z pliku do bufora
        if (count < 0) ERR("read");
        
        if (count > 0) { // Przetwarzamy i wysyłamy tylko, jeśli wczytaliśmy jakieś dane
            if (count < MSG_SIZE) {
                // Zabezpieczenie: jeśli wczytano mniej niż MSG_SIZE (np. koniec pliku),
                // dopełniamy resztę zerami, by zawsze wysłać idealnie PIPE_BUF bajtów.
                memset(buf + count, 0, MSG_SIZE - count);
            }
            // Zawsze wysyłamy całą paczkę PIPE_BUF, dzięki czemu zachowujemy atomowość
            if (write(fifo, buffer, PIPE_BUF) < 0) {
                ERR("Write");
            }
        }
    } while (count > 0);
}

int main(int argc, char** argv) { 
    int fifo, file;
    
    // Usunięto cudzysłowy z argv[0]
    if (argc != 3) usage(argv[0]);

    // Otwieramy/tworzymy FIFO obronnie
    if (mkfifo(argv[1], S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP) < 0) {
        if (errno != EEXIST) ERR("fifo create"); 
    }
    
    // Klient otwiera do pisania
    fifo = open(argv[1], O_WRONLY); 
    if (fifo < 0) ERR("open fifo");
    
    // Klient otwiera plik z danymi do czytania
    file = open(argv[2], O_RDONLY); 
    if (file < 0) ERR("open file");

    // Rozpoczynamy przesyłanie
    write_to_fifo(fifo, file);

    // Sprzątamy deskryptory (ale nie usuwamy samego pliku FIFO, zostawiamy to serwerowi)
    if (close(file) < 0) ERR("close file");
    if (close(fifo) < 0) ERR("close fifo");
    
    return EXIT_SUCCESS;
}