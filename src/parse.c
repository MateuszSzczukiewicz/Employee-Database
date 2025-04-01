#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "parse.h"

void list_employees(dbheader_t *dbhdr, employee_t *employees) {
  for (int i = 0; i < dbhdr->count; i++) {
    printf("Employee %d\n", i);
    printf("\tName: %s\n", employees[i].name);
    printf("\tAddress: %s\n", employees[i].address);
    printf("\tHours: %d\n", employees[i].hours);
  }
}

int add_employee(dbheader_t *dbhdr, employee_t **employees_ptr,
                 char *addstring) {
  dbhdr->count++;
  employee_t *tmp = realloc(*employees_ptr, dbhdr->count * sizeof(employee_t));

  if (tmp == NULL && dbhdr->count > 0) {
    perror("Error: Failed to reallocate memory for new employee");
    dbhdr->count--;
    return STATUS_ERROR;
  }
  *employees_ptr = tmp;

  char *input_copy = strdup(addstring);
  if (!input_copy) {
    perror("Failed to duplicate addstring");
    dbhdr->count--;
    return STATUS_ERROR;
  }

  char *name = strtok(input_copy, ",");
  char *addr = strtok(NULL, ",");
  char *hours_str = strtok(NULL, ",");

  if (name == NULL || addr == NULL || hours_str == NULL) {
    fprintf(stderr, "Error: Invalid format for add string. Expected 'name, "
                    "address,hours'.\n");
    free(input_copy);
    dbhdr->count--;
    return STATUS_ERROR;
  }

  char *endptr;
  errno = 0;
  long hours_val = strtol(hours_str, &endptr, 10);

  if (errno != 0 || endptr == hours_str || *endptr != '\0' || hours_val < 0 ||
      hours_val > UINT_MAX) {
    if (errno != 0)
      perror("Error converting hours string (range)");
    else if (endptr == hours_str)
      fprintf(stderr, "Error: Hours string '%s' is not a valid number.\n",
              hours_str);
    else if (*endptr != '\0')
      fprintf(stderr, "Error: Trailing character after hours number: '%s'\n",
              endptr);
    else
      fprintf(stderr, "Error: Hours value %ld out of range for unsigned int.\n",
              hours_val);

    free(input_copy);
    dbhdr->count--;
    return STATUS_ERROR;
  }

  employee_t *employees = *employees_ptr;

  strncpy(employees[dbhdr->count - 1].name, name,
          sizeof(employees[dbhdr->count - 1].name));
  employees[dbhdr->count - 1]
      .name[sizeof(employees[dbhdr->count - 1].name) - 1] = '\0';

  strncpy(employees[dbhdr->count - 1].address, addr,
          sizeof(employees[dbhdr->count - 1].address));
  employees[dbhdr->count - 1]
      .address[sizeof(employees[dbhdr->count - 1].address) - 1] = '\0';

  employees[dbhdr->count - 1].hours = (unsigned int)hours_val;

  free(input_copy);
  return STATUS_SUCCESS;
}

int delete_employee(dbheader_t *dbhdr, employee_t **employees_ptr,
                    char *username) {
  int index = -1;

  for (int i = 0; i < dbhdr->count; i++) {
    if (strcmp((*employees_ptr)[i].name, username) == 0) {
      index = i;
      break;
    }
  }

  if (index == -1) {
    fprintf(stderr, "Error: Employee '%s' not found.\n", username);
    return STATUS_ERROR;
  }

  if (index < dbhdr->count - 1) {
    memmove(&(*employees_ptr)[index], &(*employees_ptr)[index + 1],
            ((dbhdr->count - index - 1) * sizeof(employee_t)));
  }

  dbhdr->count--;

  if (dbhdr->count > 0) {
    employee_t *tmp =
        realloc(*employees_ptr, dbhdr->count * sizeof(employee_t));

    if (tmp == NULL) {
      perror("Failed to reallocate memory after deletion");
      dbhdr->count++;
      return STATUS_ERROR;
    }
    *employees_ptr = tmp;
  } else {
    free(*employees_ptr);
    *employees_ptr = NULL;
  }

  return STATUS_SUCCESS;
}

