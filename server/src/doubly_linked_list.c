#include "doubly_linked_list.h"

#include <assert.h>
#include <bits/pthreadtypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "try.h"

static void doubly_linked_list_clear(doubly_linked_list_t *self);
static node_t *doubly_linked_list_get_node_from_head(
    const doubly_linked_list_t *self, size_t pos);
static node_t *doubly_linked_list_get_node_from_tail(
    const doubly_linked_list_t *self, size_t pos);
static node_t *doubly_linked_list_get_node(
    const doubly_linked_list_t *self, size_t pos);
static void doubly_linked_list_insert_node_when_empty(
    doubly_linked_list_t *self, node_t *new_node);
static void doubly_linked_list_insert_node_in_head(
    doubly_linked_list_t *self, node_t *new_node);
static void doubly_linked_list_insert_node_in_tail(
    doubly_linked_list_t *self, node_t *new_node);
static void doubly_linked_list_insert_middle_node(
    doubly_linked_list_t *self, size_t pos, node_t *new_node);
static node_t *doubly_linked_list_remove_last_node(doubly_linked_list_t *self);
static node_t *doubly_linked_list_remove_head_node(doubly_linked_list_t *self);
static node_t *doubly_linked_list_remove_tail_node(doubly_linked_list_t *self);
static node_t *doubly_linked_list_remove_middle_node(
    doubly_linked_list_t *self, size_t pos);

void
doubly_linked_list_clear(doubly_linked_list_t *self)
{
  memset(self, 0, sizeof(doubly_linked_list_t));
}

node_t *
doubly_linked_list_get_node_from_head(
    const doubly_linked_list_t *self, size_t pos)
{
  assert(pos < doubly_linked_list_size(self));

  node_t *current = self->head;
  for (size_t i = 0; i < pos; ++i) current = node_next(current);

  return current;
}

node_t *
doubly_linked_list_get_node_from_tail(
    const doubly_linked_list_t *self, size_t pos)
{
  assert(pos < doubly_linked_list_size(self));

  node_t *current = self->head;
  for (size_t i = doubly_linked_list_size(self) - 1; i > pos; --i)
    current = node_prev(current);

  return current;
}

node_t *
doubly_linked_list_get_node(const doubly_linked_list_t *self, size_t pos)
{
  assert(pos < doubly_linked_list_size(self));

  node_t *current;
  if (pos == 0)
    current = self->head;
  else if (pos == doubly_linked_list_size(self))
    current = self->tail;
  else if (pos < doubly_linked_list_size(self) / 2)
    current = doubly_linked_list_get_node_from_head(self, pos);
  else
    current = doubly_linked_list_get_node_from_tail(self, pos);

  return current;
}

void
doubly_linked_list_insert_node_when_empty(
    doubly_linked_list_t *self, node_t *new_node)
{
  assert(doubly_linked_list_is_empty(self));

  self->head = new_node;
  self->tail = new_node;
  self->size = 1;
}

void
doubly_linked_list_insert_node_in_head(
    doubly_linked_list_t *self, node_t *new_node)
{
  assert(!doubly_linked_list_is_empty(self));

  node_link_next(new_node, self->head);
  node_link_prev(self->head, new_node);
  self->head = new_node;
  ++self->size;
}

void
doubly_linked_list_insert_node_in_tail(
    doubly_linked_list_t *self, node_t *new_node)
{
  assert(!doubly_linked_list_is_empty(self));

  node_link_prev(new_node, self->tail);
  node_link_next(self->tail, new_node);
  self->tail = new_node;
  ++self->size;
}

void
doubly_linked_list_insert_middle_node(
    doubly_linked_list_t *self, size_t pos, node_t *new_node)
{
  assert(pos > 0 && pos < doubly_linked_list_size(self));

  node_t *node_in_pos = doubly_linked_list_get_node(self, pos);

  node_link_prev(new_node, node_prev(node_in_pos));
  node_link_next(new_node, node_in_pos);
  node_link_next(node_prev(node_in_pos), new_node);
  node_link_prev(node_in_pos, new_node);
  ++self->size;
}

node_t *
doubly_linked_list_remove_last_node(doubly_linked_list_t *self)
{
  assert(doubly_linked_list_size(self) == 1);

  node_t *old_node = self->head;

  self->head = NULL;
  self->tail = NULL;
  self->size = 0;

  return old_node;
}

node_t *
doubly_linked_list_remove_head_node(doubly_linked_list_t *self)
{
  assert(doubly_linked_list_size(self) > 1);

  node_t *old_head = self->head;
  self->head = node_next(old_head);
  node_unlink_prev(self->head);
  --self->size;

  return old_head;
}

node_t *
doubly_linked_list_remove_tail_node(doubly_linked_list_t *self)
{
  assert(doubly_linked_list_size(self) > 1);

  node_t *old_tail = self->tail;
  self->tail = node_prev(old_tail);
  node_unlink_next(self->tail);
  --self->size;

  return old_tail;
}

node_t *
doubly_linked_list_remove_middle_node(doubly_linked_list_t *self, size_t pos)
{
  assert(pos > 0 && pos < doubly_linked_list_size(self) - 1);

  node_t *old_node = doubly_linked_list_get_node(self, pos);
  node_link_next(node_prev(old_node), node_next(old_node));
  node_link_prev(node_next(old_node), node_prev(old_node));
  --self->size;

  return old_node;
}

bool
doubly_linked_list_initialize(doubly_linked_list_t *self)
{
  doubly_linked_list_clear(self);
  return true;
}

