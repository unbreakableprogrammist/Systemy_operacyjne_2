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
#define STACK_SIZE 16         // Maksymalna pojemność naszej kolejki meldunków
#define DIVISION_NAMES_SIZE 128 // Maksymalna liczba unikalnych oddziałów
#define MAX_NAME 129          // Maksymalna długość nazwy oddziału (128 znaków + \0)

// Makro do wygodnego wypisywania błędów systemowych i zamykania programu
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// Makro systemowe do wznawiania przerwanych funkcji (np. close)
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
(__extension__                                                              \
({ long int __result;                                                     \
do __result = (long int) (expression);                                 \
while (__result == -1L && errno == EINTR);                             \
__result; }))
#endif

void usage(char *name) { fprintf(stderr, "USAGE: %s port\n", name); }

// ==========================================
// ZMIENNE GLOBALNE I SYGNAŁY
// ==========================================
// Flaga musi być globalna, bo funkcja sygnału (sigint_handler) nie przyjmuje własnych argumentów.
// Słowo 'volatile' mówi kompilatorowi: "ta zmienna może się zmienić w każdej chwili przez sygnał z zewnątrz".
volatile sig_atomic_t pracujemy = 1;

void sigint_handler(int sig) {
    pracujemy = 0; // Kiedy wciskamy Ctrl+C, zmieniamy flagę na 0 (zamykamy sztab)
}

int sethandler(void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL)) return -1;
    return 0;
}

int bind_udp_socket(int port) {
    int socketfd;
    if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) ERR("socket");
    
    struct sockaddr_in address; 
    int t = 1; 

    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET; 
    address.sin_port = htons(port); 
    address.sin_addr.s_addr = htonl(INADDR_ANY); 

    // Pozwala na szybki restart serwera na tym samym porcie po awarii
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)) < 0) ERR("setsockopt");
    if (bind(socketfd, (struct sockaddr *)&address, sizeof(address)) < 0) ERR("bind");

    return socketfd; 
}

// ==========================================
// STRUKTURA BAZY DANYCH SZTABU (Zamiast zmiennych globalnych)
// ==========================================
typedef struct {
    // 1. KOLEJKA FIFO (Bufor Cykliczny)
    char kolejka[STACK_SIZE][MAX_BUF]; // Tablica przechowująca tekst meldunków
    int head;  // Głowa: stąd adiutanci CZYTAJĄ najstarsze meldunki
    int tail;  // Ogon: tutaj serwer WPISUJE nowe meldunki
    int count; // Licznik: mówi nam, ile łącznie elementów jest teraz w kolejce
    pthread_mutex_t mutex_kolejki; // Kłódka chroniąca kolejkę przed chaosem
    pthread_cond_t cond_kolejki;   // Dzwonek budzący śpiących adiutantów

    // 2. NOTES Z NAZWAMI ODDZIAŁÓW
    char nazwy_oddzialow[DIVISION_NAMES_SIZE][MAX_NAME]; // Pamięć na nazwy
    int liczba_oddzialow; // Ile unikalnych nazw już zapisaliśmy
    pthread_mutex_t mutex_do_nazw; // Kłódka chroniąca notes

    // 3. MAPA SZTABOWA
    int mapa[100][100]; // Plansza 100 wierszy na 100 kolumn
    pthread_mutex_t mutex_mapa[100]; // 100 kłódek (po jednej na KAŻDY wiersz)
} Sztab;


