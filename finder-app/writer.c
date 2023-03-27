#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#define TRY(expr)                       \
  if (!(expr)) {                        \
    fprintf(                            \
      stderr,                           \
      "Error in file %s, line %d: %s\n",\
      __FILE__,                         \
      __LINE__,                         \
      #expr);                           \
    goto done;                          \
  } NULL

#define TRYC(expr, function_name)       \
  if ((expr) == -1) {                   \
    perror(function_name);              \
    fprintf(                            \
      stderr,                           \
      "Error in file %s, line %d: %s\n",\
      __FILE__,                         \
      __LINE__,                         \
      #expr);                           \
    goto done;                          \
  } NULL


int main(int argc, char *argv[])
{
  if (argc < 3) {
    printf("Usage: %s <writefile> <writestr>\n", argv[0]);
    return 1;
  }

  int error = 1;

  char *writefile = NULL;
  char *writestr = NULL;
  TRY(writefile = strdup(argv[1]));
  TRY(writestr = strdup(argv[2]));

  int fd = -2;
  TRYC(
    fd = creat(writefile, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH),
    "creat");
  
  ssize_t written = -2;
  syslog(LOG_DEBUG, "Writing %s to %s\n", writestr, writefile);
  TRYC(written = write(fd, writestr, strlen(writestr)), "write");

  error = 0;

 done:
  if (fd == -1)
    syslog(LOG_ERR, "File %s couldn't be created\n", writefile);

  if (written == -1)
    syslog(
      LOG_ERR, 
      "The message \"%s\" couldn't be written to file %s\n", 
      writestr, 
      writefile);

  if (writefile)
    free(writefile);

  if (writestr)
    free(writestr);

  if (fd != -2 && fd != -1)
    if (close(fd) == -1)
      perror("close");

  return error;
}

