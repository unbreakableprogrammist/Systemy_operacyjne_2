#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// Nasze standardowe makro do obsługi błędów
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int main(int argc, char** argv) {
    // Sprawdzamy, czy dostaliśmy host i port (np. "localhost" "2000")
    if (argc != 3) {
        fprintf(stderr, "USAGE: %s host port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* host = argv[1];
    char* port = argv[2];

    // Zgodnie z zadaniem: klient najpierw wypisuje swój PID
    pid_t my_pid = getpid();
    printf("PID=%d\n", my_pid);

    // ============================================================================
    // ETAP 1: ZNALEZIENIE SERWERA I POŁĄCZENIE (getaddrinfo)
    // ============================================================================
    
    // Zamiast ręcznie wpisywać IP, używamy getaddrinfo, 
    // które samo ogarnie czy to "localhost", "127.0.0.1" czy inne IP
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;       // Chcemy IPv4
    hints.ai_socktype = SOCK_STREAM; // Chcemy protokół strumieniowy (TCP)

    // Szukamy adresu naszego serwera
    if (getaddrinfo(host, port, &hints, &res) != 0) ERR("getaddrinfo");

    // Tworzymy gniazdo na podstawie tego, co znalazło getaddrinfo
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) ERR("socket");

    // Dzwonimy pod wskazany adres!
    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) ERR("connect");
    
    // Zwalniamy pamięć, bo adres już nam nie jest potrzebny
    freeaddrinfo(res);

    // ============================================================================
    // ETAP 2: WYSYŁANIE DANYCH (PID JAKO TEKST)
    // ============================================================================
    
    // Zamieniamy naszą liczbę PID na zwykły tekst (string)
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", my_pid);
    
    // Wysyłamy tekst do serwera (strlen nie liczy \0 i o to nam chodzi)
    if (write(sock, pid_str, strlen(pid_str)) < 0) ERR("write");

    // Dobre maniery w TCP: mówimy serwerowi "to wszystko, co miałem do powiedzenia"
    // SHUT_WR zamyka naszą stronę do pisania, ale nadal możemy czytać odpowiedzi
    shutdown(sock, SHUT_WR);

    // ============================================================================
    // ETAP 3: ODBIERANIE WYNIKU (int16_t)
    // ============================================================================
    
    int16_t net_sum; // Zmienna na dane prosto z sieci (Network Byte Order)
    
    // Czytamy równe 2 bajty (bo tyle zajmuje int16_t)
    int status = read(sock, &net_sum, sizeof(int16_t));
    
    if (status < 0) {
        ERR("read");
    } 
    else if (status == sizeof(int16_t)) {
        // SUKCES! Przyszły 2 bajty. 
        // UWAGA: Serwer wysłał to w formacie sieciowym (htons). 
        // My musimy to odkodować na format naszego komputera używając ntohs 
        // (Network TO Host Short).
        int16_t sum = ntohs(net_sum);
        
        // Zgodnie z poleceniem, wypisujemy wynik
        printf("SUM=%d\n", sum);
    } 
    else {
        // Jeśli serwer uciął połączenie w połowie i wysłał np. tylko 1 bajt
        printf("Blad: Serwer nie przyslal pelnych 2 bajtow.\n");
    }

    // ============================================================================
    // KONIEC: SPRZĄTANIE
    // ============================================================================
    
    // Zamykamy gniazdo i kończymy program
    close(sock);
    return 0;
}