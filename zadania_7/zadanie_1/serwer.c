#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/epoll.h> // Dodałem dla epolla
#include <fcntl.h>     // Dodałem dla non-blocking (fcntl)
#include <signal.h>    // Dodałem dla sygnałów
#include <ctype.h>     // Dodałem dla isdigit()

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_EVENTS 10 // Maksymalna liczba zdarzeń na raz w epollu

// Ścieżka do naszego gniazda lokalnego (np. w folderze /tmp)
#define LOCAL_SOCKET_PATH "/tmp/my_local_socket" 

// Flaga kontrolująca główną pętlę (do poprawnego zamykania przy Ctrl+C)
volatile sig_atomic_t do_work = 1;

void sigint_handler(int sig) {
    do_work = 0;
}

int sethandler(void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction)); // Zeruje strukturę obsługi sygnału
    act.sa_handler = f;                        // Ustawia funkcję, która ma obsługiwać sygnał
    if (-1 == sigaction(sigNo, &act, NULL)) return -1;
    return 0;
}

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s port\n", name);
    exit(EXIT_FAILURE);
}

int make_tcp_socket(){
    int sock;
    sock = socket(AF_INET,SOCK_STREAM,0); // Gniazdo dla tcp (SOCK_STREAM) dla ipv4 (AF_INET)
    if(sock < 0) ERR("socket");
    return sock;
}

int make_local_socket(){
    int sock;
    sock = socket(AF_UNIX,SOCK_STREAM,0); // Gniazdo dla AF_UNIX (lokalne)
    if(sock < 0) ERR("socket");
    return sock;
}

int bind_tcp_port(int port){
    int socketfd = make_tcp_socket();

    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET; // IPv4
    addr.sin_port = htons(port); // Ustawiamy port w formacie sieciowym
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Słuchamy z każdego adresu na kompie
    
    if(setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(addr)) < 0) ERR("setsockopt"); // Pozwala zrestartować serwer bez błędu
    if(bind(socketfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) ERR("bind"); // Przypinamy socket
    if(listen(socketfd, 10) < 0) ERR("listen"); // Zaczynamy słuchać
    return socketfd;
}

int bind_local_socket(const char* path){
    int socketfd = make_local_socket();
    if(unlink(path) && errno != ENOENT) ERR("unlink"); // Czyscimy stary plik z dysku, chyba że go tam nie ma

    struct sockaddr_un addr;
    memset(&addr,0,sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1); // Zostawiamy miejsce na \0
    
    // Używamy castowania na sockaddr*, tak jak w TCP
    if(bind(socketfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) ERR("bind");
    if(listen(socketfd, 4) < 0) ERR("listen");
    return socketfd;
}

// Funkcja, która zmienia socket w non-blocking (żeby nas nie zablokował przy read/accept)
void make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) ERR("fcntl get");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) ERR("fcntl set");
}

// Liczymy sumę cyfr w PID (z zadania testowego)
int16_t calculate_digit_sum(const char* str) {
    int16_t sum = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (isdigit(str[i])) {
            sum += str[i] - '0'; // trick, żeby z zamienić chara np '3' na cyfrę 3
        }
    }
    return sum;
}

