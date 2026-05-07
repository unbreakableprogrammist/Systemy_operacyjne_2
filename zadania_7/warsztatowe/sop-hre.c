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
#define MAX_FDS 1024
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

typedef struct {
    char name[32];
    int votes;
} Candidate;

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
    // usage
    if (argc != 2) usage(argv[0]);
    int port = atoi(argv[1]);
    int tcp_ssocket = bind_tcp_port(port);
    int flag = fcntl(tcp_ssocket, F_GETFL) | O_NONBLOCK;
    if (fcntl(tcp_ssocket, F_SETFL, flag) < 0) ERR("fcntl");
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("sethandler");
    
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) ERR("epoll_create1");
    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];
    event.events = EPOLLIN;
    event.data.fd = tcp_ssocket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_ssocket, &event) < 0) ERR("epoll_ctl");

    // === TABLICE STANÓW SERWERA DO ETAPU 3 ===
    
    // Nazwy państw elektorów (indeksowane 1-7)
    const char *elector_states[] = {"", "Moguncja", "Trewir", "Kolonia", "Czechy", "Palatynat", "Saksonia", "Brandenburgia"};
    // Nazwy kandydatów (indeksowane 1-3)
    const char *candidates[] = {"", "Franciszek I", "Karol V", "Henryk VIII"};
    
    int client_elector_id[MAX_FDS] = {0}; // Mapuje deskryptor(fd) na ID elektora. 0 oznacza "oczekuje na logowanie"
    int connected_electors[8] = {0};      // Tracks if elector 1-7 is online (zapisuje fd tego elektora)
    int elector_votes[8] = {0};           // Zapisuje aktualny głos (1-3) elektora (1-7). 0 = brak głosu

    int how_much_events;
    ssize_t size;

    while (1)
    {
        how_much_events = TEMP_FAILURE_RETRY(epoll_wait(epoll_fd, events, MAX_EVENTS, -1)); 
        if(how_much_events < 0) ERR("epoll_wait");

        for (int i = 0; i < how_much_events; i++){
            
            // -------------------------------------------------------------------------
            // NOWY KLIENT
            // -------------------------------------------------------------------------
            if(events[i].data.fd == tcp_ssocket){
                int new_client_fd = add_new_client(tcp_ssocket); 
                if (new_client_fd >= 0) {
                    int c_flag = fcntl(new_client_fd, F_GETFL) | O_NONBLOCK; 
                    if (fcntl(new_client_fd, F_SETFL, c_flag) < 0) ERR("fcntl");

                    struct epoll_event client_event;
                    client_event.events = EPOLLIN;
                    client_event.data.fd = new_client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &client_event) < 0) ERR("epoll_ctl");
                    
                    // Wymóg: Wiadomość powitalna jest wysyłana dopiero po identyfikacji.
                    // Zaznaczamy klienta jako niezlokalizowanego (0)
                    client_elector_id[new_client_fd] = 0; 
                    printf("Nowy klient podlaczony! (fd: %d). Oczekuje na identyfikacje...\n", new_client_fd);
                }
            }
            // -------------------------------------------------------------------------
            // DANE OD KLIENTA
            // -------------------------------------------------------------------------
            else {
                int client_fd = events[i].data.fd; 
                char data[256]; 
                memset(data, 0, sizeof(data));

                size = read(client_fd, data, sizeof(data) - 1); 
                
                if(size > 0){
                    // Czyszczenie znaków \r oraz \n wysyłanych przez netcat
                    for(int j=0; j<size; j++){
                        if(data[j] == '\n' || data[j] == '\r'){
                            data[j] = '\0';
                            break;
                        }
                    }

                    // SPRAWDZENIE STANÓW KLIENTA
                    if (client_elector_id[client_fd] == 0) {
                        // KLIENT OCZEKUJE NA IDENTYFIKACJĘ (Cyfra 1-7)
                        if (strlen(data) == 1 && data[0] >= '1' && data[0] <= '7') {
                            int e_id = data[0] - '0';
                            
                            // Weryfikacja, czy elektor o tym numerze nie jest już podłączony
                            if (connected_electors[e_id] != 0) {
                                char *msg = "Blad: Elektor o tym numerze jest juz podlaczony.\n";
                                write(client_fd, msg, strlen(msg));
                                
                                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL); 
                                close(client_fd);
                            } else {
                                // Sukces - logowanie elektora
                                connected_electors[e_id] = client_fd;
                                client_elector_id[client_fd] = e_id;
                                
                                char msg[256];
                                snprintf(msg, sizeof(msg), "Welcome, elector of %s!\n", elector_states[e_id]);
                                write(client_fd, msg, strlen(msg));
                                printf("Klient (fd %d) zalogowal sie jako elektor %d (%s).\n", client_fd, e_id, elector_states[e_id]);
                            }
                        } else {
                            // Wymóg: zły znak -> zakończ połączenie
                            char *msg = "Blad: Nieznany elektor. Rozlaczanie.\n";
                            write(client_fd, msg, strlen(msg));
                            
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL); 
                            close(client_fd);
                        }
                    } 
                    else {
                        // KLIENT JEST ZAUTORYZOWANY - OCZEKUJE NA GŁOS (Cyfra 1-3)
                        if (strlen(data) == 1 && data[0] >= '1' && data[0] <= '3') {
                            int c_id = data[0] - '0';
                            int e_id = client_elector_id[client_fd];
                            
                            // Głosy mogą się nadpisywać
                            elector_votes[e_id] = c_id;
                            
                            char msg[256];
                            snprintf(msg, sizeof(msg), "Zapisano glos. Kandydat: %s\n", candidates[c_id]);
                            write(client_fd, msg, strlen(msg));
                            printf("Elektor %d zaglosowal na kandydata %d\n", e_id, c_id);
                        } else {
                            // Wymóg: Inne znaki są ignorowane
                            // Możemy wysłać ostrzeżenie do debugowania, choć wymagania nakazują "ignorować"
                            printf("Zignorowano nieznany znak '%s' od elektora %d\n", data, client_elector_id[client_fd]);
                        }
                    }
                } 
                else if (size == 0) {
                    // ROZŁĄCZENIE KLIENTA
                    int e_id = client_elector_id[client_fd];
                    if (e_id > 0) {
                        // Zwalniamy miejsce, aby elektor mógł podłączyć się ponownie
                        connected_electors[e_id] = 0; 
                        client_elector_id[client_fd] = 0;
                        printf("Elektor %d (fd %d) rozlaczyl sie.\n", e_id, client_fd);
                    } else {
                        printf("Niezidentyfikowany klient (fd %d) rozlaczyl sie.\n", client_fd);
                    }
                    
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL); 
                    if (close(client_fd) < 0) ERR("close"); 
                }
                else if(size < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK) continue; 
                    else ERR("read"); 
                }
            }
        } 
    }
    
    return 0;
}