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

#define BACKLOG 4
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s port\n", name);
    exit(EXIT_FAILURE);
}
int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction)); // zeruje strukturę obsługi sygnału
    act.sa_handler = f;                        // ustawia funkcję, która ma obsługiwać sygnał
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1; // rejestruje handler dla danego sygnału
    return 0;
}

int make_tcp_socket() {
    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0); // gniazdo dla tcp (SOCK_STREAM) dla ipv4 (AF_INET)
    if (sock < 0) ERR("socket");
    return sock;
}
int bind_tcp_port(int port) {
    int socketfd = make_tcp_socket(); // dostajemy desktryptor do portu
    // strukturka do ktorej wpisujemy wszytskie potrzebne dane do polaczenia
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET; // IPv4
    addr.sin_port = htons(port); // ustawiamy port
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // nasluchujemy z wsztskich sieci
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(addr)) < 0) ERR("setsockopt"); // pozwala ponownie uzyc portu
    if (bind(socketfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) ERR("bind"); //bindujemy
    if (listen(socketfd, BACKLOG) < 0) ERR("listen"); // zaczynamy nasluchiwac
    return socketfd;
}

int main(int argc,char** argv) {
    if (argc != 2) usage(argv[0]);
    int port = atoi(argv[1]);
    return 0;
}
