#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source)                                                                                \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

// Zapisuje DOKŁADNIE `count` bajtów z `buf` do `fd`.
// write() może zapisać mniej niż prosisz (zwraca ile zapisał) albo zostać przerwany sygnałem
// (EINTR), więc trzeba go wołać w pętli. Przy innym błędzie kończy program (ERR).
void write_full(int fd, const char* buf, size_t count) {
  while (count > 0) {
    ssize_t w = write(fd, buf, count);
    if (w == -1) {
      if (errno == EINTR)
        continue;
      ERR("write");
    }
    buf += w;
    count -= (size_t)w;
  }
}

// Wczytuje cały plik od bieżącej pozycji `fd` do nowego bufora zakończonego '\0'.
// Rozmiar bierze z fstat(), a odczyt robi w pętli (read() może zwrócić mniej, albo EINTR).
// `*len` dostaje liczbę wczytanych bajtów (bez '\0'). Bufor trzeba zwolnić przez free().
// Nie działa dla plików bez znanego rozmiaru (pipe, /proc), tylko dla zwykłych plików.
char* read_all(int fd, size_t* len) {
  struct stat st;
  if (fstat(fd, &st) == -1)
    ERR("fstat");
  char* buf = malloc((size_t)st.st_size + 1);
  if (!buf)
    ERR("malloc");
  size_t done = 0;
  while (done < (size_t)st.st_size) {
    ssize_t r = read(fd, buf + done, (size_t)st.st_size - done);
    if (r == -1) {
      if (errno == EINTR)
        continue;
      ERR("read");
    }
    if (r == 0)
      break;
    done += (size_t)r;
  }
  buf[done] = '\0';
  *len = done;
  return buf;
}

// Szuka w buforze (NIE musi mieć '\0') pierwszej linii, której pierwszy wyraz, czyli tekst do
// pierwszej spacji lub końca linii, jest równy `key`. Porównanie jest na całe słowo, więc "num"
// nie pasuje do "numpy 1.0.0".
// Zwraca 1 i ustawia [*start, *next) na zakres tej linii RAZEM z końcowym '\n' (ostatnia linia
// pliku może go nie mieć). Zwraca 0, gdy takiej linii nie ma.
int find_line(const char* buf, size_t len, const char* key, size_t* start, size_t* next) {
  size_t key_len = strlen(key);
  for (size_t pos = 0; pos < len;) {
    const char* nl = memchr(buf + pos, '\n', len - pos);
    size_t end = nl ? (size_t)(nl - buf) + 1 : len;
    size_t word = 0;
    while (pos + word < end && buf[pos + word] != ' ' && buf[pos + word] != '\n')
      word++;
    if (word == key_len && memcmp(buf + pos, key, key_len) == 0) {
      *start = pos;
      *next = end;
      return 1;
    }
    pos = end;
  }
  return 0;
}

// Czy plik `path` zawiera linię zaczynającą się słowem `key` (patrz find_line)? 1 = tak, 0 = nie.
// Plik musi istnieć, inaczej program kończy się błędem (ERR).
int has_line(const char* path, const char* key) {
  int fd = open(path, O_RDONLY);
  if (fd == -1)
    ERR("open");
  size_t len, start, next;
  char* buf = read_all(fd, &len);
  int found = find_line(buf, len, key, &start, &next);
  free(buf);
  if (close(fd) == -1)
    ERR("close");
  return found;
}

// Usuwa z pliku pierwszą linię zaczynającą się słowem `key` (patrz find_line).
// Robi to W MIEJSCU: wczytuje plik, wycina linię w pamięci, zapisuje całość od początku i
// skraca plik przez ftruncate(). Zostaje ten sam i-węzeł, bez pliku tymczasowego i rename(),
// więc hard linki do pliku dalej widzą aktualną treść (rename() by je zerwał).
// Zwraca 1 = usunięto, 0 = nie było takiej linii (plik nietknięty).
// Uwaga: nie jest atomowe, awaria w trakcie zapisu może zostawić plik w stanie pośrednim.
int remove_line(const char* path, const char* key) {
  int fd = open(path, O_RDWR);
  if (fd == -1)
    ERR("open");
  size_t len, start, next;
  char* buf = read_all(fd, &len);
  int found = find_line(buf, len, key, &start, &next);
  if (found) {
    memmove(buf + start, buf + next, len - next);
    len -= next - start;
    if (lseek(fd, 0, SEEK_SET) == -1)
      ERR("lseek");
    write_full(fd, buf, len);
    if (ftruncate(fd, (off_t)len) == -1)
      ERR("ftruncate");
  }
  free(buf);
  if (close(fd) == -1)
    ERR("close");
  return found;
}

// Dopisuje `line` + '\n' na koniec istniejącego pliku (nie podawaj własnego '\n').
// O_APPEND zapisuje zawsze na końcu, w tym samym i-węźle, więc hard linki dalej widzą zmianę.
void append_line(const char* path, const char* line) {
  int fd = open(path, O_WRONLY | O_APPEND);
  if (fd == -1)
    ERR("open");
  write_full(fd, line, strlen(line));
  write_full(fd, "\n", 1);
  if (close(fd) == -1)
    ERR("close");
}

