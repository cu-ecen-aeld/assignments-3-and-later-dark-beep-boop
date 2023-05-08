#include "monitor.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "try.h"

static void monitor_clear(monitor_t *self);

void
monitor_clear(monitor_t *self)
{
  memset(self, 0, sizeof(monitor_t));
}

bool
monitor_initialize(monitor_t *self)
{
  monitor_clear(self);

  pthread_cond_init(&self->can_read, NULL);
  pthread_cond_init(&self->can_write, NULL);
  pthread_mutex_init(&self->condition_lock, NULL);

  return true;
}

void
monitor_finalize(monitor_t *self)
{
  pthread_cond_destroy(&self->can_read);
  pthread_cond_destroy(&self->can_write);
  pthread_mutex_destroy(&self->condition_lock);
}

monitor_t *
monitor_new(void)
{
  monitor_t *new_object = NULL;
  monitor_t *object = NULL;

  TRY_ALLOCATE(new_object, monitor_t);
  TRY(monitor_initialize(new_object), "monitor initalization failed");

  object = new_object;

done:
  if (!object && new_object)
    free(new_object);

  return object;
}

void
monitor_destroy(monitor_t *self)
{
  monitor_finalize(self);
  free(self);
}

void
monitor_start_reading(monitor_t *self)
{
  pthread_mutex_lock(&self->condition_lock);
  if (self->active_writers || self->waiting_writers) {
    ++self->waiting_readers;
    pthread_cond_wait(&self->can_read, &self->condition_lock);
    --self->waiting_readers;
  }
  ++self->active_readers;
  pthread_mutex_unlock(&self->condition_lock);

  pthread_cond_broadcast(&self->can_read);
}

void
monitor_stop_reading(monitor_t *self)
{
  pthread_mutex_lock(&self->condition_lock);
  if (--self->active_readers == 0)
    pthread_cond_broadcast(&self->can_write);
  pthread_mutex_unlock(&self->condition_lock);
}

void
monitor_start_writing(monitor_t *self)
{
  pthread_mutex_lock(&self->condition_lock);
  if (
    self->active_writers || self->active_readers || self->waiting_readers ||
    self->waiting_writers) {
    ++self->waiting_writers;
    pthread_cond_wait(&self->can_write, &self->condition_lock);
    --self->waiting_writers;
  }
  ++self->active_writers;
  pthread_mutex_unlock(&self->condition_lock);
}

void
monitor_stop_writing(monitor_t *self)
{
  pthread_mutex_lock(&self->condition_lock);
  if (--self->active_writers == 0) {
    if (self->waiting_readers)
      pthread_cond_broadcast(&self->can_read);
    else if (self->waiting_writers)
      pthread_cond_broadcast(&self->can_write);
  }
  pthread_mutex_unlock(&self->condition_lock);
}
