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
    fprintf(stderr, "\t  n - number of items (shelves)\n");
    fprintf(stderr, "\t  m - number of workers\n");
    exit(EXIT_FAILURE);
}
void zlicz(int ile,char* sh_file,int* count){
    for(int i=0;i<ile;i++){
        unsigned char c = sh_file[i];
        count[c]++;
    }
}

void children_work()
int main(int argc,char** argv){
    if(argc!=3){
        usage(argv[0]);
    }
    int m = atoi(argv[2]);
    
    for(int i=0;i<n;i++){
        printf("%c",sh_file[i]);
    }
    int count[300] = {0};
    zlicz(n,sh_file,count);
    for(int i=0;i<300;i++){
        if(count[i]>0){
            printf("Znak %c występuje %d razy\n",i,count[i]);
        }
    }
    // sprzatanie 
    if(munmap(sh_file,n) == -1) ERR("munmap");
    if(close(fd) == -1) ERR("close");
}