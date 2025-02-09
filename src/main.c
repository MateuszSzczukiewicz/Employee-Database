#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>

#include "common.h"
#include "file.h"
#include "parse.h"

void print_usage(char *argv[0]) {
  printf("Usage: %s -n -f <database file>\n", argv[0]);
  printf("\t -n  -  create new database file\n");
  printf("\t -f  -  (required) path to database file\n");
}

int main(int argc, char *argv[]) {
  char *filepath = NULL;
  char *portarg = NULL;
  unsigned short port = 0;
  bool newfile = false;
  bool list = false;
  int c;

  while ((c = getopt(argc, argv, "nf:")) != -1) {
    switch (c) {
    case 'n':
      newfile = true;
      break;
    case 'f':
      filepath = optarg;
      break;
    case '?':
      printf("Unknown option -%c\n", c);
      break;
    case 'p':
      portarg = optarg;
      break;
    case 'l':
      list = true;
      break;
    default:
      return -1;
    }

    if (filepath == NULL) {
      printf("Filepath is a required argument\n");
      print_usage(argv);

      return 0;
    }

    printf("Newfile: %d\n", newfile);
    printf("Filepath: %s\n", filepath);

    return 0;
  }
}
