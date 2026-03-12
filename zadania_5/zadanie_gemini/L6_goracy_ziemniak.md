**L6: Gorący Ziemniak (Zabawa w Głuchy Telefon)**.

---

### Plik wejściowy: `siec.txt`

Plik zawiera opis skierowanego grafu. Pierwsza linia to liczba węzłów `N` (od 3 do 10). Kolejne `N` linii to nazwa węzła (procesu) oraz wiersz macierzy sąsiedztwa (złożony z 0 i 1).
Wartość `1` na $j$-tej pozycji oznacza, że węzeł może wysłać wiadomość do węzła $j$.

```text
4
Alfa 0 1 1 0
Bravo 0 0 1 1
Charlie 1 0 0 1
Delta 0 1 0 0

```

*(Tłumaczenie: Alfa może wysłać do Bravo i Charlie. Bravo do Charlie i Delta, itd.)*

---

### Treść zadania: L6: Gorący Ziemniak

**Etap 1: Parsowanie (5 pkt)**
Główny proces otwiera plik `siec.txt`. Odczytuje liczbę `N`, a następnie wczytuje nazwy węzłów i macierz sąsiedztwa do pamięci.
Dla każdego węzła wypisuje komunikat:
`Node <Nazwa> ready. I have <liczba_krawedzi> outgoing connections.`
W przypadku błędu otwarcia pliku, wypisuje `Network is down` i kończy działanie.

**Etap 2: Infrastruktura FIFO i Procesy (7 pkt) - *[Tu jest ten "sprytny" haczyk!]***
Główny proces tworzy w systemie `N` kolejek FIFO (np. używając `mkfifo`), nazywając je odpowiednio `fifo_0`, `fifo_1`, ..., `fifo_N-1`.
Zauważ: **nie robimy FIFO dla każdej krawędzi, tylko JEDNO wejściowe FIFO dla każdego węzła!** Jeśli wielu sąsiadów chce wysłać wiadomość do "Alfy", wszyscy piszą do `fifo_0`.
Następnie rodzic tworzy `N` procesów potomnych (węzłów).

**Etap 3: Logika Grafu i Gorący Ziemniak (8 pkt)**
Rodzic (serwer) losuje jeden proces i wysyła mu przez jego FIFO początkową wartość Gorącego Ziemniaka (np. jako zwykły `int TTL = 20`).
Każdy proces potomny działa w prostej pętli:

1. Odczytuje jedną liczbę całkowitą (Ziemniaka) ze swojego własnego FIFO. *(Sprytny trik: proces czyta tylko ze SWOJEGO jednego FIFO, więc po prostu robi blokującego `read`a, nie potrzebuje funkcji `poll` ani `select`!).*
2. Po odczycie zmniejsza wartość o 1.
3. Jeśli wartość $> 0$: Wypisuje `<Nazwa> passes the potato! (TTL: <wartość>)`, losuje **jednego** ze swoich dozwolonych sąsiadów (zgodnie z wczytaną macierzą) i wysyła mu Ziemniaka przez jego FIFO. Czeka losowo od 10 do 50 ms (`usleep`).
4. Jeśli wartość $== 0$: Wypisuje `<Nazwa> got burned!`, zamyka swoje rury i **kończy działanie** (`exit(EXIT_SUCCESS)`).

**Etap 4: Brutalne sprzątanie (5 pkt)**
Rodzic po wysłaniu Ziemniaka w Etapie 3 po prostu czeka (`wait`). Ponieważ tylko JEDEN proces potomny naturalnie zakończy działanie (ten, u którego `TTL` spadnie do 0), to funkcja `wait` u rodzica odblokuje się natychmiast po jego "śmierci".
Wtedy rodzic musi natychmiast wysłać sygnał `SIGKILL` (lub `SIGTERM`) do wszystkich pozostałych przy życiu potomków, usunąć wszystkie pliki FIFO z dysku (funkcja `unlink`) i samemu się zakończyć, wypisując `Game Over. Cleanup done.`

---

### Dlaczego to zadanie jest "sprytne" i czego uczy?

1. **Graf przez FIFO:** Studenci często myślą, że krawędź w grafie = jedno FIFO (od A do B). Wtedy węzeł, do którego wchodzą 3 krawędzie, musiałby czytać z 3 różnych plików jednocześnie. To wymaga zaawansowanego kodu (tzw. multipleksowania wejścia/wyjścia). Ale rury w Linuxie są bezpieczne dla wielu piszących (atomowość do `PIPE_BUF`)! Zatem najmądrzej jest dać każdemu procesowi **tylko jedną skrzynkę pocztową** (jedno FIFO). Inni otwierają ją tylko do zapisu, a on otwiera ją do odczytu. Kodu jest 5 razy mniej.
2. **Deadlocki:** Przy otwieraniu plików FIFO, funkcja `open("fifo_x", O_RDONLY)` **zablokuje proces**, dopóki ktoś inny nie otworzy jej z flagą `O_WRONLY`. Jeśli procesy będą naiwnie otwierać rury, wszystkie się zablokują. *Sprytny trik:* Jeśli proces otworzy swoje FIFO z flagą `O_RDWR` (odczyt i zapis jednocześnie), `open` zadziała natychmiast bez blokowania, a proces nigdy nie dostanie fałszywego EOF (End of File).
3. **Pojedynczy `wait`:** Zamiast kazać procesom wysyłać sobie sygnały o końcu gry, OS sam robi brudną robotę. Skoro gra kończy się, gdy ktoś dostanie TTL=0, to ten ktoś po prostu robi `exit`. Rodzic łapie jego śmierć, zabija resztę i sprząta zabawki. Minimum kodu, czysta inżynieria.
