#ifndef MONITOR_H
#define MONITOR_H

#include <pthread.h>
#include <stdbool.h>

struct monitor
{
  unsigned active_readers;
  unsigned active_writers;
  unsigned waiting_readers;
  unsigned waiting_writers;
  pthread_cond_t can_read;
  pthread_cond_t can_write;
  pthread_mutex_t condition_lock;
};
typedef struct monitor monitor_t;

bool monitor_initialize(monitor_t *self);
void monitor_finalize(monitor_t *self);

monitor_t *monitor_new(void);
void monitor_destroy(monitor_t *self);

void monitor_start_reading(monitor_t *self);
void monitor_stop_reading(monitor_t *self);
void monitor_start_writing(monitor_t *self);
void monitor_stop_writing(monitor_t *self);

#endif /* MONITOR_H */
