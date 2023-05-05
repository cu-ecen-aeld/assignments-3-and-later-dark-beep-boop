#include <errno.h>
#include <netdb.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>

#define LOG_ERROR(message)                           \
  syslog(                                            \
      LOG_ERR,                                       \
      "ERROR (file=%s, line=%d, function=%s): %s\n", \
      __FILE__,                                      \
      __LINE__,                                      \
      __func__,                                      \
      message)

#define TRYCATCH_RETRY(                                            \
    error_condition, error_action, retry_condition, error_message) \
  while (error_condition) {                                        \
    if (!(retry_condition)) {                                      \
      LOG_ERROR(error_message);                                    \
      error_action;                                                \
      break;                                                       \
    }                                                              \
  }                                                                \
  NULL

#define TRY_RETRY(expr, retry_condition, error_message) \
  TRYCATCH_RETRY(!(expr), goto done, retry_condition, error_message)

#define TRYC_RETRY(expr, retry_condition, error_message) \
  TRYCATCH_RETRY((expr) == -1, goto done, retry_condition, error_message)

#define TRY_RETRY_ERRNO(expr, retry_condition) \
  TRY_RETRY(expr, retry_condition, strerror(errno))

#define TRYC_RETRY_ERRNO(expr, retry_condition) \
  TRYC_RETRY(expr, retry_condition, strerror(errno))

#define TRY_RETRY_ON_EINTR(expr) TRY_RETRY_ERRNO(expr, errno == EINTR)
#define TRYC_RETRY_ON_EINTR(expr) TRYC_RETRY_ERRNO(expr, errno == EINTR)

#define TRYCATCH(condition, action, message) \
  if (condition) {                           \
    LOG_ERROR(message);                      \
    action;                                  \
  }                                          \
  NULL

#define TRY(expr, message) TRYCATCH(!(expr), goto done, message)

#define TRYC(expr, message) TRYCATCH((expr) == -1, goto done, message)

#define TRY_ERRNO(expr) TRY(expr, strerror(errno))

#define TRYC_ERRNO(expr) TRYC(expr, strerror(errno))

#define TRY_GETADDRINFO(node, service, hints, res, status_var)      \
  if ((status_var = getaddrinfo(node, service, hints, res)) != 0) { \
    syslog(                                                         \
        LOG_ERR,                                                    \
        "GETADDRINFO ERROR (file=%s, line=%d, function=%s): %s\n",  \
        __FILE__,                                                   \
        __LINE__,                                                   \
        __func__,                                                   \
        gai_strerror(status_var));                                  \
    goto done;                                                      \
  }                                                                 \
  NULL
