#include <arpa/inet.h>
#include <bits/getopt_core.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

int list_employees(int fd) {
  dbproto_hdr_t buf[4096] = {0};

  dbproto_hdr_t *hdr = buf;
  hdr->type = MSG_EMPLOYEE_LIST_REQ;
  hdr->len = 0;

  hdr->type = htonl(hdr->type);
  hdr->len = htons(hdr->len);

  write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_employee_add_req));

  read(fd, hdr, sizeof(dbproto_hdr_t));

  hdr->type = ntohl(hdr->type);
  hdr->len = ntohs(hdr->len);

  if (hdr->type == MSG_ERROR) {
    printf("Unable to list employees.\n");
    close(fd);
    return STATUS_ERROR;
  }

  if (hdr->type == MSG_EMPLOYEE_LIST_RESP) {
    printf("Listing employees...\n");
    dbproto_employee_list_resp *employee =
        (dbproto_employee_list_resp *)&hdr[1];

    for (int i = 0; i < hdr->len; i++) {
      read(fd, employee, sizeof(dbproto_employee_list_resp));
      employee->hours = ntohl(employee->hours);
      printf("%s, %s, %d\n", employee->name, employee->address,
             employee->hours);
    }
  }

  return STATUS_SUCCESS;
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

int send_hello(int fd) {
  dbproto_hdr_t buf[4096] = {0};

  dbproto_hdr_t *hdr = buf;
  hdr->type = MSG_HELLO_REQ;
  hdr->len = 1;

  dbproto_hello_req *hello = (dbproto_hello_req *)&hdr[1];
  hello->proto = htons(hello->proto);

  hdr->type = htonl(hdr->type);
  hdr->len = htons(hdr->len);
  hello->proto = htons(hello->proto);

  write(fd, buf, sizeof(dbproto_hdr_t) + sizeof(dbproto_hello_req));

  read(fd, buf, sizeof(buf));

  hdr->type = ntohl(hdr->type);
  hdr->type = ntohl(hdr->len);

  if (hdr->type == MSG_ERROR) {
    printf("Protocol mismatch.\n");
    close(fd);
    return STATUS_ERROR;
  }

  printf("Server connected, protocol v1.\n");
  return STATUS_SUCCESS;
}

int main(int argc, char *argv[]) {
  char *addarg = NULL;
  char *portarg = NULL, *hostarg = NULL;
  unsigned short port = 0;
  bool list = false;

  int c;
  while ((c = getopt(argc, argv, "p:h:a:l")) != -1) {
    switch (c) {
    case 'a':
      addarg = optarg;
      break;
    case 'l':
      list = true;
      break;
    case 'p':
      portarg = optarg;
      port = atoi(portarg);
      break;
    case 'h':
      hostarg = optarg;
      break;
    case '?':
      printf("Unknown option -%c\n", c);
    default:
      return -1;
    }
  }

  if (port == 0) {
    printf("Bad port: %s\n", portarg);
    return -1;
  }

  if (hostarg == NULL) {
    printf("Must specify host with -h\n");
    return -1;
  }

  struct sockaddr_in serverInfo = {0};

  serverInfo.sin_family = AF_INET;
  serverInfo.sin_addr.s_addr = inet_addr(hostarg);
  serverInfo.sin_port = htons(port);

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) == -1) {
    perror("connect");
    close(fd);
    return 0;
  }

  if (send_hello(fd) != STATUS_SUCCESS) {
    return -1;
  }

  if (addarg) {
    send_employee(fd, addarg);
  }

  if (list) {
    list_employees(fd);
  }

  close(fd);
}
