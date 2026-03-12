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

int main(int argc, char* argv[])
{
    // Używamy naszego triku z poprzeniego zadania dla unikalnego ziarna!
    srand(time(NULL) ^ getpid()); 
    printf("Opened descriptors at start: %d\n", count_descriptors());

    // --- ETAP 1: Parsowanie pliku siec.txt ---
    FILE* plik = fopen("siec.txt", "r");
    if (plik == NULL) {
        ERR("fopen siec.txt");
    }
    
    int n;
    // Czytamy N
    if (fscanf(plik, "%d", &n) != 1) ERR("fscanf N");
    
    
    fclose(plik);

    // --- ETAP 2: Tworzenie rur FIFO i procesów ---

    // --- ETAP 3: Logika Gorącego Ziemniaka (w procesach potomnych) ---

    // --- ETAP 4: Sprzątanie (czekanie na koniec, kill, unlink) w Rodzicu ---

    return EXIT_SUCCESS;
}