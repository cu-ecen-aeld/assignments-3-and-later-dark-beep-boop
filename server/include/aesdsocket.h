#ifndef AESDSOCKET_H
#define AESDSOCKET_H

#include <stdbool.h>

bool aesdsocket_mainloop(
    const char *port, int backlog, const char *filename, bool daemon);

#endif /* AESDSOCKET_H */
