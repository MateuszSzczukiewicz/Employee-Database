#include <netinet/in.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "srvpoll.h"

static int send_response(int fd, const void *data, size_t size) {
  if (fd < 0) {
    fprintf(stderr, "send_response: Invalid file descriptor\n");
    return STATUS_ERROR;
  }
  ssize_t bytes_written = write(fd, data, size);

  if (bytes_written < 0) {
    perror("send_response: write failed");
    return STATUS_ERROR;
  }
  if ((size_t)bytes_written != size) {
    fprintf(stderr,
            "send_response: Incomplete write. Expected %zu, wrote %zd\n", size,
            bytes_written);
    return STATUS_ERROR;
  }
  return STATUS_SUCCESS;
}

int fsm_prepare_and_send_hello_resp(clientstate_t *client,
                                    unsigned char *out_buffer,
                                    size_t out_buffer_size) {
  size_t response_size = sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_resp);

  if (out_buffer_size < response_size) {
    fprintf(stderr, "Output buffer too small for HELLO response.\n");
    return STATUS_ERROR;
  }

  dbproto_hdr_t *hdr = (dbproto_hdr_t *)out_buffer;
  hdr->type = MSG_HELLO_RESP;
  hdr->len = 1;

  dbproto_hello_resp *payload =
      (dbproto_hello_resp *)(out_buffer + sizeof(dbproto_hdr_t));
  payload->proto = PROTO_VER;

  hdr->type = htons(hdr->type);
  hdr->len = htons(hdr->len);
  payload->proto = htons(payload->proto);

  return send_response(client->fd, out_buffer, response_size);
}

int fsm_prepare_and_send_add_resp(clientstate_t *client,
                                  unsigned char *out_buffer,
                                  size_t out_buffer_size) {
  size_t response_size = sizeof(dbproto_hdr_t);
  if (out_buffer_size < response_size) {
    fprintf(stderr, "Output buffer too small for ADD response.\n");
    return STATUS_ERROR;
  }

  dbproto_hdr_t *hdr = (dbproto_hdr_t *)out_buffer;
  hdr->type = MSG_EMPLOYEE_ADD_RESP;
  hdr->len = 0;

  hdr->type = htons(hdr->type);
  hdr->len = htons(hdr->len);

  return send_response(client->fd, out_buffer, response_size);
}

int fsm_prepare_and_send_error_resp(clientstate_t *client,
                                    unsigned char *out_buffer,
                                    size_t out_buffer_size,
                                    u_int16_t original_msg_type) {
  (void)original_msg_type;

  size_t response_size = sizeof(dbproto_hdr_t);
  if (out_buffer_size < response_size) {
    fprintf(stderr, "Output buffer too small for ERROR response.\n");
    return STATUS_ERROR;
  }

  dbproto_hdr_t *hdr = (dbproto_hdr_t *)out_buffer;
  hdr->type = MSG_ERROR;
  hdr->len = 0;

  hdr->type = htons(hdr->type);
  hdr->len = htons(hdr->len);

  return send_response(client->fd, out_buffer, response_size);
}

static void close_client_connection(clientstate_t *client) {
  if (client && client->fd >= 0) {
    printf("Client %d: Closing connection.\n", client->fd);
    close(client->fd);
    client->fd = -1;
    client->state = STATE_NEW;
    client->bytes_received = 0;
    memset(client->buffer, 0, BUFF_SIZE);
  }
}

