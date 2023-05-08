#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/syslog.h>
#include <unistd.h>

#include "aesdsocket.h"
#include "queue.h"
#include "try.h"

#define PORT "9000"
#define BACKLOG 20
#define FILENAME "/var/tmp/aesdsocketdata"
#define STAMPFREQSEC 10
#define STAMPFORMAT "timestamp:%a, %d %b %Y %T %z"

int
main(int argc, char *argv[])
{
  int exit_status = EXIT_FAILURE;
  bool daemon = false;

  openlog("aesdsocket", LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);

  if (argc > 1) {
    if (argc == 2 && strncmp(argv[1], "-d", 2) == 0) {
      daemon = true;
    } else {
      syslog(LOG_ERR, "ERROR: wrong arguments\n");
      return EXIT_FAILURE;
    }
  }

  TRY(
    aesdsocket_mainloop(
      PORT,
      BACKLOG,
      FILENAME,
      daemon,
      STAMPFREQSEC,
      STAMPFORMAT),
    "execution failed");

  exit_status = EXIT_SUCCESS;

done:
  exit(exit_status);
}
