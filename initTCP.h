#pragma once

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <sys/socket.h>

#ifndef PORT
#define PORT 8085
#endif

#ifndef CLIENT_SIZE
#define CLIENTS_SIZE 10
#endif

static inline int initTcp() {
  int server_fd;

  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("TCP: Socket failed");
    return 1;
  }

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = INADDR_ANY;

  int reuse = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  int bind_res = bind(server_fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (bind_res < 0) {
    perror("Не удалось прослушать порт");
    return 1;
  }

  int qlen = 50;
  setsockopt(server_fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));

  int listen_res = listen(server_fd, CLIENTS_SIZE);
  if (listen_res < 0) {
    perror("Не удалось прослушать порт");
    return 1;
  }

  return server_fd;
}
