#include "utilities.c"

void usage(const char* prog) {
  fprintf(stderr, "Usage: %s [-p path]... [-o output_file]\n", prog);
  exit(EXIT_FAILURE);
}

void print_entry(const char* path, const char* name, const struct stat* st, void* ctx) {
  fprintf(ctx, "%s %ld\n", name, (long)st->st_size);
}

void list_directory(const char* path, FILE* out) {
  DIR* dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "%s: %s\n", path, strerror(errno));
    return;
  }
  if (closedir(dir) == -1)
    ERR("closedir");

  fprintf(out, "SCIEZKA:\n%s\nLISTA PLIKÓW:\n", path);
  walk(path, 1, print_entry, out);
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
