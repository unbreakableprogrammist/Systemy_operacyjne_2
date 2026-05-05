#include "biblioteka.h"
#define BACKLOG 3
#define MAX_EVENTS 10
volatile sig_atomic_t do_work = 1; // flaga do kontrolowania głównej pętli serwera

void sigint_handler(int signum) {
    do_work = 0; // ustawiamy flagę na 0, żeby główna pętla serwera mogła się zakończyć
}
void usage(char *name) {
    fprintf(stderr, "USAGE: %s socket port\n", name); 
    exit(EXIT_FAILURE);
}

void calculate(int32_t data[5]) {
    int32_t op1,op2,result = -1 , status = 1;
    op1 = ntohl(data[0]); // zamiana z sieciowego porządku bajtów na hostowy
    op2 = ntohl(data[1]);
        switch ((char)ntohl(data[3]))
    {
        case '+':
            result = op1 + op2;
            break;
        case '-':
            result = op1 - op2;
            break;
        case '*':
            result = op1 * op2;
            break;
        case '/':
            if (!op2)
                status = 0;
            else
                result = op1 / op2;
            break;
        default:
            status = 0;
    }
    data[4] = htonl(status);
    data[2] = htonl(result);
}

void server_work(int local_socket,int tcp_socket)
{
    // epoll to takie cos ze powiadamia nas za pomoca epoll_wait() o zdarzeniach na gniazdach, zamiast czekac na kazde gniazdo z osobna, co jest bardziej efektywne
    int epoll_fd = epoll_create1(0); // Tworzymy instancję epoll
    if (epoll_fd < 0) ERR("epoll_create1");
    struct epoll_event event,events[MAX_EVENTS]; // Struktura do przechowywania informacji o zdarzeniach
    event.events = EPOLLIN; // Chcę wiedzieć, kiedy będą dane do ZBIERANIA (IN)    
    event.data.fd = local_socket; // Przypisujemy gniazdo lokalne do struktury zdarzenia
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_socket, &event) < 0) // Dodajemy gniazdo lokalne do epoll
        ERR("epoll_ctl");
    event.data.fd = tcp_socket; // Przypisujemy gniazdo TCP do struktury zdarzenia
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_socket, &event) < 0) // Dodajemy gniazdo TCP do epoll
        ERR("epoll_ctl");
    int nfds; // ile sygnałów o zdarzeniach otrzymaliśmy
    int32_t data[5]; // tablica do przechowywania danych od klientów, 5 elementów bo tyle jest w zadaniu
    ssize_t size; // rozmiar odebranych danych
    // na czas dzialania programu blokujemy sygnał SIGINT, żeby nie przerwał nam pracy w trakcie obsługi klientów
    sigset_t mask, oldmask; 
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    while(do_work){
        if ((nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask)) < 0) { // Czekamy na zdarzenia na gniazdach i uwaga ustawiamy na czas czekania stara maske bo jak tu przyjdzie SIGINT to przerwie nam epoll_wait, a my chcemy żeby przerwał dopiero po obsłużeniu klientów
            if(errno == EINTR) continue; // Jeśli przerwano przez sygnał to wywalone bo flaga sie zmieni tak czy siak wiec bezpiecznie wyjdziemy 
            ERR("epoll_wait"); // jesli to nie sygnał to jest błąd
        }else{
            for(int i = 0;i < nfds;i ++){
                int client = add_new_client(events[i].data.fd); // acceptujemy nowego klienta przekazujemy ktore gniazdo (localne czy TCP) wywołało zdarzenie
                if((size = bulk_read(client, (char*)data, sizeof(data))) < 0) // próbujemy odczytać dane od klienta
                    ERR("read"); // jeśli błąd to wywalone
                if(size == (int)sizeof(int32_t[5])) calculate(data);
                if (bulk_write(client, (char*)data, sizeof(int32_t[5])) < 0 && errno != EPIPE) 
                    ERR("write"); // jeśli błąd to wywalone, ale jeśli klient się rozłączył (EPIPE) to nie jest to błąd, więc ignorujemy
                if (close(client) < 0) ERR("close");
            }
        }
    }
    close(epoll_fd);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

int main(int argc, char** argv){
    if(argc != 3 ) usage(argv[0]);
    int local_socket = bind_local_socket(argv[1],BACKLOG);
    int new_flag = fcntl(local_socket, F_GETFL) | O_NONBLOCK; // Ustawiamy flagę O_NONBLOCK dla gniazda lokalnego tak ze nie bedzie zawiisac na operacjach 
    if (fcntl(local_socket, F_SETFL, new_flag) < 0) // Zastosuj nową flagę do gniazda lokalnego
        ERR("fcntl");
    int tcp_socket = bind_tcp_socket(atoi(argv[2]), BACKLOG);
    new_flag = fcntl(tcp_socket, F_GETFL) | O_NONBLOCK;
    if (fcntl(tcp_socket, F_SETFL, new_flag) < 0) // Zastosuj nową flagę do gniazda TCP
        ERR("fcntl");
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("SIGPIPE");
    if (sethandler(sigint_handler, SIGINT)) ERR("SIGINT");
    server_work(local_socket, tcp_socket);
    close(local_socket);
    unlink(argv[1]); // usuwamy plik gniazda lokalnego z dysku!
    close(tcp_socket);
}