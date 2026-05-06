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
#define MAX_EVENTS 10
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

int add_new_client(int socketfd){ // akceptujemy nowego klienta, socketfd to gniazdo nasłuchujące
    int clientfd;
    if ((clientfd = TEMP_FAILURE_RETRY(accept(socketfd, NULL, NULL))) < 0) {// akceptujemy klienta
        if(errno == EAGAIN || errno == EWOULDBLOCK) return -1; // w trybie nonblocking, jeśli nie ma klienta, zwracamy -1
        ERR("accept"); // w przypadku innego błędu, wypisujemy błą
    }
    return clientfd; // zwracamy deskryptor do klienta

}
ssize_t bulk_read(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            if(errno == EAGAIN || errno == EWOULDBLOCK) return -1;
            else ERR("read");
        if (0 == c)
            return len;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}


int main(int argc,char** argv) {
    if (argc != 2) usage(argv[0]);
    int port = atoi(argv[1]);
    int tcp_ssocket = bind_tcp_port(port); // tworzymy gniazdo nasłuchujące na porcie
    int flag = fcntl(tcp_ssocket, F_GETFL) | O_NONBLOCK; // ustawiamy gniazdo w tryb non-blocking
    if (fcntl(tcp_ssocket, F_SETFL, flag) < 0) ERR("fcntl");
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("sethandler"); // ignorujemy SIGPIPE, który jest wysyłany, gdy piszemy do zamknietego gniazda
    
    int epoll_fd = epoll_create1(0); // tworzymy epoll
    if (epoll_fd < 0) ERR("epoll_create1");
    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS]; // tablica do przechowywania zdarzeń, które epoll zwróci
    event.events = EPOLLIN; // interesują nas zdarzenia odczytu
    event.data.fd = tcp_ssocket; // ustawiamy dane zdarzenia na deskryptor gniazda nasłuchującego
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_ssocket, &event) < 0) ERR("epoll_ctl"); // dodajemy gniaz
    int how_much_events;
    ssize_t size;
    while (1)
    {
        how_much_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); // czekamy na zdarzenia
        if(how_much_events < 0) ERR("epoll_wait");
        for (int i = 0; i < how_much_events; i++){
            char data[20]; // bufor do przechowywania danych od klienta
            int new_client_fd = add_new_client(events[i].data.fd); // akceptujemy nowego klienta
            size = bulk_read(new_client_fd,data,sizeof(data)); // próbujemy odczytać dane od klienta
            if(size < 0){
                if(errno == EAGAIN || errno == EWOULDBLOCK) continue; // jeśli nie ma danych do odczytania, przechodzimy do następnego zdarzenia
                else ERR("read"); // w przypadku innego błędu, wypisujemy błąd
            }
            printf("wiadomosc od klienta: %s\n", data); // wypisujemy wiadomość od klienta
            if (close(new_client_fd) < 0) ERR("close"); // zamykamy gniazdo klienta
        }
        
    }
    
    return 0;
}
