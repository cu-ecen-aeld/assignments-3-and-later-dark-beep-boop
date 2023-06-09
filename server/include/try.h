#ifndef TRY_H
#define TRY_H

#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>

#define LOG_ERROR(message)                                                     \
  syslog(                                                                      \
    LOG_ERR,                                                                   \
    "ERROR (file=%s, line=%d, function=%s): %s\n",                             \
    __FILE__,                                                                  \
    __LINE__,                                                                  \
    __func__,                                                                  \
    message)

#define TRYCATCH_RETRY(                                                        \
  error_condition,                                                             \
  error_action,                                                                \
  retry_condition,                                                             \
  error_message)                                                               \
  while (error_condition) {                                                    \
    if (!(retry_condition)) {                                                  \
      LOG_ERROR(error_message);                                                \
      error_action;                                                            \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  NULL

#define TRY_RETRY(expr, retry_condition, error_message)                        \
  TRYCATCH_RETRY(!(expr), goto done, retry_condition, error_message)

#define TRYC_RETRY(expr, retry_condition, error_message)                       \
  TRYCATCH_RETRY((expr) == -1, goto done, retry_condition, error_message)

#define TRY_RETRY_ERRNO(expr, retry_condition)                                 \
  TRY_RETRY(expr, retry_condition, strerror(errno))

#define TRYC_RETRY_ERRNO(expr, retry_condition)                                \
  TRYC_RETRY(expr, retry_condition, strerror(errno))

#define TRY_RETRY_ON_EINTR(expr) TRY_RETRY_ERRNO(expr, errno == EINTR)

#define TRYC_RETRY_ON_EINTR(expr) TRYC_RETRY_ERRNO(expr, errno == EINTR)

#define TRYCATCH_CONTINUE(                                                     \
  error_condition,                                                             \
  error_action,                                                                \
  continue_condition,                                                          \
  error_message)                                                               \
  if (error_condition) {                                                       \
    if (continue_condition) {                                                  \
      continue;                                                                \
    } else {                                                                   \
      LOG_ERROR(error_message);                                                \
      error_action;                                                            \
    }                                                                          \
  }                                                                            \
  NULL

#define TRY_CONTINUE(expr, continue_condition, error_message)                  \
  TRYCATCH_CONTINUE(!(expr), goto done, continue_condition, error_message)

#define TRYC_CONTINUE(expr, continue_condition, error_message)                 \
  TRYCATCH_CONTINUE((expr) == -1, goto done, continue_condition, error_message)

#define TRY_CONTINUE_ERRNO(expr, continue_condition)                           \
  TRY_CONTINUE(expr, continue_condition, strerror(errno))

#define TRYC_CONTINUE_ERRNO(expr, continue_condition)                          \
  TRYC_CONTINUE(expr, continue_condition, strerror(errno))

#define TRY_CONTINUE_ON_EINTR(expr) TRY_CONTINUE_ERRNO(expr, errno == EINTR)

#define TRYC_CONTINUE_ON_EINTR(expr) TRYC_CONTINUE_ERRNO(expr, errno == EINTR)

#define TRYCATCH(condition, action, message)                                   \
  if (condition) {                                                             \
    LOG_ERROR(message);                                                        \
    action;                                                                    \
  }                                                                            \
  NULL

#define TRY(expr, message) TRYCATCH(!(expr), goto done, message)

#define TRYC(expr, message) TRYCATCH((expr) == -1, goto done, message)

#define TRY_ERRNO(expr) TRY(expr, strerror(errno))

#define TRYC_ERRNO(expr) TRYC(expr, strerror(errno))

#define TRY_GETADDRINFO(node, service, hints, res, status_var)                 \
  if ((status_var = getaddrinfo(node, service, hints, res)) != 0) {            \
    LOG_ERROR(gai_strerror(status_var));                                       \
    goto done;                                                                 \
  }                                                                            \
  NULL

#define TRY_ALLOCATE_MANY(dest, type, number)                                  \
  TRY_ERRNO((dest) = (type *)malloc(sizeof(type) * (number)))

#define TRY_ALLOCATE(dest, type) TRY_ALLOCATE_MANY(dest, type, 1)

#define TRY_PTHREAD_CREATE(thread, attr, start_routine, arg, status_var)       \
  if ((status_var = pthread_create(thread, attr, start_routine, arg))) {       \
    errno = status_var;                                                        \
    LOG_ERROR(strerror(errno));                                                \
    goto done;                                                                 \
  }                                                                            \
  NULL

#define TRY_PTHREAD_JOIN_NOACTION(thread, retval, status_var)                  \
  if ((status_var = pthread_join(thread, retval))) {                           \
    errno = status_var;                                                        \
    LOG_ERROR(strerror(errno));                                                \
  }                                                                            \
  NULL

#define TRY_PTHREAD_JOIN(thread, retval, status_var)                           \
  if ((status_var = pthread_join(thread, retval))) {                           \
    errno = status_var;                                                        \
    LOG_ERROR(strerror(errno));                                                \
    goto done;                                                                 \
  }                                                                            \
  NULL

#endif /* TRY_H */
