// Dołącza nagłówek z funkcjami bibliotekowymi
#include "biblioteka.h"
// Maksymalna liczba oczekujących połączeń w kolejce
#define BACKLOG 3
// Maksymalna liczba zdarzeń które epoll może zwrócić w jednym wywołaniu
#define MAX_EVENTS 10

// Globalna flaga do kontrolowania głównej pętli serwera
// volatile powoduje że kompilator nie będzie jej cachować
// sig_atomic_t gwarantuje atomowość zapisu/odczytu nawet z obsługą sygnałów
volatile sig_atomic_t do_work = 1;

// Handler dla sygnału SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    // Ustawia flagę na 0, aby główna pętla serwera mogła się bezpiecznie zakończyć
    do_work = 0;
}

// Wyświetla instrukcję poprawnego użytkowania programu
void usage(char *name) {
    // Wypisuje informację o błędu użycia
    fprintf(stderr, "USAGE: %s socket port\n", name);
    // Kończy program z kodem błędu
    exit(EXIT_FAILURE);
}

// Funkcja wykonująca obliczenia na podstawie przesłanych danych
void calculate(int32_t data[5]) {
    // Pierwszy operand
    int32_t op1;
    // Drugi operand
    int32_t op2;
    // Wynik obliczenia (inicjalnie -1)
    int32_t result = -1;
    // Status: 1 = sukces, 0 = błąd (np. dzielenie przez zero)
    int32_t status = 1;
    
    // Konwertuje pierwszy operand z sieciowego porządku bajtów (big-endian) na hostowy
    op1 = ntohl(data[0]);
    // Konwertuje drugi operand z sieciowego porządku bajtów na hostowy
    op2 = ntohl(data[1]);
    
    // Wybiera operację na podstawie znaku przesłanego w data[3]
    switch ((char)ntohl(data[3]))
    {
        // Operacja dodawania
        case '+':
            result = op1 + op2;
            break;
        // Operacja odejmowania
        case '-':
            result = op1 - op2;
            break;
        // Operacja mnożenia
        case '*':
            result = op1 * op2;
            break;
        // Operacja dzielenia
        case '/':
            // Sprawdzenie czy dzielnik nie jest zerem
            if (!op2)
                // Jeśli dzielenie przez zero, ustaw status na błąd
                status = 0;
            else
                // Inaczej wykonaj dzielenie
                result = op1 / op2;
            break;
        // Nieznana operacja
        default:
            // Ustaw status na błąd
            status = 0;
    }
    
    // Konwertuje status (sukces/błąd) do sieciowego porządku bajtów i zapisuje w data[4]
    data[4] = htonl(status);
    // Konwertuje wynik do sieciowego porządku bajtów i zapisuje w data[2]
    data[2] = htonl(result);
}

