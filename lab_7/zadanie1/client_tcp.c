#include "biblioteka.h"

void usage(char *name) { 
    // Jeśli użytkownik wpisze komendę źle, to pokazujemy mu instrukcję.
    fprintf(stderr, "USAGE: %s domain port operand1 operand2 operation \n", name); 
}

void prepare_request(char **argv, int32_t data[5]){
    // atoi() zamienia tekst np. "5" na liczbę 5.
    // htonl() zamienia tę liczbę z formatu Twojego komputera na uniwersalny format sieciowy (Big-Endian).
    data[0] = htonl(atoi(argv[3])); // operand 1
    data[1] = htonl(atoi(argv[4])); // operand 2
    data[2] = htonl(0);             // wynik (na razie 0, serwer go nadpisze)
    
    // argv[5][0] bierze pierwszy znak ze stringa (np. '+'). 
    // Rzutujemy go na int32_t, żeby wymiary w tablicy się zgadzały, a potem pakujemy przez htonl().
    data[3] = htonl((int32_t)(argv[5][0])); 
    
    // Ustawiamy domyślny status (1) przed wysłaniem.
    data[4] = htonl(1);
}
void print_answer(int32_t data[5]){
    // Wyciągamy status (data[4]). Jeśli prawda (czyli 1 - sukces), wchodzimy w IF.
    if (ntohl(data[4])) 
        // Wypisujemy ładnie równanie. Zauważ, że KAŻDY element wyciągany z tablicy 
        // musi być najpierw "odtłumaczony" funkcją ntohl(). Operator rzutujemy z powrotem na char.
        printf("%d %c %d = %d\n", ntohl(data[0]), (char)ntohl(data[3]), ntohl(data[1]), ntohl(data[2]));
    else
        // Serwer wpisał w status 0 (np. przez dzielenie przez 0 lub nieznany operator)
        printf("Operation impossible\n");
}

int main(int argc, char** argv){
    int fd;
    int32_t data[5];
    
    if(argc != 6) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    // Ignorujemy zrywanie połączenia, żeby program nie zginął od sygnału
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("SIGPIPE");

    fd = connect_tcp_socket(argv[1], argv[2]); 
    
    prepare_request(argv, data); // pakujemy dane
    
    if(bulk_write(fd, (char*)data, sizeof(data)) < 0) 
        ERR("write");
        
    // Czekamy na pełne 20 bajtów odpowiedzi
    if(bulk_read(fd, (char*)data, sizeof(data)) < (int)sizeof(int32_t[5])) 
        ERR("read");
        
    print_answer(data); // wypluwamy ładnie na ekran
    
    if (TEMP_FAILURE_RETRY(close(fd)) < 0)
        ERR("close");
        
    return EXIT_SUCCESS;
}