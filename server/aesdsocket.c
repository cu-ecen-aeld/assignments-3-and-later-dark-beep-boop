#include "aesdsocket.h"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#include "try.h"

#define PORT "9000"
#define BACKLOG 20
#define FILENAME "/var/tmp/aesdsocketdata"
#define BUFFSIZE 1024

static volatile sig_atomic_t terminate = 0;

int
main(int argc, char *argv[])
{
  int exit_status = EXIT_FAILURE;
  bool daemon = false;

  if (argc > 1) {
    if (argc == 2 && strncmp(argv[1], "-d", 2) == 0) {
      daemon = true;
    } else {
      syslog(LOG_ERR, "ERROR: wrong arguments\n");
      return EXIT_FAILURE;
    }
  }

  if (daemon)
    TRY(daemonize(), "daemonization failed");

  struct sigaction action;
  memset(&action, 0, sizeof action);
  action.sa_handler = terminate_handler;
  sigemptyset(&action.sa_mask);
  TRYC_ERRNO(sigaddset(&action.sa_mask, SIGTERM));
  TRYC_ERRNO(sigaddset(&action.sa_mask, SIGINT));
  TRYC_ERRNO(sigaction(SIGTERM, &action, NULL));
  TRYC_ERRNO(sigaction(SIGINT, &action, NULL));

  int fd;
  TRYC_ERRNO(
      fd = open(
          FILENAME,
          O_RDWR | O_APPEND | O_CREAT,
          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

  int sockfd;
  TRY(open_listening_socket(&sockfd, PORT, BACKLOG),
      "Couldn't open server socket");

  int conn_sockfd;
  char remote_name[INET6_ADDRSTRLEN];
  socklen_t addr_size;
  struct sockaddr_storage remote_addr;

  while (!terminate) {
    TRYC_CONTINUE_ON_EINTR(
        conn_sockfd =
            accept(sockfd, (struct sockaddr *) &remote_addr, &addr_size));

    TRY_ERRNO(inet_ntop(
        remote_addr.ss_family,
        get_in_addr((struct sockaddr *) &remote_addr),
        remote_name,
        sizeof remote_name));
    syslog(LOG_DEBUG, "Accepted connection from %s\n", remote_name);

    recv_and_send(conn_sockfd, fd);

    close(conn_sockfd);
    syslog(LOG_DEBUG, "Closed connection from %s\n", remote_name);
  }

  exit_status = EXIT_SUCCESS;

done:
  remove(FILENAME);

  if (conn_sockfd)
    close(conn_sockfd);

  if (sockfd)
    close(sockfd);

  exit(exit_status);
}

bool
recv_and_send(int socket_fd, int file_fd)
{
  bool ok = false;

  TRY(recv_and_write_line(file_fd, socket_fd),
      "line reception or writing failed");
  TRY(send_file(socket_fd, file_fd), "line reading or sending failed");

  ok = true;

done:
  return ok;
}

bool
daemonize(void)
{
  bool ok = false;

  pid_t daemon_pid;
  TRYC_ERRNO(daemon_pid = fork());

  if (daemon_pid != 0)
    exit(EXIT_SUCCESS);

  TRYC_ERRNO(setsid());

  TRYC_ERRNO(chdir("/"));

  for (int i = 0; i < 3; ++i) close(i);

  TRYC_ERRNO(open("/dev/null", O_RDWR));
  TRYC_ERRNO(dup(0));
  TRYC_ERRNO(dup(0));

  ok = true;

done:
  return ok;
}

bool
open_listening_socket(int *sockfd, const char *port, int backlog)
{
  bool ok = false;
  int sockfd_test;
  int yes = 1;
  struct addrinfo hints;
  struct addrinfo *servinfo = NULL;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status;
  TRY_GETADDRINFO(NULL, PORT, &hints, &servinfo, status);

  TRYC_ERRNO(sockfd_test = socket(servinfo->ai_family, SOCK_STREAM, 0));
  TRYC_ERRNO(
      setsockopt(sockfd_test, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)));

  TRYC_ERRNO(bind(sockfd_test, servinfo->ai_addr, servinfo->ai_addrlen));
  TRYC_ERRNO(listen(sockfd_test, BACKLOG));

  ok = true;
  *sockfd = sockfd_test;

done:
  if (!ok && sockfd_test != -1)
    close(sockfd_test);

  if (servinfo)
    freeaddrinfo(servinfo);

  return ok;
}

void
terminate_handler(int signo)
{
  if (signo == SIGTERM || signo == SIGINT) {
    syslog(LOG_DEBUG, "Caught signal, exiting\n");
    terminate = 1;
  }
}

bool
recv_and_write_line(int socket_fd, int file_fd)
{
  bool ok = false;
  char *line = NULL;

  TRY(recv_line(socket_fd, &line), "line reception failed");
  TRY(write_line(file_fd, line), "line writing failed");

  ok = true;

done:
  if (line)
    free(line);

  return ok;
}

bool
recv_line(int socket_fd, char **line)
{
  bool ok = false;
  bool eol = false;
  char *line_buffer = NULL;
  char buffer[BUFFSIZE] = "";
  ssize_t bytes_read;
  ssize_t line_buffer_size = 0;

  if (*line)
    line_buffer = *line;

  while (!eol) {
    TRYC_RETRY_ON_EINTR(bytes_read = recv(socket_fd, buffer, BUFFSIZE, 0));
    if (bytes_read) {
      ssize_t useful_bytes;
      char *newline_pointer = NULL;

      if ((newline_pointer = strchr(buffer, '\n')) != NULL) {
        eol = true;
        useful_bytes = newline_pointer - buffer + 1;
      } else {
        useful_bytes = bytes_read;
      }

      if (useful_bytes > 0) {
        TRY(line_buffer = (char *) realloc(
                line_buffer, line_buffer_size + useful_bytes + 1),
            "memory allocation failed");
        strncpy(line_buffer + line_buffer_size, buffer, useful_bytes);
        line_buffer_size += useful_bytes;
      }
    }
  }

  line_buffer[line_buffer_size] = '\0';

  ok = true;
  *line = line_buffer;

done:
  if (!ok && !(*line) && line_buffer)
    free(line_buffer);

  return ok;
}

bool
write_line(int file_fd, const char *line)
{
  bool ok = false;
  size_t bytes_to_write = strlen(line);

  while (bytes_to_write > 0) {
    ssize_t bytes_written;
    TRYC_RETRY_ON_EINTR(
        bytes_written = write(
            file_fd, line + strlen(line) - bytes_to_write, bytes_to_write));
    bytes_to_write -= bytes_written;
  }

  ok = true;

done:
  return ok;
}

bool
send_file(int socket_fd, int file_fd)
{
  bool ok = false;
  bool eof = false;
  char *line = NULL;

  while (!eof) {
    TRY(read_line(file_fd, &line, &eof), "line reading failed");
    if (line && strlen(line)) {
      TRY(send_line(socket_fd, line), "line sending failed");
    }
  }

  ok = true;

done:
  if (line)
    free(line);

  return ok;
}

bool
read_line(int file_fd, char **line, bool *eof)
{
  bool ok = false;
  bool eol = false;
  bool eof_local = false;
  ssize_t bytes_read;
  size_t line_buffer_size = 0;
  char *line_buffer = NULL;
  char buffer[BUFFSIZE] = "";

  if (*line)
    line_buffer = *line;

  while (!eol && !eof_local) {
    TRYC_RETRY_ON_EINTR(bytes_read = read(file_fd, buffer, BUFFSIZE));
    if (bytes_read) {
      ssize_t useful_bytes;
      char *newline_pointer = NULL;

      if ((newline_pointer = strchr(buffer, '\n')) != NULL) {
        eol = true;
        useful_bytes = newline_pointer - buffer + 1;
        TRYC_ERRNO(lseek(file_fd, SEEK_CUR, useful_bytes - BUFFSIZE));
      } else {
        useful_bytes = bytes_read;
      }

      if (useful_bytes > 0) {
        TRY_ERRNO(
            line_buffer = (char *) realloc(
                line_buffer, line_buffer_size + useful_bytes + 1));
        strncpy(line_buffer + line_buffer_size, buffer, useful_bytes);
        line_buffer_size += useful_bytes;
      }
    } else {
      eof_local = true;
    }
  }

  if (line_buffer)
    line_buffer[line_buffer_size] = '\0';

  ok = true;
  *line = line_buffer;
  *eof = eof_local;

done:
  if (!ok && !(*line) && line_buffer)
    free(line_buffer);

  return ok;
}

bool
send_line(int socket_fd, const char *line)
{
  bool ok = false;
  size_t bytes_to_send = strlen(line);

  while (bytes_to_send > 0) {
    ssize_t bytes_sent;
    TRYC_RETRY_ON_EINTR(
        bytes_sent = send(
            socket_fd, line + strlen(line) - bytes_to_send, bytes_to_send, 0));
    bytes_to_send -= bytes_sent;
  }

  ok = true;

done:
  return ok;
}

const void *
get_in_addr(const struct sockaddr *sa)
{
  void *result = NULL;

  if (sa->sa_family == AF_INET)
    result = &(((struct sockaddr_in *) sa)->sin_addr);
  else if (sa->sa_family == AF_INET6)
    result = &(((struct sockaddr_in6 *) sa)->sin6_addr);

  return result;
}
