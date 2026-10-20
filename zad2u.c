#include "utilities.c"

typedef struct {
  FILE* out;
  const char* ext;
  int depth;
} scan_t;

void usage(const char* prog) {
  fprintf(stderr, "Usage: %s -p path [-p path]... [-e extension] [-d depth] [-o]\n", prog);
  exit(EXIT_FAILURE);
}

void scan(const char* path, scan_t* s);

void print_file(const char* path, const char* name, const struct stat* st, void* ctx) {
  scan_t* s = ctx;
  if (S_ISREG(st->st_mode) && (!s->ext || has_extension(name, s->ext)))
    fprintf(s->out, "%s %ld\n", name, (long)st->st_size);
}

void enter_subdir(const char* path, const char* name, const struct stat* st, void* ctx) {
  scan_t* s = ctx;
  if (S_ISDIR(st->st_mode)) {
    scan_t sub = {s->out, s->ext, s->depth - 1};
    scan(path, &sub);
  }
}

// Najpierw pliki tego katalogu, potem podkatalogi
void scan(const char* path, scan_t* s) {
  fprintf(s->out, "path: %s\n", path);
  walk(path, 1, print_file, s);
  if (s->depth > 1)
    walk(path, 1, enter_subdir, s);
}

int main(int argc, char* argv[]) {
  char const** paths = calloc(argc, sizeof *paths);
  if (!paths)
    ERR("calloc");
  int path_count = 0;
  scan_t s = {stdout, NULL, 1};
  int use_file = 0;

  int c;
  while ((c = getopt(argc, argv, "p:e:d:o")) != -1) {
    switch (c) {
    case 'p':
      paths[path_count++] = optarg;
      break;
    case 'e':
      s.ext = optarg;
      break;
    case 'd': {
      char* end;
      long value = strtol(optarg, &end, 10);
      if (*end || value < 1 || value > INT_MAX)
        usage(argv[0]);
      s.depth = (int)value;
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

  char const* out_path = use_file ? getenv("L1_OUTPUTFILE") : NULL;
  if (out_path && !(s.out = fopen(out_path, "w")))
    ERR("fopen");

  for (int i = 0; i < path_count; i++)
    scan(paths[i], &s);

  if (out_path && fclose(s.out))
    ERR("fclose");
  free(paths);
  return EXIT_SUCCESS;
}
