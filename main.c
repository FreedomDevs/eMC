#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#define CLIENT_BUF_SIZE 2048

#define CLIENTS_SIZE 10
#include "enums.h"
#include "initTCP.h"

int setNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

#define BUILD_ERROR(str) "\x01\x00\x00\x00" str
const char CLIENT_OFERFLOW_ERROR[] =
    BUILD_ERROR("\x33\x00\x00\x00Ошибка, количество клиентов");
const char CLIENT_BUFFER_OVERFLOW_ERROR[] =
    BUILD_ERROR("\x35\x00\x00\x00Превышено размер буфера типо");
const char CLIENT_UNKNOWN_PACKET_TYPE[] =
    BUILD_ERROR("\x40\x00\x00\x00Некорректный тип пакета от клиента");

struct __attribute__((packed)) packet_header_struct {
  uint32_t id;
  uint32_t size;
};
static struct packet_header_struct header;
static ssize_t write_as_packet(int fd, uint32_t id, char *data, uint32_t size) {
  header.id = id;
  header.size = size;

  struct iovec iov[2];
  iov[0].iov_base = (void *)&header;
  iov[0].iov_len = sizeof(header);
  iov[1].iov_base = data;
  iov[1].iov_len = size;

  return writev(fd, iov, 2);
}

int main() {
  int pipe_stdin[2];
  int pipe_stdout[2];

  if (pipe(pipe_stdin) == -1) {
    perror("pipe");
    return 1;
  }

  if (pipe(pipe_stdout) == -1) {
    perror("pipe");
    return 1;
  }

  pid_t pid = fork();

  if (pid == 0) {
    chdir("/home/mikinol/servers/minecraft_servers_test/testmcprekol/");
    char *args[] = {"java", "-jar", "paper-1.21.10-117.jar", "-nogui", NULL};

    close(pipe_stdin[1]);
    close(pipe_stdout[0]);

    dup2(pipe_stdin[0], STDIN_FILENO);
    dup2(pipe_stdout[1], STDOUT_FILENO);

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);

    execvp(args[0], args);

    perror("error big piska");

    return 1;
  }

  close(pipe_stdin[0]);
  close(pipe_stdout[1]);

  int fstdin = pipe_stdin[1];
  int fstdout = pipe_stdout[0];

  setNonBlock(fstdout);
  setNonBlock(fstdin);

  int tcp_fd = initTcp();
  setNonBlock(tcp_fd);

  int epfd = epoll_create(32);

  struct epoll_event ev;

  ev.events = EPOLLIN;
  ev.data.fd = tcp_fd;
  epoll_ctl(epfd, EPOLL_CTL_ADD, tcp_fd, &ev);

  ev.events = EPOLLIN;
  ev.data.fd = fstdout;
  epoll_ctl(epfd, EPOLL_CTL_ADD, fstdout, &ev);

  struct epoll_event events[32];

  char mc_buf[1024];
  int mc_buf_pos = 0;
  bool has_susles = false;

  struct ClientInfo {
    int fd;
    char buf[CLIENT_BUF_SIZE];
    uint64_t buf_pos;
    bool isSubscribeToLogs;
  };

  struct ClientInfo clientns[CLIENTS_SIZE];
  for (int i = 0; i < CLIENTS_SIZE; i++) {
    clientns[i].fd = -1;
    clientns[i].buf_pos = 0;
  }

  while (1) {
    int count = epoll_wait(epfd, events, 32, -1);

    for (int i = 0; i < count; i++) {
      int fd = events[i].data.fd;

      if (fd == fstdout) {
        ssize_t n;
        size_t buf_size;
        while ((n = read(fstdout, mc_buf + mc_buf_pos,
                         (buf_size = sizeof(mc_buf) - mc_buf_pos - 1)) > 0)) {
          for (int i = 0; i < CLIENTS_SIZE; i++) {
            if (clientns[i].fd != -1 && clientns[i].isSubscribeToLogs) {
              ssize_t wr = write_as_packet(clientns[i].fd, TYPE_SERVER_LOG,
                                           mc_buf + mc_buf_pos, n);
              if (wr < 0) {
                perror("Ошибко! при рассылке данных клиенту");

                close(clientns[i].fd);
                clientns[i].fd = -1;
                clientns[i].isSubscribeToLogs = false;
              }
            }
          }

          mc_buf_pos += n;

          if (n < buf_size)
            break;

          has_susles = true;
          mc_buf_pos = 0;
        }

        if (n == 0) {
          fprintf(stderr, "stdout закрыт");
          close(fstdout);
        } else if (n == -1 && errno != EAGAIN) {
          perror("Ошибко с std out сервера");
        }
      } else if (fd == tcp_fd) {
        int client = accept4(tcp_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client < 0) {
          perror("client error");
          continue;
        }

        for (int i = 0;; i++) {
          if (i == CLIENTS_SIZE) {
            fprintf(stderr, "Закончилось место для клиентов");
            write(client, CLIENT_OFERFLOW_ERROR, sizeof(CLIENT_OFERFLOW_ERROR));
            close(client);
          }

          if (clientns[i].fd == -1) {
            clientns[i].fd = client;
            clientns[i].isSubscribeToLogs = false;
            break;
          }
        }

        printf("client %d connected\n", client);

        ev.events = EPOLLIN;
        ev.data.fd = client;
        epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev);
      } else {
        for (int i = 0; i < CLIENTS_SIZE; i++) {
          struct ClientInfo client = clientns[i];
          if (fd == client.fd) {
            ssize_t n;
            size_t buf_size;
            while ((n = read(client.fd, client.buf + client.buf_pos,
                             (buf_size =
                                  CLIENT_BUF_SIZE - client.buf_pos - 1))) > 0) {
              client.buf_pos += n;
              while (true) {
                if (client.buf_pos < 8) {
                  goto cont_1;
                }

                uint32_t size = ((uint32_t *)client.buf)[1];
                if (size > CLIENT_BUF_SIZE - 8) {
                  fprintf(stderr, "Клиент превысил размер буфера");
                  write(client.fd, CLIENT_BUFFER_OVERFLOW_ERROR,
                        sizeof(CLIENT_BUFFER_OVERFLOW_ERROR));
                  close(client.fd);
                  client.fd = -1;
                  break;
                }

                if (client.buf_pos - 8 >= size) {
                  uint32_t type = ((uint32_t *)client.buf)[0];

                  switch (type) {
                  case TYPE_SUBSCRIBE_TO_LOGS:
                    if (client.isSubscribeToLogs) {
                      fprintf(stderr, "Клиент прислал некорректный тип пакета");
                      close(client.fd);
                      client.fd = -1;
                      break;
                    }

                    client.isSubscribeToLogs = true;

                    printf("%d", has_susles);
                    if (!has_susles) {
                      printf("%d", mc_buf_pos);
                      if (write_as_packet(client.fd, TYPE_SERVER_LOG, mc_buf,
                                          mc_buf_pos) == -1) {
                        perror("Не удалось отправить лог сервера клиенту");
                        write(client.fd, CLIENT_UNKNOWN_PACKET_TYPE,
                              sizeof(CLIENT_UNKNOWN_PACKET_TYPE));
                        close(client.fd);
                        client.fd = -1;
                        goto end_1;
                      }

                      break;
                    }

                    header.id = TYPE_SERVER_LOG;
                    header.size = mc_buf_pos;

                    struct iovec iov[3];
                    iov[0].iov_base = (void *)&header;
                    iov[0].iov_len = sizeof(header);
                    iov[1].iov_base = mc_buf + mc_buf_pos;
                    iov[1].iov_len = sizeof(mc_buf) - mc_buf_pos;
                    iov[2].iov_base = mc_buf;
                    iov[2].iov_len = mc_buf_pos;

                    if (writev(client.fd, iov, 3) == -1) {
                      perror("Не удалось отправить лог сервера клиенту");
                      write(client.fd, CLIENT_UNKNOWN_PACKET_TYPE,
                            sizeof(CLIENT_UNKNOWN_PACKET_TYPE));
                      close(client.fd);
                      client.fd = -1;
                      goto end_1;
                    };
                    break;
                  default:
                    fprintf(stderr, "Клиент прислал некорректный тип пакета");
                    close(client.fd);
                    client.fd = -1;
                    goto end_1;
                  }

                  uint32_t start_pos = size + 8;
                  client.buf_pos = client.buf_pos - start_pos;

                  if (client.buf_pos > 0)
                    memmove(client.buf, client.buf + start_pos, client.buf_pos);
                  else
                    break;
                }
              }

            cont_1:;
            }

            if (n == 0) {
              fprintf(stderr, "Клиент закрыл поток");
              close(client.fd);
              client.fd = -1;
            } else if (errno != EAGAIN) {
              perror("Ошибко с клиентом");
              close(client.fd);
              client.fd = -1;
            }
            break;
          }
        }
      end_1:;
      }
    }
  }

  return 0;
}
