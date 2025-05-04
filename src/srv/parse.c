#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "parse.h"

static int find_employee_index(dbheader_t *dbhdr, employee_t *employees,
                               const char *name) {
  if (!employees || !name) {
    return STATUS_ERROR;
  }
  for (int i = 0; i < dbhdr->count; i++) {
    if (strcmp(employees[i].name, name) == 0) {
      return i;
    }
  }
  return STATUS_ERROR;
}

static int parse_and_validate_hours(const char *hours_str,
                                    unsigned int *out_hours) {
  if (!hours_str || !out_hours) {
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
    return STATUS_ERROR;
  }

  *out_hours = (unsigned int)hours_val;
  return STATUS_SUCCESS;
}

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

  unsigned int parsed_hours;
  if (parse_and_validate_hours(hours_str, &parsed_hours) != STATUS_SUCCESS) {
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

  employees[dbhdr->count - 1].hours = (unsigned int)parsed_hours;

  free(input_copy);
  return STATUS_SUCCESS;
}

int update_working_hours(dbheader_t *dbhdr, employee_t *employees,
                         char *updatestring) {

  char *input_copy = strdup(updatestring);
  if (!input_copy) {
    perror("Failed to duplicate updatestring");
    return STATUS_ERROR;
  }

  char *name = strtok(input_copy, ",");
  char *hours_str = strtok(NULL, ",");

  if (name == NULL || hours_str == NULL) {
    fprintf(
        stderr,
        "Error: Invalid format for update string. Expected 'name, hours'.\n");
    free(input_copy);
    return STATUS_ERROR;
  }

  unsigned int parsed_hours;
  if (parse_and_validate_hours(hours_str, &parsed_hours) != STATUS_SUCCESS) {
    free(input_copy);
    return STATUS_ERROR;
  }

  int index = find_employee_index(dbhdr, employees, name);

  if (index == -1) {
    fprintf(stderr, "Error: Employee '%s' not found.\n", name);
    free(input_copy);
    return STATUS_ERROR;
  }

  employees[index].hours = (unsigned int)parsed_hours;

  free(input_copy);
  return STATUS_SUCCESS;
}

int delete_employee(dbheader_t *dbhdr, employee_t **employees_ptr,
                    char *username) {
  if (dbhdr->count == 0 || *employees_ptr == NULL) {
    fprintf(stderr, "Error: Database is empty, cannot delete.\n");
    return STATUS_ERROR;
  }

  employee_t *employees = *employees_ptr;

  int index = find_employee_index(dbhdr, employees, username);

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
    fprintf(stderr, "Got a bad FD from the user\n");
    return STATUS_ERROR;
  }

  int realcount = dbhdr->count;

  off_t final_filesize =
      sizeof(dbheader_t) + ((off_t)realcount * sizeof(employee_t));

  dbheader_t header_to_write = *dbhdr;

  header_to_write.magic = htonl(dbhdr->magic);
  header_to_write.filesize = htonl((u_int32_t)final_filesize);
  header_to_write.count = htons(dbhdr->count);
  header_to_write.version = htons(dbhdr->version);

  if (lseek(fd, 0, SEEK_SET) == -1) {
    perror("Failed to seek to beginning of file");
    return STATUS_ERROR;
  };

  ssize_t bytes_written = write(fd, &header_to_write, sizeof(dbheader_t));
  if (bytes_written == STATUS_ERROR) {
    perror("Failed to write header");
    return STATUS_ERROR;
  }

  if (bytes_written != sizeof(dbheader_t)) {
    fprintf(stderr,
            "Error: Incomplete write for header. Expected %zu, wrote %zd\n",
            sizeof(dbheader_t), bytes_written);
    return STATUS_ERROR;
  }

  for (int i = 0; i < realcount; i++) {
    uint32_t original_hours = employees[i].hours;
    uint32_t network_hours = htonl(original_hours);

    employees[i].hours = network_hours;

    bytes_written = write(fd, &employees[i], sizeof(employee_t));

    employees[i].hours = original_hours;

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
  }

  if (ftruncate(fd, final_filesize) == -1) {
    perror("Failed to ftruncate file to final size");
    return STATUS_ERROR;
  }

  header_to_write.magic = ntohl(dbhdr->magic);
  header_to_write.filesize = ntohl((u_int32_t)final_filesize);
  header_to_write.count = ntohs(dbhdr->count);
  header_to_write.version = ntohs(dbhdr->version);

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
