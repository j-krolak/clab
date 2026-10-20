# Pułapki w zadaniach testowych L1

Ogólne rzeczy (składnia, wzorce) są w `cheatsheet.md`. Tu są błędy, które realnie łatwo zrobić.

## Wspólne dla wszystkich zadań

- **Brak sprawdzenia wyniku syscalla.** `fopen`, `opendir`, `symlink`, `mkdir`, `close` mogą zawieść. Prowadzący sprawdza to wprost (`Check the return value of every system call`).
- **`.` i `..` w rekurencji** = nieskończona pętla. Zawsze pomijaj oba.
- **`d_type` bywa `DT_UNKNOWN`.** Typ bierz z `lstat`.
- **`stat` vs `lstat`.** `stat` idzie za symlinkiem, `lstat` nie. Zepsuty symlink daje `ENOENT` w `stat`, ale nie w `lstat`.
- **`getline`: `cap` to rozmiar bufora, nie długość linii.** Długość zwraca sama funkcja. Ucięcie `\n` po `cap` nic nie robi.
- **`strtok` po `:`** tnie wartość z dwukropkiem w środku (`Foo: Bar` -> `Foo`). Użyj `strchr`.
- **Niezainicjowany struct** (`book_t book;`). Wskaźniki mają śmieci, a `free` na nich wywala program. Pisz `book_t book = {0};`.
- **`printf("%s", NULL)`** to UB. Sprawdzaj wskaźnik przed użyciem w komunikacie błędu.
- **`off_t`/`ssize_t` w `printf`.** Rzutuj na `(long)` i używaj `%ld`.
- **`errno` ginie.** Każde kolejne wywołanie (nawet `fprintf`) może go nadpisać. Zapisz od razu, jeśli potrzebujesz.
- **Pętla `readdir` i `errno`.** Ustaw `errno = 0` przed każdym `readdir` (także po `continue`) i sprawdź go po pętli, inaczej błąd odczytu katalogu przejdzie niezauważony.
- **`getopt`.** `"p:o"` = `-p` ma argument, `-o` jest flagą. Po pętli sprawdź `optind < argc` (zbędne argumenty pozycyjne).
- **Zwalnianie zasobów.** `free` wszystkiego z `strdup`/`getline`/`calloc`, `closedir`, `fclose`, `close`. Nie rób `fclose(stdout)`.
- **`umask`.** Prawa nowego katalogu to `mode & ~umask`. `0766` przy `022` daje `0744`. Używaj `0755`.
- **Bufory ścieżek.** `snprintf` do `PATH_MAX`, nigdy `sprintf`. Sprawdź, że nie obcięło.

## Zadanie 1: listing katalogów (`-p`, `-o`)

- **Nieistniejący katalog nie przerywa programu.** Komunikat na `stderr` i dalej do następnej ścieżki. `ERR` w złym miejscu zabije całość.
- **Brak `-p`** = katalog roboczy `.`.
- **`-o` co najwyżej raz.** Drugie wystąpienie to usage i błąd.
- **Dodatkowe argumenty pozycyjne** też są błędem (`optind < argc`).
- **Rozmiar obiektów, także katalogów.** Listowane są wszystkie wpisy, nie tylko pliki. Bierz `st_size` z `lstat`.
- **Wynik do pliku zamiast na stdout**, nie do obu naraz. Plik otwieraj `"w"` (nadpisuje).
- **`.` i `..`** nie powinny być w listingu (przykład ze strony ich nie pokazuje).

## Zadanie 2: skanowanie (`-p`, `-e`, `-d`, `-o`)

- **Kolejność wydruku.** W przykładzie najpierw pliki katalogu, potem podkatalogi (`dir2` przed `dir2/dir3`). Rekurencja w trakcie jednej pętli `readdir` miesza je. Zrób dwa przebiegi (`rewinddir`) albo dwa wywołania `walk`.
- **Głębokość 1 = tylko podany katalog**, nie `0`. Walidacja `-d`: `strtol`, cała liczba, `>= 1`.
- **Rozszerzenie** `txt` bez kropki. Plik `txt` ani `atxt` nie pasuje. Plik `.txt` (ukryty, sama kropka) też nie.
- **`-o` to flaga bez argumentu**, nazwa pliku jest w `L1_OUTPUTFILE`. Brak zmiennej = stdout (strona nie mówi inaczej).
- **Istniejący plik wyjściowy** ma być nadpisany (`"w"`).
- **Wypisuj tylko pliki zwykłe** (`S_ISREG`), nie katalogi i nie symlinki.
- **Symlinki do katalogów.** `lstat` nie zejdzie w link, więc nie zapętlisz się. `stat` mógłby.

