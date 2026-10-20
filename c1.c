#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source)                                                                                \
  (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void scan_d(char* path, FILE* stream) {
  DIR* dir = opendir(path);
  if (!dir)
    ERR("opendir");
  struct dirent* ep = NULL;
  struct stat st;
  errno = 0;
  char current_path[PATH_MAX];
  while ((ep = readdir(dir)) != NULL) {

    if (!snprintf(current_path, sizeof current_path, "%s/%s", path, ep->d_name))
      ERR("snprintf");

    if (lstat(current_path, &st) != 0)
      ERR("lstat");
    if (S_ISDIR(st.st_mode))
      fprintf(stream, "d ");
    if (S_ISREG(st.st_mode))
      fprintf(stream, "f ");

    fprintf(stream, "%s\n", ep->d_name);
    errno = 0;
  }
  if (errno)
    ERR("readdir");
  if (closedir(dir))
    ERR("closedir");
}

void create_file(char* path, int size, int perm) {
  umask(000);
  int fd = open(path, O_CREAT | O_EXCL | O_WRONLY, perm);
  if (fd == -1)
    ERR("open");
  FILE* f = fdopen(fd, "w");
  char data[1];
  for (int i = 0; i < size; i++) {
    if (i < size / 10)
      data[0] = (char)(rand() % 25);
    else
      data[0] = 0;
    fwrite(data, 1, 1, f);
  }
  fclose(f);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Usage: main -f <FILE_PATH>\n");
    return 0;
  }
  char c;
  char* path = NULL;
  char* create_path = NULL;
  int size = -1;
  int perm = -1;
  while ((c = getopt(argc, argv, ":f:n:p:s:")) != -1) {
    switch (c) {
    case 'f':
      path = optarg;
      break;
    case 'n':
      create_path = optarg;
      break;
    case 's':
      size = atoi(optarg);
      if (size < 0) {
        fprintf(stderr, "size have to be >= 0");
        exit(EXIT_FAILURE);
      }

      break;
    case 'p':
      errno = 0;
      perm = strtol(optarg, NULL, 8);
      if (errno != 0)
        ERR("strtol");
      if (perm < 0 || perm > 511) {
        fprintf(stderr, "permission have to be octal 000-777");
        exit(EXIT_FAILURE);
      }

      break;
    }
  }

  if (path != NULL)
    scan_d(path, stdout);
  if (create_path && size != -1 && perm != -1)
    create_file(create_path, size, perm);
  return 0;
}