// ==========================================
// WĄTEK ADIUTANTA (Konsument FIFO)
// ==========================================
void* thread_work(void *arg) {
    // Rozpakowujemy paczkę (rzutujemy anonimowy wskaźnik na wskaźnik do naszej struktury)
    Sztab *sztab = (Sztab *)arg; 
    
    char dane[MAX_BUF]; // Lokalna kartka adiutanta na skopiowany meldunek
    int x, y, p;
    char nazwa[MAX_NAME]; 
    
    while (1) {
        // --- ETAP: POBIERANIE Z KOLEJKI FIFO ---
        pthread_mutex_lock(&sztab->mutex_kolejki);

        // Śpimy dopóki kolejka jest PUSTA (count == 0) i szef nadal każe pracować
        while (sztab->count == 0 && pracujemy == 1) {
            pthread_cond_wait(&sztab->cond_kolejki, &sztab->mutex_kolejki);
        }

        // Zostaliśmy obudzeni. Jeśli szef kazał iść do domu (0) i nie ma już pracy (count == 0) - wychodzimy
        if (pracujemy == 0 && sztab->count == 0) {
            pthread_mutex_unlock(&sztab->mutex_kolejki);
            break;
        }

        // Bierzemy najstarszy meldunek z "głowy" kolejki
        strcpy(dane, sztab->kolejka[sztab->head]); 
        
        // Przesuwamy głowę do przodu. Modulo (%) sprawia, że jeśli head dojdzie do 16, wróci na 0
        sztab->head = (sztab->head + 1) % STACK_SIZE;
        
        // Skoro zabraliśmy jeden meldunek, zmniejszamy licznik
        sztab->count--; 

        pthread_mutex_unlock(&sztab->mutex_kolejki); 

        // --- ETAP: PARSOWANIE TEKSTU ---
        int parsed_items = sscanf(dane, "%d %d %d %128[^\n]", &x, &y, &p, nazwa);
        if (parsed_items != 4) continue; // Ignorujemy zepsute teksty
        if (x < 0 || x > 99 || y < 0 || y > 99 || (p != 0 && p != 1)) continue; // Ignorujemy złe pozycje

        usleep(10000); // 10ms - symulacja ciężkiej, myślowej pracy adiutanta

        // --- ETAP: REJESTRACJA W NOTESIE ---
        int id_oddzialu = -1; // Na start zakładamy, że nie znamy tego oddziału
        pthread_mutex_lock(&sztab->mutex_do_nazw);
        
        // Przeszukujemy notes
        for (int i = 0; i < sztab->liczba_oddzialow; i++) {
            if (strcmp(sztab->nazwy_oddzialow[i], nazwa) == 0) {
                id_oddzialu = i; // Znaleźliśmy! Zapisujemy jego numer
                break;
            }
        }
        
        // Jeśli to nowy oddział i mamy jeszcze miejsce w notesie, dopisujemy go
        if (id_oddzialu == -1 && sztab->liczba_oddzialow < DIVISION_NAMES_SIZE){
            id_oddzialu = sztab->liczba_oddzialow;
            strcpy(sztab->nazwy_oddzialow[id_oddzialu], nazwa);
            sztab->liczba_oddzialow++;
        }
        pthread_mutex_unlock(&sztab->mutex_do_nazw);
        
        if (id_oddzialu == -1) {
            fprintf(stderr, "Brak miejsca w rejestrze dla oddziału: %s\n", nazwa);
            continue; // Bez ID nie możemy ruszyć dalej
        }

        // --- ETAP: RYSOWANIE NA MAPIE ---
        // KROK 1: Wymazanie STAREJ pozycji oddziału z planszy
        for(int wiersz=0; wiersz<100; wiersz++) {
            pthread_mutex_lock(&sztab->mutex_mapa[wiersz]); // Zamykamy tylko sprawdzany wiersz
            for(int kolumna=0; kolumna<100; kolumna++) {
                if (sztab->mapa[wiersz][kolumna] == id_oddzialu) {
                    sztab->mapa[wiersz][kolumna] = -1; // -1 oznacza pustą trawę
                }
            }
            pthread_mutex_unlock(&sztab->mutex_mapa[wiersz]);
        }
        
        // KROK 2: Wpisanie NOWEJ pozycji na planszę (na podstawie odebranych współrzędnych)
        pthread_mutex_lock(&sztab->mutex_mapa[y]); // Zamykamy konkretny wiersz 'y'
        sztab->mapa[y][x] = id_oddzialu; // Stawiamy pionek
        pthread_mutex_unlock(&sztab->mutex_mapa[y]);
        
        // Raport sztabowy
        char *strona = (p == 1) ? "Nasz" : "Wrogi";
        printf("[Sztab] %s oddział '%s' (ID: %d) ustawił się na [%d:%d]\n", strona, nazwa, id_oddzialu, x, y);
    }
    return NULL;
}

