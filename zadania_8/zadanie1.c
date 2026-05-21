#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#define MAX_BUF 576
#define STACK_SIZE 16
#define DIVISION_NAMES_SIZE 128
#define MAX_NAME 129

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

typedef struct {
    char tekst[MAX_BUF];
    struct sockaddr_in nadawca;
} message_t;

typedef struct {
    // Wszystko, co współdzielą wątki, trzymamy tutaj zamiast w globalach.
    message_t stos[STACK_SIZE + 1];
    int top;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char nazwy_oddzialow[DIVISION_NAMES_SIZE][MAX_NAME];
    int liczba_oddzialow;
    pthread_mutex_t mutex_do_nazw;
    int przynaleznosc[DIVISION_NAMES_SIZE];
    struct sockaddr_in ostatni_nadawca[DIVISION_NAMES_SIZE];
    int ma_ostatni_nadawca[DIVISION_NAMES_SIZE];
    int mapa[100][100];
    pthread_mutex_t mutex_mapa[100];
    int socket_fd;
} app_state_t;

// Funkcja, która odpali się, gdy wciśniesz Ctrl+C
void sigint_handler(int sig) {
    (void)sig;
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

// Wypisujemy aktualny stan mapy sztabowej.
// Każdy wiersz chronimy osobnym mutexem, żeby nie czytać danych w trakcie aktualizacji.
void drukuj_mape(app_state_t *state) {
    printf("Mapa sztabowa:\n");
    for (int y = 0; y < 100; y++) {
        pthread_mutex_lock(&state->mutex_mapa[y]);
        for (int x = 0; x < 100; x++) {
            if (state->mapa[y][x] == -1) {
                printf(" .");
            } else {
                printf("%2d", state->mapa[y][x]);
            }
        }
        pthread_mutex_unlock(&state->mutex_mapa[y]);
        printf("\n");
    }
    fflush(stdout);
}

// Wybieramy losowy sojuszniczy oddział, który ma już zapamiętany ostatni adres nadawcy.
// Jeśli nie ma jeszcze żadnego takiego oddziału, zwracamy -1.
int losowy_sojusznik(app_state_t *state) {
    int kandydaci[DIVISION_NAMES_SIZE];
    int liczba_kandydatow = 0;

    pthread_mutex_lock(&state->mutex_do_nazw);
    for (int i = 0; i < state->liczba_oddzialow; i++) {
        if (state->przynaleznosc[i] == 1 && state->ma_ostatni_nadawca[i]) {
            kandydaci[liczba_kandydatow++] = i;
        }
    }

    if (liczba_kandydatow == 0) {
        pthread_mutex_unlock(&state->mutex_do_nazw);
        return -1;
    }

    int wybor = kandydaci[rand() % liczba_kandydatow];
    pthread_mutex_unlock(&state->mutex_do_nazw);
    return wybor;
}

// Wątek Napoleona: co 30 ms pokazuje mapę i wysyła rozkaz do losowego sojusznika.
void* napoleon_work(void *arg) {
    app_state_t *state = arg;

    // Jedno ziarno losowości na cały wątek - dzięki temu wybory są mniej przewidywalne.
    srand((unsigned int)(time(NULL) ^ getpid()));

    while (pracujemy) {
        drukuj_mape(state);

        int id_oddzialu = losowy_sojusznik(state);
        if (id_oddzialu >= 0) {
            // Budujemy rozkaz w tym samym formacie, który rozumieją adiutanci.
            char rozkaz[MAX_BUF];
            int x = rand() % 100;
            int y = rand() % 100;

            pthread_mutex_lock(&state->mutex_do_nazw);
            struct sockaddr_in adres = state->ostatni_nadawca[id_oddzialu];
            char nazwa[MAX_NAME];
            strcpy(nazwa, state->nazwy_oddzialow[id_oddzialu]);
            pthread_mutex_unlock(&state->mutex_do_nazw);

            snprintf(rozkaz, sizeof(rozkaz), "%d %d %d %s", x, y, 1, nazwa);

            // Rozkaz odsyłamy na ostatni znany adres sojusznika.
            if (TEMP_FAILURE_RETRY(sendto(state->socket_fd, rozkaz, strlen(rozkaz), 0,
                                          (struct sockaddr *)&adres, sizeof(adres))) < 0) {
                perror("sendto");
            }
        }

        usleep(30000);
    }

    return NULL;
}

void* thread_work(void *arg) {
    app_state_t *state = arg;
    message_t raport;
    int x,y,p;
    char nazwa[129]; // 128 znaków + 1 na znak końca '\0'
    while (1) {
        pthread_mutex_lock(&state->mutex);

        // Zauważ zmianę: Śpij tylko wtedy, gdy NIE MA raportów I NADAL PRACUJEMY
        while (state->top == 0 && pracujemy == 1) {
            pthread_cond_wait(&state->cond, &state->mutex);
        }

        // Zostaliśmy obudzeni! Sprawdzamy, czy to alarm końcowy.
        // Jeśli szef kazał kończyć i stos jest pusty - zdejmujemy fartuch i wychodzimy z pętli.
        if (pracujemy == 0 && state->top == 0) {
            pthread_mutex_unlock(&state->mutex);
            break;
        }
        // Zdejmujemy jeden raport ze stosu i kopiujemy go do lokalnego bufora.
        state->top--; // zdejmujemy dane
        raport = state->stos[state->top];
        pthread_mutex_unlock(&state->mutex); // odblokowujemy bo juz skopiowane

        // i parsujemy tekst
        // %d - czytaj liczbę całkowitą
        // %128[^\n] - czytaj do 128 znaków dopóki nie napotkasz znaku nowej linii (pozwala na spacje w nazwie)
        int parsed_items = sscanf(raport.tekst, "%d %d %d %128[^\n]", &x, &y, &p, nazwa);
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

        usleep(10000); // 10ms - symulacja czasu potrzebnego na przetworzenie raportu

        int id_oddzialu = -1;
        pthread_mutex_lock(&state->mutex_do_nazw);
        // Sprawdzamy, czy nazwa oddziału już istnieje na liście
        for (int i = 0; i < state->liczba_oddzialow; i++) {
            if (strcmp(state->nazwy_oddzialow[i], nazwa) == 0) {
                id_oddzialu = i; // Znaleźliśmy oddział, zapamiętujemy jego ID
                break;
            }
        }
        // Jeśli nie znaleźliśmy oddziału, dodajemy go do listy
        if (id_oddzialu == -1 && state->liczba_oddzialow < DIVISION_NAMES_SIZE){
            id_oddzialu = state->liczba_oddzialow;
            strcpy(state->nazwy_oddzialow[id_oddzialu], nazwa);
            state->liczba_oddzialow++;
        }

        pthread_mutex_unlock(&state->mutex_do_nazw);
        if (id_oddzialu == -1) {
            fprintf(stderr, "Brak miejsca w rejestrze dla oddziału: %s\n", nazwa);
            continue; // Nie mamy ID, nie możemy ustawić go na mapie
        }

        // Zapamiętujemy ostatni adres nadawcy i przynależność tego oddziału.
        pthread_mutex_lock(&state->mutex_do_nazw);
        state->przynaleznosc[id_oddzialu] = p;
        state->ostatni_nadawca[id_oddzialu] = raport.nadawca;
        state->ma_ostatni_nadawca[id_oddzialu] = 1;
        pthread_mutex_unlock(&state->mutex_do_nazw);
        // aktualizujemy mapę 
        for(int wiersz=0;wiersz<100;wiersz++) {
            pthread_mutex_lock(&state->mutex_mapa[wiersz]);
            for(int kolumna=0;kolumna<100;kolumna++) {
                if (state->mapa[wiersz][kolumna] == id_oddzialu) {
                    state->mapa[wiersz][kolumna] = -1; // usuwamy stare pozycje oddziału
                }
            }
            pthread_mutex_unlock(&state->mutex_mapa[wiersz]);
        }
        pthread_mutex_lock(&state->mutex_mapa[y]);
        state->mapa[y][x] = id_oddzialu; // ustawiamy nową pozycję oddziału
        pthread_mutex_unlock(&state->mutex_mapa[y]);
        // Jeśli wszystko poszło dobrze, wypisujemy raport sztabowy!
        char *strona = (p == 1) ? "Nasz" : "Wrogi";
        printf("%s oddział %s był widziany na pozycji %d:%d\n", strona, nazwa, x, y);

    }
    return NULL;
}

void serwer_work(int socked_fd, app_state_t *state) {
    message_t odebrany;
    while (pracujemy) {
        // Odbieramy nie tylko treść meldunku, ale też adres nadawcy.
        socklen_t addrLen = sizeof(odebrany.nadawca);
        int recived_bytes = recvfrom(socked_fd, odebrany.tekst, MAX_BUF-1, 0,
                                     (struct sockaddr *)&odebrany.nadawca, &addrLen);
        if (recived_bytes < 0) {
            if (errno == EINTR) {
                // Funkcja przerwana przez nasz sygnał SIGINT.
                // Pętla while zaraz sprawdzi 'pracujemy' i sama się zakończy.
                continue;
            }
            ERR("recvfrom"); // To zadziała, jeśli błąd był krytyczny (nie spowodowany sygnałem)
        }
        odebrany.tekst[recived_bytes] = '\0';
        // Wrzucamy gotowy raport na stos, skąd zabiorą go adiutanci.
        pthread_mutex_lock(&state->mutex);
        // jesli nie mamy 16 elementow na stosie to wrzucamy nowy ( maks element to 16 bo mamy tablice o jeden wieksza)
        if (state->top < STACK_SIZE) {
            state->stos[state->top] = odebrany;
            state->top++;
            pthread_cond_signal(&state->cond);
        }else {
            fprintf(stderr, "Serwer: Stos jest pełny! Meldunek przepadł w chaosie bitwy.\n");
        }
        pthread_mutex_unlock(&state->mutex);
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

    // Jedna lokalna struktura z całym stanem serwera.
    app_state_t state;
    memset(&state, 0, sizeof(state));
    pthread_mutex_init(&state.mutex, NULL);
    pthread_cond_init(&state.cond, NULL);
    pthread_mutex_init(&state.mutex_do_nazw, NULL);
    state.top = 0;
    state.liczba_oddzialow = 0;
    state.socket_fd = socket_fd;

    for (int i = 0; i < DIVISION_NAMES_SIZE; i++) {
        state.przynaleznosc[i] = -1;
    }

    for(int i=0; i<100; i++) {
        pthread_mutex_init(&state.mutex_mapa[i], NULL);
        for(int j=0; j<100; j++) {
            state.mapa[i][j] = -1; // -1 oznacza, że pole jest puste
        }
    }

    // Startujemy 4 pracujące wątki, każdy dostaje wskaźnik do tego samego stanu.
    pthread_t adiutanci[4];
    for (int i=0; i<4; i++) {
        if (pthread_create(&adiutanci[i], NULL, thread_work, &state) != 0) {
            fprintf(stderr, "Błąd podczas tworzenia wątku!\n");
            exit(EXIT_FAILURE);
        }
    }

    // Dodatkowy wątek Napoleona: pokazuje mapę i wysyła rozkazy do sojuszników.
    pthread_t napoleon;
    if (pthread_create(&napoleon, NULL, napoleon_work, &state) != 0) {
        fprintf(stderr, "Błąd podczas tworzenia wątku Napoleona!\n");
        exit(EXIT_FAILURE);
    }

    serwer_work(socket_fd, &state);


    // ====== SEKCJA ZAMYKANIA ======
    // Jeśli tu dotarliśmy, oznacza to, że wciśnięto Ctrl+C i wyszliśmy z pętli serwer_work.
    printf("\nZamykanie sztabu. Budzenie adiutantów do wyjścia...\n");

    // Budzimy WSZYSTKICH uśpionych adiutantów (alarm przeciwpożarowy!)
    // Wstaną, zobaczą że pracujemy == 0, sprawdzą czy stos jest pusty i wyjdą.
    pthread_cond_broadcast(&state.cond);

    // Czekamy na każdego z nich (pthread_join), żeby mieć pewność, że odłożyli długopisy
    for (int i = 0; i < 4; i++) {
        if (pthread_join(adiutanci[i], NULL) != 0) {
            ERR("pthread_join");
        }
    }

    // Czekamy jeszcze na Napoleona, żeby skończył ostatni cykl drukowania i wysyłania.
    if (pthread_join(napoleon, NULL) != 0) {
        ERR("pthread_join");
    }

    // Zamykamy gniazdo i uroczyście niszczymy narzędzia do synchronizacji
    if (TEMP_FAILURE_RETRY(close(socket_fd)) < 0) ERR("close");
    pthread_mutex_destroy(&state.mutex);
    pthread_cond_destroy(&state.cond);
    pthread_mutex_destroy(&state.mutex_do_nazw);
    // Każdy wiersz mapy miał własny mutex, więc sprzątamy je po kolei.
    for(int i=0; i<100; i++) {
        pthread_mutex_destroy(&state.mutex_mapa[i]);
    }
    printf("Sztab zamknięty bezpiecznie.\n");
    return EXIT_SUCCESS;
}