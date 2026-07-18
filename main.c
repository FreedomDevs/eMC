#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "clients.h"

#include "enums.h"
#include "initSocket.h"
#include "server_stdout.h"

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
const char CLIENT_BAD_REQUEST[] =
    BUILD_ERROR("\x25\x00\x00\x00Некорректный запрос");

#define STATE_OFF 0
#define STATE_ON 1
#define STATE_DISABLING 2
static unsigned char server_state = STATE_OFF;

static pid_t pid = -2;
static int pipe_stdin[2];
static int pipe_stdout[2];

static inline void start_minecraft_server() {
  pid = fork();
  if (pid == 0) {
    chdir("/home/mikinol/servers/minecraft_servers_test/testmcprekol/");
    char *args[] = {"java", "-jar", "paper-1.21.10-117.jar", "-nogui", NULL};

    close(pipe_stdin[1]);
    close(pipe_stdout[0]);

    dup2(pipe_stdin[0], STDIN_FILENO);
    dup2(pipe_stdout[1], STDOUT_FILENO);
    dup2(pipe_stdout[1], STDERR_FILENO);

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);

    execvp(args[0], args);

    perror("error big piska");
    _exit(1);
  }
}

static inline void process_clent_event(struct ClientInfo *client,
                                       int stdin_fd) {
  ssize_t n;
  size_t buf_size;
  while ((n = read(client->fd, client->buf + client->buf_pos,
                   (buf_size = CLIENT_BUF_SIZE - client->buf_pos - 1))) > 0) {
    client->buf_pos += n;
    while (true) {
      if (client->buf_pos < 8) {
        goto cont_1;
      }

      uint32_t size = ((uint32_t *)client->buf)[1];
      if (size > CLIENT_BUF_SIZE - 8) {
        fprintf(stderr, "Клиент превысил размер буфера");
        write(client->fd, CLIENT_BUFFER_OVERFLOW_ERROR,
              sizeof(CLIENT_BUFFER_OVERFLOW_ERROR));
        close(client->fd);
        client->fd = -1;
        break;
      }

      if (client->buf_pos - 8 >= size) {
        uint32_t type = ((uint32_t *)client->buf)[0];

        switch (type) {
        case TYPE_SUBSCRIBE_TO_LOGS:
          if (client->isSubscribeToLogs) {
            fprintf(stderr, "Клиент прислал некорректные данные в пакете");
            write(client->fd, CLIENT_BAD_REQUEST, sizeof(CLIENT_BAD_REQUEST));
            close(client->fd);
            client->fd = -1;
            return;
          }

          client->isSubscribeToLogs = true;

          if (!has_susles) {
            write_as_packet(TYPE_SERVER_LOG, mc_buf, mc_buf_pos);
            if (write_packet(client->fd) == -1) {
              perror("Не удалось отправить лог сервера клиенту");
              write(client->fd, CLIENT_UNKNOWN_PACKET_TYPE,
                    sizeof(CLIENT_UNKNOWN_PACKET_TYPE));
              close(client->fd);
              client->fd = -1;
              return;
            }

            break;
          }

          header.id = TYPE_SERVER_LOG;
          header.size = sizeof(mc_buf);

          static struct iovec iov[3];
          iov[0].iov_base = (void *)&header;
          iov[0].iov_len = sizeof(header);
          iov[1].iov_base = mc_buf + mc_buf_pos;
          iov[1].iov_len = sizeof(mc_buf) - mc_buf_pos;
          iov[2].iov_base = mc_buf;
          iov[2].iov_len = mc_buf_pos;

          if (writev(client->fd, iov, 3) == -1) {
            perror("Не удалось отправить лог сервера клиенту");
            write(client->fd, CLIENT_UNKNOWN_PACKET_TYPE,
                  sizeof(CLIENT_UNKNOWN_PACKET_TYPE));
            close(client->fd);
            client->fd = -1;
            return;
          };
          break;
        case TYPE_SEND_COMMAND:
          write(stdin_fd, client->buf + 8, size);
          break;
        case TYPE_ENABLE_SERVER:
          if (server_state == STATE_ON)
            break;

          server_state = STATE_ON;
          start_minecraft_server();
          break;
        case TYPE_RESTART_SERVER:
          if (server_state == STATE_ON)
            kill(pid, SIGTERM);
          break;
        case TYPE_DISABLE_SERVER:
          if (server_state == STATE_ON) {
            server_state = STATE_DISABLING;
            kill(pid, SIGTERM);
          }

          break;
        default:
          fprintf(stderr, "Клиент прислал некорректный тип пакета");
          write(client->fd, CLIENT_BAD_REQUEST, sizeof(CLIENT_BAD_REQUEST));
          close(client->fd);
          client->fd = -1;
          return;
        }

        uint32_t start_pos = size + 8;
        client->buf_pos = client->buf_pos - start_pos;

        if (client->buf_pos > 0)
          memmove(client->buf, client->buf + start_pos, client->buf_pos);
        else
          break;
      }
    }

  cont_1:;
  }

  if (n == 0) {
    fprintf(stderr, "Клиент закрыл поток");
    close(client->fd);
    client->fd = -1;
  } else if (errno != EAGAIN) {
    perror("Ошибко с клиентом");
    close(client->fd);
    client->fd = -1;
  }
}

