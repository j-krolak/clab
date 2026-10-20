# SOP1 L1: ściągawka (system plików)

## Nagłówki
```c
#define _GNU_SOURCE   // getline, strdup, DT_* itd.
#include <dirent.h> <errno.h> <fcntl.h> <limits.h> <stdio.h> <stdlib.h>
#include <string.h> <sys/stat.h> <sys/uio.h> <unistd.h>
```

## Błędy
```c
#define ERR(s) (perror(s), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
```
- Sprawdzaj wynik KAŻDEGO syscalla (`-1`, `NULL`). `errno` czytaj od razu po błędzie.
- Wyjątek: `EEXIST` przy `mkdir`/`symlink` bywa oczekiwany (np. katalog gatunku).

## Katalogi
```c
DIR* d = opendir(p); if (!d) ERR("opendir");
struct dirent* e;
for (errno = 0; (e = readdir(d)); errno = 0) {   // errno=0 też przy `continue`
  if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
  snprintf(full, sizeof full, "%s/%s", p, e->d_name);
  lstat(full, &st);                              // pewniejsze niż d_type
}
if (errno) ERR("readdir");
closedir(d);
rewinddir(d);                                    // drugi przebieg po tym samym DIR*
```
- `d_type`: `DT_DIR`, `DT_REG`, `DT_LNK`, może być `DT_UNKNOWN`. Wtedy `lstat`.
- Rekurencja: zawsze pomiń `.` i `..`, inaczej nieskończona pętla.

## stat
```c
stat(p, &st)   // idzie ZA symlinkiem (rozmiar pliku docelowego)
lstat(p, &st)  // rozmiar/typ samego linku
S_ISREG(st.st_mode)  S_ISDIR(st.st_mode)  S_ISLNK(st.st_mode)
st.st_size  (off_t, rzutuj na (long) do printf)   st.st_nlink  st.st_ino
```
- Do sprawdzenia, czy coś istnieje: `stat == -1 && errno == ENOENT`.
- Zepsuty symlink: `stat` -> `ENOENT`, `lstat` -> OK.

## mkdir i umask
- Prawa = `mode & ~umask`. Przy umask `022`: `0777 -> 0755`, `0766 -> 0744` (brak `x` dla innych).
- Dla katalogów daj `0755`. Bit `x` = prawo wejścia.
- Istniejący katalog: `errno == EEXIST`.

## Symlinki
```c
symlink(cel, sciezka_linku)   // cel jest zapisany dosłownie jako tekst
```
- Cel względny liczy się OD KATALOGU, W KTÓRYM LEŻY LINK, nie od cwd.
- `index/by-title/X` -> cel `../../library/a/b.txt` (2x `..`, bo 2 poziomy w górę). `index/by-genre/g/X` -> 3x `..`.
- Duplikat nazwy: `symlink` zwraca -1, `errno == EEXIST`.
- `readlink` NIE dopisuje `\0`.

## Parsowanie `klucz:wartość`
```c
len = getline(&line, &cap, f);       // len = DŁUGOŚĆ linii, cap = rozmiar bufora (nie mylić!)
line[strcspn(line, "\n")] = '\0';    // ucięcie \n
char* c = strchr(line, ':'); if (!c) continue;
*c = '\0'; label = line; value = c + 1;
strdup(value) ... free(line) po pętli
```
- `strtok` tnie po każdym `:` i gubi resztę wartości. Lepiej `strchr`.

## Obcięcie do N bajtów w nazwie
```c
snprintf(buf, sizeof buf, "%s/%.*s", dir, 64, name);   // max 64 bajty z name
```

## readv (rekordy binarne)
```c
unsigned int size; char title[65];
struct iovec iov[2] = {{&size, sizeof size}, {title, 64}};
ssize_t r = readv(fd, iov, 2);       // -1 błąd, 68 OK, inaczej obcięty plik
title[64] = '\0';
liczba_rekordów = st.st_size / 68;   // fstat(fd)
```
- `readv` może zwrócić mniej niż zażądano. Sprawdzaj `== 68`.

## getopt
```c
while ((c = getopt(argc, argv, "p:o:e")) != -1) switch (c) { case 'p': optarg ... default: usage(); }
if (optind < argc) usage();           // zostały argumenty pozycyjne
```
- `"p:"` = opcja z argumentem, `"o"` = flaga bez argumentu.
- Wielokrotne `-p`: tablica `calloc(argc, sizeof *t)`.

