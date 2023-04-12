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

#include "try.h"

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
main()
{
  int backlog = 20;
  int conn_sockfd = 0;
  int exit_status = -1;
  int fd = 0;
  int sockfd = 0;
  int curr_status;
  int linesize = 0;
  int yes = 1;
  int written = 0;
  char remote_name[INET6_ADDRSTRLEN];
  char buffer[256];
  char *line = NULL;
  const char port[] = "9000";
  const char filename[] = "/var/tmp/aesdsocketdata";
  socklen_t addr_size;
  struct addrinfo hints;
  struct addrinfo *servinfo = NULL;
  struct sockaddr_storage remote_addr;
  
  memset(buffer, 0, sizeof buffer);
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((curr_status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    fprintf(
        stderr,
        "GETADDRINFO ERROR (file=%s, line=%d, function=%s): %s\n",
        __FILE__,
        __LINE__,
        __func__,
        gai_strerror(curr_status));
    goto done;
  }

  TRY_NONNEG(sockfd = socket(
      servinfo->ai_family,
      servinfo->ai_socktype,
      servinfo->ai_protocol));
  TRY_NONNEG(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)));
  TRY_NONNEG(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen));
  TRY_NONNEG(listen(sockfd, backlog));

  TRY_NONNEG(fd = open(
    filename,
    O_RDWR | O_APPEND | O_CREAT,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));

  TRY_NONNEG(conn_sockfd = accept(
      sockfd,
      (struct sockaddr *) &remote_addr,
      &addr_size));
  inet_ntop(
      remote_addr.ss_family,
      get_in_addr((struct sockaddr *) &remote_addr),
      remote_name,
      sizeof remote_name);
  syslog(LOG_DEBUG, "Accepted connection from %s\n", remote_name);

  /* net to file */
  size_t pos = sizeof buffer;
  while (pos == sizeof buffer) {
    TRY_NONNEG(curr_status = recv(conn_sockfd, buffer, sizeof buffer, 0));

    if (curr_status == 0) {
      ERROR_LOG("Connection closed");
      goto done;
    }
    
    pos = 0;
    while (buffer[pos] != '\n' && pos < sizeof buffer)
      ++pos;

    TRY_NONZERO(line = realloc(line, linesize + pos));
    memcpy(line + linesize, buffer, pos);
    linesize += pos;
  }
  linesize += 2;
  TRY_NONZERO(line = realloc(line, linesize));
  line[linesize - 2] = '\n';
  line[linesize - 1] = 0;
  TRY_NONNEG(written = write(fd, line, strlen(line)));
  free(line);
  linesize = 0;
  memset(buffer, 0, sizeof buffer);

  /* file to net */
  TRY_NONNEG(lseek(fd, 0, SEEK_SET));

  pos = sizeof buffer;
  ssize_t ret;
  ssize_t len = sizeof buffer;
  char *buffer_p = buffer;
  while (pos == sizeof buffer) {
    while (len != 0 && (ret = read(fd, buffer_p, len)) != 0) {
      if (ret == -1) {
        if (errno == EINTR) {
          continue;
        }
        perror("ERROR: read");
        goto done;
      }
      len -= ret;
      buffer_p += ret;
    }

    pos = 0;
    while (buffer[pos] != '\n' && pos < sizeof buffer)
      ++pos;

    TRY_NONZERO(line = realloc(line, linesize + pos));
    memcpy(line + linesize, buffer, pos);
    linesize += pos;
  }

  exit_status = EXIT_SUCCESS;

done:
  if (line)
    free(line);
  if (fd)
    close(fd);
  if (conn_sockfd)
    close(conn_sockfd);
  if (sockfd)
    close(sockfd);
  if (servinfo)
    freeaddrinfo(servinfo);
  exit(exit_status);
}

