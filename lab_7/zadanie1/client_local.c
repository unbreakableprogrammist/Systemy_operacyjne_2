#include "biblioteka.h"

void usage(char *name) { 
    // Argumenty: nazwa_programu, ścieżka_gniazda, op1, op2, znak 
    // W sumie 5 argumentów!
    fprintf(stderr, "USAGE: %s sciezka_do_gniazda operand1 operand2 operation \n", name); 
    fprintf(stderr, "Przyklad: %s /tmp/kalkulator.sock 5 7 +\n", name);
}

// prepare_request_local będzie miało trochę inne indeksy argv niż TCP,
// bo nie podajemy portu, więc wszystko przesuwa się o jeden w lewo!
void prepare_request_local(char **argv, int32_t data[5]){
    data[0] = htonl(atoi(argv[2])); // operand 1 (teraz to argv[2])
    data[1] = htonl(atoi(argv[3])); // operand 2 (teraz to argv[3])
    data[2] = htonl(0);             // puste miejsce na wynik
    
    // argv[4][0] - pierwszy znak ze stringa (np. '+')
    data[3] = htonl((int32_t)(argv[4][0])); 
    data[4] = htonl(1);             // domyślny status (sukces)
}

void print_answer(int32_t data[5]){
    if (ntohl(data[4])) 
        printf("%d %c %d = %d\n", ntohl(data[0]), (char)ntohl(data[3]), ntohl(data[1]), ntohl(data[2]));
    else
        printf("Operation impossible\n");
}

int main(int argc, char** argv){
    int fd;
    int32_t data[5];
    
    // Jak ustaliliśmy w usage, potrzebujemy dokładnie 5 słów
    if(argc != 5) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("SIGPIPE");

    // Dzwonimy! Tym razem podajemy tylko ścieżkę do pliku (argv[1])
    fd = connect_local_socket(argv[1]); 
    
    // Pakujemy paczkę (korzystamy z naszej lokalnej wersji indeksowania)
    prepare_request_local(argv, data); 
    
    // Wrzucamy zapytanie do "rury"
    if(bulk_write(fd, (char*)data, sizeof(data)) < 0) 
        ERR("write");
        
    // Blokujemy program i czekamy aż serwer przetrawi dane i je odeśle
    if(bulk_read(fd, (char*)data, sizeof(data)) < (int)sizeof(int32_t[5])) 
        ERR("read");
        
    // Pokazujemy co policzył serwer
    print_answer(data);
    
    // Odkładamy słuchawkę
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
        
    return EXIT_SUCCESS;
}