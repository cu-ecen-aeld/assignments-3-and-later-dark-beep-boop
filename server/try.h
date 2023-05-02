#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>

#define TRYC_NONBLOCK(expr, action)                         \
  if ((expr) == -1) {                                       \
    if (errno == EAGAIN || errno == EWOULDBLOCK) {          \
      usleep(5000);                                         \
      action;                                               \
    } else {                                                \
      syslog(                                               \
          LOG_ERR,                                          \
          "ERROR (file=%s, line=%d, function=%s): %s\n",    \
          __FILE__,                                         \
          __LINE__,                                         \
          __func__,                                         \
          strerror(errno));                                 \
      goto done;                                            \
    }                                                       \
  } NULL

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


