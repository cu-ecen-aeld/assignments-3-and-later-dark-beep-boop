#include <stdio.h>
#include <errno.h>

#define ERROR_LOG(format, ...)                                  \
    fprintf(                                                    \
        stderr,                                                 \
        "ERROR (file=%s, line=%d, function=%s): " format "\n",  \
        __FILE__,                                               \
        __LINE__,                                               \
        __func__,                                               \
        ##__VA_ARGS__)

#ifdef DEBUG
#define DEBUG_LOG(format, ...)                                      \
    printf(                                                         \
        "DEBUG(filename=%s, line=%d, function=%s): " format "\n",   \
        basename(__FILE__),                                         \
        __LINE__,                                                   \
        __func__,                                                   \
        ##__VA_ARGS__)
#else
#define DEBUG_LOG(format, ...)
#endif

#define TRY_NONZERO(expr)                                       \
    if ((expr) == 0) {                                          \
        fprintf(                                                \
            stderr,                                             \
            "EXCEPTION (file=%s, line=%d, function=%s): %s\n",  \
            __FILE__,                                           \
            __LINE__,                                           \
            __func__,                                           \
            #expr);                                             \
        perror("ERROR (" #expr ")");                            \
        goto done;                                              \
    } NULL

#define TRY_NONNEG(expr)                                        \
    if ((expr) < 0) {                                           \
        fprintf(                                                \
            stderr,                                             \
            "EXCEPTION (file=%s, line=%d, function=%s): %s\n",  \
            __FILE__,                                           \
            __LINE__,                                           \
            __func__,                                           \
            #expr);                                             \
        perror("ERROR (" #expr ")");                            \
        goto done;                                              \
    } NULL

#define TRY_GETERRNO(expr)                                      \
    expr;                                                       \
    if (errno) {                                                \
        fprintf(                                                \
            stderr,                                             \
            "EXCEPTION (file=%s, line=%d, function=%s): %s\n",  \
            __FILE__,                                           \
            __LINE__,                                           \
            __func__,                                           \
            #expr);                                             \
        perror("ERROR (" #expr ")");                            \
        goto done;                                              \
    } NULL

#define TRY_SETERRNO(expr)                                      \
    if ((errno = (expr)) != 0) {                                \
        fprintf(                                                \
            stderr,                                             \
            "EXCEPTION (file=%s, line=%d, function=%s): %s\n",  \
            __FILE__,                                           \
            __LINE__,                                           \
            __func__,                                           \
            #expr);                                             \
        perror("ERROR (" #expr ")");                            \
        goto done;                                              \
    } NULL

