#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SHOP_FILENAME "./shop"
#define MIN_SHELVES 8
#define MAX_SHELVES 256
#define MIN_WORKERS 1
#define MAX_WORKERS 64

#define ERR(source)                                     \
    do                                                  \
    {                                                   \
        fprintf(stderr, "%s:%d\n", __FILE__, __LINE__); \
        perror(source);                                 \
        kill(0, SIGKILL);                               \
        exit(EXIT_FAILURE);                             \
    } while (0)

void usage(char* program_name)
{
    fprintf(stderr, "Usage: \n");
    fprintf(stderr, "\t%s n m\n", program_name);
    fprintf(stderr, "\t  n - number of items (shelves), %d <= n <= %d\n", MIN_SHELVES, MAX_SHELVES);
    fprintf(stderr, "\t  m - number of workers, %d <= m <= %d\n", MIN_WORKERS, MAX_WORKERS);
    exit(EXIT_FAILURE);
}

void ms_sleep(unsigned int milli)
{
    time_t sec = (int)(milli / 1000);
    milli = milli - (sec * 1000);
    struct timespec ts = {0};
    ts.tv_sec = sec;
    ts.tv_nsec = milli * 1000000L;
    if (nanosleep(&ts, &ts))
        ERR("nanosleep");
}

void swap(int* x, int* y)
{
    int tmp = *y;
    *y = *x;
    *x = tmp;
}

void shuffle(int* array, int n)
{
    for (int i = n - 1; i > 0; i--)
    {
        int j = rand() % (i + 1);
        swap(&array[i], &array[j]);
    }
}

void print_array(int* array, int n)
{
    for (int i = 0; i < n; ++i)
    {
        printf("%3d ", array[i]);
    }
    printf("\n");
}

// Zmiana w strukturze: lepsza nazwa i dodatkowe pola z Etapu 4
typedef struct {
    int running;
    pthread_mutex_t mutex;
    int ile; // liczba pracowników, którzy skończyli pracę
} shared_data_t;

// Poprawione nagłówki funkcji - używamy pthread_mutex_t*
void children_work(int id, int* shelves, pthread_mutex_t* mutexes, int n, int m) {
    printf("[%d] Worker reports for a night shift\n", getpid()); // 
    srand(time(NULL) ^ getpid());
    
    // Rzutujemy pamięć za muteksami na strukturę sterującą
    shared_data_t* sd = (shared_data_t*)((char*)mutexes + n * sizeof(pthread_mutex_t));

    for(int i = 0; i < 10; i++) {
        // Sprawdzenie flagi zakończenia
        pthread_mutex_lock(&sd->mutex);
        if(!sd->running) {
            pthread_mutex_unlock(&sd->mutex);
            break;
        }
        pthread_mutex_unlock(&sd->mutex);

        int s1 = rand() % n;
        int s2;
        do { s2 = rand() % n; } while(s1 == s2);

        int low = s1 < s2 ? s1 : s2;
        int high = s1 > s2 ? s1 : s2;
        
        // Prawidłowe blokowanie struktur muteksów
        pthread_mutex_lock(&mutexes[low]);
        pthread_mutex_lock(&mutexes[high]);

        if(shelves[low] > shelves[high]) {
            swap(&shelves[low], &shelves[high]);
            ms_sleep(100); // [cite: 30]
        }

        pthread_mutex_unlock(&mutexes[high]);
        pthread_mutex_unlock(&mutexes[low]);
    }

    pthread_mutex_lock(&sd->mutex);
    sd->ile++;
    pthread_mutex_unlock(&sd->mutex);
}

