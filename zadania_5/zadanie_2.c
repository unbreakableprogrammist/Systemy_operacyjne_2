#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include<string.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *name) {
    fprintf(stderr, "USAGE: %s n\n", name);
    exit(EXIT_FAILURE);
}

void player(int id,int read_pipe,int write_pipe,int M){
    char komunikat[16];
    char newround[16] = "new_round"; // jesli to bedzie w komunikacie to bedzie oznaczalo losuj liczbe
    char end[16] = "end_game"; // jesli to bedzie w komunikacie bedzie oznaczalo koniec gry
    
    while(1){
        memset(komunikat, 0, sizeof(komunikat)); // Wypełnia całą tablicę zerami
        int status = read(read_pipe,komunikat,sizeof(komunikat));
        if(status < 0){
            if(errno == EINTR) continue;
            else ERR("read");
        }
        if(status == 0) break; // nasz odbiorca przestal istniec 
        int points;
        if(komunikat[0] == 'l'){ // jesli pierwsza literka bedzie l to bedzie nastepnie liczba
            points = atoi(komunikat+1);
        }else if(strcmp(komunikat,newround)==0){
            
        }
    }

    if(close(read_pipe)<0) ERR("close pipe");
    if(close(write_pipe)<0) ERR("close pipe");
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
    for(int i=0;i<n;i++){
        children[i] = fork();
        if(children[i] == -1) ERR("fork");
        if(children[i] == 0){
            player(i,PS[i][0],SP[i][1],m);
            exit(EXIT_SUCCESS);
        }
        // rodzic :
        if(close(PS[i][0])==-1) ERR("close"); // zamykamy czytanie od serwera
        if(close(SP[i][1])==-1) ERR("close"); // zamykamy pisanie od dziecka
    }
    // rundy
    int winners = 0;
    int winners_id[n];
    int points[n];
    char komunikat[16] = "new_round";
    for(int r=0;r<m;r++){

    }

}