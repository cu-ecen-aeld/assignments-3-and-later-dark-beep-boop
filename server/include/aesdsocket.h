#ifndef AESDSOCKET_H
#define AESDSOCKET_H

#include <stdbool.h>
#include <time.h>

bool aesdsocket_mainloop(
  const char *port,
  int backlog,
  const char *filename,
  bool is_regular_file,
  bool daemon,
  bool use_timestamp,
  time_t timestamp_frequency_seconds,
  const char *timestamp_format,
  const char *seekto_command);

#endif /* AESDSOCKET_H */
