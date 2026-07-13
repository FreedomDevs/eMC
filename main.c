#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PORT 8080

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
  } else {

    close(pipe_stdin[0]);
    close(pipe_stdout[1]);

    int fstdin = pipe_stdin[1];
    int fstdout = pipe_stdout[0];

    char buf[1024];
    ssize_t n;

    while ((n = read(fstdout, buf, sizeof(buf) - 1)) > 0) {
      buf[n] = '\0';
      printf("Received: %s", buf);
    }

    int status;

    waitpid(pid, &status, 0);

    printf("Hello piska %d\n", status);

    int epfd = epoll_create1(0);
  }

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

  int listen_res = listen(server_fd, 10);
  if (listen_res < 0) {
    perror("Не удалось прослушать порт");
    return 1;
  }

  return server_fd;
}
