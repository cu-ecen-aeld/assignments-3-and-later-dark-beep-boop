#ifndef QUEUE_H
#define QUEUE_H

#include <bits/pthreadtypes.h>
#include <stdbool.h>

#include "doubly_linked_list.h"

typedef doubly_linked_list_t queue_t;

bool queue_initialize(queue_t *self);
void queue_finalize(queue_t *self);

queue_t *queue_new(void);
void queue_destroy(queue_t *self);

bool queue_is_empty(queue_t *self);
bool queue_enqueue(queue_t *self, pthread_t tid);
pthread_t queue_dequeue(queue_t *self);

#endif /* QUEUE_H */
