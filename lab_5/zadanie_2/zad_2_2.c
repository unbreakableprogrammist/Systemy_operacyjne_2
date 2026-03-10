#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h> // DODANE: dla memset
#include <signal.h> // DODANE: dla sigaction

#define MAX_BUFF 200

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

volatile sig_atomic_t sigint_received = 0; 

void usage(char *name) {
    fprintf(stderr, "USAGE: %s n\n", name);
    exit(EXIT_FAILURE);
}

int sethandler(void (*f)(int), int sigNo) {
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

void sigint_child(int sig){
    if(rand() % 100 < 20) exit(EXIT_SUCCESS); 
}

void sigint_parent(int sig){
    sigint_received = 1; 
}

void sigchild_handler(int sig){
    usleep(20);
    pid_t pid;
    while(1){
        pid = waitpid(0, NULL, WNOHANG); 
        if(pid == 0) return; 
        if(pid < 0){ 
            if(errno == ECHILD) return; 
            else ERR("waitpid"); 
        }
    }
}

void child_work(int i, int P[2], int R[2]) {
    char c, buf[MAX_BUFF + 1];
    unsigned char s;
    srand(getpid());
    
    if (sethandler(sigint_child, SIGINT))
        ERR("Setting SIGINT handler in child");
        
    while(1){
        if(TEMP_FAILURE_RETRY(read(P[0], &c, 1)) < 1)
            ERR("read from P in child");
            
        // POPRAWKA 1: Najpierw losujemy, potem wpisujemy do buf[0]
        s = 1 + rand() % MAX_BUFF; 
        buf[0] = s; 
        memset(buf + 1, c, s); 
        
        if(TEMP_FAILURE_RETRY(write(R[1], buf, s + 1)) < 0)
            ERR("write to R in child");
    }
    
    if(close(P[0]) < 0) ERR("close P[0] in child");
    if(close(R[1]) < 0) ERR("close R[1] in child");
}

int main(int argc, char** argv) {
    if(argc != 2) usage(argv[0]);
    int n = atoi(argv[1]); 
    
    int P[n][2]; 
    int R[2];    
    
    if(pipe(R) < 0) ERR("pipe R"); 
    
    for(int i = 0; i < n; i++) {
        if(pipe(P[i]) < 0) ERR("pipe P"); 
        
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        
        if(pid == 0) { 
            if(close(R[0]) < 0) ERR("close R[0]"); 
            if(close(P[i][1]) < 0) ERR("close P[i][1]"); 
            
            for(int j = 0; j < i; j++) {
                if(close(P[j][1]) < 0) ERR("close old pipes");
            }
            
            child_work(i, P[i], R);
            exit(EXIT_SUCCESS); 
        }
        if(close(P[i][0]) < 0) ERR("close P[i][0] in parent");
    }

    if(close(R[1]) < 0) ERR("close R[1] in parent");

    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE handler");
    if (sethandler(sigint_parent, SIGINT)) ERR("Setting SIGINT handler in parent");
    if (sethandler(sigchild_handler, SIGCHLD)) ERR("Setting SIGCHLD handler in parent");
    
    unsigned char ile;
    char wiadomosc[MAX_BUFF + 1];
    srand(time(NULL)); // Inicjalizacja randomizacji dla rodzica
    
    for(;;){
        if(sigint_received == 1){ 
            int wylosowane_dziecko = rand() % n;
            int proby = 0;
            while(P[wylosowane_dziecko][1] == -1 && proby < n){
                // POPRAWKA 3: Idziemy po kolei (+ 1)
                wylosowane_dziecko = (wylosowane_dziecko + 1) % n;
                proby++;
            }
            if(P[wylosowane_dziecko][1] != -1){
                char losowa_literka = 'a' + rand() % 26;
                if(TEMP_FAILURE_RETRY(write(P[wylosowane_dziecko][1], &losowa_literka, 1)) != 1){
                    close(P[wylosowane_dziecko][1]);
                    P[wylosowane_dziecko][1] = -1;
                }
            }
            sigint_received = 0;
        }
        
        int status = read(R[0], &ile, 1); 
        
        // POPRAWKA 2: Zamknięty blok if dla błędów
        if(status < 0){ 
            if(errno == EINTR) continue; 
            else ERR("read");
        } 
        
        if(status == 0) break; 
        
        int dlugosc = ile;
        if (TEMP_FAILURE_RETRY(read(R[0], wiadomosc, dlugosc)) < dlugosc) 
            ERR("read data from R");
            
        wiadomosc[dlugosc] = '\0'; 
        printf("\nOdebrano od dziecka: %s\n", wiadomosc);
    }

    // Sprzątanie po rodzicu na koniec
    printf("Koniec. Zamykam program.\n");
    if(close(R[0]) < 0) ERR("close R[0]");
    for(int i = 0; i < n; i++) {
        // POPRAWKA 4: Zamykamy tylko te, które nie mają wartości -1
        if(P[i][1] != -1) {
            if(close(P[i][1]) < 0) ERR("close P[i][1]");
        }
    }
    
    return EXIT_SUCCESS;
}