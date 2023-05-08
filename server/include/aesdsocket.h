#ifndef AESDSOCKET_H
#define AESDSOCKET_H

#include <stdbool.h>
#include <time.h>

bool aesdsocket_mainloop(
  const char *port,
  int backlog,
  const char *filename,
  bool daemon,
  time_t timestamp_frequency_seconds,
  const char *timestamp_format);

#endif /* AESDSOCKET_H */
