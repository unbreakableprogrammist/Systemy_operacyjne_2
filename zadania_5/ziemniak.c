#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_NODES 10
#define MAX_NAME_LEN 64

// Makro do obsługi błędów: wypisuje plik, linię, opis błędu, 
// zabija wszystkie procesy w grupie i kończy program.
#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

// Struktura opisująca węzeł wczytany z grafu
typedef struct { 
    char nazwa[MAX_NAME_LEN];
    int sasiedzi[MAX_NODES]; // Wiersz macierzy sąsiedztwa (1 = krawędź)
    int liczba_sasiadow;     // Ile jedynek w wierszu (ułatwia losowanie)
} Wezel;

// Paczka danych podróżująca po sieci rur
typedef struct { 
    int target_id;           // Indeks węzła, który ma odebrać wiadomość
    int path[MAX_NODES * 5]; // Historia odwiedzonych węzłów (trasa)
    int path_len;            // Liczba skoków, które już wykonano
    int ttl;                 // Time To Live - licznik, który maleje (ochrona przed pętlą)
} Wiadomosc;

// Zmienne sig_atomic_t są bezpieczne do modyfikacji wewnątrz handlerów sygnałów.
// volatile mówi kompilatorowi, że wartość może zmienić się nagle (poza głównym nurtem kodu).
volatile sig_atomic_t wyslij_wiadomosc = 0; 
volatile sig_atomic_t dziala_program = 1;   

// Handler dla Ctrl+C - tylko ustawia flagę, nie robi ciężkich operacji (zasada bezpieczeństwa)
void sigint_handler(int sig) { wyslij_wiadomosc = 1; }

// Handler dla Ctrl+\ - kończy główną pętlę serwera
void sigquit_handler(int sig) { dziala_program = 0; }