## Zadanie 3: `sop-venv` (menedżer pakietów)

- **Usuwanie linii z `requirements`.** Nie kopiuj pliku (`tmp` + `rename`), bo `rename` zrywa hard linki do pliku. Edytuj w miejscu: `read_all`, `memmove`, `lseek(0)`, `write_full`, `ftruncate`. Gotowe: `remove_line` w `utilities.c`.
- **Dopasowanie po nazwie pakietu**, nie po prefiksie: `num` nie może usunąć `numpy`.
- **Brak pakietu** przy `-r` to błąd, nie cichy sukces. Funkcja musi zwrócić, czy coś usunęła.
- **Duplikat pakietu** (ta sama nazwa, inna wersja) = błąd. Sprawdź PRZED utworzeniem pliku, inaczej zostanie osierocony plik.
- **Plik pakietu `-r--r--r--`** tworzysz `open(..., O_CREAT|O_EXCL, 0444)`. Zapis przez ten sam deskryptor działa. Usunięcie pliku zależy od praw do KATALOGU, nie do pliku.
- **`-c` w dwóch formach.** Przykład używa `-c nazwa`, składnia `-c -v nazwa`. Obsłuż oba.
- **Wiele `-v`**: operacja na każdym środowisku po kolei. Błąd w którymkolwiek kończy program (nie kontynuuj).
- **Opcje w dowolnej kolejności.** `getopt` to załatwia, o ile nie parsujesz `argv` ręcznie.
- **Nieistniejące środowisko** to komunikat `sop-venv: the environment does not exist` + usage. Sprawdź katalog przez `stat` (rozróżnij `ENOENT` od innego błędu).
- **Nazwa pakietu z `/`** wyjdzie poza katalog środowiska. Odrzuć.
- **Wersja** to wszystko po `==`. `numpy==` (pusta) jest błędem.

## Zadanie 4: indeks książek

- **Etap 2 nazywa linki nazwą pliku**, nie tytułem. Nie parsuj tam metadanych i nie kończ błędem przy braku tytułu.
- **Brak `title` w etapie 3** = pomiń w `by-title` i `by-genre`, bez błędu. Brak `genre` = pomiń tylko `by-genre`.
- **Ścieżka celu symlinku liczy się od katalogu, w którym leży LINK.** `index/by-title/X` -> `../../library/...`. `index/by-genre/g/X` -> `../../../library/...`. Pomyłka o jedno `..` i wszystkie linki są zepsute. Sprawdź `ls -l` i `cat` przez link.
- **Obcinanie do 64 BAJTÓW**, nie znaków: `"%.64s"`. Dotyczy tytułu i gatunku. Polskie znaki mogą zostać ucięte w środku (tak chce zadanie).
- **`index` już istnieje** = błąd. **Duplikat nazwy** w `by-visible-title` = błąd (`EEXIST` z `symlink`).
- **`by-genre/<genre>`** twórz z `mkdir_if_missing` (dla drugiej książki tego gatunku `EEXIST` jest normalny).
- **Katalog `index` obok `library`**, nie wewnątrz. W środku zostałby wzięty za książki przy ponownym uruchomieniu.
- **Śmieci po poprzednich uruchomieniach.** Przed każdym testem `rm -rf index`.
- **Etap 4, `readv`.** Rekord ma 68 bajtów: 4 bajty `unsigned int` i 64 bajty tytułu. Tablica na tytuł `char[65]` z ostatnim bajtem `0`. Tytuł dokładnie 64-znakowy nie ma `\0` w pliku.
- **`readv` może zwrócić mniej niż 68.** Sprawdź `== 68`.
- **Liczba rekordów** = `st_size / 68` (z `fstat`).
- **Rozmiar książki** bierz przez `stat` na linku w `by-title` (idzie za linkiem), nie `lstat` (to rozmiar samego linku).
- **Brak książki** to `ENOENT` z `stat`, inny błąd to prawdziwy błąd.
- **Komunikaty dokładnie:** `Book "<tytuł>" is missing` oraz `Book "<tytuł>" size mismatch (<expected> vs <actual>)`. Kolejność liczb przyjęta: z bazy, potem z pliku. Sprawdź na `database_correct`.

## Przed oddaniem

- Kompilacja: `gcc -Wall -Wextra -Wshadow -fsanitize=address,undefined`. Zero ostrzeżeń.
- Uruchom każdy scenariusz błędu raz (zły argument, brak pliku, duplikat).
- `valgrind`/sanitizer na ścieżce błędu też, nie tylko na sukcesie.
- Usuń zakomentowany kod i `printf` do debugowania.