// mkdir(), które nie traktuje istniejącej ścieżki (errno == EEXIST) jako błędu. Inne błędy
// kończą program. Użyteczne np. dla index/by-genre/<gatunek>, gdy gatunek się powtarza.
// Uprawnienia = mode & ~umask. Dla katalogów daj 0755. Nie sprawdza, czy to katalog, a nie plik.
void mkdir_if_missing(const char* path, mode_t mode) {
  if (mkdir(path, mode) == -1 && errno != EEXIST)
    ERR("mkdir");
}

// Skleja dwie części ścieżki w nowy, zaalokowany napis "a/b". Zwolnij przez free().
// Nie dubluje ukośnika, gdy `a` już kończy się na '/'. Nie sprawdza, czy ścieżka istnieje.
char* path_join(const char* a, const char* b) {
  size_t a_len = strlen(a);
  int needs_slash = a_len > 0 && a[a_len - 1] != '/';
  size_t size = a_len + (size_t)needs_slash + strlen(b) + 1;
  char* joined = malloc(size);
  if (!joined)
    ERR("malloc");
  snprintf(joined, size, "%s%s%s", a, needs_slash ? "/" : "", b);
  return joined;
}

// Tworzy symlink `link_path` wskazujący na `target`. Każdy błąd kończy program (ERR).
// `target` jest zapisany dosłownie, a względny liczy się OD KATALOGU, W KTÓRYM LEŻY LINK.
// Nie sprawdza, czy cel istnieje, więc zepsutą ścieżkę zobaczysz dopiero przy użyciu linku.
// errno: EEXIST = `link_path` już zajęte (np. duplikat nazwy), ENOENT = brak katalogu linku.
void make_symlink(const char* target, const char* link_path) {
  if (symlink(target, link_path) == -1)
    ERR("symlink");
}

// Tworzy dir/name -> up/target, gdzie `name` jest obcięte do `max_name` BAJTÓW.
// `up` to droga z katalogu `dir` do katalogu, od którego liczony jest `target`, np. dla
// dir = "index/by-title" i target = "library/a.txt" daj up = "../../", a dla
// "index/by-genre/<gatunek>" daj "../../../". Złe `up` = zepsuty link bez żadnego błędu.
void make_relative_link(const char* up, const char* target, const char* dir, const char* name,
                        size_t max_name) {
  char* link_target = path_join(up, target);
  char* short_name = strndup(name, max_name);
  if (!short_name)
    ERR("strndup");
  char* link_path = path_join(dir, short_name);
  make_symlink(link_target, link_path);
  free(link_target);
  free(short_name);
  free(link_path);
}

typedef void (*walk_fn)(const char* path, const char* name, const struct stat* st, void* ctx);

// Przechodzi po katalogu `dir_path` i wywołuje cb(pełna_ścieżka, nazwa, stat, ctx) dla każdego
// wpisu: plików, katalogów i symlinków, ale bez "." i "..".
// `depth`: 1 = tylko ten katalog, 2 = też jego podkatalogi itd. (INT_MAX = całe drzewo).
// stat pochodzi z lstat(), więc symlinków NIE rozwija i nie wchodzi w nie. Typ sprawdzaj
// przez S_ISREG(st->st_mode) / S_ISDIR(...), nie przez d_type (bywa DT_UNKNOWN).
// cb jest wołane dla katalogu PRZED zejściem do niego, a kolejność wpisów jest dowolna.
// `ctx` to dowolny wskaźnik przekazywany do cb (np. FILE* albo struktura ze stanem).
void walk(const char* dir_path, int depth, walk_fn cb, void* ctx) {
  DIR* dir = opendir(dir_path);
  if (!dir)
    ERR("opendir");
  struct dirent* e;
  for (errno = 0; (e = readdir(dir)); errno = 0) {
    if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
      continue;
    char* path = path_join(dir_path, e->d_name);
    struct stat st;
    if (lstat(path, &st) == -1)
      ERR("lstat");
    cb(path, e->d_name, &st, ctx);
    if (S_ISDIR(st.st_mode) && depth > 1)
      walk(path, depth - 1, cb, ctx);
    free(path);
  }
  if (errno)
    ERR("readdir");
  if (closedir(dir) == -1)
    ERR("closedir");
}

// Czy nazwa pliku kończy się na ".`ext`"? `ext` podajesz bez kropki.
// "a.txt" + "txt" = 1. Nie pasuje: "atxt" (brak kropki), "txt" i ".txt" (brak nazwy przed kropką).
int has_extension(const char* name, const char* ext) {
  size_t n = strlen(name), e = strlen(ext);
  return n > e + 1 && name[n - e - 1] == '.' && strcmp(name + n - e, ext) == 0;
}

// Parsuje linię "klucz:wartość" (np. z getline). MODYFIKUJE `line`: ucina '\n' i wstawia '\0'
// w miejsce pierwszego ':'. `*key` i `*value` wskazują do środka `line`, więc żyją tylko tak
// długo jak ona (potrzebna trwała kopia = strdup). Wartość może zawierać kolejne ':'
// ("title:Foo: Bar" daje "Foo: Bar"). Zwraca 0, gdy brak ':' albo klucz lub wartość są puste.
int split_key_value(char* line, char** key, char** value) {
  line[strcspn(line, "\n")] = '\0';
  char* colon = strchr(line, ':');
  if (!colon || colon == line || !colon[1])
    return 0;
  *colon = '\0';
  *key = line;
  *value = colon + 1;
  return 1;
}
