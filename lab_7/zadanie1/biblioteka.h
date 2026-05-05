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


int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction)); // zeruje strukturę obsługi sygnału
    act.sa_handler = f;                        // ustawia funkcję, która ma obsługiwać sygnał
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1; // rejestruje handler dla danego sygnału
    return 0;
}

int make_tcp_socket() 
{
    int sock;
    sock = socket(PF_INET, SOCK_STREAM, 0); // tworzy gniazdo TCP dla IPv4
    if (sock < 0)
        ERR("socket"); // błąd przy tworzeniu gniazda
    return sock;         // zwraca deskryptor gniazda
}

// Funkcja: Robi gniazdo TCP które słucha na danym porcie (serwer)
// Słownie: Tworzymy pusty port serwera, ustawiamy na nim nasłuchiwanie
int bind_tcp_socket(int port, int backlog_size)
{
    struct sockaddr_in addr;             // struktura z adresem IPv4 i portem
    int socketfd = make_tcp_socket();    // POPRAWKA: Używamy nowej poprawnej nazwy
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

// Funkcja: Robi lokalne gniazdo UNIX (takie jak plik ale dla komunikacji międzyprocesowej)
// Słownie: Tworzymy taki "pseudo-plik" do komunikacji
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
// Funkcja: Tworzy plik gniazda UNIX i ustawia na nim nasłuchiwanie
// Słownie: Robi plik (name) poprzez który procesy mogą się komunikować
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
// Funkcja: Akceptuje nowego klienta z kolejki oczekujących (jeśli jakiś czeka)
// Słownie: Wyciąga klienta z kolejki lub mówi że nie ma żadnego (w trybie nonblocking)
}

int add_new_client(int sfd)
{
    int nfd;
    
    // Próba wyciągnięcia klienta z kolejki gniazda nasłuchującego
    if ((nfd = TEMP_FAILURE_RETRY(accept(sfd, NULL, NULL))) < 0)
    {
        // Obsługa fałszywego alarmu (tryb nieblokujący)
        if (EAGAIN == errno || EWOULDBLOCK == errno)
            return -1;
            
        // Twardy błąd systemu
        ERR("accept");
    }
    
    // Sukces - zwracamy nowe gniazdo do komunikacji
    return nfd;
// Funkcja: Czyta DOKŁADNIE count bajtów (albo mniej jeśli koniec pliku)
// Słownie: read() może czytać mniej niż chcemy, ta funkcja powtarza czytanie aż się napełni
// lub skończy się plik
}

ssize_t bulk_read(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (0 == c)
            return len;
        buf += c;
        len += c;
// Funkcja: Pisze DOKŁADNIE count bajtów lub wszystko się nie uda
// Słownie: write() może napisać mniej niż chcemy, ta funkcja powtarza pisanie aż wszystko pójdzie
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

// Funkcja: Konwertuje tekst adresu IP i portu na strukturę systemową
// Słownie: Bierze "192.168.1.1" i "8080" i przetwarza na coś co system rozumie
// Obsługuje DNS lookup jeśli podamy nazwę zamiast IP
struct sockaddr_in make_address(char *address, char* port) {
    int ret;
    struct sockaddr_in addr;
    struct addrinfo *result;
    struct addrinfo hints = {};
    hints.ai_family = AF_INET;
    
    if ((ret = getaddrinfo(address, port, &hints, &result))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
        exit(EXIT_FAILURE);
    }
    
    addr = *(struct sockaddr_in *)(result->ai_addr);
    freeaddrinfo(result);
    return addr;
}

int connect_tcp_socket(char* name, char* port) {
    struct sockaddr_in addr;
    int socketfd;
    socketfd = make_tcp_socket();
    addr = make_address(name, port);
    if (connect(socketfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0)
    {
        ERR("connect");
    }
    return socketfd;
}