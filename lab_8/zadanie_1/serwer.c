#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bits/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include 'biblioteka.h'

#define MAXBUFF 576
#define MAXCLIENTS 5

void usage(char *name) { fprintf(stderr, "USAGE: %s port\n", name); }

struct connection {
    int free; // czy dany slot jest zajety przez klienta czy pusty
    int32_t chunkNo; // ktora to paczka od niego
    struct sockaddr_in clientAddr; // adres klienta i inne takie
};

// funkcja ktora sluzy do znalezenia indeksu w tablicy clients ( jesli klient jest podlaczony)
int findIndex(struct sockaddr_in clientAddr, struct connection clients[MAXCLIENTS]) {
    int empty = -1;
    int pos = -1;
    for (int i = 0; i < MAXCLIENTS; i++) {
        if (clients[i].free) {
            empty = i; // na wypadek gdyby to byl nowy klient
        }
        else if (memcmp(&addr,&clients[i].clientAddr,sizeof(clients[i].clientAddr)) == 0) {
            pos = i; // jesli znalezlismy naszego klienta to zapamietujemy
            break;
        }
    }
    // zapisujemy nowego klienta
    if (pos == -1 && empty != -1) {
        clients[empty].free = 0;
        clients[empty].chunkNo = 0;
        clients[empty].clientAddr = clientAddr;
        pos = empty;
    }
    return pos;
}

void server_work(int fd) {
    struct sockaddr_in clientAddr; // na przychodzacego klienta
    struct connection clients[MAXCLIENTS]; // na przychodzace adresy
    char buf[MAXBUF + 1]; // Bufor na dane (dodajemy +1 na znak końca stringa '\0')
    for (int i = 0; i < MAXCLIENTS; i++) {
        clients[i].free = 1;
    }
    while (1) {
        // czytamy dane
        int receivedBytes;
        socklen_t addrLen = sizeof(clientAddr);
        if ((receivedBytes = TEMP_FAILURE_RETRY(recvfrom(fd,buf,MAXBUF,0,(struct sockaddr*)&clientAddr,&addrLen))) < 0) {
            ERR("recvfrom");
        }
        buf[receivedBytes] = '\0'; // dodajemy na koncu tekstu znak konca tekstu
        int index  = findIndex(clientAddr,clients);
        if (index >= 0) {
            int chuckNo = nthol(*((int32_t*)buf)); // wyciagamy numer paczki (zamieniamy buf na wskaznik na int32 -> wyciagamy wartosc -> zmieniamy na zrozumiala liczbe)
            if (chuckNo > clients[index].chunkNo + 1) { // jesli numer tej paczki jest wiekszy niz numer ostatniej paczki + 1 to continute
                continue;
            }else if (chuckNo == clients[index].chunkNo + 1) {
                
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (sethandler(SIG_IGN, SIGPIPE)) // ignorujemy sigpipe
        ERR("Seting SIGPIPE:");
    int port = atoi(argv[1]);
    int fd = bind_udp_socket(port);
    server_work(fd);
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}