#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define MAX_NAME 64
#define ENTRY_SIZE (sizeof(unsigned int) + MAX_NAME)

typedef struct {
  char* title;
  char* author;
  char* genre;
} book_t;

void free_book(book_t* book) {
  free(book->title);
  free(book->author);
  free(book->genre);
}

void get_book_metadata(const char* path, book_t* book) {
  FILE* f = fopen(path, "r");
  if (!f)
    ERR("fopen");

  char* line = NULL;
  size_t cap = 0;
  ssize_t len;
  while ((len = getline(&line, &cap, f)) != -1) {
    line[strcspn(line, "\n")] = '\0';
    char* colon = strchr(line, ':');
    if (!colon)
      continue;
    *colon = '\0';
    char const* label = line;
    char const* value = colon + 1;
    if (!*value)
      continue;

    if (strcmp(label, "author") == 0 && !book->author)
      book->author = strdup(value);
    else if (strcmp(label, "title") == 0 && !book->title)
      book->title = strdup(value);
    else if (strcmp(label, "genre") == 0 && !book->genre)
      book->genre = strdup(value);
  }
  if (ferror(f))
    ERR("getline");

  free(line);
  if (fclose(f))
    ERR("fclose");
}

// `up` leads from the directory holding the link back to the program's directory
void make_link(const char* book_path, const char* up, const char* dir, const char* name) {
  char target[PATH_MAX], link_path[PATH_MAX];
  snprintf(target, sizeof target, "%s%s", up, book_path);
  snprintf(link_path, sizeof link_path, "%s/%.*s", dir, MAX_NAME, name);
  if (symlink(target, link_path) == -1)
    ERR("symlink");
}

void index_book(const char* path, const char* file_name) {
  make_link(path, "../../", "index/by-visible-title", file_name);

  book_t book = {0};
  get_book_metadata(path, &book);
  if (book.title) {
    make_link(path, "../../", "index/by-title", book.title);
    if (book.genre) {
      char genre_dir[PATH_MAX];
      snprintf(genre_dir, sizeof genre_dir, "index/by-genre/%.*s", MAX_NAME, book.genre);
      if (mkdir(genre_dir, 0755) == -1 && errno != EEXIST)
        ERR("mkdir");
      make_link(path, "../../../", genre_dir, book.title);
    }
  }
  free_book(&book);
}

void walk(const char* path) {
  DIR* dir = opendir(path);
  if (!dir)
    ERR("opendir");

  struct dirent* entry;
  for (errno = 0; (entry = readdir(dir)); errno = 0) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    char sub[PATH_MAX];
    snprintf(sub, sizeof sub, "%s/%s", path, entry->d_name);
    if (entry->d_type == DT_DIR)
      walk(sub);
    else if (entry->d_type == DT_REG)
      index_book(sub, entry->d_name);
  }
  if (errno)
    ERR("readdir");
  if (closedir(dir))
    ERR("closedir");
}

void create_index_dirs(void) {
  char const* dirs[] = {"index", "index/by-visible-title", "index/by-title", "index/by-genre"};
  for (size_t i = 0; i < sizeof dirs / sizeof *dirs; i++)
    if (mkdir(dirs[i], 0755) == -1)
      ERR("mkdir");
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

  create_index_dirs();
  walk("library");
  if (argc == 2)
    check_database(argv[1]);
  return EXIT_SUCCESS;
}
