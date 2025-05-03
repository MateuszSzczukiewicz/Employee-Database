#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "file.h"

int create_db_file(char *filename) {
  int fd = open(filename, O_RDWR | O_CREAT | O_EXCL, 0644);
  if (fd == STATUS_ERROR) {
    perror("create_db_file failed");
    return STATUS_ERROR;
  }

  return fd;
}

int open_db_file(char *filename) {
  int fd = open(filename, O_RDWR);
  if (fd == STATUS_ERROR) {
    perror("open_db_file failed");
    return STATUS_ERROR;
  }

  return fd;
}
