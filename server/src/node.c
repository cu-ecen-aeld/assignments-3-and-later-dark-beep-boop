#include "node.h"

#include <bits/pthreadtypes.h>
#include <stdlib.h>
#include <string.h>

#include "try.h"

static void node_clear(node_t *self);

void
node_clear(node_t *self)
{
  memset(self, 0, sizeof(node_t));
}

bool
node_initialize(node_t *self)
{
  node_clear(self);
  return true;
}

void
node_finalize(node_t *self)
{
}

node_t *
node_new(void)
{
  node_t *object = NULL;
  node_t *new_object = NULL;

  TRY_ALLOCATE(new_object, node_t);
  TRY(node_initialize(new_object), "node initialization failed");

  object = new_object;

done:
  if (!object && new_object)
    free(new_object);

  return object;
}

void
node_destroy(node_t *self)
{
  node_finalize(self);
  free(self);
}

void
node_set(node_t *self, pthread_t tid)
{
  self->tid = tid;
}

pthread_t
node_get(const node_t *self)
{
  return self->tid;
}

node_t *
node_next(const node_t *self)
{
  return self->next;
}

node_t *
node_prev(const node_t *self)
{
  return self->prev;
}

void
node_link_next(node_t *self, node_t *next)
{
  self->next = next;
}

void
node_link_prev(node_t *self, node_t *prev)
{
  self->prev = prev;
}

void
node_unlink_next(node_t *self)
{
  self->next = NULL;
}

void
node_unlink_prev(node_t *self)
{
  self->prev = NULL;
}

bool
node_has_next(const node_t *self)
{
  return self->next != NULL;
}

bool
node_has_prev(const node_t *self)
{
  return self->prev != NULL;
}