int main(int argc, char* argv[]) {
    // Ziarno losowości unikalne dla procesu (czas + PID)
    srand(time(NULL) ^ getpid());

    // ==========================================
    // ETAP 1: Parsowanie pliku siec.txt
    // ==========================================
    FILE* plik = fopen("siec.txt", "r");
    if (plik == NULL) { printf("Network is down\n"); exit(EXIT_FAILURE); }
    
    int n;
    if (fscanf(plik, "%d", &n) != 1) ERR("fscanf N");
    Wezel wezly[n]; 

    for (int i = 0; i < n; i++) {
        if (fscanf(plik, "%s", wezly[i].nazwa) != 1) ERR("fscanf nazwa");
        wezly[i].liczba_sasiadow = 0;
        for (int j = 0; j < n; j++) {
            if (fscanf(plik, "%d", &wezly[i].sasiedzi[j]) != 1) ERR("fscanf macierz");
            if (wezly[i].sasiedzi[j] == 1) wezly[i].liczba_sasiadow++;
        }
    }
    fclose(plik);

    // ==========================================
    // ETAP 2: Budowa hydrauliki (Rury i FIFO)
    // ==========================================
    
    // Tworzymy JEDEN plik FIFO, którym wszystkie dzieci będą raportować sukces do Rodzica.
    const char* raport_fifo = "raport.fifo"; 
    if (mkfifo(raport_fifo, 0666) == -1 && errno != EEXIST) ERR("mkfifo raport");

    // Tworzymy N rur nienazwanych. Każda rura[i] to "skrzynka odbiorcza" i-tego węzła.
    int rury[n][2]; 
    for (int i = 0; i < n; i++) if (pipe(rury[i]) == -1) ERR("pipe");

    // ==========================================
    // ETAP 3: Tworzenie dzieci (Węzłów sieci)
    // ==========================================
    pid_t children[n];
    for (int i = 0; i < n; i++) {
        children[i] = fork();
        if (children[i] == -1) ERR("fork");

        if (children[i] == 0) { // --- START KODU DZIECKA ---
            
            // Dzieci mają ignorować Ctrl+C (reaguje na nie tylko Rodzic)
            signal(SIGINT, SIG_IGN); 
            // Dzieci kończą pracę domyślnie po otrzymaniu sygnału SIGTERM od Rodzica
            signal(SIGTERM, SIG_DFL);

            // Każde dziecko otwiera FIFO raportowe do zapisu.
            int raport_fd = open(raport_fifo, O_WRONLY); 

            // SPRZĄTANIE NIEPOTRZEBNYCH KOŃCÓWEK (Klucz do poprawnego IPC):
            for (int j = 0; j < n; j++) {
                if (i == j) {
                    // To moja rura: zamykam pisanie (rury[j][1]), zostawiam czytanie (rury[j][0])
                    close(rury[j][1]); 
                    // Ustawiam MOJE czytanie na NIEBLOKUJĄCE (O_NONBLOCK).
                    // Dzięki temu read() nie zatrzyma procesu, gdy rura jest pusta.
                    fcntl(rury[j][0], F_SETFL, O_NONBLOCK); 
                } else {
                    // Nie czytam z cudzych rur - zamykam ich rury[j][0]
                    close(rury[j][0]); 
                    // Jeśli węzeł j nie jest moim sąsiadem, nie będę do niego pisał - zamykam rury[j][1]
                    if (wezly[i].sasiedzi[j] == 0) close(rury[j][1]);
                }
            }

            Wiadomosc msg;
            while (1) { 
                // Próba odczytu ze swojej rury (płynie tu info od Rodzica lub innych Node'ów)
                if (read(rury[i][0], &msg, sizeof(Wiadomosc)) == sizeof(Wiadomosc)) {
                    
                    // Rejestrujemy wizytę w tym węźle
                    msg.path[msg.path_len++] = i; 
                    msg.ttl--; // Wiadomość traci energię (zabezpieczenie przed cyklami)

                    if (msg.target_id == i) {
                        // SUKCES: To ja byłem celem!
                        printf("\n[%s] ZNALAZLEM! Wysylam sciezke do szefa przez FIFO.\n", wezly[i].nazwa);
                        write(raport_fd, &msg, sizeof(Wiadomosc)); 
                    } 
                    else if (msg.ttl > 0 && wezly[i].liczba_sasiadow > 0) {
                        // PRZEKAZYWANIE: Szukamy losowego sąsiada z macierzy sąsiedztwa
                        int wylosowany = rand() % wezly[i].liczba_sasiadow;
                        int licznik = 0;
                        for(int j = 0; j < n; j++) {
                            if(wezly[i].sasiedzi[j] == 1) {
                                if(licznik == wylosowany) { 
                                    // Wrzucamy wiadomość do rury sąsiada
                                    write(rury[j][1], &msg, sizeof(Wiadomosc)); 
                                    break; 
                                }
                                licznik++;
                            }
                        }
                    }
                }
                // Krótki sen (50ms), żeby nie zużywać 100% CPU w pętli nieblokującej (polling)
                usleep(50000); 
            }
            // --- KONIEC KODU DZIECKA ---
        }
    }

    // ==========================================
    // ETAP 4: KOD RODZICA (SERWERA)
    // ==========================================
    
    // Rejestracja sygnałów dla Rodzica przy pomocy sigaction (bardziej profesjonalne niż signal)
    struct sigaction act_int = {0}, act_quit = {0};
    act_int.sa_handler = sigint_handler; sigaction(SIGINT, &act_int, NULL);
    act_quit.sa_handler = sigquit_handler; sigaction(SIGQUIT, &act_quit, NULL);

    // Rodzic nie czyta z żadnej rury nienazwanej, więc zamyka rury[i][0]
    for (int i = 0; i < n; i++) { 
        close(rury[i][0]); 
        // UWAGA: rury[i][1] (zapis) zostają otwarte! Rodzic musi mieć jak "wstrzyknąć" startowego ziemniaka.
    }
    
    // Otwarcie FIFO raportowego do odczytu w trybie nieblokującym
    int raport_fd = open(raport_fifo, O_RDONLY | O_NONBLOCK);

    printf("\nSieć gotowa. Uzyj Ctrl+C (paczka) lub Ctrl+\\ (koniec).\n");

    while (dziala_program) {
        // Jeśli handler Ctrl+C ustawił flagę wysłania:
        if (wyslij_wiadomosc) {
            wyslij_wiadomosc = 0; 
            Wiadomosc msg;
            memset(&msg, 0, sizeof(Wiadomosc));
            
            int start_node = rand() % n; // Losowy punkt wejścia
            msg.target_id = rand() % n;  // Losowy cel
            msg.ttl = 20;                // Limit skoków

            printf("\n--- RODZIC: Start [%s] -> Cel [%s] ---\n", wezly[start_node].nazwa, wezly[msg.target_id].nazwa);
            // Wysłanie wiadomości bezpośrednio do rury wybranego dziecka
            write(rury[start_node][1], &msg, sizeof(Wiadomosc));
        }

        // Sprawdzanie FIFO czy jakieś dziecko zaraportowało sukces
        Wiadomosc odp;
        if (read(raport_fd, &odp, sizeof(Wiadomosc)) == sizeof(Wiadomosc)) {
            printf("\n>>> SERWER ODEBRAL RAPORT <<<\nTrasa: ");
            for(int i = 0; i < odp.path_len; i++) {
                printf("%s%s", wezly[odp.path[i]].nazwa, (i == odp.path_len-1 ? "" : " -> "));
            }
            printf("\n");
        }
        usleep(50000); // Polling serwera (nieblokujące sprawdzanie flag i FIFO)
    }

    // ==========================================
    // ETAP 5: SPRZĄTANIE (Po Ctrl+\)
    // ==========================================
    printf("\nSprzatam i koncze...\n");
    for(int i = 0; i < n; i++) { 
        kill(children[i], SIGTERM); // Wysyłamy sygnał do dzieci, by zakończyły pętle
        wait(NULL);                 // Zbieramy procesy, by nie zostały ZOMBIE
        close(rury[i][1]);          // Zamykamy rury zapisu
    }
    close(raport_fd);               // Zamykamy FIFO
    unlink(raport_fifo);            // Usuwamy plik FIFO z dysku
    
    printf("Posprzatane. Do widzenia!\n");
    return EXIT_SUCCESS;
}