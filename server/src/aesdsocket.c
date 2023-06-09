#include "aesdsocket.h"
#include "aesd_ioctl.h"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <bits/pthreadtypes.h>
#include <bits/time.h>
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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "aesd_ioctl.h"
#include "monitor.h"
#include "queue.h"
#include "try.h"

#define BUFFSIZE 1024

static volatile sig_atomic_t termination_flag = 0;
static volatile sig_atomic_t timestamp_flag = 0;

static void aesdsocket_terminate_handler(int signo);
static void aesdsocket_timestamp_handler(int signo);
static bool aesdsocket_daemonize(void);
static bool aesdsocket_open_listening_socket(
  int *sockfd,
  const char *port,
  int backlog);
static const void *aesdsocket_get_in_addr(const struct sockaddr *sa);
static bool aesdsocket_take_timestamp(
  const char *timestamp_format,
  const char *filename,
  monitor_t *file_monitor);

struct aesdsocket_thread_arg
{
  int conn_sockfd;
  char *filename;
  char *remote_name;
  monitor_t *file_monitor;
  const char *seekto_command;
};
typedef struct aesdsocket_thread_arg aesdsocket_thread_arg_t;

static void *aesdsocket_start_thread(void *arg);
static bool aesdsocket_serve(
  int socket_fd,
  const char *filename,
  monitor_t *file_monitor,
  const char *seekto_command);
static bool aesdsocket_recv_line(int socket_fd, char **line);
static bool aesdsocket_write_line(
  int file_fd,
  const char *line,
  monitor_t *file_monitor);
static bool aesdsocket_read_and_send_file(
  int socket_fd,
  int file_fd,
  monitor_t *file_monitor);
static bool aesdsocket_read_line(
  int file_fd,
  char **line,
  bool *eof,
  monitor_t *file_monitor);
static bool aesdsocket_send_line(int socket_fd, const char *line);

void
aesdsocket_terminate_handler(int signo)
{
  if (signo == SIGTERM || signo == SIGINT) {
    syslog(LOG_DEBUG, "Caught signal, exiting\n");
    termination_flag = 1;
  }
}

void
aesdsocket_timestamp_handler(int signo)
{
  if (signo == SIGALRM)
    timestamp_flag = 1;
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

  for (int i = 0; i < 3; ++i)
    close(i);

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
    result = &(((struct sockaddr_in *)sa)->sin_addr);
  else if (sa->sa_family == AF_INET6)
    result = &(((struct sockaddr_in6 *)sa)->sin6_addr);

  return result;
}

