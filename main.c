#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 8080
#define CLIENTS_SIZE 10

int setNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int initTcp();

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

  char *args[] = {"ls", "-a", NULL};

  if (pid == 0) {

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

  int epfd = epoll_create(0);

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
    bool isSubscribeToLogs;
  };

  struct ClientInfo clientns[CLIENTS_SIZE];

  for (int i = 0; i < CLIENTS_SIZE; i++) {
    clientns[i].fd = -1;
  }

  while (1) {
    int count = epoll_wait(epfd, events, 32, -1);

    for (int i = 0; i < count; i++) {

      int fd = events[i].data.fd;

      if (fd == fstdout) {
        ssize_t n;
        while ((n = read(fstdout, mc_buf + mc_buf_pos,
                         sizeof(mc_buf) - mc_buf_pos - 1)) > 0) {

          for (int i = 0; i < CLIENTS_SIZE; i++) {
            if (clientns[i].fd != -1 && clientns[i].isSubscribeToLogs) {
              ssize_t wr = write(clientns[i].fd, mc_buf + mc_buf_pos, n);
              if (wr < 0) {
                perror("Ошибко! при рассылке данных клиенту");

                close(clientns[i].fd);
                clientns[i].fd = -1;
                clientns[i].isSubscribeToLogs = false;
              }
            }
          }

          mc_buf_pos += n;
          if (mc_buf_pos + 1 == sizeof(mc_buf)) {
            has_susles = true;
            mc_buf_pos = 0;
          }
        }

        if (n == 0) {
          fprintf(stderr, "Быдло сервер закрыл std out :(");
        } else if (errno != EAGAIN) {
          perror("Ошибко с std out сервера");
        }
      } else if (fd == tcp_fd) {
        int client = accept4(tcp_fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client < 0) {
          perror("client error");
          continue;
        }
        printf("client %d connected\n", client);
      }
    }
  }

  char buf[1024];
  ssize_t n;

  while ((n = read(fstdout, buf, sizeof(buf) - 1)) > 0) {
    buf[n] = '\0';
    printf("Received: %s", buf);
  }

  int status;

  waitpid(pid, &status, 0);

  printf("Hello piska %d\n", status);

  return 0;
}

int initTcp() {
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