// Główna funkcja serwera obsługująca klientów
void server_work(int local_socket,int tcp_socket)
{
    // epoll to mechanizm do efektywnego monitorowania wielu gniazd jednocześnie
    // epoll_wait() powiadamia nas o zdarzeniach zamiast czekać na każde gniazdo osobno
    int epoll_fd = epoll_create1(0); // Tworzymy nową instancję epoll
    // Sprawdzenie czy epoll_create1 zwróciła błąd
    if (epoll_fd < 0) ERR("epoll_create1");
    
    // Struktura do przechowywania informacji o zdarzeniach
    struct epoll_event event;
    // Tablica do przechowywania zdarzeń zwróconych przez epoll_wait
    struct epoll_event events[MAX_EVENTS];
    
    // Ustawiamy że chcemy być powiadamiani o zdarzeniach EPOLLIN (dane dostępne do przeczytania)
    event.events = EPOLLIN;
    // Przypisujemy gniazdo lokalne do struktury zdarzenia
    event.data.fd = local_socket;
    // Dodajemy gniazdo lokalne do monitorowania przez epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_socket, &event) < 0)
        ERR("epoll_ctl");
    
    // Przypisujemy gniazdo TCP do struktury zdarzenia
    event.data.fd = tcp_socket;
    // Dodajemy gniazdo TCP do monitorowania przez epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tcp_socket, &event) < 0)
        ERR("epoll_ctl");
    
    // Liczba zdarzeń które się zdarzyły i zostały zwrócone przez epoll_wait
    int nfds;
    // Tablica do przechowywania danych od klientów (5 pól: 2 operandy, wynik, operacja, status)
    int32_t data[5];
    // Rozmiar odebranych danych
    ssize_t size;
    
    // Blokujemy sygnał SIGINT na czas działania programu
    // To powoduje że sygnał nie przerwnie nam pracy w trakcie obsługi klientów
    sigset_t mask, oldmask;
    // Tworzy pusty zbiór sygnałów
    sigemptyset(&mask);
    // Dodaje SIGINT do zbioru sygnałów do zablokowania
    sigaddset(&mask, SIGINT);
    // Blokuje sygnały ze zbioru mask, stare ustawienia zapisuje w oldmask
    sigprocmask(SIG_BLOCK, &mask, &oldmask);
    
    // Główna pętla serwera trwa dopóki do_work != 0
    while(do_work){
        // epoll_pwait czeka na zdarzenia z możliwością obsługi sygnału
        // -1 oznacza czekanie bez limitu czasowego
        // &oldmask jest maską sygnałów do przywrócenia na czas czekania
        // dzięki temu SIGINT może nam przerwać czekanie
        if ((nfds = epoll_pwait(epoll_fd, events, MAX_EVENTS, -1, &oldmask)) < 0) {
            // Jeśli epoll_pwait zwrócił błąd
            // EINTR oznacza że został przerwany sygnałem (to OK)
            if(errno == EINTR) continue; // Wznowić pętlę, flaga do_work się zmieni
            // Jeśli to nie sygnał, to jest poważny błąd
            ERR("epoll_wait");
        }else{
            // Przetwarzamy wszystkie zdarzenia zwrócone przez epoll_wait
            for(int i = 0;i < nfds;i ++){
                // Akceptujemy nowego klienta
                // events[i].data.fd zawiera które gniazdo wywołało zdarzenie (lokalne czy TCP)
                int client = add_new_client(events[i].data.fd);
                
                // Próbujemy odczytać dane od klienta do tablicy data
                size = bulk_read(client, (char*)data, sizeof(data));
                // Sprawdzenie czy było błąd podczas czytania
                if(size < 0)
                    ERR("read");
                    
                // Jeśli odczytaliśmy dokładnie 5 int32_t (20 bajtów)
                if(size == (int)sizeof(int32_t[5])) 
                    // Wykonaj obliczenia na danych
                    calculate(data);
                
                // Wysyłamy wyniki z powrotem do klienta
                if (bulk_write(client, (char*)data, sizeof(int32_t[5])) < 0 && errno != EPIPE) 
                    // Jeśli błąd ale nie jest to EPIPE (klient się rozłączył), to błąd
                    ERR("write");
                
                // Zamykamy deskryptor gniazda komunikacyjnego z tym klientem
                if (close(client) < 0) ERR("close");
            }
        }
    }
    
    // Zamykamy epoll
    close(epoll_fd);
    // Przywracamy pierwotną maskę sygnałów
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

// Główna funkcja programu
int main(int argc, char** argv){
    // Sprawdzenie czy program otrzymał dokładnie 3 argumenty (nazwa + socket + port)
    if(argc != 3 ) usage(argv[0]);
    
    // Tworzy i binduje gniazdo lokalne UNIX do ścieżki z argv[1]
    int local_socket = bind_local_socket(argv[1],BACKLOG);
    
    // Pobiera aktualne flagi gniazda lokalnego
    int new_flag = fcntl(local_socket, F_GETFL) | O_NONBLOCK;
    // Ustawia flagę O_NONBLOCK aby gniazdo nie czekało na operacjach blokujących
    if (fcntl(local_socket, F_SETFL, new_flag) < 0)
        ERR("fcntl");
    
    // Tworzy i binduje gniazdo TCP do portu z argv[2]
    // atoi konwertuje string portu na liczbę całkowitą
    int tcp_socket = bind_tcp_socket(atoi(argv[2]), BACKLOG);
    
    // Pobiera aktualne flagi gniazda TCP
    new_flag = fcntl(tcp_socket, F_GETFL) | O_NONBLOCK;
    // Ustawia flagę O_NONBLOCK aby gniazdo nie czekało na operacjach blokujących
    if (fcntl(tcp_socket, F_SETFL, new_flag) < 0)
        ERR("fcntl");
    
    // Rejestruje handler dla SIGPIPE (ignoruje go)
    // SIGPIPE jest wysyłany gdy piszemy do zamkniętego gniazda
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("SIGPIPE");
    
    // Rejestruje handler dla SIGINT (Ctrl+C)
    // Handler zmieni globalną flagę do_work
    if (sethandler(sigint_handler, SIGINT)) ERR("SIGINT");
    
    // Główna pętla serwera obsługująca klientów
    server_work(local_socket, tcp_socket);
    
    // Zamyka gniazdo lokalne
    close(local_socket);
    // Usuwa plik gniazda lokalnego z dysku
    unlink(argv[1]);
    // Zamyka gniazdo TCP
    close(tcp_socket);
}