void handle_client_fsm(dbheader_t *dbhdr, employee_t **employees,
                       clientstate_t *client, int dbfd) {
  if (!client || client->fd < 0) {
    fprintf(stderr, "handle_client_fsm: Invalid client state or fd.\n");
    return;
  }

  unsigned char *buffer_ptr = client->buffer;
  dbproto_hdr_t *incoming_hdr = (dbproto_hdr_t *)buffer_ptr;

  u_int16_t msg_type = ntohs(incoming_hdr->type);
  u_int16_t msg_len = ntohs(incoming_hdr->len);

  if (client->state == STATE_HELLO) {
    if (msg_type != MSG_HELLO_REQ || msg_len != 1) {
      fprintf(stderr,
              "Client %d: Expected MSG_HELLO_REQ(len=1) in STATE_HELLO, got "
              "type %u (len=%u)\n",
              client->fd, msg_type, msg_len);
      if (fsm_prepare_and_send_error_resp(client, client->buffer, BUFF_SIZE,
                                          msg_type) != STATUS_SUCCESS) {
      }
      close_client_connection(client);
      return;
    }

    if (client->bytes_received <
        sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_req)) {
      fprintf(stderr,
              "Client %d: Incomplete HELLO_REQ message (expected header + "
              "payload).\n",
              client->fd);
      return;
    }

    dbproto_hello_req *hello_payload =
        (dbproto_hello_req *)(buffer_ptr + sizeof(dbproto_hdr_t));
    u_int16_t client_proto_ver = ntohs(hello_payload->proto);

    if (client_proto_ver != PROTO_VER) {
      fprintf(stderr,
              "Client %d: Protocol version mismatch. Expected %u, got %u\n",
              client->fd, PROTO_VER, client_proto_ver);
      if (fsm_prepare_and_send_error_resp(client, client->buffer, BUFF_SIZE,
                                          msg_type) != STATUS_SUCCESS) {
      }
      close_client_connection(client);
      return;
    }

    if (fsm_prepare_and_send_hello_resp(client, client->buffer, BUFF_SIZE) !=
        STATUS_SUCCESS) {
      close_client_connection(client);
      return;
    }
    client->state = STATE_MSG;
    printf("Client %d: Upgraded to STATE_MSG.\n", client->fd);
  }

  if (client->state == STATE_MSG) {
    if (msg_type == MSG_EMPLOYEE_ADD_REQ) {
      dbproto_employee_add_req *employee_payload =
          (dbproto_employee_add_req *)(buffer_ptr + sizeof(dbproto_hdr_t));

      char safe_employee_data[sizeof(employee_payload->data) + 1];
      memcpy(safe_employee_data, employee_payload->data,
             sizeof(employee_payload->data));
      safe_employee_data[sizeof(employee_payload->data)] = '\0';

      printf("Client %d: Received ADD_REQ for employee: \"%.*s\"\n", client->fd,
             (int)strnlen(safe_employee_data, sizeof(safe_employee_data) - 1),
             safe_employee_data);

      if (add_employee(dbhdr, employees, safe_employee_data) !=
          STATUS_SUCCESS) {
        fprintf(stderr, "Client %d: Failed to add employee internally.\n",
                client->fd);
        close_client_connection(client);
        return;
      }

      if (fsm_prepare_and_send_add_resp(client, client->buffer, BUFF_SIZE) !=
          STATUS_SUCCESS) {
        fprintf(stderr,
                "Client %d: Employee added, but FAILED to send ADD_RESP.\n",
                client->fd);
        close_client_connection(client);
        return;
      }

      printf("Client %d: Employee added successfully. Saving to file...\n",
             client->fd);
      if (output_file(dbfd, dbhdr, *employees) != STATUS_SUCCESS) {
        fprintf(stderr,
                "CRITICAL: Client %d: Employee added and client notified, BUT "
                "FAILED TO SAVE DATABASE TO FILE!\n",
                client->fd);
      } else {
        printf("Client %d: Database saved to file successfully.\n", client->fd);
      }
    } else {
      fprintf(stderr, "Client %d: Unknown message type %u in STATE_MSG.\n",
              client->fd, msg_type);
      if (fsm_prepare_and_send_error_resp(client, client->buffer, BUFF_SIZE,
                                          msg_type) != STATUS_SUCCESS) {
      }
      close_client_connection(client);
      return;
    }
  }
}

void init_clients(clientstate_t *states) {
  if (!states)
    return;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    states[i].fd = -1;
    states[i].state = STATE_NEW;
    states[i].bytes_received = 0;
    memset(states[i].buffer, 0, BUFF_SIZE);
  }
}

int find_free_slot(const clientstate_t *states) {
  if (!states)
    return -1;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (states[i].fd == -1) {
      return i;
    }
  }
  return -1;
}

int find_slot_by_fd(const clientstate_t *states, int fd) {
  if (!states || fd < 0)
    return -1;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (states[i].fd == fd) {
      return i;
    }
  }
  return -1;
}
