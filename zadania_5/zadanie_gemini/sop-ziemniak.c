#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
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

int set_handler(void (*f)(int), int sig)
{
    struct sigaction act = {0};
    act.sa_handler = f;
    if (sigaction(sig, &act, NULL) == -1)
        return -1;
    return 0;
}

void msleep(int millisec)
{
    struct timespec tt;
    tt.tv_sec = millisec / 1000;
    tt.tv_nsec = (millisec % 1000) * 1000000;
    while (nanosleep(&tt, &tt) == -1)
    {
        if (errno != EINTR) ERR("nanosleep");
    }
}

int count_descriptors()
{
    int count = 0;
    DIR* dir;
    struct dirent* entry;
    struct stat stats;
    if ((dir = opendir("/proc/self/fd")) == NULL)
        ERR("opendir");
    char path[PATH_MAX];
    getcwd(path, PATH_MAX);
    chdir("/proc/self/fd");
    do
    {
        errno = 0;
        if ((entry = readdir(dir)) != NULL)
        {
            if (lstat(entry->d_name, &stats))
                ERR("lstat");
            if (!S_ISDIR(stats.st_mode))
                count++;
        }
    } while (entry != NULL);
    if (chdir(path))
        ERR("chdir");
    if (closedir(dir))
        ERR("closedir");
    return count - 1;  // one descriptor for open directory
}

// Struktura pomocnicza do trzymania w pamięci danych o węźle
typedef struct {
    char nazwa[MAX_NAME_LEN];
    int sasiedzi[MAX_NODES]; // Macierz jednowymiarowa dla danego węzła (1 = połączenie, 0 = brak)
} Wezel;



#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_KNIGHT_NAME_LENGHT 64

void wczytaj_frankow_open() {
    int fd = open("franci.txt", O_RDONLY);
    if (fd == -1) {
        printf("Franks have not arrived on the battlefield\n"); // [cite: 15]
        exit(EXIT_FAILURE);
    }

    // Zakładamy, że plik zmieści się w 4 KB. Czytamy całość na raz.
    char bufor[4096];
    int przeczytano = read(fd, bufor, sizeof(bufor) - 1);
    
    if (przeczytano <= 0) {
        perror("Błąd czytania lub pusty plik");
        exit(EXIT_FAILURE);
    }
    bufor[przeczytano] = '\0'; // Bezpiecznie zamykamy tekst

    int n;
    int offset = 0; // Przesunięcie w naszym buforze
    int przeczytane_znaki = 0;

    // Czytamy N i sprawdzamy, ile znaków zajął ten odczyt
    sscanf(bufor + offset, "%d%n", &n, &przeczytane_znaki);
    offset += przeczytane_znaki; // Przesuwamy się za liczbę N i entera

    char imie[MAX_KNIGHT_NAME_LENGHT];
    int hp, atak;

    for (int i = 0; i < n; i++) {
        // Czytamy rycerza i aktualizujemy offset [cite: 17]
        sscanf(bufor + offset, "%s %d %d%n", imie, &hp, &atak, &przeczytane_znaki);
        offset += przeczytane_znaki;
        
        printf("I am Frankish knight %s. I will serve my king with my %d HP and %d attack.\n", 
               imie, hp, atak); // 
    }

    close(fd);
}


int main(int argc, char* argv[])
{
    // Używamy naszego triku z poprzeniego zadania dla unikalnego ziarna!
    srand(time(NULL) ^ getpid()); 
    printf("Opened descriptors at start: %d\n", count_descriptors());

    // --- ETAP 1: Parsowanie pliku siec.txt ---
    FILE* siec = fopen(fra)
    

    // --- ETAP 2: Tworzenie rur FIFO i procesów ---

    // --- ETAP 3: Logika Gorącego Ziemniaka (w procesach potomnych) ---

    // --- ETAP 4: Sprzątanie (czekanie na koniec, kill, unlink) w Rodzicu ---

    return EXIT_SUCCESS;
}