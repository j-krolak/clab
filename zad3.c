#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define PACKAGE_FILE_SIZE 64

char const* prog_name;

void usage(void) {
  fprintf(stderr, "usage: %s -c <env> | -v <env> [-v <env>]... (-i <package>==<version> | -r <package>)\n",
          prog_name);
}

void fail(const char* msg) {
  fprintf(stderr, "sop-venv: %s\n", msg);
  usage();
  exit(EXIT_FAILURE);
}

void requirements_path(const char* env, char* buf, size_t size) {
  snprintf(buf, size, "%s/requirements", env);
}

void create_environment(const char* env) {
  if (mkdir(env, 0755) == -1) {
    if (errno == EEXIST)
      fail("the environment already exists");
    ERR("mkdir");
  }
  char path[PATH_MAX];
  requirements_path(env, path, sizeof path);
  int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
  if (fd == -1)
    ERR("open");
  if (close(fd))
    ERR("close");
}

void check_environment_exists(const char* env) {
  struct stat st;
  if (stat(env, &st) == -1) {
    if (errno == ENOENT)
      fail("the environment does not exist");
    ERR("stat");
  }
  if (!S_ISDIR(st.st_mode))
    fail("the environment does not exist");
}

int has_package(const char* req_path, const char* name) {
  FILE* f = fopen(req_path, "r");
  if (!f)
    ERR("fopen");
  char* line = NULL;
  size_t cap = 0;
  int found = 0;
  while (getline(&line, &cap, f) != -1) {
    size_t name_len = strcspn(line, " \n");
    if (name_len == strlen(name) && strncmp(line, name, name_len) == 0) {
      found = 1;
      break;
    }
  }
  free(line);
  if (fclose(f))
    ERR("fclose");
  return found;
}

void install_package(const char* env, const char* spec) {
  char const* sep = strstr(spec, "==");
  if (!sep || sep == spec || !sep[2])
    fail("invalid package specification");
  char name[NAME_MAX + 1];
  size_t name_len = (size_t)(sep - spec);
  if (name_len > NAME_MAX || memchr(spec, '/', name_len) || memchr(spec, ' ', name_len))
    fail("invalid package name");
  memcpy(name, spec, name_len);
  name[name_len] = '\0';
  char const* version = sep + 2;

  check_environment_exists(env);
  char req_path[PATH_MAX], pkg_path[PATH_MAX];
  requirements_path(env, req_path, sizeof req_path);
  snprintf(pkg_path, sizeof pkg_path, "%s/%s", env, name);

  if (has_package(req_path, name))
    fail("the package is already installed");

  int fd = open(pkg_path, O_WRONLY | O_CREAT | O_EXCL, 0444);
  if (fd == -1)
    ERR("open");
  char content[PACKAGE_FILE_SIZE];
  for (int i = 0; i < PACKAGE_FILE_SIZE; i++)
    content[i] = (char)('a' + rand() % 26);
  if (write(fd, content, sizeof content) != sizeof content)
    ERR("write");
  if (close(fd))
    ERR("close");

  FILE* req = fopen(req_path, "a");
  if (!req)
    ERR("fopen");
  fprintf(req, "%s %s\n", name, version);
  if (fclose(req))
    ERR("fclose");
}

void remove_package(const char* env, const char* name) {
  check_environment_exists(env);
  char req_path[PATH_MAX], tmp_path[PATH_MAX + 8], pkg_path[PATH_MAX];
  requirements_path(env, req_path, sizeof req_path);
  snprintf(tmp_path, sizeof tmp_path, "%s.tmp", req_path);
  snprintf(pkg_path, sizeof pkg_path, "%s/%s", env, name);

  FILE* in = fopen(req_path, "r");
  if (!in)
    ERR("fopen");
  FILE* out = fopen(tmp_path, "w");
  if (!out)
    ERR("fopen");

  char* line = NULL;
  size_t cap = 0;
  int found = 0;
  while (getline(&line, &cap, in) != -1) {
    size_t name_len = strcspn(line, " \n");
    if (name_len == strlen(name) && strncmp(line, name, name_len) == 0)
      found = 1;
    else if (fputs(line, out) == EOF)
      ERR("fputs");
  }
  free(line);
  if (fclose(in) || fclose(out))
    ERR("fclose");

  if (!found) {
    if (unlink(tmp_path) == -1)
      ERR("unlink");
    fail("the package is not installed");
  }
  if (rename(tmp_path, req_path) == -1)
    ERR("rename");
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

  int operations = create + (install != NULL) + (remove_name != NULL);
  if (operations != 1)
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
