#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define MONTE_CARLO_ITERS 100000
#define LOG_LEN 8

#define ERR(source) \
    (fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), perror(source), kill(0, SIGKILL), exit(EXIT_FAILURE))

void usage(char* name)
{
    fprintf(stderr, "USAGE: %s n\n", name);
    fprintf(stderr, "10000 >= n > 0 - number of children\n");
    exit(EXIT_FAILURE);
}
// myslimy o tym jak o prawej gornej cwiartce wtedy pole tej cwiartki to pi/4 a pole prawej gornej cwiartki kwadratu to 1 wiec stosunek zachowany 
void children_work(int i,float* pi, char* log_ptr) {
    srand(getpid()); // ustawiamy ziarno generatora pseudolosowego na PID procesu potomnego
    int inside_circle = 0;
    for(int j=0;j<MONTE_CARLO_ITERS;j++) {
        double x = (double)rand()/RAND_MAX; // rand zwraca liczbe z przedzialu [0,RAND_MAX], wiec dzielimy przez RAND_MAX zeby otrzymac liczbe z przedzialu [0,1]
        double y = (double)rand()/RAND_MAX;
        if(x*x+y*y<=1) inside_circle++; // punkt (x,y) jest wewnatrz kola o promieniu 1, jesli x^2+y^2<=1
    }
    pi[i] = ((float)inside_circle/MONTE_CARLO_ITERS)*4.0f;
    // printf("%f\n",pi[i]);
    // teraz zapisujemy do log.txt 
    char buf[LOG_LEN+1];
    snprintf(buf,LOG_LEN + 1,"%7.5f\n",pi[i]);
    memcpy(log_ptr+i*LOG_LEN,buf,LOG_LEN);
}

int main(int argc, char ** argv) {
    if(argc!=2) usage("invalid number of arguments");

    int N = atoi(argv[1]);
    if(N<=0 && N>=30) usage("N");

    // otwieramy plik log.txt do zapisu
    int log_fd = open("log.txt",O_CREAT | O_RDWR | O_TRUNC, 0644);
    if(log_fd==-1) ERR("open");
    if(ftruncate(log_fd,LOG_LEN*8)) ERR("ftruncate"); // rozmiar pliku log.txt to 8*LOG_LEN bajtów

    char* log_ptr = (char*)mmap(NULL, LOG_LEN*8, PROT_READ | PROT_WRITE, MAP_SHARED, log_fd, 0);
    if(log_ptr==MAP_FAILED) ERR("mmap");
    if(close(log_fd)) ERR("close"); // zamykamy deskryptor pliku, bo już nie będzie potrzebny

    float* pi = (float*)mmap(NULL,N*sizeof(float),PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(pi==MAP_FAILED) ERR("mmap");

    for(int i=0;i<N;i++) {
        int pid = fork();
        if(pid==-1) ERR("fork");
        if(pid==0) { // proces potomny
            children_work(i, pi, log_ptr);
            exit(EXIT_SUCCESS);
        }
    }
    while(wait(NULL)>0){} // czekamy az dzieci sie zamkna 
    // tutaj dzieci cos zapisaly juz swoje dane do pi wiec trzeba wyciagnac srednia 
    double sum = 0.0;
    for(int i=0;i<N;i++){
        sum += pi[i];
    }
    sum /= N;

    if(munmap(pi,N*sizeof(float))) ERR("munmap");
    if(msync(log_ptr,LOG_LEN*8,MS_SYNC)) ERR("msync"); // synchronizujemy dane z pamieci z plikiem log.txt
    if(munmap(log_ptr,LOG_LEN*8)) ERR("munmap");

    printf("Average pi: %f\n",sum);
    
}
