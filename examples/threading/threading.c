#include "threading.h"
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <errno.h>
#include <string.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
//#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

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

#define TRY(expr)                                               \
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

#define TRYC(expr)                                              \
    if ((expr) == -1) {                                         \
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

#define TRYE(expr)                                              \
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

#define TRYPTHREADS(expr)                                       \
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


void *
threadfunc(void* thread_param)
{
    struct thread_data *args = (struct thread_data *) thread_param;

    args->thread_complete_success = false;
    TRYE(usleep(args->wait_to_obtain_ms * 1000));
    TRYC(pthread_mutex_lock(args->mutex));
    TRYE(usleep(args->wait_to_release_ms * 1000));
    TRYC(pthread_mutex_unlock(args->mutex));
    args->thread_complete_success = true;

done:
    return thread_param;
}

bool
start_thread_obtaining_mutex(
    pthread_t *thread,
    pthread_mutex_t *mutex,
    int wait_to_obtain_ms,
    int wait_to_release_ms)
{
    bool ok = false;
    struct thread_data *params = NULL;

    TRY(params = (struct thread_data *) malloc(sizeof(struct thread_data)));
    memset(params, 0, sizeof(struct thread_data));
    params->mutex = mutex;
    params->wait_to_obtain_ms = wait_to_obtain_ms;
    params->wait_to_release_ms = wait_to_release_ms;

    TRYPTHREADS(pthread_create(thread, NULL, threadfunc, params));
    ok = thread != 0;

done:
    return ok;
}