bool
aesdsocket_take_timestamp(
  const char *timestamp_format,
  const char *filename,
  monitor_t *file_monitor)
{
  bool ok = false;
  struct timespec timestamp;
  struct tm local_timestamp;
  char timestamp_buffer[BUFFSIZE] = "";
  size_t timestamp_size = 0;
  int file_fd = -1;

  TRYC_ERRNO(
    file_fd = open(
      filename,
      O_WRONLY | O_APPEND | O_CREAT,
      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

  TRYC_ERRNO(clock_gettime(CLOCK_REALTIME, &timestamp));
  TRY(
    localtime_r(&timestamp.tv_sec, &local_timestamp),
    "localtime timestamp conversion failed");
  TRY(
    timestamp_size =
      strftime(timestamp_buffer, BUFFSIZE, timestamp_format, &local_timestamp),
    "timestamp string creation failed");
  timestamp_buffer[timestamp_size] = '\n';
  timestamp_buffer[timestamp_size + 1] = '\0';

  syslog(LOG_DEBUG, "%s\n", timestamp_buffer);
  TRY(
    aesdsocket_write_line(file_fd, timestamp_buffer, file_monitor),
    "couldn't write the timestamp to the file");

  ok = true;

done:
  if (file_fd != -1)
    close(file_fd);

  return ok;
}

void *
aesdsocket_start_thread(void *arg)
{
  aesdsocket_thread_arg_t *thread_arg = arg;
  int conn_sockfd = thread_arg->conn_sockfd;
  char *filename = thread_arg->filename;
  char *remote_name = thread_arg->remote_name;
  monitor_t *file_monitor = thread_arg->file_monitor;
  const char *seekto_command = thread_arg->seekto_command;
  free(thread_arg);

  syslog(LOG_DEBUG, "Accepted connection from %s\n", remote_name);

  TRY(
    aesdsocket_serve(conn_sockfd, filename, file_monitor, seekto_command),
    "thread execution failed");

done:
  if (conn_sockfd != -1)
    close(conn_sockfd);

  syslog(LOG_DEBUG, "Closed connection from %s\n", remote_name);

  if (filename)
    free(filename);

  if (remote_name)
    free(remote_name);

  return NULL;
}

bool
aesdsocket_serve(
  int socket_fd,
  const char *filename,
  monitor_t *file_monitor,
  const char *seekto_command)
{
  bool ok = false;
  char *line = NULL;
  int file_fd = -1;
  struct aesd_seekto seekto_arg;
  char *command_ptr;
  size_t command_size;
  char *command_offset_ptr;
  size_t command_offset_size;
  char *end_ptr;
  char command_buffer[BUFFSIZE] = {};

  TRYC_ERRNO(
    file_fd = open(
      filename,
      O_RDWR | O_APPEND | O_CREAT,
      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

  TRY(aesdsocket_recv_line(socket_fd, &line), "line reception failed");
  if (line) {
    if (strncmp(line, seekto_command, strlen(seekto_command)) == 0) {
      command_ptr = strchr(line, ':');
      if (command_ptr) {
        ++command_ptr;
        command_offset_ptr = strchr(command_ptr, ',');
        if (command_offset_ptr) {
          command_size = command_offset_ptr - command_ptr;
          ++command_offset_ptr;
          end_ptr = strchr(command_offset_ptr, '\n');
          if (end_ptr) {
            command_offset_size = end_ptr - command_offset_ptr;
            strncpy(command_buffer, command_ptr, command_size);
            command_buffer[command_size] = '\0';
            seekto_arg.write_cmd = atoi(command_buffer);
            strncpy(command_buffer, command_offset_ptr, command_offset_size);
            command_buffer[command_offset_size] = '\0';
            seekto_arg.write_cmd_offset = atoi(command_buffer);
            TRYC_ERRNO(ioctl(file_fd, AESDCHAR_IOCSEEKTO, &seekto_arg));
          }
        }
      }
    } else {
      TRY(
        aesdsocket_write_line(file_fd, line, file_monitor),
        "line writing failed");

      close(file_fd);
      file_fd = -1;

      TRYC_ERRNO(
        file_fd = open(
          filename,
          O_RDONLY | O_APPEND | O_CREAT,
          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
    }
  }

  TRY(
    aesdsocket_read_and_send_file(socket_fd, file_fd, file_monitor),
    "line reading or sending failed");

  ok = true;

done:
  if (line)
    free(line);

  if (file_fd != -1)
    close(file_fd);

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

  while (!eol && !termination_flag) {
    TRYC_CONTINUE_ON_EINTR(bytes_read = recv(socket_fd, buffer, BUFFSIZE, 0));
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
        TRY(
          line_buffer =
            (char *)realloc(line_buffer, line_buffer_size + useful_bytes + 1),
          "memory allocation failed");
        strncpy(line_buffer + line_buffer_size, buffer, useful_bytes);
        line_buffer_size += useful_bytes;
      }
    }
  }

  if (line_buffer)
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
      bytes_written =
        write(file_fd, line + strlen(line) - bytes_to_write, bytes_to_write));
    bytes_to_write -= bytes_written;
  }
  monitor_stop_writing(file_monitor);

  ok = true;

done:
  return ok;
}

bool
aesdsocket_read_and_send_file(
  int socket_fd,
  int file_fd,
  monitor_t *file_monitor)
{
  bool ok = false;
  bool eof = false;
  char *line = NULL;

  while (!eof) {
    TRY(
      aesdsocket_read_line(file_fd, &line, &eof, file_monitor),
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
  int file_fd,
  char **line,
  bool *eof,
  monitor_t *file_monitor)
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

  monitor_start_reading(file_monitor);
  while (!eol && !eof_local) {
    TRYC_RETRY_ON_EINTR(bytes_read = read(file_fd, buffer, BUFFSIZE));
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
        TRY_ERRNO(
          line_buffer =
            (char *)realloc(line_buffer, line_buffer_size + useful_bytes + 1));
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

  while (bytes_to_send > 0 && !termination_flag) {
    ssize_t bytes_sent;
    TRYC_RETRY_ON_EINTR(
      bytes_sent =
        send(socket_fd, line + strlen(line) - bytes_to_send, bytes_to_send, 0));
    bytes_to_send -= bytes_sent;
  }

  ok = true;

done:
  return ok;
}

bool
aesdsocket_mainloop(
  const char *port,
  int backlog,
  const char *filename,
  bool is_regular_file,
  bool daemon,
  bool use_timestamp,
  time_t timestamp_frequency_seconds,
  const char *timestamp_format,
  const char *seekto_command)
{
  bool ok = false;
  struct sigaction action;
  int sockfd = -1;
  int conn_sockfd = -1;
  char remote_name[INET6_ADDRSTRLEN] = {};
  struct sockaddr_storage remote_addr;
  socklen_t addr_size = sizeof(struct sockaddr_storage);
  queue_t *thread_queue = NULL;
  monitor_t *write_file_monitor = NULL;
  aesdsocket_thread_arg_t *thread_arg = NULL;
  timer_t timestamp_timer = NULL;
  struct itimerspec timestamp_frequency;

  if (daemon)
    TRY(aesdsocket_daemonize(), "daemonization failed");

  memset(&action, 0, sizeof(action));
  action.sa_handler = aesdsocket_terminate_handler;
  sigemptyset(&action.sa_mask);
  TRYC_ERRNO(sigaddset(&action.sa_mask, SIGTERM));
  TRYC_ERRNO(sigaddset(&action.sa_mask, SIGINT));
  TRYC_ERRNO(sigaction(SIGTERM, &action, NULL));
  TRYC_ERRNO(sigaction(SIGINT, &action, NULL));

  TRY(
    aesdsocket_open_listening_socket(&sockfd, port, backlog),
    "Couldn't open server socket");

  TRY(thread_queue = queue_new(), "queue creation failed");
  TRY(write_file_monitor = monitor_new(), "monitor creation failed");

  if (use_timestamp) {
    memset(&action, 0, sizeof(action));
    action.sa_handler = aesdsocket_timestamp_handler;
    sigemptyset(&action.sa_mask);
    TRYC_ERRNO(sigaddset(&action.sa_mask, SIGALRM));
    TRYC_ERRNO(sigaction(SIGALRM, &action, NULL));

    TRYC_ERRNO(timer_create(CLOCK_MONOTONIC, NULL, &timestamp_timer));

    memset(&timestamp_frequency, 0, sizeof(timestamp_frequency));
    timestamp_frequency.it_value.tv_sec = timestamp_frequency_seconds;
    timestamp_frequency.it_interval.tv_sec = timestamp_frequency_seconds;
    TRYC_ERRNO(timer_settime(timestamp_timer, 0, &timestamp_frequency, NULL));
  }

  while (!termination_flag) {
    if (timestamp_flag) {
      TRY(
        aesdsocket_take_timestamp(
          timestamp_format,
          filename,
          write_file_monitor),
        "couldn't take timestamp");

      timestamp_flag = 0;
    }

    TRYC_CONTINUE_ON_EINTR(
      conn_sockfd =
        accept(sockfd, (struct sockaddr *)&remote_addr, &addr_size));

    const char *in_addr =
      aesdsocket_get_in_addr((struct sockaddr *)&remote_addr);
    TRY_ERRNO(inet_ntop(
      remote_addr.ss_family,
      in_addr,
      remote_name,
      sizeof remote_name));

    TRY_ALLOCATE(thread_arg, aesdsocket_thread_arg_t);
    thread_arg->conn_sockfd = conn_sockfd;
    TRY_ERRNO(thread_arg->filename = strdup(filename));
    TRY_ERRNO(thread_arg->remote_name = strndup(remote_name, INET6_ADDRSTRLEN));
    thread_arg->file_monitor = write_file_monitor;
    thread_arg->seekto_command = seekto_command;

    pthread_t tid;
    int status;
    TRY_PTHREAD_CREATE(&tid, NULL, aesdsocket_start_thread, thread_arg, status);
    TRY(queue_enqueue(thread_queue, tid), "tid enqueue failed");
    thread_arg = NULL;
  }

  ok = true;

done:
  if (timestamp_timer)
    timer_delete(timestamp_timer);

  if (thread_queue) {
    while (!queue_is_empty(thread_queue)) {
      pthread_t tid = queue_dequeue(thread_queue);
      int status;
      TRY_PTHREAD_JOIN_NOACTION(tid, NULL, status);
    }
    queue_destroy(thread_queue);
  }

  if (write_file_monitor)
    monitor_destroy(write_file_monitor);

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

  if (is_regular_file)
    remove(filename);

  return ok;
}
