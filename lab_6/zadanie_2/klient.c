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
    fprintf(stderr, "USAGE: %s server_pid\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc,char** argv){
    if(argc != 2) usage("number of arguments");

    int server_pid = atoi(argv[1]);
    if (server_pid == 0)
        usage(argv[0]);
    srand(getpid());

    char file_name[30];
    sprintf(file_name,"/%d-board",server_pid);

    int shm = shm_open(file_name,O_RDWR,0666);

    if(shm == -1) ERR("shm open");

    char* shm_ptr = (char*)mmap(NULL,SHM_SIZE,PROT_READ | PROT_WRITE,MAP_SHARED,shm,0);
    if(shm_ptr == MAP_FAILED) ERR("mmap");

    pthread_mutex_t* mtx = (pthread_mutex_t*)shm_ptr;
    int N = *(int*)(shm_ptr+sizeof(pthread_mutex_t));
    char* board = shm_ptr + sizeof(pthread_mutex_t) + sizeof(int);

    int points = 0;
    while(1){
        int error;
        if((error = pthread_mutex_lock(mtx))!=0){
            if(error == EOWNERDEAD) pthread_mutex_consistent(mtx);
            else ERR("mutex");
        }
        int random = 1 + rand()%10;
        if(random == 1){
            printf("OOPS Finisz \n");
            exit(EXIT_SUCCESS);
        }
        int x = rand()%N;
        int y = rand()%N;
        if(board[x*N+y] != 0){
            printf("found %d points\n",(int)board[x*N+y]);
            points+=(int)board[x*N+y];
            board[x*N+y] = 0;
        }else{
            printf("GAME OVER, SCORE: %d\n", points);
            pthread_mutex_unlock(mtx);
            exit(EXIT_SUCCESS);
        }
        pthread_mutex_unlock(mtx);
    }
}