void
doubly_linked_list_finalize(doubly_linked_list_t *self)
{
  assert(doubly_linked_list_is_empty(self));
}

doubly_linked_list_t *
doubly_linked_list_new(void)
{
  doubly_linked_list_t *new_object = NULL;
  doubly_linked_list_t *object = NULL;

  TRY_ALLOCATE(new_object, doubly_linked_list_t);
  TRY(doubly_linked_list_initialize(new_object),
      "doubly linked list initialization failed");

  object = new_object;

done:
  if (!object && new_object)
    free(new_object);

  return object;
}

void
doubly_linked_list_destroy(doubly_linked_list_t *self)
{
  doubly_linked_list_finalize(self);
  free(self);
}

size_t
doubly_linked_list_size(const doubly_linked_list_t *self)
{
  return self->size;
}

bool
doubly_linked_list_is_empty(const doubly_linked_list_t *self)
{
  return doubly_linked_list_size(self) == 0;
}

void
doubly_linked_list_set(
    const doubly_linked_list_t *self, size_t pos, pthread_t tid)
{
  assert(pos < doubly_linked_list_size(self));

  node_set(doubly_linked_list_get_node(self, pos), tid);
}

void
doubly_linked_list_set_head(const doubly_linked_list_t *self, pthread_t tid)
{
  assert(!doubly_linked_list_is_empty(self));

  node_set(self->head, tid);
}

void
doubly_linked_list_set_tail(const doubly_linked_list_t *self, pthread_t tid)
{
  assert(!doubly_linked_list_is_empty(self));

  node_set(self->tail, tid);
}

pthread_t
doubly_linked_list_get(const doubly_linked_list_t *self, size_t pos)
{
  assert(pos < doubly_linked_list_size(self));

  return node_get(doubly_linked_list_get_node(self, pos));
}

pthread_t
doubly_linked_list_get_head(const doubly_linked_list_t *self)
{
  assert(!doubly_linked_list_is_empty(self));

  return node_get(self->head);
}

pthread_t
doubly_linked_list_get_tail(const doubly_linked_list_t *self)
{
  assert(!doubly_linked_list_is_empty(self));

  return node_get(self->tail);
}

bool
doubly_linked_list_insert(doubly_linked_list_t *self, size_t pos, pthread_t tid)
{
  assert(pos <= doubly_linked_list_size(self));

  bool ok = false;
  node_t *new_node = NULL;

  TRY(new_node = node_new(), "couldn't create a new node");
  node_set(new_node, tid);

  if (doubly_linked_list_is_empty(self))
    doubly_linked_list_insert_node_when_empty(self, new_node);
  else if (pos == 0)
    doubly_linked_list_insert_node_in_head(self, new_node);
  else if (pos == doubly_linked_list_size(self))
    doubly_linked_list_insert_node_in_tail(self, new_node);
  else
    doubly_linked_list_insert_middle_node(self, pos, new_node);

  ok = true;

done:
  return ok;
}

bool
doubly_linked_list_insert_head(doubly_linked_list_t *self, pthread_t tid)
{
  bool ok = false;
  node_t *new_node = NULL;

  TRY(new_node = node_new(), "couldn't create a new node");
  node_set(new_node, tid);

  if (doubly_linked_list_is_empty(self))
    doubly_linked_list_insert_node_when_empty(self, new_node);
  else
    doubly_linked_list_insert_node_in_head(self, new_node);

  ok = true;

done:
  return ok;
}

bool
doubly_linked_list_insert_tail(doubly_linked_list_t *self, pthread_t tid)
{
  bool ok = false;
  node_t *new_node = NULL;

  TRY(new_node = node_new(), "couldn't create a new node");
  node_set(new_node, tid);

  if (doubly_linked_list_is_empty(self))
    doubly_linked_list_insert_node_when_empty(self, new_node);
  else
    doubly_linked_list_insert_node_in_tail(self, new_node);

  ok = true;

done:
  return ok;
}

pthread_t
doubly_linked_list_remove(doubly_linked_list_t *self, size_t pos)
{
  assert(pos < doubly_linked_list_size(self));

  node_t *old_node = NULL;

  if (doubly_linked_list_is_empty(self))
    old_node = doubly_linked_list_remove_last_node(self);
  else if (pos == 0)
    old_node = doubly_linked_list_remove_head_node(self);
  else if (pos == doubly_linked_list_size(self))
    old_node = doubly_linked_list_remove_tail_node(self);
  else
    old_node = doubly_linked_list_remove_middle_node(self, pos);

  pthread_t tid = node_get(old_node);
  if (old_node)
    node_destroy(old_node);

  return tid;
}

pthread_t
doubly_linked_list_remove_head(doubly_linked_list_t *self)
{
  assert(!doubly_linked_list_is_empty(self));

  node_t *old_head = NULL;

  if (doubly_linked_list_size(self) == 1)
    old_head = doubly_linked_list_remove_last_node(self);
  else
    old_head = doubly_linked_list_remove_head_node(self);

  pthread_t tid = node_get(old_head);
  if (old_head)
    node_destroy(old_head);

  return tid;
}

pthread_t
doubly_linked_list_remove_tail(doubly_linked_list_t *self)
{
  assert(!doubly_linked_list_is_empty(self));

  node_t *old_tail = NULL;

  if (doubly_linked_list_size(self) == 1)
    old_tail = doubly_linked_list_remove_last_node(self);
  else
    old_tail = doubly_linked_list_remove_tail_node(self);

  pthread_t tid = node_get(old_tail);
  if (old_tail)
    node_destroy(old_tail);

  return tid;
}
