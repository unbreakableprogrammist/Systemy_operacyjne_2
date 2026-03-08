#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *name) {
    fprintf(stderr, "USAGE: %s n\n", name);
    exit(EXIT_FAILURE);
}

// Funkcja dziecka - dostaje tylko SWOJĄ rurę P i wspólną R
void child_work(int i, int P[2], int R[2]) {

    // na koniec zamykamy koncowki 
    if(close(P[0]) < 0) ERR("close P[0] in child");
    if(close(R[1]) < 0) ERR("close R[1] in child");
    return EXIT_SUCCESS;
}

int main(int argc, char** argv) {
    if(argc != 2) usage(argv[0]);
    int n = atoi(argv[1]); 
    
    int P[n][2]; // Tablica rur dla każdego dziecka
    int R[2];    // Główna rura (od dzieci do rodzica)
    
    if(pipe(R) < 0) ERR("pipe R"); 
    
    for(int i = 0; i < n; i++) {
        // tworzymy rure
        if(pipe(P[i]) < 0) ERR("pipe P"); 
        
        // robimy dzieci 
        pid_t pid = fork();
        if(pid < 0) ERR("fork");
        
        if(pid == 0) { 
            // Dziecko zamyka to, czego nie używa w R (czytanie)
            if(close(R[0]) < 0) ERR("close R[0]"); 
            // Dziecko zamyka to, czego nie używa w SWOIM P (pisanie)
            if(close(P[i][1]) < 0) ERR("close P[i][1]"); 
            
            // Zamykamy rury BRACI (te, które rodzic otworzył we wcześniejszych obiegach pętli)
            for(int j = 0; j < i; j++) {
                if(close(P[j][1]) < 0) ERR("close old pipes");
            }
            child_work(i, P[i], R);
            
        }
        // rodzic od razu zamyka P do czytania 
        if(close(P[i][0]) < 0) ERR("close P[i][0] in parent");
    }
    
    // Rodzic zamyka końcówkę do pisania w R, bo będzie tylko czytał
    if(close(R[1]) < 0) ERR("close R[1] in parent");
    
    int running = 1;
    int seed = time(NULL);
    int random_char = rand_r(&seed) % 26 + 'a'; // losujemy znak do wysłania
    while(running) {
        // Rodzic losuje dziecko, do którego wyśle znak
        int child_index = rand_r(&seed) % n;
        
        // Rodzic wysyła znak do wylosowanego dziecka
        if(write(P[child_index][1], &random_char, 1) < 0) ERR("write to child");
        
        char response;
        if(read(R[0], &response, 1) < 0) ERR("read from child");   
    }
    
    // Czekamy na śmierć wszystkich dzieci (żeby nie było sierot)
    while(wait(NULL) > 0);
    
    // Sprzątanie na koniec
    if(close(R[0]) < 0) ERR("close R[0]");
    for(int i = 0; i < n; i++) {
        if(close(P[i][1]) < 0) ERR("close P[i][1]");
    }
    
    return EXIT_SUCCESS;
}