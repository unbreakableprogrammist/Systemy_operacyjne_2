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

int main(int argc,char ** argv){
    if(argc != 2) usage(argc);
    int n = atoi(argv[1]);
    if(n<=0 || n>=30) usage(n);

    pid_t my_pid = getpid();
    srand(my_pid); 
    printf("MY PID IS : %d\n",my_pid);

    char file_name[40];
    sprintf(file_name,"/%d-board",my_pid);
    int fd = shm_open(file_name,O_CREAT | O_EXCL | O_RDWR,0666);
    if(fd < 0 ) ERR("shm open");

    if(ftruncate(fd,SHM_SIZE) == -1) ERR(ftruncate);

    char* shm_ptr = (char*)mmap(NULL,SHM_SIZE,PROT_READ | PROT_WRITE , MAP_SHARED , fd, 0);
    if(shm_ptr == MAP_FAILED) ERR(mmap);
 
    // zapisujemy sobie poczatki wskaznikow 
    pthread_mutex_t* mutex = (pthread_mutex_t*)shm_ptr;  
    char* N_shared = shm_ptr + sizeof(pthread_mutex_t);
    char* board_shared = N_shared + sizeof(int);

    pthread_mutexattr_t mutex_attr;
    if(pthread_mutexattr_init(&mutex_attr) != 0) ERR("mutex attr init");
    if(pthread_mutexattr_setpshared(&mutex_attr,PTHREAD_PROCESS_SHARED) != 0) ERR("mutex attr setpshared");
    if(pthread_mutexattr_setrobust(&mutex_attr,PTHREAD_MUTEX_ROBUST) != 0) ERR("mutex attr setrobust");
    if(pthread_mutex_init(mutex,&mutex_attr) != 0) ERR("mutex init");
    if(pthread_mutexattr_destroy(&mutex_attr) != 0) ERR("mutex attr destroy"); // atrybuty juz niepotrzebne

    *N_shared = n;
    for(int i=0;i<n;i++){
        for(int j=0;j<n;j++){
            board_shared[i*n+j] = 1 + rand() % 9; // losujemy liczby od 1 do 9
        }
    }

    struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = sigint_handler;
    if(sigaction(SIGINT,&sa,NULL) == -1) ERR("sigaction");

    while(running){
        int error;
        if(error = pthread_mutex_lock(mutex) != 0){
            if(error == EOWNERDEAD) pthread_mutex_consistent(mutex);
            else ERR("mutex lock");
        }
        for(int i=0;i<n;i++){
            for(int j=0;i<n;j++){
                printf("%d",board_shared[i*n+j]);
            }
            printf("\n");
        }
        pthread_mutex_unlock(mutex);
        struct timespec t = {3,0};
        while(nanosleep(&t,NULL) > 0){}
    }
    pthread_mutex_destroy(mutex);
    munmap(shm_ptr,1024);
    shm_unlink(file_name);

    return EXIT_SUCCESS;

}