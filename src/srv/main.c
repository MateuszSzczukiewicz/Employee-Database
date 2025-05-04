#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <bits/getopt_core.h>
#include <getopt.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "file.h"
#include "parse.h"
#include "srvpoll.h"

#define MAX_CLIENTS 256

clientstate_t clientStates[MAX_CLIENTS] = {0};

void print_usage(char *argv[]) {
  fprintf(stderr,
          "Usage: %s -f <database file> [-n] [-a <name,addr,hours>] [-l]\n",
          argv[0]);
  fprintf(stderr, "\t-f <database file>  (required) Path to database file\n");
  fprintf(stderr,
          "\t-n                 Create a new database file (must not exist)\n");
  fprintf(stderr,
          "\t-a <data>          Add employee record (name,address,hours)\n");
  fprintf(stderr, "\t-u <data>          Update employee hours (name,hours)\n");
  fprintf(stderr, "\t-d <name>          Remove employee record (name)\n");
  fprintf(stderr, "\t-l                 List employee records\n");
}

int poll_loop(unsigned short port, dbheader_t *dbhdr, employee_t *employees) {
  int listen_fd, conn_fd, freeSlot;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);

  struct pollfd fds[MAX_CLIENTS + 1];
  int nfds = 1;
  int opt = 1;

  init_clients(clientStates);

  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt");
    close(listen_fd);
    exit(EXIT_FAILURE);
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("bind");
    exit(EXIT_FAILURE);
  }

  if (listen(listen_fd, 10) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", PORT);

  memset(fds, 0, sizeof(fds));
  fds[0].fd = listen_fd;
  fds[0].events = POLLIN;
  nfds = 1;

  while (1) {
    int ii = 1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (clientStates[i].fd != -1) {
        fds[ii].fd = clientStates[i].fd;
        fds[ii].events = POLLIN;
        ii++;
      }
    }

    int n_events = poll(fds, nfds, -1);
    if (n_events == -1) {
      perror("poll");
      exit(EXIT_FAILURE);
    }

    if (fds[0].revents & POLLIN) {
      if ((conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr,
                            &client_len)) == -1) {
        perror("accept");
        continue;
      }

      printf("New connection from %s:%d\n", inet_ntoa(client_addr.sin_addr),
             ntohs(client_addr.sin_port));

      freeSlot = find_free_slot(clientStates);
      if (freeSlot == -1) {
        printf("Server full: closing new connection\n");
        close(conn_fd);
      } else {
        clientStates[freeSlot].fd = conn_fd;
        clientStates[freeSlot].state = STATE_CONNECTED;
        nfds++;
        printf("Slot %d has fd %d\n", freeSlot, clientStates[freeSlot].fd);
      }

      n_events--;
    }

    for (int i = 1; i <= nfds && n_events > 0; i++) {
      if (fds[i].revents & POLLIN) {
        n_events--;

        int fd = fds[i].fd;
        int slot = find_slot_by_fd(clientStates, fd);
        ssize_t bytes_read = read(fd, &clientStates[slot].buffer,
                                  sizeof(clientStates[slot].buffer) - 1);
        if (bytes_read <= 0) {
          close(fd);
          if (slot == -1) {
            printf("Tried to close fd that doesnt exist?\n");
          } else {
            clientStates[slot].fd = -1;
            clientStates[slot].state = STATE_DISCONNECTED;
            printf("Client disconnected or error\n");
            nfds--;
          }
        } else {
          handle_client_fsm(dbhdr, &employees, &clientStates[slot], fd);
        }
      }
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  char *filepath = NULL;
  char *portarg = NULL;
  unsigned short port = 0;
  bool newfile = false;
  bool list = false;
  int c;
  int ret = EXIT_FAILURE;

  int dbfd = -1;
  dbheader_t *dbhdr = NULL;
  employee_t *employees = NULL;

  while ((c = getopt(argc, argv, "nf:p:")) != -1) {
    switch (c) {
    case 'n':
      newfile = true;
      break;
    case 'f':
      filepath = optarg;
      break;
    case 'p':
      portarg = optarg;
      port = atoi(portarg);
      if (port == 0) {
        printf("Bad port: %s\n", portarg);
      }
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

  if (port == 0) {
    fprintf(stderr, "Error: Port not set\n");
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

  if (poll_loop(port, dbhdr, employees) != STATUS_SUCCESS) {
    goto cleanup;
  };

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