// ==========================================
// WĄTEK SERWERA (Producent FIFO)
// ==========================================
void serwer_work(int socket_fd, Sztab *sztab) {
    char received_buffer[MAX_BUF];
    while (pracujemy) {
        int recived_bytes = recvfrom(socket_fd, received_buffer, MAX_BUF-1, 0, NULL, NULL);
        if (recived_bytes < 0) {
            if (errno == EINTR) continue; // Pętla wyłapie zmianę 'pracujemy' na 0
            ERR("recvfrom"); 
        }
        received_buffer[recived_bytes] = '\0'; // Zabezpieczenie napisu
        
        // --- ETAP: WKŁADANIE DO KOLEJKI FIFO ---
        pthread_mutex_lock(&sztab->mutex_kolejki);
        
        // Sprawdzamy czy w kolejce jest jeszcze miejsce (czy count < 16)
        if (sztab->count < STACK_SIZE) {
            // Wpisujemy w miejsce, na które aktualnie wskazuje ogon
            strcpy(sztab->kolejka[sztab->tail], received_buffer);
            
            // Przesuwamy ogon o jedno pole dalej. Modulo (%) robi "pętlę" z 15 na 0
            sztab->tail = (sztab->tail + 1) % STACK_SIZE;
            
            // Zwiększamy liczbę paczek w kolejce
            sztab->count++;
            
            // Dzwonimy, żeby obudzić jakiegoś śpiącego adiutanta
            pthread_cond_signal(&sztab->cond_kolejki);
        } else {
            fprintf(stderr, "Serwer: Kolejka jest pełna! Meldunek przepadł.\n");
        }
        
        pthread_mutex_unlock(&sztab->mutex_kolejki);
    }
}

// ==========================================
// FUNKCJA MAIN
// ==========================================
int main(int argc, char *argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Ignorujemy błędy zerwanego potoku i podpinamy naszą flagę pod Ctrl+C
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE:");
    if (sethandler(sigint_handler, SIGINT)) ERR("Setting SIGINT:");
    
    int port = atoi(argv[1]);
    int socket_fd = bind_udp_socket(port);

    // INICJALIZACJA STRUKTURY SZTABU
    Sztab dane_sztabu;
    
    // Na start kolejka i notes są puste
    dane_sztabu.head = 0;
    dane_sztabu.tail = 0;
    dane_sztabu.count = 0;
    dane_sztabu.liczba_oddzialow = 0;
    
    // Inicjalizacja głównych kłódek i dzwonka
    pthread_mutex_init(&dane_sztabu.mutex_kolejki, NULL);
    pthread_cond_init(&dane_sztabu.cond_kolejki, NULL);
    pthread_mutex_init(&dane_sztabu.mutex_do_nazw, NULL);

    // Inicjalizacja kłódek mapy i wypełnianie planszy trawą (-1)
    for(int i=0; i<100; i++) {
        pthread_mutex_init(&dane_sztabu.mutex_mapa[i], NULL);
        for(int j=0; j<100; j++) {
            dane_sztabu.mapa[i][j] = -1; 
        }
    }

    // Tworzenie 4 pracusiów (adiutantów) i wręczanie im kluczy do sztabu
    pthread_t adiutanci[4];
    for (int i=0; i<4; i++) {
        if (pthread_create(&adiutanci[i], NULL, thread_work, &dane_sztabu) != 0) {
            ERR("pthread create");
        }
    }

    // WŁĄCZAMY SERWER (Ten wiersz zablokuje program w pętli)
    serwer_work(socket_fd, &dane_sztabu);

    // ==========================================
    // ZAMYKANIE SZTABU (Po wciśnięciu Ctrl+C)
    // ==========================================
    printf("\nZamykanie sztabu...\n");
    
    // Budzimy WSZYSTKICH adiutantów (alarm!), żeby sprawdzili flagę i wrócili do domu
    pthread_cond_broadcast(&dane_sztabu.cond_stosu); // UWAGA: Literówka! Popraw na cond_kolejki
    pthread_cond_broadcast(&dane_sztabu.cond_kolejki);
    
    // Czekamy, aż każdy grzecznie wyjdzie z funkcji
    for (int i = 0; i < 4; i++) {
        if (pthread_join(adiutanci[i], NULL) != 0) ERR("pthread_join");
    }

    if (TEMP_FAILURE_RETRY(close(socket_fd)) < 0) ERR("close");
    
    // Niszczymy narzędzia do synchronizacji, żeby oddać pamięć do systemu
    pthread_mutex_destroy(&dane_sztabu.mutex_kolejki);
    pthread_cond_destroy(&dane_sztabu.cond_kolejki);
    pthread_mutex_destroy(&dane_sztabu.mutex_do_nazw);
    for(int i=0; i<100; i++) {
        pthread_mutex_destroy(&dane_sztabu.mutex_mapa[i]);
    }
    
    printf("Sztab zamknięty bezpiecznie.\n");
    return EXIT_SUCCESS;
}