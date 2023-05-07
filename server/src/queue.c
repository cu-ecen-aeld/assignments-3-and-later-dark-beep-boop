#include "queue.h"

#include "doubly_linked_list.h"

bool
queue_initialize(queue_t *self)
{
  return doubly_linked_list_initialize(self);
}

void
queue_finalize(queue_t *self)
{
  doubly_linked_list_finalize(self);
}

queue_t *
queue_new(void)
{
  return doubly_linked_list_new();
}

void
queue_destroy(queue_t *self)
{
  doubly_linked_list_destroy(self);
}

bool
queue_is_empty(queue_t *self)
{
  return doubly_linked_list_is_empty(self);
}

bool
queue_enqueue(queue_t *self, pthread_t tid)
{
  return doubly_linked_list_insert_head(self, tid);
}

pthread_t
queue_dequeue(queue_t *self)
{
  return doubly_linked_list_remove_tail(self);
}
