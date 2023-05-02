#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
#include <stdbool.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "try.h"

#define PORT "9000"
#define BACKLOG 20
#define FILENAME "/var/tmp/aesdsocketdata"
#define BUFFSIZE 1024

static bool terminate = false;

void handler(int signo);
void recv_and_send(int socket_fd, int writer_fd);
void recv_packet(int socket_fd, int writer_fd);
void send_file(int socket_fd);
void *get_in_addr(struct sockaddr *sa);
bool open_listening_socket(int *sockfd, const char *port, int backlog);
bool daemonize(void);

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

  struct sigaction action;
  memset(&action, 0, sizeof(struct sigaction));
  action.sa_handler = handler;
  TRYC(sigaction(SIGTERM, &action, NULL));
  TRYC(sigaction(SIGINT, &action, NULL));

  if (daemon)
    TRY(daemonize());

  int fd;
  TRYC(fd = open(
    FILENAME,
    O_RDWR | O_APPEND | O_CREAT,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

  int sockfd;
  TRY(open_listening_socket(&sockfd, PORT, BACKLOG));

  int conn_sockfd;
  char remote_name[INET6_ADDRSTRLEN];
  socklen_t addr_size;
  struct sockaddr_storage remote_addr;

  while (!terminate) {
    TRYC_NONBLOCK(
        conn_sockfd = accept(
            sockfd,
            (struct sockaddr *) &remote_addr,
            &addr_size),
        continue);

    inet_ntop(
        remote_addr.ss_family,
        get_in_addr((struct sockaddr *) &remote_addr),
        remote_name,
        sizeof remote_name);
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

void
recv_and_send(int socket_fd, int writer_fd)
{
  recv_packet(socket_fd, writer_fd);
  send_file(socket_fd);
}

bool
daemonize(void)
{
  bool ok = false;

  pid_t daemon_pid;
  TRYC(daemon_pid = fork());

  if (daemon_pid != 0)
    exit(EXIT_SUCCESS);

  TRYC(setsid());

  TRYC(chdir("/"));

  for (int i = 0; i < 3; ++i)
    close(i);

  open("/dev/null", O_RDWR);
  dup(0);
  dup(0);

  ok = true;

done:
  return ok;
}

bool
open_listening_socket(int *sockfd, const char *port, int backlog)
{
  bool ok = false;
  int sockfd_test;
  int status;
  int yes = 1;
  struct addrinfo hints;
  struct addrinfo *servinfo = NULL;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  TRY_GETADDRINFO(getaddrinfo(NULL, PORT, &hints, &servinfo), status);

  TRYC(sockfd_test = socket(servinfo->ai_family, SOCK_STREAM | SOCK_NONBLOCK, 0));
  TRYC(setsockopt(sockfd_test, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)));

  TRYC(bind(sockfd_test, servinfo->ai_addr, servinfo->ai_addrlen));
  TRYC(listen(sockfd_test, BACKLOG));

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
handler(int signo)
{
  if (signo == SIGTERM || signo == SIGINT) {
    syslog(LOG_DEBUG, "Caught signal, exiting\n");
    terminate = true;
  }
}

void
recv_packet(int socket_fd, int writer_fd) {
  ssize_t bytes_read, bytes_written;
  char buffer[BUFFSIZE] = {};
  bool eol = false;

  while (!eol){
    if ((bytes_read = recv(socket_fd, buffer, BUFFSIZE, 0)) != 0) {
      if (bytes_read == -1) {
        if (errno == EINTR) {
          continue;
        }
        syslog(
            LOG_ERR,
            "ERROR (file=%s, line=%d, function=%s): %s\n",
            __FILE__,
            __LINE__,
            __func__,
            "recv"); 
        return;
      }	

      if (strchr(buffer,'\n') != NULL)
        eol = 1;

      while ((bytes_written = write(writer_fd, buffer, bytes_read)) == -1) {
        if (errno != EINTR) {
          syslog(
            LOG_ERR,
            "ERROR (file=%s, line=%d, function=%s): %s\n",
            __FILE__,
            __LINE__,
            __func__,
            "recv"); 
          return;
        }
      }
    }
  }

  return;
}

void
send_file(int socket_fd) {
  ssize_t bytes_read, bytes_written;
  char buffer[BUFFSIZE] = "";
  int fd;
  fd = open(
      FILENAME,
      O_RDWR | O_APPEND | O_CREAT,
      S_IRUSR | S_IRGRP | S_IROTH);
  while ((bytes_read = read(fd, buffer, BUFFSIZE)) != 0) {
    if (bytes_read == -1) {
      if (errno == EINTR) {
        continue;
      }
      syslog(
          LOG_ERR,
          "ERROR (file=%s, line=%d, function=%s): %s\n",
          __FILE__,
          __LINE__,
          __func__,
          "recv"); 
      return;
    }

    while ((bytes_written = write(socket_fd, buffer, bytes_read)) == -1) {
      if (errno != EINTR){
        syslog(
            LOG_ERR,
            "ERROR (file=%s, line=%d, function=%s): %s\n",
            __FILE__,
            __LINE__,
            __func__,
            "recv");
        return;
      }
    }
  }

  close(fd);
  return;
}

void *
get_in_addr(struct sockaddr *sa)
{
  void *result = NULL;

  if (sa->sa_family == AF_INET)
    result = &(((struct sockaddr_in*)sa)->sin_addr);
  else if (sa->sa_family == AF_INET6)
    result = &(((struct sockaddr_in6*)sa)->sin6_addr);

  return result;
}


