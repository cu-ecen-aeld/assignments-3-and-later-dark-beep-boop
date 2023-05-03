#ifndef AESDSOCKET_H
#define AESDSOCKET_H

#include <stdbool.h>
#include <sys/socket.h>

void handler(int signo);
bool recv_and_send(int socket_fd, int file_fd);
bool recv_and_write_line(int file_fd, int socket_fd);
bool recv_line(int socket_fd, char **line);
bool write_line(int file_fd, const char *line);
bool send_file(int socket_fd, int file_fd);
bool read_line(int file_fd, char **line, bool *eof);
bool send_line(int socket_fd, const char *line);
const void *get_in_addr(const struct sockaddr *sa);
bool open_listening_socket(int *sockfd, const char *port, int backlog);
bool daemonize(void);

#endif /* AESDSOCKET_H */
