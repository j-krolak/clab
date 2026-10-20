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
  fprintf(stderr, "Usage: %s -p path [-p path]... [-e extension] [-d depth] [-o]\n", prog);
  exit(EXIT_FAILURE);
}

int has_extension(const char* name, const char* ext) {
  if (!ext)
    return 1;
  size_t name_len = strlen(name), ext_len = strlen(ext);
  return name_len > ext_len + 1 && name[name_len - ext_len - 1] == '.' &&
         strcmp(name + name_len - ext_len, ext) == 0;
}

void scan(const char* path, int depth, const char* ext, FILE* out) {
  DIR* dir = opendir(path);
  if (!dir)
    ERR("opendir");

  fprintf(out, "path: %s\n", path);
  struct dirent* entry;
  for (errno = 0; (entry = readdir(dir)); errno = 0) {
    char full[PATH_MAX];
    snprintf(full, sizeof full, "%s/%s", path, entry->d_name);
    struct stat st;
    if (lstat(full, &st) == -1)
      ERR("lstat");
    if (S_ISREG(st.st_mode) && has_extension(entry->d_name, ext))
      fprintf(out, "%s %ld\n", entry->d_name, (long)st.st_size);
  }
  if (errno)
    ERR("readdir");

  if (depth > 1) {
    rewinddir(dir);
    for (errno = 0; (entry = readdir(dir)); errno = 0) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      char full[PATH_MAX];
      snprintf(full, sizeof full, "%s/%s", path, entry->d_name);
      struct stat st;
      if (lstat(full, &st) == -1)
        ERR("lstat");
      if (S_ISDIR(st.st_mode))
        scan(full, depth - 1, ext, out);
    }
    if (errno)
      ERR("readdir");
  }

  if (closedir(dir))
    ERR("closedir");
}

int main(int argc, char* argv[]) {
  char const** paths = calloc(argc, sizeof *paths);
  if (!paths)
    ERR("calloc");
  int path_count = 0;
  char const* ext = NULL;
  int depth = 1;
  int use_file = 0;

  int c;
  while ((c = getopt(argc, argv, "p:e:d:o")) != -1) {
    switch (c) {
    case 'p':
      paths[path_count++] = optarg;
      break;
    case 'e':
      ext = optarg;
      break;
    case 'd': {
      char* end;
      long value = strtol(optarg, &end, 10);
      if (*end || value < 1 || value > INT_MAX)
        usage(argv[0]);
      depth = (int)value;
      break;
    }
    case 'o':
      use_file = 1;
      break;
    default:
      usage(argv[0]);
    }
  }
  if (optind < argc || path_count == 0)
    usage(argv[0]);

  FILE* out = stdout;
  char const* out_path = use_file ? getenv("L1_OUTPUTFILE") : NULL;
  if (out_path && !(out = fopen(out_path, "w")))
    ERR("fopen");

  for (int i = 0; i < path_count; i++)
    scan(paths[i], depth, ext, out);

  if (out_path && fclose(out))
    ERR("fclose");
  free(paths);
  return EXIT_SUCCESS;
}
