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

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

typedef struct {
    char nazwa[MAX_NAME_LEN];
    int sasiedzi[MAX_NODES];
    int liczba_sasiadow;
} Wezel;

typedef struct {
    int target_id;           // Kogo szukamy
    int path[MAX_NODES * 5]; // Trasa przebyta przez wiadomość
    int path_len;            // Długość trasy
    int ttl;                 // "Time To Live" - by wiadomość nie krążyła w nieskończoność
} Wiadomosc;

// Flagi globalne dla Rodzica
volatile sig_atomic_t wyslij_wiadomosc = 0; // Flaga ustawiana przez Ctrl+C
volatile sig_atomic_t dziala_program = 1;   // Flaga ustawiana przez Ctrl+\ (SIGQUIT)

// Handlery sygnałów
void sigint_handler(int sig) {
    wyslij_wiadomosc = 1; // Ctrl+C -> wypuść ziemniaka!
}

void sigquit_handler(int sig) {
    dziala_program = 0; // Ctrl+\ -> wyłącz program
}

int main(int argc, char* argv[]) {
    srand(time(NULL) ^ getpid());

    // ==========================================
    // ETAP 1: Parsowanie pliku
    // ==========================================
    FILE* plik = fopen("siec.txt", "r");
    if (plik == NULL) {
        printf("Network is down\n");
        exit(EXIT_FAILURE);
    }
    
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
    // ETAP 2: Infrastruktura (Rury i FIFO)
    // ==========================================
    const char* raport_fifo = "raport.fifo";
    if (mkfifo(raport_fifo, 0666) == -1 && errno != EEXIST) ERR("mkfifo raport");

    int rury[n][2];
    for (int i = 0; i < n; i++) if (pipe(rury[i]) == -1) ERR("pipe");

    char nazwy_fifo[n][32];
    for (int i = 0; i < n; i++) {
        snprintf(nazwy_fifo[i], sizeof(nazwy_fifo[i]), "fifo_node_%d.fifo", i);
        if (mkfifo(nazwy_fifo[i], 0666) == -1 && errno != EEXIST) ERR("mkfifo wezel");
    }

    // ==========================================
    // ETAP 3: Procesy Potomne (Node'y)
    // ==========================================
    pid_t children[n];
    for (int i = 0; i < n; i++) {
        children[i] = fork();
        if (children[i] == -1) ERR("fork");

        if (children[i] == 0) { // --- KOD DZIECKA ---
            // Dzieci ignorują Ctrl+C, żeby nie odpalać akcji wysyłania (robi to tylko Rodzic)
            signal(SIGINT, SIG_IGN); 
            // Dzieci zamykają się ładnie na sygnał SIGTERM
            signal(SIGTERM, SIG_DFL);

            int my_in_fifo = open(nazwy_fifo[i], O_RDWR | O_NONBLOCK);
            int raport_fd = open(raport_fifo, O_RDWR | O_NONBLOCK);

            for (int j = 0; j < n; j++) {
                if (i == j) {
                    close(rury[j][1]); 
                    fcntl(rury[j][0], F_SETFL, O_NONBLOCK); // Nieblokujące czytanie z własnej rury
                } else {
                    close(rury[j][0]);
                    if (wezly[i].sasiedzi[j] == 0) close(rury[j][1]);
                }
            }

            Wiadomosc msg;
            while (1) { // Dziecko kręci się, aż Rodzic go nie zabije
                int got_msg = 0;

                // 1. Czytamy z FIFO od Rodzica
                if (read(my_in_fifo, &msg, sizeof(Wiadomosc)) == sizeof(Wiadomosc)) got_msg = 1;
                // 2. Czytamy z rury od innego Node'a
                else if (read(rury[i][0], &msg, sizeof(Wiadomosc)) == sizeof(Wiadomosc)) got_msg = 1;

                if (got_msg) {
                    msg.path[msg.path_len++] = i; // Zapisujemy swój krok
                    msg.ttl--;

                    if (msg.target_id == i) {
                        // DOSZŁO DO CELU!
                        printf("\n[%s] DOSTALEM WIADOMOSC! Zglaszam do centrali.\n", wezly[i].nazwa);
                        write(raport_fd, &msg, sizeof(Wiadomosc)); // Zwracamy przez FIFO do Rodzica
                    } 
                    else if (msg.ttl > 0 && wezly[i].liczba_sasiadow > 0) {
                        // PRZEKAZUJEMY DALEJ
                        int wylosowany = rand() % wezly[i].liczba_sasiadow;
                        int licznik = 0;
                        for(int j = 0; j < n; j++) {
                            if(wezly[i].sasiedzi[j] == 1) {
                                if(licznik == wylosowany) { 
                                    write(rury[j][1], &msg, sizeof(Wiadomosc)); 
                                    break; 
                                }
                                licznik++;
                            }
                        }
                    } else if (msg.ttl <= 0) {
                         printf("[%s] Wiadomosc umarla smiercia naturalna (TTL = 0).\n", wezly[i].nazwa);
                    }
                }
                usleep(50000); // Nie blokujemy CPU
            }
        }
    }

    // ==========================================
    // ETAP 4: KOD RODZICA (SERWERA)
    // ==========================================
    
    // Ustawiamy handlery dla Rodzica
    struct sigaction act_int = {0}, act_quit = {0};
    act_int.sa_handler = sigint_handler;
    sigaction(SIGINT, &act_int, NULL);
    act_quit.sa_handler = sigquit_handler;
    sigaction(SIGQUIT, &act_quit, NULL);

    for (int i = 0; i < n; i++) { close(rury[i][0]); close(rury[i][1]); }

    int fifo_dzieci[n];
    for(int i=0; i<n; i++) fifo_dzieci[i] = open(nazwy_fifo[i], O_WRONLY | O_NONBLOCK); // Zmieniono na WRONLY
    
    int raport_fd = open(raport_fifo, O_RDONLY | O_NONBLOCK);

    printf("\nSerwer uruchomiony.\n");
    printf("-> Wcisnij Ctrl+C aby puscic wiadomosc!\n");
    printf("-> Wcisnij Ctrl+\\ aby zakonczyc program.\n");

    while (dziala_program) {
        
        // 1. Jeśli wciśnięto Ctrl+C
        if (wyslij_wiadomosc) {
            wyslij_wiadomosc = 0; // Zerujemy flagę
            
            Wiadomosc msg;
            memset(&msg, 0, sizeof(Wiadomosc));
            int start_node = rand() % n;
            msg.target_id = rand() % n;
            msg.path_len = 0;
            msg.ttl = 20; // 20 skoków zanim zniknie

            printf("\n--- RODZIC WYSYLA: Cel: [%s]. Start z: [%s] ---\n", wezly[msg.target_id].nazwa, wezly[start_node].nazwa);
            write(fifo_dzieci[start_node], &msg, sizeof(Wiadomosc));
        }

        // 2. Nieblokujące sprawdzanie, czy przyszła odpowiedź od Node'a
        Wiadomosc odp;
        if (read(raport_fd, &odp, sizeof(Wiadomosc)) == sizeof(Wiadomosc)) {
            printf("\n>>> RAPORT ODCZYTANY PRZEZ RODZICA <<<\n");
            printf("Cel [%s] osiagniety! Trasa: ", wezly[odp.target_id].nazwa);
            for(int i = 0; i < odp.path_len; i++) {
                printf("%s", wezly[odp.path[i]].nazwa);
                if(i < odp.path_len - 1) printf(" -> ");
            }
            printf("\n\n");
        }

        usleep(50000); // Sprawdzamy co 50ms (0.05 sekundy)
    }

    // ==========================================
    // ETAP 5: SPRZĄTANIE (Po wciśnięciu Ctrl+\)
    // ==========================================
    printf("\nZamykanie programu...\n");
    
    // Zabijamy delikatnie wszystkie dzieci (żeby nie wisiały w tle)
    for(int i = 0; i < n; i++) kill(children[i], SIGTERM);
    for(int i = 0; i < n; i++) wait(NULL);

    unlink(raport_fifo);
    for(int i = 0; i < n; i++) unlink(nazwy_fifo[i]);
    
    printf("Posprzatane. Narazie!\n");
    return EXIT_SUCCESS;
}