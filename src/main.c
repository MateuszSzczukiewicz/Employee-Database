#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
  fprintf(stderr,
          "Usage: %s -f <database file> [-n] [-a <name,addr,hours>] [-l]\n",
          argv[0]);
  fprintf(stderr, "\t-f <database file>  (required) Path to database file\n");
  fprintf(stderr,
          "\t-n                 Create a new database file (must not exist)\n");
  fprintf(stderr,
          "\t-a <data>          Add employee record (name,address,hours)\n");
  fprintf(stderr, "\t-l                 List employee records\n");
}

int main(int argc, char *argv[]) {
  char *filepath = NULL;
  char *addstring = NULL;
  bool newfile = false;
  bool list = false;
  int c;

  int dbfd = -1;
  struct dbheader_t *dbhdr = NULL;
  struct employee_t *employees = NULL;
  int ret = EXIT_FAILURE;

  while ((c = getopt(argc, argv, "nf:a:l")) != -1) {
    switch (c) {
    case 'n':
      newfile = true;
      break;
    case 'f':
      filepath = optarg;
      break;
    case 'a':
      addstring = optarg;
      break;
    case 'l':
      list = true;
      break;
    case '?':
      print_usage(argv);
      goto cleanup;
    default:
      print_usage(argv);
      goto cleanup;
    }
  }

  if (filepath == NULL) {
    fprintf(stderr, "Error: Filepath (-f) is a required argument\n");
    print_usage(argv);

    goto cleanup;
  }

  if (newfile) {
    dbfd = create_db_file(filepath);
    if (dbfd == STATUS_ERROR) {
      goto cleanup;
    }

    if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      goto cleanup;
    }
  } else {
    dbfd = open_db_file(filepath);
    if (dbfd == STATUS_ERROR) {
      goto cleanup;
    }

    if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      goto cleanup;
    }
  }

  if (read_employees(dbfd, dbhdr, &employees) != STATUS_SUCCESS) {
    goto cleanup;
  }

  if (addstring) {
    dbhdr->count++;
    struct employee_t *tmp_employees =
        realloc(employees, dbhdr->count * sizeof(struct employee_t));

    if (tmp_employees == NULL && dbhdr->count > 0) {
      perror("Error: Failed to reallocate memory for new employee");
      dbhdr->count--;
      goto cleanup;
    }
    employees = tmp_employees;

    if (add_employee(dbhdr, employees, addstring) != STATUS_SUCCESS) {
      fprintf(stderr, "Error: Failed to parse or add employee data.\n");
      goto cleanup;
    };
  }

  if (list) {
    list_employees(dbhdr, employees);
  }

  if (output_file(dbfd, dbhdr, employees) != STATUS_SUCCESS) {
    goto cleanup;
  };

  ret = EXIT_SUCCESS;

cleanup:
  if (dbhdr != NULL) {
    free(dbhdr);
    dbhdr = NULL;
  }
  if (employees != NULL) {
    free(employees);
    employees = NULL;
  }

  if (dbfd >= 0) {
    if (close(dbfd) == -1) {
      perror("Error closing file descriptor");
      ret = EXIT_FAILURE;
    }
    dbfd = -1;
  }

  return ret;
}
