#pragma once

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifndef CLIENT_SIZE
#define CLIENTS_SIZE 10
#endif

static inline int initTcp(uint16_t PORT) {
  int server_fd = socket(AF_INET6, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("TCP: Socket failed");
    _exit(1);
  }

  struct sockaddr_in6 addr = {0};
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(PORT);
  addr.sin6_addr = in6addr_any;

  int reuse = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  int bind_res = bind(server_fd, (const struct sockaddr *)&addr, sizeof(addr));
  if (bind_res < 0) {
    perror("Не удалось прослушать порт");
    _exit(1);
  }

  int qlen = 50;
  setsockopt(server_fd, SOL_TCP, TCP_FASTOPEN, &qlen, sizeof(qlen));

  int listen_res = listen(server_fd, CLIENTS_SIZE);
  if (listen_res < 0) {
    perror("Не удалось прослушать порт");
    _exit(1);
  }

  return server_fd;
}

static inline int initUnix(const char *PATH) {
  int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("UNIX: Socket failed");
    _exit(1);
  }

  struct sockaddr_un server_addr;
  memset(&server_addr, 0, sizeof(struct sockaddr_un));
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, PATH, sizeof(server_addr.sun_path) - 1);

  unlink(PATH);

  if (bind(server_fd, (struct sockaddr *)&server_addr,
           sizeof(struct sockaddr_un)) == -1) {
    perror("Ошибка bind");
    close(server_fd);
    _exit(1);
  }

  if (listen(server_fd, CLIENTS_SIZE) == -1) {
    perror("Ошибка listen");
    close(server_fd);
    _exit(1);
  }

  return server_fd;
}
