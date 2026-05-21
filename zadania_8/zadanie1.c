#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>

#define MAX_BUF 576
#define STACK_SIZE 16

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
(__extension__                                                              \
({ long int __result;                                                     \
do __result = (long int) (expression);                                 \
while (__result == -1L && errno == EINTR);                             \
__result; }))
#endif

void usage(char *name) { fprintf(stderr, "USAGE: %s port\n", name); }

// Flaga kontrolująca działanie całego programu
volatile sig_atomic_t pracujemy = 1;

// Funkcja, która odpali się, gdy wciśniesz Ctrl+C
void sigint_handler(int sig) {
    pracujemy = 0;
}

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

int make_udp_socket() {
    int socketfd;
    // Tworzymy gniazdo: AF_INET (IPv4), SOCK_DGRAM (datagramy UDP), 0 (domyślny protokół).
    if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        ERR("socket"); // Jeśli socket() zwróci liczbę ujemną, przerywamy program z błędem.

    return socketfd; // Zwracamy "numer" (deskryptor) naszego nowego gniazda.
}

int bind_udp_socket(int port) {
    struct sockaddr_in address; // Deklarujemy strukturę, która przechowa adres IPv4 i port.
    int socketfd = make_udp_socket(); // Wywołujemy naszą funkcję wyżej, by stworzyć gniazdo.
    int t = 1; // Zmienna pomocnicza (wartość 1 = "prawda") potrzebna dla opcji SO_REUSEADDR.

    // Zerujemy pamięć struktury, żeby uniknąć tzw. "śmieci" w pamięci.
    memset(&address, 0, sizeof(struct sockaddr_in));

    // Konfigurujemy strukturę adresową:
    address.sin_family = AF_INET; // Ustawiamy rodzinę adresów na IPv4.
    address.sin_port = htons(port); // Konwertujemy port z formatu procesora na format sieciowy i przypisujemy.
    address.sin_addr.s_addr = htonl(INADDR_ANY); // Konwertujemy i ustawiamy nasłuchiwanie na wszystkich adresach IP maszyny.

    // Ustawiamy opcję SO_REUSEADDR. Pozwala to na ponowne, natychmiastowe użycie tego samego portu,
    // gdyby serwer uległ awarii i musiał zostać zrestartowany (inaczej system zablokowałby port na dłuższą chwilę).
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)) < 0)
        ERR("setsockopt");

    // "Przywiązujemy" (bindowanie) nasze gniazdo do skonfigurowanego wyżej adresu i portu.
    // Rzutujemy wskaźnik &address na ogólny typ (struct sockaddr *), bo tego wymaga ta funkcja.
    if (bind(socketfd, (struct sockaddr *)&address, sizeof(address)) < 0)
        ERR("bind");

    return socketfd; // Zwracamy gotowe, zbindowane gniazdo serwera.
}

