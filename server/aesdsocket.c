#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
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
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define PORT "9000"
#define BACKLOG 20
#define FILENAME "/var/tmp/aesdsocketdata"
#define BUFFSIZE 1024

#define TRY_GETADDRINFO(expr, status)                             \
  if ((status = expr) != 0) {                                     \
    syslog(                                                       \
        LOG_ERR,                                                  \
        "GETADDRINFO ERROR (file=%s, line=%d, function=%s): %s\n",\
        __FILE__,                                                 \
        __LINE__,                                                 \
        __func__,                                                 \
        gai_strerror(status));                                    \
    goto done;                                                    \
  } NULL

#define TRYC(expr)                                      \
  if ((expr) == -1) {                                   \
    syslog(                                             \
        LOG_ERR,                                        \
        "ERROR (file=%s, line=%d, function=%s): %s\n",  \
        __FILE__,                                       \
        __LINE__,                                       \
        __func__,                                       \
        strerror(errno));                               \
    goto done;                                          \
  } NULL

#define TRY(expr)                                       \
  if ((expr) == 0) {                                    \
    syslog(                                             \
        LOG_ERR,                                        \
        "ERROR (file=%s, line=%d, function=%s): %s\n",  \
        __FILE__,                                       \
        __LINE__,                                       \
        __func__,                                       \
        #expr);                                         \
    goto done;                                          \
  } NULL

bool terminate = false;

void
handler (int signo)
{
  if (signo == SIGTERM || signo == SIGINT) {
    syslog(LOG_DEBUG, "Caught signal, exiting\n");
    terminate = true;
  }
}

void
write_line(int socket_fd, int writer_fd) {
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
send_contents(int socket_fd) {
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
  else
    result = &(((struct sockaddr_in6*)sa)->sin6_addr);

  return result;
}

int
main(int argc, char *argv[])
{
  int exit_status = -1;

  struct sigaction action;
  memset(&action,0,sizeof(struct sigaction));
  action.sa_handler=handler;
  if(sigaction(SIGTERM, &action, NULL) != 0) {
    syslog(
        LOG_ERR,
        "ERROR (file=%s, line=%d, function=%s): %s\n",
        __FILE__,
        __LINE__,
        __func__,
        "recv");
    goto done;
  }
  if(sigaction(SIGINT, &action, NULL) != 0) {
    syslog(
        LOG_ERR,
        "ERROR (file=%s, line=%d, function=%s): %s\n",
        __FILE__,
        __LINE__,
        __func__,
        "recv");
    goto done;
  }

  /* Socket initialization */
  struct addrinfo hints;
  struct addrinfo *servinfo = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int status;
  TRY_GETADDRINFO(getaddrinfo(NULL, PORT, &hints, &servinfo), status);

  int sockfd;
  TRYC(sockfd = socket(
    servinfo->ai_family,
    SOCK_STREAM | SOCK_NONBLOCK,
    0));

  int yes = 1;
  TRYC(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)));

  TRYC(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen));
  TRYC(listen(sockfd, BACKLOG));

  pid_t daemon_pid;
  if (argc == 2) {
    if(strcmp(argv[1], "-d") == 0){
      daemon_pid = fork();
      if (daemon_pid == -1)
        return -1;
      else if (daemon_pid != 0)
        exit(EXIT_SUCCESS);

      setsid();
      chdir("/");
      open("/dev/null",O_RDWR);
      dup(0);
      dup(0);
    }
  }

  int fd;
  TRYC(fd = open(
    FILENAME,
    O_RDWR | O_APPEND | O_CREAT,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

  int conn_sockfd;
  char remote_name[INET6_ADDRSTRLEN];
  socklen_t addr_size;
  struct sockaddr_storage remote_addr;

  while (1) {
    if (terminate)
      goto done;

    if ((conn_sockfd = accept(
      sockfd,
      (struct sockaddr *) &remote_addr,
      &addr_size)) == -1) {
      if (errno == EAGAIN) {
        usleep(5000);
        continue;
      } else {
        syslog(
            LOG_ERR,
            "ERROR (file=%s, line=%d, function=%s): %s\n",
            __FILE__,
            __LINE__,
            __func__,
            strerror(errno));
        goto done;
      }
    }

    inet_ntop(
        remote_addr.ss_family,
        get_in_addr((struct sockaddr *) &remote_addr),
        remote_name,
        sizeof remote_name);
    syslog(LOG_DEBUG, "Accepted connection from %s\n", remote_name);

    write_line(conn_sockfd, fd);
    send_contents(conn_sockfd);

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
  if (servinfo)
    freeaddrinfo(servinfo);
  exit(exit_status);
}

