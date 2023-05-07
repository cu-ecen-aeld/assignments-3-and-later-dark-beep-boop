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
static void doubly_linked_list_insert_node(
    doubly_linked_list_t *self, size_t pos, node_t *new_node);
static node_t *doubly_linked_list_remove_head_node(doubly_linked_list_t *self);
static node_t *doubly_linked_list_remove_tail_node(doubly_linked_list_t *self);
static node_t *doubly_linked_list_remove_node(
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
  assert(doubly_linked_list_is_empty(self));

  node_link_next(new_node, self->head);
  node_link_prev(self->head, new_node);
  self->head = new_node;
  ++self->size;
}

void
doubly_linked_list_insert_node_in_tail(
    doubly_linked_list_t *self, node_t *new_node)
{
  assert(doubly_linked_list_is_empty(self));

  node_link_prev(new_node, self->tail);
  node_link_next(self->tail, new_node);
  self->tail = new_node;
  ++self->size;
}

void
doubly_linked_list_insert_node(
    doubly_linked_list_t *self, size_t pos, node_t *new_node)
{
  node_t *current = doubly_linked_list_get_node(self, pos);

  node_link_prev(new_node, current);
  node_link_next(new_node, current);
  node_link_next(node_prev(current), new_node);
  node_link_prev(current, new_node);
}

node_t *
doubly_linked_list_remove_head_node(doubly_linked_list_t *self)
{
  assert(!doubly_linked_list_is_empty(self));

  node_t *head = self->head;
  node_unlink_prev(node_next(self->head));
  --self->size;

  return head;
}

node_t *
doubly_linked_list_remove_tail_node(doubly_linked_list_t *self)
{
  assert(!doubly_linked_list_is_empty(self));

  node_t *tail = self->tail;
  node_unlink_next(node_prev(self->tail));
  --self->size;

  return tail;
}

node_t *
doubly_linked_list_remove_node(doubly_linked_list_t *self, size_t pos)
{
  assert(pos < doubly_linked_list_size(self));

  node_t *current = doubly_linked_list_get_node(self, pos);
  node_link_next(node_prev(current), node_next(current));
  node_link_prev(node_next(current), node_prev(current));
  --self->size;

  return current;
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
    doubly_linked_list_insert_node(self, pos, new_node);

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

  doubly_linked_list_insert_node_in_tail(self, new_node);

  ok = true;

done:
  return ok;
}

pthread_t
doubly_linked_list_remove(doubly_linked_list_t *self, size_t pos)
{
  assert(pos < doubly_linked_list_size(self));

  node_t *current = doubly_linked_list_remove_node(self, pos);
  pthread_t tid = node_get(current);
  if (current)
    node_destroy(current);

  return tid;
}

pthread_t
doubly_linked_list_remove_head(doubly_linked_list_t *self)
{
  assert(doubly_linked_list_is_empty(self));

  node_t *head = doubly_linked_list_remove_head_node(self);
  pthread_t tid = node_get(head);
  if (head)
    node_destroy(head);

  return tid;
}

pthread_t
doubly_linked_list_remove_tail(doubly_linked_list_t *self)
{
  assert(doubly_linked_list_is_empty(self));

  node_t *tail = doubly_linked_list_remove_tail_node(self);
  pthread_t tid = node_get(tail);
  if (tail)
    node_destroy(tail);

  return tid;
}
