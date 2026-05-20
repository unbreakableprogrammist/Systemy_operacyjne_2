#include "biblioteka.h"
#define MAX_BUF 576

// Zmienna globalna do obsługi sygnału alarmu.
// Typ volatile sig_atomic_t gwarantuje, że modyfikacja tej zmiennej
// przez procedurę obsługi sygnału (w tle) nie zostanie zepsuta przez optymalizator kompilatora.
volatile sig_atomic_t last_signal = 0;

void sigalrm_handler(int sig) {
    last_signal = sig;
}

void client_work(int client_socket, struct sockaddr_in server_addr, int file_fd) {
    char send_buf[MAX_BUF];
    char recv_buf[MAX_BUF];

    // Zostawiamy 8 bajtów (2 * 32 bity) na metadane: numer paczki i flagę końca.
    int offset = 2 * sizeof(int32_t);
    int32_t chunkNo = 0; // Licznik wysłanych pakietów
    ssize_t read_bytes;  // Ilość przeczytanych bajtów z pliku
    int counter;         // Licznik retransmisji (maks 5)

    do {
        // Czyścimy bufory na początku każdego obiegu, aby uniknąć wysłania "śmieci" z poprzednich paczek
        memset(send_buf, 0, MAX_BUF);
        memset(recv_buf, 0, MAX_BUF);

        // Czytamy z pliku z przesunięciem (offset), chroniąc początek bufora na nasze flagi
        read_bytes = TEMP_FAILURE_RETRY(read(file_fd, send_buf + offset, MAX_BUF - offset));
        if (read_bytes < 0) ERR("read");

        chunkNo++;

        // Zapisujemy numer paczki w pierwszych 4 bajtach bufora.
        // Konwersja htonl jest niezbędna, aby serwer (niezależnie od swojej architektury) odczytał to poprawnie.
        *((int32_t*)send_buf) = htonl(chunkNo);

        // Sprawdzamy, czy dobrnęliśmy do końca pliku
        if (read_bytes < MAX_BUF - offset) {
            // Arytmetyka wskaźników: + 1 przesuwa wskaźnik o całe 4 bajty w prawo.
            // Zapisujemy tu flagę wartości 1, co dla serwera oznacza "To już ostatni fragment".
            *((int32_t*)send_buf + 1) = htonl(1);
        } else {
            *((int32_t*)send_buf + 1) = htonl(0);
        }

        counter = 0;

        // Pętla retransmisji (wysyłanie i oczekiwanie na potwierdzenie)
        do {
            counter++;
            socklen_t len = sizeof(server_addr);

            // Rzutujemy strukturę na (struct sockaddr*), bo tego wymaga definicja funkcji z POSIX.
            if (TEMP_FAILURE_RETRY(sendto(client_socket, send_buf, MAX_BUF, 0, (struct sockaddr*)&server_addr, len)) < 0) {
                ERR("sendto");
            }

            // Ustawiamy sprzętowy budzik na 0,5 sekundy (500 000 mikrosekund).
            struct itimerval ts;
            memset(&ts, 0, sizeof(struct itimerval));
            ts.it_value.tv_usec = 500000;
            setitimer(ITIMER_REAL, &ts, NULL);

            last_signal = 0; // Czysta karta przed wejściem w nasłuch

            // Nasłuchujemy potwierdzenia (ACK) od serwera.
            // Celowo nie używamy TEMP_FAILURE_RETRY, ponieważ chcemy, aby sygnał SIGALRM przerwał tę funkcję.
            while (recv(client_socket, recv_buf, MAX_BUF, 0) < 0) {
                if (errno != EINTR) ERR("recv");          // Każdy błąd inny niż przerwanie systemowe to krytyczna awaria
                if (last_signal == SIGALRM) break;        // Budzik zadzwonił - opuszczamy pętlę nasłuchującą
            }

            // Weryfikujemy, czy odebraliśmy odpowiedź i czy zgadza się numer potwierdzanej paczki.
            // Pętla powtarza się maksymalnie 5 razy.
        } while (*((int32_t*)recv_buf) != (int32_t)htonl(chunkNo) && counter <= 5);

        // Jeśli wyszliśmy z pętli powyżej z powodu przekroczenia limitu prób (counter > 5),
        // przerywamy główną pętlę while, uciekając z funkcji. Serwer prawdopodobnie padł.
        if (*((int32_t*)recv_buf) != (int32_t)htonl(chunkNo) && counter > 5) {
            fprintf(stderr, "Brak odpowiedzi od serwera po 5 próbach. Przerywam.\n");
            break;
        }

    } while (read_bytes == MAX_BUF - offset); // Kręcimy się, dopóki z pliku czytamy pełne pakiety
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr, "USAGE: %s IP PORT PLIK\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Wyłączamy domyślne zabijanie procesu przy przerwanych łączykach (SIGPIPE)
    // oraz podpinamy naszą własną funkcję obsługi pod sygnał budzika (SIGALRM).
    if (sethandler(SIG_IGN, SIGPIPE)) ERR("Setting SIGPIPE:");
    if (sethandler(sigalrm_handler, SIGALRM)) ERR("Setting SIGALRM:");

    int file_fd = TEMP_FAILURE_RETRY(open(argv[3], O_RDONLY));
    if (file_fd < 0) {
        ERR("Unable to open temp file");
    }

    // Inicjalizacja struktury docelowej dla serwera
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    // Konwersja portu i IP do formatu sieciowego (Network Byte Order)
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        ERR("inet_pton");
    }

    // Tworzenie gniazda UDP klienta (klient nie musi robić bind(), system sam przypisze mu port)
    int client_socket = make_udp_socket();

    client_work(client_socket, serv_addr, file_fd);

    // Sprzątanie po zakończeniu komunikacji
    if (TEMP_FAILURE_RETRY(close(file_fd)) < 0) ERR("close");
    if (TEMP_FAILURE_RETRY(close(client_socket)) < 0) ERR("close");

    return EXIT_SUCCESS;
}