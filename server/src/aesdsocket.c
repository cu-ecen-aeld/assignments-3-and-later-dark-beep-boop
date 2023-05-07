#include "aesdsocket.h"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <bits/pthreadtypes.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
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
#include <time.h>
#include <unistd.h>

#include "monitor.h"
#include "queue.h"
#include "try.h"

#define BUFFSIZE 1024

static volatile sig_atomic_t terminate = 0;

static void aesdsocket_terminate_handler(int signo);
static bool aesdsocket_daemonize(void);
static bool aesdsocket_open_listening_socket(
    int *sockfd, const char *port, int backlog);
static const void *aesdsocket_get_in_addr(const struct sockaddr *sa);

struct aesdsocket_thread_arg
{
  int conn_sockfd;
  int write_file_fd;
  char *filename;
  char *remote_name;
  monitor_t *file_monitor;
};
typedef struct aesdsocket_thread_arg aesdsocket_thread_arg_t;

static void *aesdsocket_start_thread(void *arg);
static bool aesdsocket_serve(
    int socket_fd,
    int write_file_fd,
    const char *filename,
    monitor_t *file_monitor);
static bool aesdsocket_recv_and_write_line(
    int file_fd, int socket_fd, monitor_t *file_monitor);
static bool aesdsocket_recv_line(int socket_fd, char **line);
static bool aesdsocket_write_line(
    int file_fd, const char *line, monitor_t *file_monitor);
static bool aesdsocket_read_and_send_file(
    int socket_fd, int file_fd, monitor_t *file_monitor);
static bool aesdsocket_read_line(
    int file_fd, char **line, bool *eof, monitor_t *file_monitor);
static bool aesdsocket_send_line(int socket_fd, const char *line);

void
aesdsocket_terminate_handler(int signo)
{
  if (signo == SIGTERM || signo == SIGINT) {
    syslog(LOG_DEBUG, "Caught signal, exiting\n");
    terminate = 1;
  }
}

bool
aesdsocket_daemonize(void)
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
aesdsocket_open_listening_socket(int *sockfd, const char *port, int backlog)
{
  bool ok = false;
  int sockfd_test = -1;
  int yes = 1;
  struct addrinfo hints;
  struct addrinfo *servinfo = NULL;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status;
  TRY_GETADDRINFO(NULL, port, &hints, &servinfo, status);

  TRYC_ERRNO(sockfd_test = socket(servinfo->ai_family, SOCK_STREAM, 0));
  TRYC_ERRNO(
      setsockopt(sockfd_test, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)));

  TRYC_ERRNO(bind(sockfd_test, servinfo->ai_addr, servinfo->ai_addrlen));
  TRYC_ERRNO(listen(sockfd_test, backlog));

  ok = true;
  *sockfd = sockfd_test;

done:
  if (!ok && sockfd_test != -1)
    close(sockfd_test);

  if (servinfo)
    freeaddrinfo(servinfo);

  return ok;
}

const void *
aesdsocket_get_in_addr(const struct sockaddr *sa)
{
  void *result = NULL;

  if (sa->sa_family == AF_INET)
    result = &(((struct sockaddr_in *) sa)->sin_addr);
  else if (sa->sa_family == AF_INET6)
    result = &(((struct sockaddr_in6 *) sa)->sin6_addr);

  return result;
}

void *
aesdsocket_start_thread(void *arg)
{
  aesdsocket_thread_arg_t *thread_arg = arg;
  int conn_sockfd = thread_arg->conn_sockfd;
  int write_file_fd = thread_arg->write_file_fd;
  char *filename = thread_arg->filename;
  char *remote_name = thread_arg->remote_name;
  monitor_t *file_monitor = thread_arg->file_monitor;
  free(thread_arg);

  syslog(LOG_DEBUG, "Accepted connection from %s\n", remote_name);

  TRY(aesdsocket_serve(conn_sockfd, write_file_fd, filename, file_monitor),
      "thread execution failed");

done:
  if (filename)
    free(filename);

  if (remote_name)
    free(remote_name);

  if (conn_sockfd != -1)
    close(conn_sockfd);

  syslog(LOG_DEBUG, "Closed connection from %s\n", remote_name);

  return NULL;
}