void manager_work(int* shelves, pthread_mutex_t* mutexes, int n, int m) {
    printf("[%d] Manager reports for a night shift\n", getpid()); // 
    shared_data_t* sd = (shared_data_t*)((char*)mutexes + n * sizeof(pthread_mutex_t));

    while(1) {
        ms_sleep(500); // 
        print_array(shelves, n); // 
        msync(shelves, n * sizeof(int), MS_SYNC); // 

        pthread_mutex_lock(&sd->mutex);
        
        // Sprawdzenie czy wszyscy skończyli
        if(sd->ile == m) {
            sd->running = 0;
            pthread_mutex_unlock(&sd->mutex);
            break;
        }

        // Sprawdzenie porządku
        int sorted = 1;
        for(int i = 1; i < n; i++) {
            if(shelves[i-1] > shelves[i]) {
                sorted = 0;
                break;
            }
        }

        if(sorted) {
            printf("[%d] The shop shelves are sorted\n", getpid()); // 
            sd->running = 0;
            pthread_mutex_unlock(&sd->mutex);
            break;
        }
        pthread_mutex_unlock(&sd->mutex);
    }
}
int main(int argc, char** argv) {
    // 1. Walidacja argumentów wejściowych [cite: 23]
    if (argc != 3) usage(argv[0]);
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if (n < MIN_SHELVES || n > MAX_SHELVES) usage(argv[0]);
    if (m < MIN_WORKERS || m > MAX_WORKERS) usage(argv[0]);

    // 2. Przygotowanie pliku SHOP_FILENAME i mapowanie półek [cite: 25]
    int fd = open(SHOP_FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (fd == -1) ERR("open");
    if (ftruncate(fd, n * sizeof(int)) == -1) ERR("ftruncate");

    int* shm_ptr = (int*)mmap(NULL, n * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_ptr == MAP_FAILED) ERR("mmap shop");
    close(fd); // Deskryptor nie jest już potrzebny po mmap

    // 3. Mapowanie anonimowej pamięci na muteksy i zmienne współdzielone [cite: 32, 33]
    size_t shared_size = n * sizeof(pthread_mutex_t) + sizeof(shared_data_t);
    void* shm_base = mmap(NULL, shared_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm_base == MAP_FAILED) ERR("mmap anonymous");

    // Wyznaczamy wskaźniki wewnątrz zmapowanej pamięci
    pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)shm_base;
    shared_data_t* sd = (shared_data_t*)((char*)shm_base + n * sizeof(pthread_mutex_t));

    // 4. Konfiguracja atrybutów muteksów (Robust i Process Shared) 
    pthread_mutexattr_t mutex_attr;
    if (pthread_mutexattr_init(&mutex_attr)) ERR("pthread_mutexattr_init");
    if (pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED)) ERR("pthread_mutexattr_setpshared");
    if (pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST)) ERR("pthread_mutexattr_setrobust");

    // Inicjalizacja muteksów dla każdej półki
    for (int i = 0; i < n; i++) {
        if (pthread_mutex_init(&mutex_ptr[i], &mutex_attr)) ERR("pthread_mutex_init shelf");
    }

    // Inicjalizacja struktury kontrolnej
    sd->running = 1;
    sd->ile = 0; // Licznik pracowników, którzy skończyli (Etap 3)
    // alive_count zainicjalizujesz tutaj, gdy przejdziesz do Etapu 4
    if (pthread_mutex_init(&sd->mutex, &mutex_attr)) ERR("pthread_mutex_init shared_data");
    
    pthread_mutexattr_destroy(&mutex_attr);

    // 5. Inicjalizacja tablicy produktów, przetasowanie i wypisanie [cite: 26, 27]
    srand(time(NULL));
    for (int i = 0; i < n; ++i) {
        shm_ptr[i] = i + 1;
    }
    shuffle(shm_ptr, n);
    
    printf("Initial shop state:\n");
    print_array(shm_ptr, n);

    // 6. Tworzenie procesów pracowników [cite: 28]
    for (int i = 0; i < m; i++) {
        pid_t pid = fork();
        if (pid == -1) ERR("fork worker");
        if (pid == 0) {
            children_work(i, shm_ptr, mutex_ptr, n, m);
            exit(EXIT_SUCCESS);
        }
    }

    // 7. Tworzenie procesu kierownika [cite: 37]
    pid_t manager_pid = fork();
    if (manager_pid == -1) ERR("fork manager");
    if (manager_pid == 0) {
        manager_work(shm_ptr, mutex_ptr, n, m);
        exit(EXIT_SUCCESS);
    }

    // 8. Oczekiwanie na zakończenie wszystkich procesów 
    // Czekamy na M pracowników + 1 kierownika
    for (int i = 0; i < m + 1; i++) {
        wait(NULL);
    }

    // 9. Komunikat końcowy i sprzątanie 
    printf("Night shift in Bitronka is over\n");
    print_array(shm_ptr, n);

    // Niszczenie muteksów i odmapowanie pamięci
    for (int i = 0; i < n; i++) pthread_mutex_destroy(&mutex_ptr[i]);
    pthread_mutex_destroy(&sd->mutex);
    
    munmap(shm_ptr, n * sizeof(int));
    munmap(shm_base, shared_size);

    return EXIT_SUCCESS;
}
