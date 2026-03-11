#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *name) {
    fprintf(stderr, "USAGE: %s n\n", name);
    exit(EXIT_FAILURE);
}

int main(int argc,char** argv){
    if(argc != 3) usage(argv[0]);
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if(n <= 2 || m <= 5) usage(argv[0]);

    pid_t children[n];
    int SP[n][2]; // Serwer -> player
    int PS[n][2]; // player -> Serwer
    for(int i=0;i<n;i++){
        if(pipe(PS[i])==-1) ERR("pipe");
        if(pipe(SP[i])==-1) ERR("pipe");
    }
    
}