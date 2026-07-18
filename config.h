#pragma once
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

static uint16_t PORT;
static const char *SOCKET_PATH;

static inline long getenv_long(const char *name, long default_val) {
  char *env_str = getenv(name);

  if (env_str == NULL || *env_str == '\0') {
    return default_val;
  }

  char *endptr;
  errno = 0;

  long val = strtol(env_str, &endptr, 10);

  if (errno == ERANGE || endptr == env_str || *endptr != '\0') {
    return default_val;
  }
  return val;
}

static inline const char *getenv_str(const char *name,
                                     const char *default_val) {
  char *env_str = getenv(name);
  if (env_str == NULL || *env_str == '\0') {
    return default_val;
  }

  return env_str;
}

static inline void init_config() {
  PORT = getenv_long("PORT", 8085);
  SOCKET_PATH = getenv_str("SOCKET_PATH", "socket.sock");
}
