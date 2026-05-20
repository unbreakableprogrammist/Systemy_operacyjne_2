#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <bits/socket.h>
#include <netinet/in.h>
#include <sys/socket.h>


#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)             \
(__extension__({                               \
long int __result;                         \
do                                         \
__result = (long int)(expression);     \
while (__result == -1L && errno == EINTR); \
__result;                                  \
}))
#endif

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

int sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;
    if (-1 == sigaction(sigNo, &act, NULL))
        return -1;
    return 0;
}

int make_udp_socket() {
    int socketfd;
    // Tworzymy gniazdo: AF_INET (IPv4), SOCK_DGRAM (datagramy UDP), 0 (domyślny protokół).
    if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        ERR("socket"); // Jeśli socket() zwróci liczbę ujemną, przerywamy program z błędem.

    return socketfd; // Zwracamy "numer" (deskryptor) naszego nowego gniazda.
}

int bind_udp_socket(int port) {
    struct sockaddr_in address; // Deklarujemy strukturę, która przechowa adres IPv4 i port.
    int socketfd = make_udp_socket(); // Wywołujemy naszą funkcję wyżej, by stworzyć gniazdo.
    int t = 1; // Zmienna pomocnicza (wartość 1 = "prawda") potrzebna dla opcji SO_REUSEADDR.

    // Zerujemy pamięć struktury, żeby uniknąć tzw. "śmieci" w pamięci.
    memset(&address, 0, sizeof(struct sockaddr_in));

    // Konfigurujemy strukturę adresową:
    address.sin_family = AF_INET; // Ustawiamy rodzinę adresów na IPv4.
    address.sin_port = htons(port); // Konwertujemy port z formatu procesora na format sieciowy i przypisujemy.
    address.sin_addr.s_addr = htonl(INADDR_ANY); // Konwertujemy i ustawiamy nasłuchiwanie na wszystkich adresach IP maszyny.

    // Ustawiamy opcję SO_REUSEADDR. Pozwala to na ponowne, natychmiastowe użycie tego samego portu,
    // gdyby serwer uległ awarii i musiał zostać zrestartowany (inaczej system zablokowałby port na dłuższą chwilę).
    if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t)) < 0)
        ERR("setsockopt");

    // "Przywiązujemy" (bindowanie) nasze gniazdo do skonfigurowanego wyżej adresu i portu.
    // Rzutujemy wskaźnik &address na ogólny typ (struct sockaddr *), bo tego wymaga ta funkcja.
    if (bind(socketfd, (struct sockaddr *)&address, sizeof(address)) < 0)
        ERR("bind");

    return socketfd; // Zwracamy gotowe, zbindowane gniazdo serwera.
}