// zmienne globalne - stos mutexy i semfaory
char stos[STACK_SIZE+1][MAX_BUF]; // tablica podwojna
int top = 0; // wskaznik ile na stosie
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* thread_work(void *arg) {
    char dane[MAX_BUF];
    int x,y,p;
    char nazwa[129]; // 128 znaków + 1 na znak końca '\0'
    while (1) {
        pthread_mutex_lock(&mutex);

        // Zauważ zmianę: Śpij tylko wtedy, gdy NIE MA raportów I NADAL PRACUJEMY
        while (top == 0 && pracujemy == 1) {
            pthread_cond_wait(&cond, &mutex);
        }

        // Zostaliśmy obudzeni! Sprawdzamy, czy to alarm końcowy.
        // Jeśli szef kazał kończyć i stos jest pusty - zdejmujemy fartuch i wychodzimy z pętli.
        if (pracujemy == 0 && top == 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }
         // tutaj jestesmy jesli sie wybudzilismy
        top--; // zdejmujemy dane
        strcpy(dane, stos[top]); // przekopiowujemy do swojego bufora
        pthread_mutex_unlock(&mutex); // odblokowujemy bo juz skopiowane

        // i parsujemy tekst
        // %d - czytaj liczbę całkowitą
        // %128[^\n] - czytaj do 128 znaków dopóki nie napotkasz znaku nowej linii (pozwala na spacje w nazwie)
        int parsed_items = sscanf(dane, "%d %d %d %128[^\n]", &x, &y, &p, nazwa);
        // Sprawdzamy czy sscanf poprawnie przeczytał wszystkie 4 elementy
        if (parsed_items != 4) {
            fprintf(stderr, "Błąd parsowania: Zły format wiadomości. Odrzucam.\n");
            continue; // Nie ubijamy serwera! Po prostu ignorujemy tę paczkę.
        }

        // Sprawdzamy czy liczby są w dozwolonym zakresie
        // Z instrukcji: X, Y to liczby od 0 do 99, P to 0 lub 1
        if (x < 0 || x > 99 || y < 0 || y > 99 || (p != 0 && p != 1)) {
            fprintf(stderr, "Błąd logiczny: Dane poza dozwolonym zakresem. Odrzucam.\n");
            continue; // Znowu: ignorujemy złą paczkę i kręcimy się dalej
        }

        // Jeśli wszystko poszło dobrze, wypisujemy raport sztabowy!
        char *strona = (p == 1) ? "Nasz" : "Wrogi";
        printf("%s oddział %s był widziany na pozycji %d:%d\n", strona, nazwa, x, y);

    }
    return NULL;
}

void serwer_work(int socked_fd) {
    char received_buffer[MAX_BUF];
    while (pracujemy) {
        int recived_bytes = recvfrom(socked_fd, received_buffer, MAX_BUF-1, 0, NULL,NULL);
        if (recived_bytes < 0) {
            if (errno == EINTR) {
                // Funkcja przerwana przez nasz sygnał SIGINT.
                // Pętla while zaraz sprawdzi 'pracujemy' i sama się zakończy.
                continue;
            }
            ERR("recvfrom"); // To zadziała, jeśli błąd był krytyczny (nie spowodowany sygnałem)
        }
        received_buffer[recived_bytes] = '\0';
            // lokujemy nasza tablice
        pthread_mutex_lock(&mutex);
        // jesli nie mamy 16 elementow na stosie to wrzucamy nowy ( maks element to 16 bo mamy tablice o jeden wieksza)
        if (top < STACK_SIZE) {
            strcpy(stos[top], received_buffer);
            top++;
            pthread_cond_signal(&cond);
        }else {
            fprintf(stderr, "Serwer: Stos jest pełny! Meldunek przepadł w chaosie bitwy.\n");
        }
        pthread_mutex_unlock(&mutex);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Seting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT)) ERR("Setting SIGINT:");
    int port = atoi(argv[1]);
    int socket_fd = bind_udp_socket(port);

    pthread_t adiutanci[4];
    for (int i=0; i<4; i++) {
        if (pthread_create(&adiutanci[i], NULL, thread_work, NULL) != 0) {
            fprintf(stderr, "Błąd podczas tworzenia wątku!\n");
            exit(EXIT_FAILURE);
        }
    }

    serwer_work(socket_fd);


    // ====== SEKCJA ZAMYKANIA ======
    // Jeśli tu dotarliśmy, oznacza to, że wciśnięto Ctrl+C i wyszliśmy z pętli serwer_work.
    printf("\nZamykanie sztabu. Budzenie adiutantów do wyjścia...\n");

    // Budzimy WSZYSTKICH uśpionych adiutantów (alarm przeciwpożarowy!)
    // Wstaną, zobaczą że pracujemy == 0, sprawdzą czy stos jest pusty i wyjdą.
    pthread_cond_broadcast(&cond);

    // Czekamy na każdego z nich (pthread_join), żeby mieć pewność, że odłożyli długopisy
    for (int i = 0; i < 4; i++) {
        if (pthread_join(adiutanci[i], NULL) != 0) {
            ERR("pthread_join");
        }
    }

    // Zamykamy gniazdo i uroczyście niszczymy narzędzia do synchronizacji
    if (TEMP_FAILURE_RETRY(close(socket_fd)) < 0) ERR("close");
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    printf("Sztab zamknięty bezpiecznie.\n");
    return EXIT_SUCCESS;
}