int main(int argc,char** argv){
    if(argc!=2) usage(argv[0]);
    int port=atoi(argv[1]);

    // Ignorujemy ten brzydki błąd kiedy piszemy do klienta co uciekł
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("sethandler SIGPIPE");
    // Obsługujemy Ctrl+C (do łagodnego wychodzenia)
    if (sethandler(sigint_handler, SIGINT)) ERR("sethandler SIGINT");

    // === TWORZENIE GNIAZD NASŁUCHUJĄCYCH ===
    int tcp_socket = bind_tcp_port(port);
    make_nonblocking(tcp_socket);

    int local_socket = bind_local_socket(LOCAL_SOCKET_PATH);
    make_nonblocking(local_socket);

    // === TWORZENIE EPOLLA ===
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) ERR("epoll_create1");

    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    // Dodajemy pierwszego "dzwonkarza" - TCP
    event.events = EPOLLIN;
    event.data.fd = tcp_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_socket, &event) < 0) ERR("epoll_ctl");

    // Dodajemy drugiego "dzwonkarza" - UNIX DOMAIN SOCKET
    event.events = EPOLLIN;
    event.data.fd = local_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_socket, &event) < 0) ERR("epoll_ctl");

    printf("Serwer uruchomiony. TCP:%d, UNIX:%s\n", port, LOCAL_SOCKET_PATH);

    int16_t max_sum = 0; // Śledzimy największą sumę (dla zadania)
    int how_much_events;

    // === GŁÓWNA PĘTLA SERWERA ===
    while(do_work){
        how_much_events = TEMP_FAILURE_RETRY(epoll_wait(epoll_fd, events, MAX_EVENTS, -1));
        
        if (how_much_events < 0) {
            if (errno == EINTR) break; // Przerwanie sygnałem, wychodzimy z while
            ERR("epoll_wait");
        }

        for (int i = 0; i < how_much_events; i++){
            
            // ============================================================================
            // SYTUACJA 1: NOWE POŁĄCZENIE (Z sieci TCP ALBO z dysku lokalnego)
            // ============================================================================
            if(events[i].data.fd == tcp_socket || events[i].data.fd == local_socket){
                
                int listening_fd = events[i].data.fd;
                // Acceptujemy ktokolwiek to jest (TCP albo UNIX)
                int new_client_fd = accept(listening_fd, NULL, NULL);
                
                if (new_client_fd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) ERR("accept");
                    continue; // Jeśli w sumie nie było kogo brać, lecimy dalej
                }

                make_nonblocking(new_client_fd); // Od razu na non-blocking

                // Dodajemy nowego ziomka do epolla, żeby go podglądać
                struct epoll_event client_event;
                client_event.events = EPOLLIN;
                client_event.data.fd = new_client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &client_event);
                
                printf("Nowy klient podlaczony (fd: %d)!\n", new_client_fd);
            }
            // ============================================================================
            // SYTUACJA 2: STARY KLIENT WŁAŚNIE COŚ NAPISAŁ (Wysyła swój PID)
            // ============================================================================
            else {
                int client_fd = events[i].data.fd;
                char buffer[64];
                memset(buffer, 0, sizeof(buffer));

                // Czytamy co tam klient napisał
                ssize_t size = read(client_fd, buffer, sizeof(buffer) - 1);

                if (size > 0) {
                    // Robimy robotę: liczymy tą sumę cyfr
                    int16_t sum = calculate_digit_sum(buffer);
                    
                    if (sum > max_sum) {
                        max_sum = sum;
                    }

                    // Wysyłamy odpowiedź do klienta (musimy zmienić na Network Byte Order: htons)
                    int16_t net_sum = htons(sum);
                    write(client_fd, &net_sum, sizeof(int16_t));
                    
                    printf("Rozlaczam klienta po obsluzeniu (fd: %d)\n", client_fd);
                    // Klient zadowolony, wyrzucamy go za drzwi i sprzątamy
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                } 
                else if (size == 0) {
                    // Jak klient sam się chamsko rozłączył
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                } 
                else {
                    // Ochrona przed fałszywymi pobudkami epolla (EAGAIN)
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                    }
                }
            }
        } // koniec for
    } // koniec while

    // ============================================================================
    // SPRZĄTANIE NA KONIEC (jak zrobisz Ctrl+C)
    // ============================================================================
    printf("\nSerwer zamyka sie. HIGH SUM=%d\n", max_sum);
    
    close(tcp_socket);
    close(local_socket);
    close(epoll_fd);
    
    // Ważne - wywalamy też plik naszego gniazda unixowego z dysku po wyjściu!
    unlink(LOCAL_SOCKET_PATH);

    return 0;
}