#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
    (__extension__({                               \
        long int __result;                         \
        do                                         \
            __result = (long int)(expression);     \
        while (__result == -1L && errno == EINTR); \
        __result;                                  \
    }))
#endif

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *name) { fprintf(stderr, "USAGE: %s socket port\n", name); }

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction)); // zeruje strukturę obsługi sygnału
    act.sa_handler = f;                        // ustawia funkcję, która ma obsługiwać sygnał
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1; // rejestruje handler dla danego sygnału
    return 0;
}

int make_tcp_socet()
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0); // tworzy gniazdo TCP dla IPv4
    if (sock < 0)
        ERR("socket"); // błąd przy tworzeniu gniazda
    return sock;         // zwraca deskryptor gniazda
}

int bind_tcp_socket(int port, int backlog_size)
{
    struct sockaddr_in addr;             // struktura z adresem IPv4 i portem
    int socketfd = make_tcp_socet();     // tworzy gniazdo TCP
    memset(&addr, 0, sizeof(addr));      // czyści strukturę adresu
    addr.sin_family = AF_INET;           // ustawia rodzinę adresów na IPv4
    addr.sin_port = htons(port);         // zamienia port na kolejność bajtów sieciowych
    addr.sin_addr.s_addr = htons(INADDR_ANY); // nasłuch na wszystkich interfejsach
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        ERR("setsockopt"); // pozwala szybko ponownie użyć portu
    if (bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        ERR("bind"); // przypisuje port do gniazda
    if (listen(socketfd, backlog_size) < 0)
        ERR("listen"); // ustawia gniazdo w tryb nasłuchiwania
    return socketfd; // zwraca gotowe gniazdo serwera
}

int make_local_socket(char *name, struct sockaddr_un *adr)
{
    int sock;
    sock = socket(PF_UNIX, SOCK_STREAM, 0); // tworzy gniazdo lokalne UNIX
    if (sock < 0)
        ERR("socket"); // błąd przy tworzeniu gniazda

    memset(adr, 0, sizeof(struct sockaddr_un)); // czyści strukturę adresu UNIX
    adr->sun_family = AF_UNIX;                   // ustawia rodzinę adresów UNIX
    strncpy(adr->sun_path, name, sizeof(adr->sun_path) - 1); // wpisuje ścieżkę socketu

    return sock; // zwraca deskryptor gniazda
}

int bind_local_socket(char *name, int backlog_size)
{
    struct sockaddr_un addr;
    int socketfd;
    
    // Zabezpieczenie: usuń stary plik z dysku, chyba że go nie ma (ENOENT)
    if (unlink(name) < 0 && errno != ENOENT)
        ERR("unlink");
        
    // Wywołaj nasz pierwszy klocek. Dostaniemy socketfd i wypełnioną zmienną 'addr'
    socketfd = make_local_socket(name, &addr);
    
    // "Zaślubiny": Połącz deskryptor (socketfd) z plikiem opisanym w strukturze 'addr'
    // Używamy makra SUN_LEN dla precyzyjnego rozmiaru adresu.
    if (bind(socketfd, (struct sockaddr *)&addr, SUN_LEN(&addr)) < 0)
        ERR("bind");
        
    // Przełącz gniazdo w tryb pasywny (nasłuchujący gości) z kolejką backlog_size
    if (listen(socketfd, backlog_size) < 0)
        ERR("listen");
        
    // Zwróć w pełni gotowe gniazdo serwera
    return socketfd;
}