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
/*
pomysl na zadanie 
najpierw tworzymy rure, zamykamy nie uzywane 
SIGPIPE ignorujemy 
*/
volatile sig_atomic_t running = 1;

void sigint_handler(int sig){
    running = 0;
}

void children_work(int id,int read_pipe,int write_pipe){
    unsigned int seed = time(NULL) ^ id;
    char buf[32];
    while(running){
        int status = read(read_pipe,buf,sizeof(buf)-1);
        if(status<0) break;
        if(status == 0) break;
        buf[status] = '\0';
        int number = atoi(buf);
        printf("Dziecko %d (PID: %d) odebrało: %d\n", id, getpid(), number);
        if(number == 0) break;
        int random = rand_r(&seed) % 21 - 10;
        number+=random;
        int wpisane = snprintf(buf,sizeof(buf),"%d",number);
        if(write(write_pipe,buf,wpisane)<0) break;
        seed++;
    }
    if(close(read_pipe)<0) ERR("close");
    if(close(write_pipe)<0) ERR("close");

}

int main(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    if(sigaction(SIGINT, &sa, NULL)<0) ERR("sigaction");

    signal(SIGPIPE, SIG_IGN); // ignorujemy SIGPIPE bo bedziemy sprawdzac po prostu w read ( 0) i write ( -1) 
    int P1[2],P2[2],B[2]; // p1 do pierwszego dziecka , p2 do drugiego , b - pomiedzy dziecmi
    // R -> P1[1] ------ P1[0] -> D1 -> B[1] ----- B[0]-> D2 -> P2[1] ------- P2-> R 
    if(pipe(P1)< 0 ) ERR("pipe");
    if(pipe(P2)< 0 ) ERR("pipe");
    if(pipe(B)< 0 ) ERR("pipe");
    
    pid_t dzieci[2];
    dzieci[0] = fork();
    if(dzieci[0]==0){ // tworzymy pierwsze dziecko 
        if(close(P1[1])<0) ERR("close");
        if(close(P2[0])<0) ERR("close");
        if(close(P2[1])<0) ERR("close");
        if(close(B[0])<0) ERR("close");
        children_work(0,P1[0],B[1]);
        exit(EXIT_SUCCESS);
    }
    dzieci[1] = fork();
    if(dzieci[1]==0){ 
        if(close(P1[1])<0) ERR("close");
        if(close(P1[0])<0) ERR("close");
        if(close(P2[0])<0) ERR("close");
        if(close(B[1])<0) ERR("close");
        children_work(1,B[0],P2[1]);
        exit(EXIT_SUCCESS);
    }
    // teraz jestemy w rodzicu 
    if(close(P1[0])<0) ERR("close");
    if(close(P2[1])<0) ERR("close");
    if(close(B[1])<0) ERR("close");
    if(close(B[0])<0) ERR("close");
   
    write(P1[1],"1",1); // wpisujemy do rury 
    char message[32];
    srand(time(NULL));
    while(running){
        int status = read(P2[0],message,sizeof(message)-1);
        if(status<0) break;
        if(status == 0) break;
        message[status] = '\0';
        int number = atoi(message);
        printf("Rodzic PID: %d odebrał: %d\n", getpid(), number);
        if(number == 0) break;
        int random = rand()% 21 - 10;
        number += random;
        int dlugosc_nowego_tekstu = snprintf(message, sizeof(message), "%d",number);
        if(write(P1[1],message,dlugosc_nowego_tekstu)<0) break; 
    }

    if(close(P1[1])<0) ERR("close");
    if(close(P2[0])<0) ERR("close");
    for(int i=0;i<2;i++){
        wait(NULL);
    }
    return EXIT_SUCCESS;
}