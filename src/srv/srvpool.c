#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"
#include "srvpoll.h"

void fsm_reply_hello(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_HELLO_RESP);
  hdr->len = htons(1);
  dbproto_hello_resp *hello = (dbproto_hello_resp *)&hdr[1];
  hello->proto = htons(PROTO_VER);

  write(client->fd, hdr, sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_resp));
}

void fsm_reply_add(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_EMPLOYEE_ADD_RESP);
  hdr->len = htons(0);

  write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void fsm_reply_hello_err(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_ERROR);
  hdr->len = htons(0);

  write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

void fsm_reply_add_err(clientstate_t *client, dbproto_hdr_t *hdr) {
  hdr->type = htonl(MSG_ERROR);
  hdr->len = htons(0);

  write(client->fd, hdr, sizeof(dbproto_hdr_t));
}

int send_employee(int fd, char *addstr) {
  dbproto_hdr_t buf[4096] = {0};

  dbproto_hdr_t *hdr = buf;
  hdr->type = MSG_EMPLOYEE_ADD_REQ;
  hdr->len = 1;

  dbproto_employee_add_req *employee = (dbproto_employee_add_req *)&hdr[1];
  strncpy((char *)&employee->data, addstr, sizeof(employee->data));

  employee->data[sizeof(employee->data) - 1] = '\0';

  hdr->type = htons(hdr->type);
  hdr->len = htons(hdr->len);

  write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_add_req));

  read(fd, buf, sizeof(buf));

  hdr->type = ntohl(hdr->type);
  hdr->len = ntohs(hdr->len);

  if (hdr->type == MSG_ERROR) {
    printf("Improper format for add employee string.\n");
    close(fd);
    return STATUS_ERROR;
  }

  if (hdr->type == MSG_EMPLOYEE_ADD_RESP) {
    printf("Employee succesfully added.\n");
  }

  return STATUS_SUCCESS;
}

void handle_client_fsm(dbheader_t *dbhdr, employee_t **employees,
                       clientstate_t *client, int dbfd) {
  dbproto_hdr_t *hdr = (dbproto_hdr_t *)client->buffer;

  hdr->type = ntohl(hdr->type);
  hdr->type = ntohs(hdr->len);

  if (client->state == STATE_HELLO) {
    if (hdr->type != MSG_HELLO_REQ || hdr->len != 1) {
      printf("Didn't get MSG_HELLO in HELLO state...\n");
      fsm_reply_hello_err(client, hdr);
      return;
    }

    dbproto_hello_req *hello = (dbproto_hello_req *)&hdr[1];
    hello->proto = ntohs(hello->proto);
    if (hello->proto != PROTO_VER) {
      printf("Protocol mismatch...\n");
      fsm_reply_hello_err(client, hdr);
      return;
    }

    fsm_reply_hello(client, hdr);
    client->state = STATE_MSG;
    printf("Client upgraded to STATE_MSG.\n");
  }

  if (client->state == STATE_MSG) {
    if (hdr->type == MSG_EMPLOYEE_ADD_REQ) {
      dbproto_employee_add_req *employee = (dbproto_employee_add_req *)&hdr[1];

      printf("Adding employee: %s\n", employee->data);
      if (add_employee(dbhdr, employees, (char *)employee->data) !=
          STATUS_SUCCESS) {
        fsm_reply_add_err(client, hdr);
        return;
      } else {
        fsm_reply_add(client, hdr);
        output_file(dbfd, dbhdr, *employees);
      }
    }
  }
}

void init_clients(clientstate_t *states) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    states[i].fd = -1;
    states[i].state = STATE_NEW;
    memset(&states[i].buffer, '\0', BUFF_SIZE);
  }
}

int find_free_slot(clientstate_t *states) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (states[i].fd == -1) {
      return i;
    }
  }
  return -1;
}

int find_slot_by_fd(clientstate_t *states, int fd) {
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (states[i].fd == fd) {
      return i;
    }
  }
  return -1;
}