## Zapis do pliku zamiast stdout
```c
FILE* out = stdout; if (path && !(out = fopen(path, "w"))) ERR("fopen");   // "w" nadpisuje
... fclose(out) tylko gdy to nie stdout
getenv("NAZWA")   // NULL gdy brak
```

## Usuwanie linii z pliku (np. requirements)
Nie da się wyciąć środka pliku w miejscu. Dwie poprawne metody:

**1. Plik tymczasowy + rename (zalecane)**
```c
in = fopen(req, "r"); out = fopen(req ".tmp", "w");
while (getline(...) != -1) if (!pasuje(line)) fputs(line, out);
fclose(in); fclose(out);
if (!found) { unlink(tmp); błąd "not installed"; }
rename(tmp, req);                 // atomowo podmienia, w obrębie tego samego systemu plików
```
**2. W pamięci + ftruncate** (gdy plik mały): wczytaj całość, `ftruncate(fd, 0)`, `lseek(fd, 0, SEEK_SET)`, zapisz bez linii.

Uwagi:
- Dopasuj po NAZWIE, nie po całej linii: `strcspn(line, " \n")` i porównaj długość + `strncmp`. Inaczej `num` pasuje do `numpy`.
- Samo `fopen(req, "w")` przed odczytem kasuje plik. Nie otwieraj do zapisu tego, co jeszcze czytasz.
- `rename` tworzy NOWY i-węzeł, więc hard linki do starego `requirements` dalej wskazują starą treść. Edycja w miejscu (`ftruncate` + zapis) zmienia plik widoczny przez wszystkie hard linki.
- Sprawdź `found` PRZED `rename`. Usuwanie pakietu, którego nie ma, ma być błędem.

## Hard linki
```c
link(istniejący, nowa_nazwa)   // druga nazwa TEGO SAMEGO i-węzła
unlink(sciezka)                // usuwa JEDNĄ nazwę
```
- Dane znikają dopiero, gdy `st_nlink == 0` i nikt nie ma pliku otwartego. `unlink` jednej nazwy nie kasuje treści widocznej przez inną.
- Ten sam `st_ino` + `st_dev` = ten sam plik.
- Nie wolno linkować katalogów (`EPERM`) ani przez granice systemów plików (`EXDEV`).
- `rename` też działa tylko w obrębie jednego systemu plików (`EXDEV`).
- `st_nlink` liczy nazwy (dla symlinku liczy sam symlink).

| | hard link | symlink |
|---|---|---|
| wskazuje na | i-węzeł | tekst ze ścieżką |
| po usunięciu celu | działa dalej | zepsuty (`ENOENT`) |
| katalogi, inne FS | nie | tak |
| `stat` | ten sam plik | idzie za linkiem (`lstat` nie) |

W tych zadaniach używamy symlinków, bo cel ma być WIDOCZNY jako ścieżka względna.

## Usuwanie
- `unlink(plik)`: plik/symlink. `rmdir(katalog)`: tylko pusty. `remove()` robi jedno z dwóch.
- Pakiet z prawami `-r--r--r--` da się usunąć: o `unlink` decydują prawa do KATALOGU, nie do pliku.
- Plik tylko do odczytu tworzysz tak: `open(p, O_WRONLY|O_CREAT|O_EXCL, 0444)` (zapis przez ten sam deskryptor działa).

## Sprzątanie i dobre praktyki
- `free` wszystkiego z `strdup`/`getline`/`calloc`, `closedir`, `fclose`, `close`.
- `O_EXCL` przy tworzeniu plików: błąd, jeśli już istnieje.
- `PATH_MAX` na bufory ścieżek, `snprintf` zamiast `sprintf`.
- Nie ufaj `d_type`, nie ufaj, że `readdir` zwraca po kolei.
- Kompilacja do sprawdzenia: `gcc -Wall -Wextra -Wshadow -fsanitize=address,undefined`.

## Wzorzec zad4 (indeks)
```
index/
  by-visible-title/<nazwa pliku>        -> ../../library/<ścieżka>
  by-title/<title do 64B>               -> ../../library/<ścieżka>
  by-genre/<genre do 64B>/<title>       -> ../../../library/<ścieżka>
```
Kolejność: `mkdir index` (błąd, jeśli istnieje) -> podkatalogi -> `walk("library")` -> opcjonalnie `check_database`.
