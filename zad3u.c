#include "utilities.c"
#include <time.h>

#define PACKAGE_FILE_SIZE 64

char const* prog_name;

void usage(void) {
  fprintf(stderr,
          "usage: %s -c <env> | -v <env> [-v <env>]... (-i <package>==<version> | -r <package>)\n",
          prog_name);
}

void fail(const char* msg) {
  fprintf(stderr, "sop-venv: %s\n", msg);
  usage();
  exit(EXIT_FAILURE);
}

void create_environment(const char* env) {
  if (mkdir(env, 0755) == -1) {
    if (errno == EEXIST)
      fail("the environment already exists");
    ERR("mkdir");
  }
  char req_path[PATH_MAX];
  snprintf(req_path, sizeof req_path, "%s/requirements", env);
  int fd = open(req_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd == -1)
    ERR("open");
  if (close(fd) == -1)
    ERR("close");
}

void check_environment_exists(const char* env) {
  struct stat st;
  if (stat(env, &st) == -1) {
    if (errno == ENOENT || errno == ENOTDIR)
      fail("the environment does not exist");
    ERR("stat");
  }
  if (!S_ISDIR(st.st_mode))
    fail("the environment does not exist");
}

void install_package(const char* env, const char* spec) {
  char const* sep = strstr(spec, "==");
  if (!sep || sep == spec || !sep[2])
    fail("invalid package specification");
  size_t name_len = (size_t)(sep - spec);
  if (name_len > NAME_MAX || memchr(spec, '/', name_len) || memchr(spec, ' ', name_len))
    fail("invalid package name");
  char name[NAME_MAX + 1];
  memcpy(name, spec, name_len);
  name[name_len] = '\0';

  check_environment_exists(env);
  char req_path[PATH_MAX], pkg_path[PATH_MAX], entry[PATH_MAX];
  snprintf(req_path, sizeof req_path, "%s/requirements", env);
  snprintf(pkg_path, sizeof pkg_path, "%s/%s", env, name);

  if (has_line(req_path, name))
    fail("the package is already installed");

  int fd = open(pkg_path, O_WRONLY | O_CREAT | O_EXCL, 0444);
  if (fd == -1)
    ERR("open");
  char content[PACKAGE_FILE_SIZE];
  for (int i = 0; i < PACKAGE_FILE_SIZE; i++)
    content[i] = (char)('a' + rand() % 26);
  write_full(fd, content, sizeof content);
  if (close(fd) == -1)
    ERR("close");

  snprintf(entry, sizeof entry, "%s %s", name, sep + 2);
  append_line(req_path, entry);
}

void remove_package(const char* env, const char* name) {
  check_environment_exists(env);
  char req_path[PATH_MAX], pkg_path[PATH_MAX];
  snprintf(req_path, sizeof req_path, "%s/requirements", env);
  snprintf(pkg_path, sizeof pkg_path, "%s/%s", env, name);

  if (!remove_line(req_path, name))
    fail("the package is not installed");
  if (unlink(pkg_path) == -1 && errno != ENOENT)
    ERR("unlink");
}

int main(int argc, char* argv[]) {
  prog_name = argv[0];
  srand((unsigned)time(NULL));

  char const** envs = calloc(argc, sizeof *envs);
  if (!envs)
    ERR("calloc");
  int env_count = 0;
  int create = 0;
  char const* install = NULL;
  char const* remove_name = NULL;

  int c;
  while ((c = getopt(argc, argv, "cv:i:r:")) != -1) {
    switch (c) {
    case 'c':
      create = 1;
      break;
    case 'v':
      envs[env_count++] = optarg;
      break;
    case 'i':
      if (install)
        fail("only one -i is allowed");
      install = optarg;
      break;
    case 'r':
      if (remove_name)
        fail("only one -r is allowed");
      remove_name = optarg;
      break;
    default:
      usage();
      return EXIT_FAILURE;
    }
  }

  if (create + (install != NULL) + (remove_name != NULL) != 1)
    fail("exactly one of -c, -i, -r is required");

  if (create) {
    // accepts both `-c name` and `-c -v name`
    if (env_count == 0 && optind == argc - 1)
      envs[env_count++] = argv[optind++];
    if (env_count != 1 || optind != argc)
      fail("-c takes exactly one environment name");
    create_environment(envs[0]);
  } else {
    if (env_count == 0 || optind != argc)
      fail("at least one -v <env> is required");
    for (int i = 0; i < env_count; i++) {
      if (install)
        install_package(envs[i], install);
      else
        remove_package(envs[i], remove_name);
    }
  }

  free(envs);
  return EXIT_SUCCESS;
}
