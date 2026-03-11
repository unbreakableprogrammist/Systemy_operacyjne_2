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
    srand(time(NULL) + id); // zeby kazdy player mial inny seed
    char komunikat[16];
    char newround[16] = "new_round"; // jesli to bedzie w komunikacie to bedzie oznaczalo losuj liczbe
    char end[16] = "end_game"; // jesli to bedzie w komunikacie bedzie oznaczalo koniec gry
    int odwiedzone_karty[M];
    memset(odwiedzone_karty, 0, sizeof(odwiedzone_karty)); // wypełniamy tablice zerami
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
            printf("Player %d: %d points\n",id,points);
        }else if(strcmp(komunikat,newround)==0){
            int card = rand() % M; // losujemy karte
            while(odwiedzone_karty[card] == 1){ // sprawdzamy czy juz nie wylosowalismy tej karty
                card = rand() % M;
            }
            odwiedzone_karty[card] = 1; // oznaczamy karte
            char card_str[16] = {0}; // Magia C: cała tablica od razu wypełniona zerami '\0'
            snprintf(card_str, sizeof(card_str), "%d", card+1); 
            if(write(write_pipe,card_str,sizeof(card_str))==-1) ERR("write");
        }else if(strcmp(komunikat,end)==0){
            break; // koniec gry
        }
    }

    if(close(read_pipe)<0) ERR("close pipe");
    if(close(write_pipe)<0) ERR("close pipe");
}

int main(int argc,char** argv){
    if(argc != 3) usage(argv[0]);
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if(n < 2 || n > 5 || m < 5 || m > 10) usage(argv[0]); 

    pid_t children[n];
    int SP[n][2]; // Serwer -> player
    int PS[n][2]; // player -> Serwer
    
    // Tworzenie wszystkich rur
    for(int i=0;i<n;i++){
        if(pipe(PS[i])==-1) ERR("pipe");
        if(pipe(SP[i])==-1) ERR("pipe");
    }
    
    // Tworzenie procesów
    for(int i=0;i<n;i++){
        children[i] = fork();
        if(children[i] == -1) ERR("fork");
        if(children[i] == 0){ // DZIECKO
            // Zamykamy rury innych graczy i nieużywane końcówki
            for(int j=0; j<n; j++) {
                if (j == i) {
                    close(SP[j][1]); // nie pisze do Serwera przez tę rurę
                    close(PS[j][0]); // nie czyta od siebie
                } else {
                    close(SP[j][0]); close(SP[j][1]);
                    close(PS[j][0]); close(PS[j][1]);
                }
            }
            player(i, SP[i][0], PS[i][1], m);
            exit(EXIT_SUCCESS);
        }
        
        // RODZIC
        if(close(SP[i][0])==-1) ERR("close"); 
        if(close(PS[i][1])==-1) ERR("close"); 
    }
    
    // Tablica punktów CAŁEJ gry
    int sum_points[n];
    memset(sum_points, 0, sizeof(sum_points)); // wypełniamy tablice zerami

    char komunikat[16] = "new_round";
    char end[16] = "end_game";
    char score[16];
    
    // RUNDY
    for(int r=0;r<m;r++){
        printf("\nNEW ROUND\n"); 
        
        for(int i = 0; i < n; i++){
            if(write(SP[i][1],komunikat,sizeof(komunikat))==-1) ERR("write");
        }
        
        int max_points = -1;
        int rzucone_karty[n]; // Zapamiętujemy, kto rzucił jaką kartę
        
        // 1. Odbieramy karty
        for(int i=0;i<n;i++){
            memset(score, 0, sizeof(score)); 
            int status = read(PS[i][0],score,sizeof(score));
            if(status < 0){
                if(errno == EINTR) continue;
                else ERR("read");
            }
            if(status == 0) break; 
            int karta = atoi(score);
            rzucone_karty[i] = karta;
            printf("Got number %d from player %d\n", karta, i);
            
            if(karta > max_points){
                max_points = karta;
            }
        }
        int winners = 0;
        for(int i=0; i<n; i++) {
            if(rzucone_karty[i] == max_points) winners++;
        }
        
        // 3. Rozdajemy punkty WSZYSTKIM graczom
        int points_to_send = n / winners; 
        for(int i=0; i<n; i++){
            int zdobyto_w_tej_rundzie = 0;
            if(rzucone_karty[i] == max_points) {
                zdobyto_w_tej_rundzie = points_to_send;
                sum_points[i] += zdobyto_w_tej_rundzie; // sumujemy globalny wynik
            }
            snprintf(score, sizeof(score), "l%d", zdobyto_w_tej_rundzie);
            if(write(SP[i][1],score,sizeof(score))==-1) ERR("write"); 
        }
    }
    
    printf("\n--- TABELA WYNIKOW ---\n");
    for(int i=0; i<n; i++) {
        printf("Gracz %d: %d pkt\n", i, sum_points[i]);
    }
    
    // koniec gry
    for(int i=0;i<n;i++){
        if(write(SP[i][1],end,sizeof(end))==-1) ERR("write"); 
        close(SP[i][1]);
        close(PS[i][0]);
    }
    
    for(int i=0;i<n;i++){
        if(wait(NULL)==-1) ERR("wait");                
    }
    return EXIT_SUCCESS;
}