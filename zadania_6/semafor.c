#include <fcntl.h>      // Flagi O_CREAT, O_EXCL
#include <sys/stat.h>   // Prawa dostępu np. 0666
#include <semaphore.h>  // Funkcje semaforów
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>      // Obsługa błędów (errno, EEXIST)

#define SEM_NAME "/moj_bezpieczny_semafor"

int main() {
    sem_t *sem;
    int is_creator = 0; // Flaga, żeby wiedzieć, czy to my stworzyliśmy semafor

    // 1. Próba atomowego utworzenia nowego semafora
    // O_CREAT | O_EXCL - utwórz, ale zgłoś błąd, jeśli już istnieje!
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);

    if (sem == SEM_FAILED) {
        // Jeśli się nie udało, sprawdzamy DLACZEGO
        if (errno == EEXIST) {
            printf("Semafor już istnieje. Podłączam się do niego...\n");
            
            // 2. Otwieramy istniejący semafor (flaga 0 - po prostu otwórz)
            sem = sem_open(SEM_NAME, 0);
            if (sem == SEM_FAILED) {
                perror("Błąd podłączania do istniejącego semafora");
                exit(EXIT_FAILURE);
            }
        } else {
            // Jakiś inny błąd (np. brak uprawnień)
            perror("Błąd tworzenia semafora");
            exit(EXIT_FAILURE);
        }
    } else {
        printf("Utworzono nowy semafor!\n");
        is_creator = 1; // Zaznaczamy, że jesteśmy twórcą
    }

    // --- TUTAJ MOŻESZ KORZYSTAĆ Z SEMAFORA ---
    printf("Czekam na semafor...\n");
    sem_wait(sem);
    
    printf("W sekcji krytycznej...\n");
    // (Symulacja pracy)
    
    sem_post(sem);
    printf("Zwolniono semafor.\n");
    // -----------------------------------------

    // 3. Sprzątanie
    // Każdy proces, który używał semafora, musi go zamknąć
    sem_close(sem);

    // UWAGA: Tylko JEDEN proces (najlepiej ten, który go stworzył lub główny)
    // powinien usunąć semafor z systemu, inaczej zniknie dla innych!
    if (is_creator) {
        // Zdejmij komentarz, jeśli chcesz, żeby semafor zniknął po zakończeniu:
        // sem_unlink(SEM_NAME); 
        // printf("Semafor usunięty z systemu.\n");
    }

    return EXIT_SUCCESS;
}