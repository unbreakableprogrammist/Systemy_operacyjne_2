#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>
#include <unistd.h>

#define SEM_NAME "/moj_bezpieczny_semafor"
#define SHM_NAME "/moje_shared_memory"
#define NUM_PROCS 2 // Ilu procesów oczekuje bariera

// Struktura, która wyląduje w pamięci współdzielonej
typedef struct {
    pthread_barrier_t barrier;
    pthread_mutex_t mutex;     // Mutex jest absolutnie wymagany do CondVar!
    pthread_cond_t cond;
    int work_done;             // Flaga dla CondVar
} shared_data_t;

int main() {
    sem_t *sem;
    int is_creator = 0;

    // 1. Próba atomowego utworzenia semafora (Wartość 0 = zamknięty na start!)
    sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0);

    if (sem == SEM_FAILED) {
        if (errno == EEXIST) {
            printf("[P2] Podłączam się do istniejącego systemu...\n");
            sem = sem_open(SEM_NAME, 0);
            if (sem == SEM_FAILED) { perror("sem_open"); exit(EXIT_FAILURE); }
        } else {
            perror("Błąd tworzenia semafora"); exit(EXIT_FAILURE);
        }
    } else {
        printf("[P1] Utworzono system synchronizacji (Jestem Twórcą)!\n");
        is_creator = 1;
    }

    // 2. Obsługa Pamięci Współdzielonej
    int shm_fd;
    if (is_creator) {
        shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, 0666);
        ftruncate(shm_fd, sizeof(shared_data_t));
    } else {
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    }
    
    shared_data_t *sd = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    close(shm_fd); // Deskryptor niepotrzebny po mmap

    // 3. Inicjalizacja PTHREAD (TYLKO TWÓRCA)
    if (is_creator) {
        pthread_barrierattr_t b_attr;
        pthread_mutexattr_t m_attr;
        pthread_condattr_t c_attr;

        // Inicjalizacja atrybutów
        pthread_barrierattr_init(&b_attr);
        pthread_mutexattr_init(&m_attr);
        pthread_condattr_init(&c_attr);

        // KLUCZOWE: Ustawienie PTHREAD_PROCESS_SHARED
        pthread_barrierattr_setpshared(&b_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutexattr_setpshared(&m_attr, PTHREAD_PROCESS_SHARED);
        pthread_condattr_setpshared(&c_attr, PTHREAD_PROCESS_SHARED);

        // Inicjalizacja obiektów w pamięci współdzielonej
        pthread_barrier_init(&sd->barrier, &b_attr, NUM_PROCS);
        pthread_mutex_init(&sd->mutex, &m_attr);
        pthread_cond_init(&sd->cond, &c_attr);
        sd->work_done = 0;

        // Niszczenie atrybutów
        pthread_barrierattr_destroy(&b_attr);
        pthread_mutexattr_destroy(&m_attr);
        pthread_condattr_destroy(&c_attr);

        // Otwieramy semafor, informując inne procesy: "Inicjalizacja zakończona!"
        sem_post(sem); 
    } else {
        // Inne procesy czekają, aż twórca skończy inicjalizację structa
        printf("[P2] Czekam na zakończenie inicjalizacji przez Twórcę...\n");
        sem_wait(sem); 
        sem_post(sem); // Oddajemy klucz dalej, gdyby było więcej procesów
    }

    // ========================================================
    // LOGIKA SYNCHRONIZACJI: BARIERA I COND_VAR
    // ========================================================

    // A) BARIERA - Czekamy na spotkanie wszystkich procesów
    printf("[%d] Czekam na barierze...\n", getpid());
    int b_res = pthread_barrier_wait(&sd->barrier);
    if (b_res == PTHREAD_BARRIER_SERIAL_THREAD) {
        printf("[%d] Wszyscy dotarli! Rozpoczynamy...\n", getpid());
    }

    // B) CONDITIONAL VARIABLE - Model Producent/Konsument
    if (is_creator) {
        // TWÓRCA - Zrobi "pracę" i obudzi innych
        printf("[P1] Pracuję...\n");
        sleep(2); // Symulacja ciężkiej pracy

        pthread_mutex_lock(&sd->mutex);
        sd->work_done = 1;
        printf("[P1] Praca skończona. Wysyłam sygnał (Broadcast)!\n");
        pthread_cond_broadcast(&sd->cond);
        pthread_mutex_unlock(&sd->mutex);
    } else {
        // INNE PROCESY - Czekają na warunek
        pthread_mutex_lock(&sd->mutex);
        while (sd->work_done == 0) {
            printf("[P2] Czekam na sygnał od twórcy...\n");
            // Cond_wait na czas snu zwalnia mutex, a po przebudzeniu go znów zakłada
            pthread_cond_wait(&sd->cond, &sd->mutex);
        }
        printf("[P2] Otrzymałem sygnał! Warunek spełniony.\n");
        pthread_mutex_unlock(&sd->mutex);
    }

    // ========================================================
    // 4. Sprzątanie
    // ========================================================
    
    // Ponownie synchronizujemy na barierze przed sprzątaniem
    pthread_barrier_wait(&sd->barrier);

    if (is_creator) {
        // Tylko twórca niszczy obiekty
        pthread_barrier_destroy(&sd->barrier);
        pthread_mutex_destroy(&sd->mutex);
        pthread_cond_destroy(&sd->cond);
        
        // Odpinanie pamięci i semafora z systemu
        shm_unlink(SHM_NAME);
        sem_unlink(SEM_NAME);
        printf("[P1] Pamięć i semafor wyczyszczone z systemu.\n");
    }

    munmap(sd, sizeof(shared_data_t));
    sem_close(sem);

    return EXIT_SUCCESS;
}