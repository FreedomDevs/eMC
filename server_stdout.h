
#pragma once

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "clients.h"
#include "enums.h"
#include "packets.h"

static char mc_buf[16 * 1024];
static int mc_buf_pos = 0;
static bool has_susles = false;

static inline void send_packet_to_all_clients() {
  for (int i = 0; i < CLIENTS_SIZE; i++) {
    struct ClientInfo *client = &clients[i];
    if (client->fd != -1) {
      if (clients[i].isSubscribeToLogs) {
        ssize_t wr = write_packet(client->fd);

        if (wr < 0) {
          perror("Ошибко! при рассылке данных клиенту");

          close(client->fd);
          client->fd = -1;
        }
      }
    }
  }
}

static inline ssize_t read_server_stdout(const int fstdout) {
  ssize_t n;
  size_t buf_size;
  while ((n = read(fstdout, mc_buf + mc_buf_pos,
                   (buf_size = sizeof(mc_buf) - mc_buf_pos - 1))) > 0) {
    write_as_packet(TYPE_SERVER_LOG, mc_buf + mc_buf_pos, n);
    send_packet_to_all_clients();

    mc_buf_pos += n;

    if (n < buf_size)
      break;

    has_susles = true;
    mc_buf_pos = 0;
  }

  return n;
}