void handle_sigchld(int sig) {
  int saved_errno = errno;
  int status;
  pid_t pid;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    fprintf(stderr, "\n[Сигнал] Дочерний процесс PID %d завершился.\n", pid);
    if (WIFEXITED(status)) {
      fprintf(stderr, "[Сигнал] Код возврата: %d\n", WEXITSTATUS(status));
    }

    if (server_state == STATE_ON) {
      start_minecraft_server();
      continue;
    }

    server_state = STATE_OFF;
  }
}

int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  setvbuf(stderr, NULL, _IONBF, 0);

  if (pipe(pipe_stdin) == -1) {
    perror("pipe");
    return 1;
  }

  if (pipe(pipe_stdout) == -1) {
    perror("pipe");
    return 1;
  }

  struct sigaction sa;
  sa.sa_handler = &handle_sigchld;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("Ошибка регистрации sigaction");
    return 1;
  }

  server_state = STATE_ON;
  start_minecraft_server();

  int fstdin = pipe_stdin[1];
  int fstdout = pipe_stdout[0];

  setNonBlock(fstdout);
  setNonBlock(fstdin);

  int tcp_fd = initTcp();
  setNonBlock(tcp_fd);

  int unix_fd = initUnix();
  setNonBlock(unix_fd);

  int epfd = epoll_create(32);

  struct epoll_event ev;

  ev.events = EPOLLIN;
  ev.data.fd = tcp_fd;
  epoll_ctl(epfd, EPOLL_CTL_ADD, tcp_fd, &ev);

  ev.events = EPOLLIN;
  ev.data.fd = unix_fd;
  epoll_ctl(epfd, EPOLL_CTL_ADD, unix_fd, &ev);

  ev.events = EPOLLIN;
  ev.data.fd = fstdout;
  epoll_ctl(epfd, EPOLL_CTL_ADD, fstdout, &ev);

  struct epoll_event events[32];

  initClients();

  while (1) {
    int count = epoll_wait(epfd, events, 32, -1);

    for (int i = 0; i < count; i++) {
      int fd = events[i].data.fd;

      if (fd == fstdout) {
        ssize_t ret = read_server_stdout(fstdout);

        if (ret == 0) {
          fprintf(stderr, "stdout закрыт");
          close(fstdout);
        } else if (ret == -1 && errno != EAGAIN) {
          perror("Ошибко с std out сервера");
        }
      } else if (fd == tcp_fd || fd == unix_fd) {
        int client = accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client < 0) {
          perror("client error");
          continue;
        }

        for (int i = 0;; i++) {
          if (i == CLIENTS_SIZE) {
            fprintf(stderr, "Закончилось место для клиентов\n");
            write(client, CLIENT_OFERFLOW_ERROR, sizeof(CLIENT_OFERFLOW_ERROR));
            close(client);
          }

          if (clients[i].fd == -1) {
            clients[i].fd = client;
            clients[i].isSubscribeToLogs = false;
            clients[i].buf_pos = 0;
            break;
          }
        }

        printf("client %d connected\n", client);

        ev.events = EPOLLIN;
        ev.data.fd = client;
        epoll_ctl(epfd, EPOLL_CTL_ADD, client, &ev);
      } else {
        for (int i = 0; i < CLIENTS_SIZE; i++) {
          struct ClientInfo *client = &clients[i];
          if (fd == client->fd) {
            process_clent_event(client, fstdin);
            break;
          }
        }
      }
    }
  }

  kill(pid, SIGTERM);
  return 0;
}
