#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

#define SHM_SIZE 1024

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s N\n", name);
    fprintf(stderr, "3 < N <= 30 - board size\n");
    exit(EXIT_FAILURE);
}

volatile sig_atomic_t running = 1;
void sigint_handler(int sig){
    running = 0;
}

int main(int argc, char ** argv){
    if(argc != 2) usage(argv[0]); // Poprawione: argv[0]
    int n = atoi(argv[1]);
    if(n <= 0 || n >= 30) usage(argv[0]); // Poprawione: argv[0]

    pid_t my_pid = getpid();
    srand(my_pid); 
    printf("MY PID IS : %d\n", my_pid);

    char file_name[40];
    sprintf(file_name, "/%d-board", my_pid);
    
    // O_EXCL jest super - serwer nie nadpisze istniejącej planszy
    int fd = shm_open(file_name, O_CREAT | O_EXCL | O_RDWR, 0666);
    if(fd < 0) ERR("shm open");

    if(ftruncate(fd, SHM_SIZE) == -1) ERR("truncate");

    char* shm_ptr = (char*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shm_ptr == MAP_FAILED) ERR("mmap"); // Poprawione: cudzysłów
 
    // Podział pamięci: [Mutex][N][Board]
    pthread_mutex_t* mutex = (pthread_mutex_t*)shm_ptr;  
    int* N_shared = (int*)(shm_ptr + sizeof(pthread_mutex_t));
    char* board_shared = shm_ptr + sizeof(int) + sizeof(pthread_mutex_t);

    // Konfiguracja ROBUST MUTEX
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    *N_shared = n;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++){
            board_shared[i * n + j] = 1 + rand() % 9;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    if(sigaction(SIGINT, &sa, NULL) == -1) ERR("sigaction");

    while(running){
        int error;
        // Poprawione priorytety operatorów przez nawiasy
        if((error = pthread_mutex_lock(mutex)) != 0){
            if(error == EOWNERDEAD) {
                pthread_mutex_consistent(mutex);
            } else {
                ERR("mutex lock");
            }
        }
        
        for(int i = 0; i < n; i++){
            for(int j = 0; j < n; j++){ // Poprawione: j < n
                printf("%d ", board_shared[i * n + j]); // Dodana spacja
            }
            printf("\n");
        }
        
        pthread_mutex_unlock(mutex);
        printf("------------------------------\n");
        struct timespec t = {3, 0};
        nanosleep(&t, NULL);
    }
    
    // Sprzątanie po wyjściu z pętli
    pthread_mutex_destroy(mutex);
    munmap(shm_ptr, SHM_SIZE);
    shm_unlink(file_name);

    printf("\nServer cleaned up and exited.\n");
    return EXIT_SUCCESS;
}