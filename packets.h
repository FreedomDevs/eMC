#pragma once

#include <bits/types/struct_iovec.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>

struct __attribute__((packed)) packet_header_struct {
  uint32_t id;
  uint32_t size;
};

static struct iovec iov[2];

static struct packet_header_struct header;
static inline void write_as_packet(uint32_t id, char *data, uint32_t size) {
  header.id = id;
  header.size = size;

  iov[0].iov_base = (void *)&header;
  iov[0].iov_len = sizeof(header);
  iov[1].iov_base = data;
  iov[1].iov_len = size;
}

static inline ssize_t write_packet(int fd) {
  return writev(fd, iov, sizeof(iov) / sizeof(struct iovec));
}
