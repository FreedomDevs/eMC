#pragma once

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

struct __attribute((packed)) PermanentStats {
  ssize_t max_ram;
  ssize_t high_ram;
  ssize_t max_swap_ram;
  ssize_t high_swap_ram;
  int32_t cpu_max;
};

struct __attribute__((packed)) UpdatableStats {
  ssize_t cur_ram;
  ssize_t cur_swap_ram;

  ssize_t user_usec;
  ssize_t system_usec;
};

static struct PermanentStats permanentStats;
static struct UpdatableStats resSt;

static int dirfd;
static uid_t uid;

static char anal_buffer[512];

static inline ssize_t get_long(const char *str) {
  if (str == NULL || *str == '\0') {
    return -1;
  }

  char *endptr;
  errno = 0;

  long val = strtol(str, &endptr, 10);

  if (errno == ERANGE || endptr == str) {
    return -1;
  }
  return val;
}

static inline ssize_t getFileBody(int dirfd, const char *file_name) {
  int filefd = openat(dirfd, file_name, O_RDONLY);
  if (filefd == -1) {
    perror("Не удалось открыть файл");
    return -1;
  }

  ssize_t fileBody = read(filefd, anal_buffer, sizeof(anal_buffer));
  if (fileBody == -1) {
    perror("Не удалось прочинать содержимое файла");
    close(filefd);
    return -1;
  }
  close(filefd);

  ssize_t fileBB = get_long(anal_buffer);
  if (fileBB == -1) {
    fprintf(stderr, "Не удалось спарсить число\n");
    return -1;
  }

  return fileBB;
}

static inline void parsCpuParams(int dirfd) {
  int filefd = openat(dirfd, "cpu.stat", O_RDONLY);
  if (filefd == -1) {
    perror("Не удалось открыть файл");
    return;
  }

  ssize_t n = read(filefd, anal_buffer, sizeof(anal_buffer));
  close(filefd);
  if (n == -1) {
    perror("Не удалось прочитать содержимое файла");
    return;
  }

  int last_i = 0;
  for (char *i = anal_buffer + 13; i < anal_buffer + sizeof(anal_buffer); i++) {
    if (*i == 'u') {
      i += 9;

      char *endptr;
      ssize_t val = strtol(i, &endptr, 10);
      if (errno == ERANGE || endptr == i) {
        return;
      }

      i = endptr;

      resSt.user_usec = val;
    } else if (*i == 's') {
      i += 11;

      char *endptr;
      ssize_t val = strtol(i, &endptr, 10);
      if (errno == ERANGE || endptr == i) {
        return;
      }

      i = endptr;
      resSt.system_usec = val;
      return;
    }
  }
}

static inline void getSliceDirFdAndUid() {
  uid = getuid();

  snprintf(anal_buffer, sizeof(anal_buffer),
           "/sys/fs/cgroup/user.slice/user-%d.slice", uid);
  dirfd = open(anal_buffer, O_RDONLY | O_DIRECTORY);
  if (dirfd == -1) {
    perror("Ошибко папка не открыласть :(");
    return;
  }
}

static inline void getSliceMonitroings() {
  if (dirfd == -1)
    return;

  permanentStats.max_ram = getFileBody(dirfd, "memory.max");
  permanentStats.high_ram = getFileBody(dirfd, "memory.high");
  permanentStats.max_swap_ram = getFileBody(dirfd, "memory.swap.max");
  permanentStats.high_swap_ram = getFileBody(dirfd, "memory.swap.high");
  permanentStats.cpu_max = getFileBody(dirfd, "cpu.max");
}

static inline void updateSliceMonitroings() {
  if (dirfd == -1)
    return;

  resSt.cur_ram = getFileBody(dirfd, "memory.current");
  resSt.cur_swap_ram = getFileBody(dirfd, "memory.swap.current");

  parsCpuParams(dirfd);
}
