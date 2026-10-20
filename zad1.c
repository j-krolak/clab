#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(const char* prog) {
  fprintf(stderr, "Usage: %s [-p path]... [-o output_file]\n", prog);
  exit(EXIT_FAILURE);
}

void list_directory(const char* path, FILE* out) {
  DIR* dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "%s: %s\n", path, strerror(errno));
    return;
  }

  fprintf(out, "SCIEZKA:\n%s\nLISTA PLIKÓW:\n", path);
  struct dirent* entry;
  for (errno = 0; (entry = readdir(dir)); errno = 0) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
      continue;
    char full[PATH_MAX];
    snprintf(full, sizeof full, "%s/%s", path, entry->d_name);
    struct stat st;
    if (lstat(full, &st) == -1)
      ERR("lstat");
    fprintf(out, "%s %ld\n", entry->d_name, (long)st.st_size);
  }
  if (errno)
    ERR("readdir");
  if (closedir(dir))
    ERR("closedir");
}

int main(int argc, char* argv[]) {
  char const** paths = calloc(argc, sizeof *paths);
  if (!paths)
    ERR("calloc");
  int path_count = 0;
  char const* out_path = NULL;

  int c;
  while ((c = getopt(argc, argv, "p:o:")) != -1) {
    switch (c) {
    case 'p':
      paths[path_count++] = optarg;
      break;
    case 'o':
      if (out_path)
        usage(argv[0]);
      out_path = optarg;
      break;
    default:
      usage(argv[0]);
    }
  }
  if (optind < argc)
    usage(argv[0]);

  FILE* out = stdout;
  if (out_path && !(out = fopen(out_path, "w")))
    ERR("fopen");

  if (path_count == 0)
    list_directory(".", out);
  for (int i = 0; i < path_count; i++)
    list_directory(paths[i], out);

  if (out_path && fclose(out))
    ERR("fclose");
  free(paths);
  return EXIT_SUCCESS;
}
