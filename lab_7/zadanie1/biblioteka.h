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

int make_tcp_socet(){
    int sock;
    sock = socket(PF_INET,SOCK_STREAM,0); // robi nam port na TCP
    if(sock < 0) ERR("socket");
    return sock;
}

int bind_tcp_socket(int port,int backlog_size){
    struct sockaddr_in addr; // struktura przechowująca adresy i port i tak dalej
    int socketfd = make_tcp_socet();
    memset(&addr,0,sizeof(addr)); // zerujemy strukturę
    addr.sin_family = AF_INET; // uzywamy IPv4
    addr.sin_port = htons(port); // port, htons - host to network short, zamienia kolejność bajtów na taka dobra dla sieci 
    addr.sin_addr.s_addr = htons(INADDR_ANY); // adres IP, INADDR_ANY - nasłuchuj na wszystkich interfejsach IP(czyli na wszyskich sieciach podlaczonych)
    if(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) ERR("setsockopt"); // ustawiamy zeby nie bylo timewaita
    if(bind(socketfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) ERR("bind"); // przypisujemy port do gniazda
    if(listen(socketfd, backlog_size) < 0) ERR("listen"); // ustawiamy gniazdo w stan nasłuchiwania, backlog_size - maksymalna liczba oczekujących połączeń
    return socketfd; // zwracamy deskryptor gniazda
}
