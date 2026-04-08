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

void children_work(int id,int* shelves,pthread_mutex_t* mutexes,int n){
    printf("%d worker starts shift\n",getpid());
    srand(time(NULL) ^ (getpid()));
    for(int i=0;i<10;i++){
        int shelf1 = rand() % n;
        int shelf2;
        do{
            shelf2 = rand() % n;
        }while(shelf1 == shelf2);

        int low = shelf1 < shelf2 ? shelf1 : shelf2; // mniejszy indeks
        int high = shelf1 > shelf2 ? shelf1 : shelf2;
        
        int error1 = pthread_mutex_lock(&mutexes[low]);
        if(error1 == EOWNERDEAD) pthread_mutex_consistent(&mutexes[low]);
        else if(error1 != 0) ERR("pthread_mutex_lock");
        int error2 = pthread_mutex_lock(&mutexes[high]); 
        if(error2 == EOWNERDEAD) pthread_mutex_consistent(&mutexes[high]);
        else if(error2 != 0) ERR("pthread_mutex_lock");

        if(shelves[low] > shelves[high]){
            int pom = shelves[low];
            shelves[low] = shelves[high];
            shelves[high] = pom;
        }
        pthread_mutex_unlock(&mutexes[high]);
        pthread_mutex_unlock(&mutexes[low]);
   
    }
}

int main(int argc, char** argv) {     
    if(argc != 3) usage(argv[0]);
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if(n < 8 || n > 256) usage(argv[0]);
    if(m < 1 || m > 64) usage(argv[0]);

    // create file 
    int fd = open(SHOP_FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if(fd == -1) ERR("open");
    if(ftruncate(fd,n*sizeof(int)) == -1) ERR("ftruncate");

    int* shm_ptr = (int*)mmap(NULL,n*sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(shm_ptr == MAP_FAILED) ERR("mmap");

    pthread_mutex_t* mutex_ptr = (pthread_mutex_t*)mmap(NULL,n*sizeof(pthread_mutex_t),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(mutex_ptr == MAP_FAILED) ERR("mmap");
    pthread_mutexattr_t mutex_attr;
    if(pthread_mutexattr_init(&mutex_attr)) ERR("pthread_mutexattr_init");
    if(pthread_mutexattr_setpshared(&mutex_attr,PTHREAD_PROCESS_SHARED)) ERR("pthread_mutexattr_setpshared");
    if(pthread_mutexattr_setrobust(&mutex_attr,PTHREAD_MUTEX_ROBUST)) ERR("pthread_mutexattr_setrobust");
    for(int i=0;i<n;i++){
        if(pthread_mutex_init(&mutex_ptr[i],&mutex_attr)) ERR("pthread mutex init ");
    }
    pthread_mutexattr_destroy(&mutex_attr);


    for(int i = 0; i < n; ++i) {
        shm_ptr[i] = i+1;
    }
    shuffle(shm_ptr, n);
    print_array(shm_ptr, n);

    for(int i = 0;i<m;i++){
        pid_t pid = fork();
        if(pid == -1) ERR("fork)");
        if(pid == 0){
            children_work(i,shm_ptr,mutex_ptr,n);
            exit(EXIT_SUCCESS);
        }
    }
}
