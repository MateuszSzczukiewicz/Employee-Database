#ifndef SRVPOLL_H
#define SRVPOLL_H

#include "parse.h"
#include <stddef.h>

#define MAX_CLIENTS 256
#define PORT 8080
#define BUFF_SIZE 4096

typedef enum {
  STATE_NEW,
  STATE_HELLO,
  STATE_MSG,
  STATE_CLOSING,
  STATE_DISCONNECTED,
} state_e;

typedef struct {
  int fd;
  state_e state;
  unsigned char buffer[BUFF_SIZE];
  size_t bytes_received;
} clientstate_t;

void handle_client_fsm(dbheader_t *dbhdr, employee_t **employees,
                       clientstate_t *client, int dbfd);

void init_clients(clientstate_t *states);

int find_free_slot(const clientstate_t *states);

int find_slot_by_fd(const clientstate_t *states, int fd);

#endif