int read_employees(int fd, dbheader_t *dbhdr, employee_t **employeesOut) {
  if (fd < 0) {
    printf("Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  int count = dbhdr->count;

  if (count == 0) {
    *employeesOut = NULL;
    return STATUS_SUCCESS;
  }

  employee_t *employees = calloc(count, sizeof(employee_t));
  if (employees == NULL) {
    perror("Failed to allocate memory for employees");
    return STATUS_ERROR;
  }

  ssize_t bytes_read = read(fd, employees, count * sizeof(employee_t));

  if (bytes_read == -1) {
    perror("Failed to read employees from file");
    free(employees);
    return STATUS_ERROR;
  }
  if (bytes_read != count * sizeof(employee_t)) {
    fprintf(stderr,
            "Error: Incomplete read for employees. Expected %zu, got %zd\n",
            count * sizeof(employee_t), bytes_read);
    free(employees);
    return STATUS_ERROR;
  }

  for (int i = 0; i < count; i++) {
    employees[i].hours = ntohl(employees[i].hours);
  }

  *employeesOut = employees;
  return STATUS_SUCCESS;
}

int output_file(int fd, dbheader_t *dbhdr, employee_t *employees) {
  if (fd < 0) {
    printf("Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  int realcount = dbhdr->count;

  dbheader_t header_to_write = *dbhdr;

  header_to_write.magic = htonl(dbhdr->magic);
  header_to_write.filesize =
      htonl(sizeof(dbheader_t) + (sizeof(employee_t) * realcount));
  header_to_write.count = htons(dbhdr->count);
  header_to_write.version = htons(dbhdr->version);

  if (lseek(fd, 0, SEEK_SET) == -1) {
    perror("Failed to seek to beginning of file");
    return STATUS_ERROR;
  };

  ssize_t bytes_written = write(fd, &header_to_write, sizeof(dbheader_t));
  if (bytes_written != sizeof(dbheader_t)) {
    fprintf(stderr,
            "Error: Incomplete write for header. Expected %zu, wrote %zd\n",
            sizeof(dbheader_t), bytes_written);
    return STATUS_ERROR;
  }

  for (int i = 0; i < realcount; i++) {
    employees[i].hours = htonl(employees[i].hours);
    bytes_written = write(fd, &employees[i], sizeof(employee_t));
    if (bytes_written == -1) {
      char error_msg[100];
      snprintf(error_msg, sizeof(error_msg),
               "Failed to write employee %d to file", i);
      perror(error_msg);
      return STATUS_ERROR;
    }
    if (bytes_written != sizeof(employee_t)) {
      fprintf(
          stderr,
          "Error: Incomplete write for employee %d. Expected %zu, wrote %zd\n",
          i, sizeof(employee_t), bytes_written);
      return STATUS_ERROR;
    }

    employees[i].hours = ntohl(employees[i].hours);
  }

  return STATUS_SUCCESS;
}

int validate_db_header(int fd, dbheader_t **headerOut) {
  if (fd < 0) {
    printf("Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  dbheader_t *header = calloc(1, sizeof(dbheader_t));
  if (header == NULL) {
    perror("Malloc failed create a db header\n");
    return STATUS_ERROR;
  }

  ssize_t bytes_read = read(fd, header, sizeof(dbheader_t));
  if (bytes_read == STATUS_ERROR) {
    perror("Failed to read header from file");
    free(header);
    return STATUS_ERROR;
  }
  if (bytes_read != sizeof(dbheader_t)) {
    fprintf(stderr,
            "Error: Incomplete read for header. Expected %zu, got %zd\n",
            sizeof(dbheader_t), bytes_read);
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

int create_db_header(int fd, dbheader_t **headerOut) {
  dbheader_t *header = calloc(1, sizeof(dbheader_t));
  if (header == NULL) {
    perror("Malloc failed to create db header");
    return STATUS_ERROR;
  }

  header->version = 0x1;
  header->count = 0;
  header->magic = HEADER_MAGIC;
  header->filesize = sizeof(dbheader_t);

  *headerOut = header;

  return STATUS_SUCCESS;
}
