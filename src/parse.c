#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "parse.h"

void list_employees(struct dbheader_t *dbhdr, struct employee_t *employees) {
  int i = 0;
  for (; i < dbhdr->count; i++) {
    printf("Employee %d\n", i);
    printf("\tName: %s\n", employees[i].name);
    printf("\tAddress: %s\n", employees[i].address);
    printf("\tHours: %d\n", employees[i].hours);
  }
}

int add_employee(struct dbheader_t *dbhdr, struct employee_t *employees,
                 char *addstring) {
  printf("%s\n", addstring);

  char *name = strtok(addstring, ",");

  char *addr = strtok(NULL, ",");

  char *hours = strtok(NULL, ",");

  printf("%s %s %s\n", name, addr, hours);

  strncpy(employees[dbhdr->count - 1].name, name,
          sizeof(employees[dbhdr->count - 1].name));
  employees[dbhdr->count - 1]
      .name[sizeof(employees[dbhdr->count - 1].name) - 1] = '\0';

  strncpy(employees[dbhdr->count - 1].address, addr,
          sizeof(employees[dbhdr->count - 1].address));
  employees[dbhdr->count - 1]
      .address[sizeof(employees[dbhdr->count - 1].address) - 1] = '\0';

  employees[dbhdr->count - 1].hours = atoi(hours);

  return STATUS_SUCCESS;
}

int read_employees(int fd, struct dbheader_t *dbhdr,
                   struct employee_t **employeesOut) {
  if (fd < 0) {
    printf("Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  int count = dbhdr->count;

  if (count == 0) {
    *employeesOut = NULL;
    return STATUS_SUCCESS;
  }

  struct employee_t *employees = calloc(count, sizeof(struct employee_t));
  if (employees == NULL) {
    perror("Failed to allocate memory for employees");
    return STATUS_ERROR;
  }

  ssize_t bytes_read = read(fd, employees, count * sizeof(struct employee_t));

  if (bytes_read == -1) {
    perror("Failed to read employees from file");
    free(employees);
    return STATUS_ERROR;
  }
  if (bytes_read != count * sizeof(struct employee_t)) {
    fprintf(stderr,
            "Error: Incomplete read for employees. Expected %zu, got %zd\n",
            count * sizeof(struct employee_t), bytes_read);
    free(employees);
    return STATUS_ERROR;
  }

  int i = 0;
  for (; i < count; i++) {
    employees[i].hours = ntohl(employees[i].hours);
  }

  *employeesOut = employees;
  return STATUS_SUCCESS;
}

int output_file(int fd, struct dbheader_t *dbhdr,
                struct employee_t *employees) {
  if (fd < 0) {
    printf("Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  int realcount = dbhdr->count;

  dbhdr->magic = htonl(dbhdr->magic);
  dbhdr->filesize = htonl(sizeof(struct dbheader_t) +
                          (sizeof(struct employee_t) * realcount));
  dbhdr->count = htonl(dbhdr->count);
  dbhdr->version = htonl(dbhdr->version);

  if (lseek(fd, 0, SEEK_SET) == -1) {
    perror("Failed to seek to beginning of file");
    return STATUS_ERROR;
  };

  ssize_t bytes_written = write(fd, dbhdr, sizeof(struct dbheader_t));
  if (bytes_written != sizeof(struct dbheader_t)) {
    fprintf(stderr,
            "Error: Incomplete write for header. Expected %zu, wrote %zd\n",
            sizeof(struct dbheader_t), bytes_written);
    return STATUS_ERROR;
  }

  int i = 0;
  for (; i < realcount; i++) {
    employees[i].hours = htonl(employees[i].hours);
    bytes_written = write(fd, &employees[i], sizeof(struct employee_t));
    if (bytes_written == -1) {
      char error_msg[100];
      snprintf(error_msg, sizeof(error_msg),
               "Failed to write employee %d to file", i);
      perror(error_msg);
      return STATUS_ERROR;
    }
    if (bytes_written != sizeof(struct employee_t)) {
      fprintf(
          stderr,
          "Error: Incomplete write for employee %d. Expected %zu, wrote %zd\n",
          i, sizeof(struct employee_t), bytes_written);
      return STATUS_ERROR;
    }

    employees[i].hours = ntohl(employees[i].hours);
  }

  return STATUS_SUCCESS;
}

int validate_db_header(int fd, struct dbheader_t **headerOut) {
  if (fd < 0) {
    printf("Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
  if (header == NULL) {
    printf("Malloc failed create a db header\n");
    return STATUS_ERROR;
  }

  ssize_t bytes_read = read(fd, header, sizeof(struct dbheader_t));
  if (bytes_read == STATUS_ERROR) {
    perror("Failed to read header from file");
    free(header);
    return STATUS_ERROR;
  }
  if (bytes_read != sizeof(struct dbheader_t)) {
    fprintf(stderr,
            "Error: Incomplete read for header. Expected %zu, got %zd\n",
            sizeof(struct dbheader_t), bytes_read);
    free(header);
    return STATUS_ERROR;
  }

  header->version = ntohs(header->version);
  header->count = ntohs(header->count);
  header->magic = ntohl(header->magic);
  header->filesize = ntohl(header->filesize);

  if (header->magic != HEADER_MAGIC) {
    fprintf(stderr, "Error: Invalid magic number. Expected 0x%X, got 0x%X\n",
            HEADER_MAGIC, header->magic);
    free(header);
    return STATUS_ERROR;
  }

  if (header->version != 1) {
    fprintf(stderr, "Error: Unsupported database version. Expected 1, got %d\n",
            header->version);
    free(header);
    return STATUS_ERROR;
  }

  struct stat dbstat = {0};
  if (fstat(fd, &dbstat) == -1) {
    perror("Failed to get file status");
    free(header);
    return STATUS_ERROR;
  };

  if (header->filesize != dbstat.st_size) {
    fprintf(stderr,
            "Error: Corrupted database. Header filesize (%u) does not match "
            "actual file size (%ld).\n",
            header->filesize, dbstat.st_size);
    free(header);
    return STATUS_ERROR;
  }

  *headerOut = header;

  return STATUS_SUCCESS;
}

int create_db_header(int fd, struct dbheader_t **headerOut) {
  struct dbheader_t *header = calloc(1, sizeof(struct dbheader_t));
  if (header == NULL) {
    perror("Malloc failed to create db header");
    return STATUS_ERROR;
  }

  header->version = 0x1;
  header->count = 0;
  header->magic = HEADER_MAGIC;
  header->filesize = sizeof(struct dbheader_t);

  *headerOut = header;

  return STATUS_SUCCESS;
}
