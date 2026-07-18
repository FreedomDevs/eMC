#pragma once

#include <stdint.h>

#define CLIENT_BUF_SIZE 2048

#define CLIENTS_SIZE 10

struct ClientInfo {
  int fd;
  char buf[CLIENT_BUF_SIZE];
  uint64_t buf_pos;
  bool isSubscribeToLogs;
  bool isSubscribeToMonitorings;
};

static struct ClientInfo clients[CLIENTS_SIZE];

static inline void initClients() {
  for (int i = 0; i < CLIENTS_SIZE; i++) {
    clients[i].fd = -1;
  }
}
