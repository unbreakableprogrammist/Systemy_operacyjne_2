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

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

// Funkcja dziecka
void child_work(int i, int P[2], int R[2]) {
    
    
    // 4. Na koniec zamykamy używane końcówki
    if(close(P[0]) < 0) ERR("close P[0] in child");
    if(close(R[1]) < 0) ERR("close R[1] in child");
    
    // Funkcja void, więc nic nie zwracamy
}

void parent_sigint(int sig){
    
}

int main(int argc, char** argv) {
    if(argc != 2) usage(argv[0]);
    int n = atoi(argv[1]); 
    
    int P[n][2]; // Tablica rur dla każdego dziecka
    int R[2];    // Główna rura (od dzieci do rodzica)
    
    if(pipe(R) < 0) ERR("pipe R"); 
    
    for(int i = 0; i < n; i++) {
        // Tworzymy rurę indywidualną dla dziecka
        if(pipe(P[i]) < 0) ERR("pipe P"); 
        
        // Robimy dzieci 
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        
        if(pid == 0) { 
            // --- KOD DZIECKA ---
            if(close(R[0]) < 0) ERR("close R[0]"); 
            if(close(P[i][1]) < 0) ERR("close P[i][1]"); 
            
            for(int j = 0; j < i; j++) {
                if(close(P[j][1]) < 0) ERR("close old pipes");
            }
            
            child_work(i, P[i], R);
            
            // KRYTYCZNE: Dziecko musi tu bezwzględnie zakończyć działanie!
            exit(EXIT_SUCCESS); 
        }
        
        // --- KOD RODZICA (wewnątrz pętli) ---
        // Rodzic od razu zamyka P do czytania, bo będzie do dziecka tylko pisał
        if(close(P[i][0]) < 0) ERR("close P[i][0] in parent");
    }

    // Rodzic zamyka końcówkę do pisania w R, bo będzie tylko czytał
    if(close(R[1]) < 0) ERR("close R[1] in parent");
    
    
    
    // Czekamy na śmierć wszystkich dzieci (sprzątanie po zombi)
    while(wait(NULL) > 0);
    
    // Sprzątanie po rodzicu na koniec
    if(close(R[0]) < 0) ERR("close R[0]");
    for(int i = 0; i < n; i++) {
        if(close(P[i][1]) < 0) ERR("close P[i][1]");
    }
    
    return EXIT_SUCCESS;
}