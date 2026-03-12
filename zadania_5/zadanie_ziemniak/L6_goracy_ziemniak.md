 **L6: Gorący Ziemniak**.

---

### Plik wejściowy: `siec.txt`

*(Bez zmian – zawiera graf skierowany pokazujący, kto komu może rzucić ziemniaka)*

```text
4
Alfa 0 1 1 0
Bravo 0 0 1 1
Charlie 1 0 0 1
Delta 0 1 0 0

```

---

### Treść zadania: L6: Gorący Ziemniak (Wersja Ostateczna)

**Etap 1: Parsowanie (5 pkt)**
Główny proces otwiera plik `siec.txt`. Odczytuje liczbę `N`, a następnie wczytuje nazwy węzłów i macierz sąsiedztwa do pamięci.
Dla każdego węzła wypisuje komunikat:
`Node <Nazwa> ready. I have <liczba_krawedzi> outgoing connections.`

**Etap 2: Infrastruktura - Rury, FIFO i Sygnały (7 pkt) -**

1. Główny proces tworzy **JEDNĄ kolejkę FIFO** (np. `raport_fifo`). Posłuży ona do komunikacji: **Węzły $\rightarrow$ Główny Proces**.
2. Następnie tworzy tablicę `N` łączy nienazwanych (`pipe`). Każdy węzeł będzie miał przypisaną jedną rurę jako swoją "skrzynkę odbiorczą" na Ziemniaka (komunikacja: **Węzeł $\rightarrow$ Węzeł**).
3. Główny proces i procesy potomne ustawiają **obsługę sygnałów**. Dzieci mają reagować na sygnał `SIGUSR1`, który będzie rozkazem do bezpiecznego zakończenia pracy.
4. Rodzic tworzy `N` procesów potomnych. Każde dziecko zamyka nieużywane końcówki rur (zostawia sobie tylko czytanie ze swojej rury, pisanie do rur sąsiadów oraz pisanie do FIFO).

**Etap 3: Logika Dzieci - Nieblokujące Czytanie (8 pkt)**
Każdy proces potomny (węzeł) zaraz po starcie ustawia końcówkę odczytu ze swojej rury w tryb **nieblokujący** (używając funkcji `fcntl` z flagą `O_NONBLOCK`). Następnie kręci się w pętli `while(dziala)`, gdzie zmienna `dziala` zmienia się na 0 tylko, gdy przyjdzie sygnał `SIGUSR1`.
W pętli:

1. Próbuje odczytać Ziemniaka (liczbę całkowitą) ze swojej rury.
2. **Jeśli rura jest pusta** (read zwraca `-1` i ustawia błąd na `EAGAIN`/`EWOULDBLOCK`), proces usypia na chwilę (np. 50 ms za pomocą `usleep`) i próbuje ponownie w następnym obrocie pętli.
3. **Jeśli odczyta Ziemniaka**:
* Zmniejsza jego `TTL` o 1.
* Jeśli `TTL > 0`: Wypisuje `<Nazwa> passes the potato!`, losuje dozwolonego sąsiada z macierzy i wpisuje Ziemniaka do jego rury.
* Jeśli `TTL == 0`: Ziemniak wybuchł! Proces wypisuje `<Nazwa> got burned!` i **zapisuje do FIFO** specjalny komunikat dla Rodzica (np. swój numer ID), po czym wraca do pętli (czeka na sygnał końca gry).



**Etap 4: Serwer i Sprzątanie (5 pkt)**
Rodzic (wątek główny) po utworzeniu dzieci:

1. Inicjuje grę: losuje jeden węzeł i wysyła do jego rury początkowego Ziemniaka (np. `TTL = 20`).
2. Otwiera **FIFO** do odczytu (blokująco) i czeka.
3. Gdy tylko z FIFO nadejdzie raport "Ziemniak wybuchł u węzła X!", Rodzic wie, że gra skończona.
4. Rodzic wysyła sygnał `SIGUSR1` do wszystkich `N` dzieci (rozkaz ewakuacji).
5. Rodzic w pętli wywołuje `wait()`, aby posprzątać wszystkie zakończone procesy potomne, usuwa plik FIFO (`unlink("raport_fifo")`) i wypisuje `Game Over. Cleanup done.`

---