bool
aesdsocket_serve(
    int socket_fd,
    int write_file_fd,
    const char *filename,
    monitor_t *file_monitor)
{
  bool ok = false;
  int read_file_fd = -1;

  TRY(aesdsocket_recv_and_write_line(write_file_fd, socket_fd, file_monitor),
      "line reception or writing failed");

  TRYC_ERRNO(
      read_file_fd =
          open(filename, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
  TRY(aesdsocket_read_and_send_file(socket_fd, read_file_fd, file_monitor),
      "line reading or sending failed");

  ok = true;

done:
  if (read_file_fd != -1)
    close(read_file_fd);

  return ok;
}

bool
aesdsocket_recv_and_write_line(
    int file_fd, int socket_fd, monitor_t *file_monitor)
{
  bool ok = false;
  char *line = NULL;

  TRY(aesdsocket_recv_line(socket_fd, &line), "line reception failed");
  TRY(aesdsocket_write_line(file_fd, line, file_monitor),
      "line writing failed");

  ok = true;

done:
  if (line)
    free(line);

  return ok;
}

bool
aesdsocket_recv_line(int socket_fd, char **line)
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
aesdsocket_write_line(int file_fd, const char *line, monitor_t *file_monitor)
{
  bool ok = false;
  size_t bytes_to_write = strlen(line);

  monitor_start_writing(file_monitor);
  while (bytes_to_write > 0) {
    ssize_t bytes_written;
    TRYC_RETRY_ON_EINTR(
        bytes_written = write(
            file_fd, line + strlen(line) - bytes_to_write, bytes_to_write));
    bytes_to_write -= bytes_written;
  }
  monitor_stop_writing(file_monitor);

  ok = true;

done:
  return ok;
}

bool
aesdsocket_read_and_send_file(
    int socket_fd, int file_fd, monitor_t *file_monitor)
{
  bool ok = false;
  bool eof = false;
  char *line = NULL;

  while (!eof) {
    TRY(aesdsocket_read_line(file_fd, &line, &eof, file_monitor),
        "line reading failed");
    if (line && strlen(line)) {
      TRY(aesdsocket_send_line(socket_fd, line), "line sending failed");
    }
  }

  ok = true;

done:
  if (line)
    free(line);

  return ok;
}

bool
aesdsocket_read_line(
    int file_fd, char **line, bool *eof, monitor_t *file_monitor)
{
  bool ok = false;
  bool eol = false;
  bool eof_local = false;
  ssize_t bytes_read;
  size_t line_buffer_size = 0;
  char *line_buffer = NULL;
  char buffer[BUFFSIZE] = "";
  off_t file_off = 0;

  TRYC_ERRNO(file_off = lseek(file_fd, 0, SEEK_CUR));

  if (*line)
    line_buffer = *line;

  monitor_start_reading(file_monitor);
  while (!eol && !eof_local) {
    TRYC_RETRY_ON_EINTR(bytes_read = read(file_fd, buffer, BUFFSIZE));
    if (bytes_read) {
      ssize_t useful_bytes;
      char *newline_pointer = NULL;

      if ((newline_pointer = strchr(buffer, '\n')) != NULL) {
        eol = true;
        useful_bytes = newline_pointer - buffer + 1;
        file_off += useful_bytes;
        TRYC_ERRNO(lseek(file_fd, file_off, SEEK_SET));
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
  monitor_stop_reading(file_monitor);

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
aesdsocket_send_line(int socket_fd, const char *line)
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

bool
aesdsocket_mainloop(
    const char *port, int backlog, const char *filename, bool daemon)
{
  bool ok = false;
  struct sigaction action;
  int write_file_fd = -1;
  int sockfd = -1;
  int conn_sockfd = -1;
  char remote_name[INET6_ADDRSTRLEN] = {};
  socklen_t addr_size;
  struct sockaddr_storage remote_addr;
  queue_t *thread_queue = NULL;
  monitor_t *file_monitor = NULL;
  aesdsocket_thread_arg_t *thread_arg = NULL;

  if (daemon)
    TRY(aesdsocket_daemonize(), "daemonization failed");

  memset(&action, 0, sizeof action);
  action.sa_handler = aesdsocket_terminate_handler;
  sigemptyset(&action.sa_mask);
  TRYC_ERRNO(sigaddset(&action.sa_mask, SIGTERM));
  TRYC_ERRNO(sigaddset(&action.sa_mask, SIGINT));
  TRYC_ERRNO(sigaction(SIGTERM, &action, NULL));
  TRYC_ERRNO(sigaction(SIGINT, &action, NULL));

  TRYC_ERRNO(
      write_file_fd = open(
          filename,
          O_WRONLY | O_APPEND | O_CREAT,
          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

  TRY(aesdsocket_open_listening_socket(&sockfd, port, backlog),
      "Couldn't open server socket");

  TRY(thread_queue = queue_new(), "queue creation failed");
  TRY(file_monitor = monitor_new(), "monitor creation failed");

  while (!terminate) {
    TRYC_CONTINUE_ON_EINTR(
        conn_sockfd =
            accept(sockfd, (struct sockaddr *) &remote_addr, &addr_size));

    TRY_ERRNO(inet_ntop(
        remote_addr.ss_family,
        aesdsocket_get_in_addr((struct sockaddr *) &remote_addr),
        remote_name,
        sizeof remote_name));

    TRY_ALLOCATE(thread_arg, aesdsocket_thread_arg_t);
    thread_arg->conn_sockfd = conn_sockfd;
    thread_arg->write_file_fd = write_file_fd;
    TRY_ERRNO(thread_arg->filename = strdup(filename));
    TRY_ERRNO(thread_arg->remote_name = strndup(remote_name, INET6_ADDRSTRLEN));
    thread_arg->file_monitor = file_monitor;

    pthread_t tid;
    int status;
    TRY_PTHREAD_CREATE(&tid, NULL, aesdsocket_start_thread, thread_arg, status);
    TRY(queue_enqueue(thread_queue, tid), "tid enqueue failed");
  }

  ok = true;

done:
  if (thread_queue) {
    while (!queue_is_empty(thread_queue)) {
      pthread_t tid = queue_dequeue(thread_queue);
      int status;
      TRY_PTHREAD_JOIN_NOACTION(tid, NULL, status);
    }
    queue_destroy(thread_queue);
  }

  if (file_monitor)
    monitor_destroy(file_monitor);

  if (thread_arg) {
    if (thread_arg->filename)
      free(thread_arg->filename);
    if (thread_arg->remote_name)
      free(thread_arg->remote_name);
    free(thread_arg);
  }

  if (conn_sockfd != -1)
    close(conn_sockfd);

  if (sockfd != -1)
    close(sockfd);

  if (write_file_fd != -1)
    close(write_file_fd);

  remove(filename);

  return ok;
}
