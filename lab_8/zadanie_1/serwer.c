#include "biblioteka.h"

#define MAXBUF 576      // Poprawiono na jedno 'F'
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
        // POPRAWKA: zamieniono addr na clientAddr
        else if (memcmp(&clientAddr, &clients[i].clientAddr, sizeof(clients[i].clientAddr)) == 0) {
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
        if ((receivedBytes = TEMP_FAILURE_RETRY(recvfrom(fd, buf, MAXBUF, 0, (struct sockaddr*)&clientAddr, &addrLen))) < 0) {
            ERR("recvfrom");
        }

        buf[receivedBytes] = '\0'; // dodajemy na koncu tekstu znak konca tekstu
        int index = findIndex(clientAddr, clients);

        if (index >= 0) {
            // POPRAWKA: chunkNo zamiast chuckNo oraz ntohl zamiast nthol
            int32_t chunkNo = ntohl(*((int32_t*)buf));

            if (chunkNo > clients[index].chunkNo + 1) {
                continue;
            } else if (chunkNo == clients[index].chunkNo + 1) {
                // POPRAWKA: ntohl zamiast nthol
                int ending_flag = ntohl(*(((int32_t*)buf) + 1));

                if (ending_flag) { // jesli to ostatnia paczka
                    printf("Last Part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t));
                    clients[index].free = 1; // POPRAWKA: clients zamiast con
                } else {
                    // Zwykła paczka w środku pliku
                    printf("Part %d\n%s\n", chunkNo, buf + 2 * sizeof(int32_t));
                }
                clients[index].chunkNo++; // POPRAWKA: clients zamiast con
            }

            // Odsyłamy klientowi DOKŁADNIE to samo, co nam wysłał.
            // POPRAWKA: zamieniono addr na clientAddr oraz size na addrLen
            if (TEMP_FAILURE_RETRY(sendto(fd, buf, MAXBUF, 0, (struct sockaddr *)&clientAddr, addrLen)) < 0) {
                // EPIPE oznacza, że klient uciekł, zwalniamy miejsce
                if (EPIPE == errno) clients[index].free = 1; // POPRAWKA: clients zamiast con
                else ERR("sendto");
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
    int fd = bind_udp_socket(port); // Używamy naszej poprzednio napisanej funkcji
    server_work(fd);

    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");

    fprintf(stderr, "Server has terminated.\n");
    return EXIT_SUCCESS;
}