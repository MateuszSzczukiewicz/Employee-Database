#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[]) {
  printf("Usage: %s -n -f <database file>\n", argv[0]);
  printf("\t -n  -  create new database file\n");
  printf("\t -f  -  (required) path to database file\n");
  return;
}

int main(int argc, char *argv[]) {
  char *filepath = NULL;
  char *portarg = NULL;
  char *addstring = NULL;
  unsigned short port = 0;
  bool newfile = false;
  bool list = false;
  int c;

  int dbfd = -1;
  struct dbheader_t *dbhdr = NULL;
  struct employee_t *employees = NULL;

  while ((c = getopt(argc, argv, "nf:a:l")) != -1) {
    switch (c) {
    case 'n':
      newfile = true;
      break;
    case 'f':
      filepath = optarg;
      break;
    case 'p':
      portarg = optarg;
      break;
    case 'a':
      addstring = optarg;
      break;
    case 'l':
      list = true;
      break;
    case '?':
      printf("Unknown option -%c\n", c);
      break;
    default:
      return STATUS_ERROR;
    }
  }

  if (filepath == NULL) {
    printf("Filepath is a required argument\n");
    print_usage(argv);

    return STATUS_SUCCESS;
  }

  if (newfile) {
    dbfd = create_db_file(filepath);
    if (dbfd == STATUS_ERROR) {
      printf("Unable to create database file\n");
      return STATUS_ERROR;
    }

    if (create_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      printf("Failed to open database header\n");
      return STATUS_ERROR;
    }
  } else {
    dbfd = open_db_file(filepath);
    if (dbfd == STATUS_ERROR) {
      printf("Unable to open database file\n");
      return STATUS_ERROR;
    }

    if (validate_db_header(dbfd, &dbhdr) == STATUS_ERROR) {
      printf("Failed to validate database header\n");
      return STATUS_ERROR;
    }
  }

  if (read_employees(dbfd, dbhdr, &employees) != STATUS_SUCCESS) {
    printf("Failed to read employees");
    return STATUS_SUCCESS;
  }

  if (addstring) {
    dbhdr->count++;
    employees = realloc(employees, dbhdr->count * (sizeof(struct employee_t)));
    add_employee(dbhdr, employees, addstring);
  }

  if (list) {
    list_employees(dbhdr, employees);
  }

  output_file(dbfd, dbhdr, employees);

  return STATUS_SUCCESS;
}
