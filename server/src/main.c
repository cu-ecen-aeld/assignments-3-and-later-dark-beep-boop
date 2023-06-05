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
#ifdef USE_AESD_CHAR_DEVICE
#define FILENAME "/dev/aesdchar"
#define ISREGULAR false
#define USESTAMP false
#else
#define FILENAME "/var/tmp/aesdsocketdata"
#define ISREGULAR true
#define USESTAMP true
#endif /* USE_AESD_CHAR_DEVICE */
#define STAMPFREQSEC 10
#define STAMPFORMAT "timestamp:%a, %d %b %Y %T %z"
#define SEEKTO_COMMAND "AESDCHAR_IOCSEEKTO"

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
      ISREGULAR,
      daemon,
      USESTAMP,
      STAMPFREQSEC,
      STAMPFORMAT,
      SEEKTO_COMMAND),
    "execution failed");

  exit_status = EXIT_SUCCESS;

done:
  exit(exit_status);
}
