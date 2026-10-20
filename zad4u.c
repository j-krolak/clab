#include "utilities.c"
#include <sys/uio.h>

#define MAX_NAME 64
#define ENTRY_SIZE (sizeof(unsigned int) + MAX_NAME)

typedef struct {
  char* title;
  char* genre;
} book_t;

void free_book(book_t* book) {
  free(book->title);
  free(book->genre);
}

void get_book_metadata(const char* path, book_t* book) {
  FILE* f = fopen(path, "r");
  if (!f)
    ERR("fopen");

  char* line = NULL;
  size_t cap = 0;
  while (getline(&line, &cap, f) != -1) {
    char *key, *value;
    if (!split_key_value(line, &key, &value))
      continue;
    if (strcmp(key, "title") == 0 && !book->title)
      book->title = strdup(value);
    else if (strcmp(key, "genre") == 0 && !book->genre)
      book->genre = strdup(value);
  }
  if (ferror(f))
    ERR("getline");

  free(line);
  if (fclose(f))
    ERR("fclose");
}

void index_book(const char* path, const char* name, const struct stat* st, void* ctx) {
  if (!S_ISREG(st->st_mode))
    return;

  make_relative_link("../../", path, "index/by-visible-title", name, MAX_NAME);

  book_t book = {0};
  get_book_metadata(path, &book);
  if (book.title) {
    make_relative_link("../../", path, "index/by-title", book.title, MAX_NAME);
    if (book.genre) {
      char genre_dir[PATH_MAX];
      snprintf(genre_dir, sizeof genre_dir, "index/by-genre/%.*s", MAX_NAME, book.genre);
      mkdir_if_missing(genre_dir, 0755);
      make_relative_link("../../../", path, genre_dir, book.title, MAX_NAME);
    }
  }
  free_book(&book);
}

void check_database(const char* db_path) {
  int fd = open(db_path, O_RDONLY);
  if (fd == -1)
    ERR("open");
  struct stat db_stat;
  if (fstat(fd, &db_stat) == -1)
    ERR("fstat");

  off_t count = db_stat.st_size / ENTRY_SIZE;
  for (off_t i = 0; i < count; i++) {
    unsigned int size;
    char title[MAX_NAME + 1];
    struct iovec iov[2] = {{&size, sizeof size}, {title, MAX_NAME}};
    ssize_t got = readv(fd, iov, 2);
    if (got == -1)
      ERR("readv");
    if ((size_t)got != ENTRY_SIZE) {
      fprintf(stderr, "truncated database entry\n");
      exit(EXIT_FAILURE);
    }
    title[MAX_NAME] = '\0';

    char link_path[PATH_MAX];
    snprintf(link_path, sizeof link_path, "index/by-title/%s", title);
    struct stat book_stat;
    if (stat(link_path, &book_stat) == -1) {
      if (errno != ENOENT)
        ERR("stat");
      printf("Book \"%s\" is missing\n", title);
      continue;
    }
    if ((unsigned long)book_stat.st_size != size)
      printf("Book \"%s\" size mismatch (%u vs %ld)\n", title, size, (long)book_stat.st_size);
  }

  if (close(fd))
    ERR("close");
}

int main(int argc, char* argv[]) {
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [database]\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (mkdir("index", 0755) == -1)
    ERR("mkdir");
  mkdir_if_missing("index/by-visible-title", 0755);
  mkdir_if_missing("index/by-title", 0755);
  mkdir_if_missing("index/by-genre", 0755);

  walk("library", INT_MAX, index_book, NULL);
  if (argc == 2)
    check_database(argv[1]);
  return EXIT_SUCCESS;
}
