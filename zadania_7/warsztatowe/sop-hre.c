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
        // czekamy na zdarzenia (od naszego portiera epoll), -1 oznacza czekanie w nieskończoność
        how_much_events = TEMP_FAILURE_RETRY(epoll_wait(epoll_fd, events, MAX_EVENTS, -1)); 
        if(how_much_events < 0) ERR("epoll_wait");

        for (int i = 0; i < how_much_events; i++){
            
            // ============================================================================
            // SYTUACJA 1: Zdarzenie na głównym gnieździe nasłuchującym (NOWY KLIENT DZWONI)
            // ============================================================================
            if(events[i].data.fd == tcp_ssocket){
                
                // akceptujemy nowego klienta
                int new_client_fd = add_new_client(tcp_ssocket); 
                
                if (new_client_fd >= 0) {
                    printf("Nowy klient podlaczony! (fd: %d)\n", new_client_fd);
                    
                    // ustawiamy gniazdo nowego klienta w tryb non-blocking (bardzo ważne!)
                    int c_flag = fcntl(new_client_fd, F_GETFL) | O_NONBLOCK; 
                    if (fcntl(new_client_fd, F_SETFL, c_flag) < 0) ERR("fcntl");

                    // dodajemy tego nowego klienta do naszego epolla, żeby mógł go obserwować
                    struct epoll_event client_event;
                    client_event.events = EPOLLIN; // interesują nas zdarzenia odczytu
                    client_event.data.fd = new_client_fd; // przypisujemy mu jego własny deskryptor
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &client_event) < 0) ERR("epoll_ctl");

                    // wysyłamy powitanie zgodnie z wymogiem zadania
                    char *welcome = "Welcome, elector!\n";
                    write(new_client_fd, welcome, strlen(welcome));
                }
            }
            // ============================================================================
            // SYTUACJA 2: Zdarzenie na gnieździe klienta (STARY KLIENT WŁAŚNIE COŚ NAPISAŁ)
            // ============================================================================
            else {
                
                // jeśli to nie jest gniazdo nasłuchujące, to jest to gniazdo klienta
                int client_fd = events[i].data.fd; 
                char data[256]; // bufor do przechowywania danych od klienta (ciut większy na tekst)
                memset(data, 0, sizeof(data)); // czyścimy bufor, żeby nie było w nim śmieci z pamięci

                // próbujemy odczytać dane od klienta (używamy zwykłego read, bo czytamy tekst z netcata, a nie paczkę bajtów!)
                size = read(client_fd, data, sizeof(data) - 1); 
                
                if(size > 0){
                    // wypisujemy wiadomość od klienta
                    printf("wiadomosc od klienta (fd %d): %s", client_fd, data); 
                } 
                else if (size == 0) {
                    // jeśli read zwraca 0, oznacza to ZAWSZE, że klient rozłączył się z netcata
                    printf("klient (fd %d) sie rozlaczyl.\n", client_fd);
                    
                    // usuwamy go z obserwowanych przez epoll
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL); 
                    
                    // zamykamy gniazdo klienta
                    if (close(client_fd) < 0) ERR("close"); 
                }
                else if(size < 0){
                    // jeśli nie ma danych do odczytania, przechodzimy do następnego zdarzenia
                    if(errno == EAGAIN || errno == EWOULDBLOCK) continue; 
                    // w przypadku innego błędu, wypisujemy błąd
                    else ERR("read"); 
                }
            }
        } // koniec pętli for
    } // koniec pętli while
    
    return 0;
}
