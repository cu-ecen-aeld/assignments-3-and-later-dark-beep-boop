#ifndef DOUBLY_LINKED_LIST_H
#define DOUBLY_LINKED_LIST_H

#include <bits/pthreadtypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "node.h"

struct doubly_linked_list
{
  node_t *head;
  node_t *tail;
  size_t size;
};
typedef struct doubly_linked_list doubly_linked_list_t;

bool doubly_linked_list_initialize(doubly_linked_list_t *self);
void doubly_linked_list_finalize(doubly_linked_list_t *self);

doubly_linked_list_t *doubly_linked_list_new(void);
void doubly_linked_list_destroy(doubly_linked_list_t *self);

size_t doubly_linked_list_size(const doubly_linked_list_t *self);
bool doubly_linked_list_is_empty(const doubly_linked_list_t *self);

void doubly_linked_list_set(
    const doubly_linked_list_t *self, size_t pos, pthread_t tid);
void doubly_linked_list_set_head(
    const doubly_linked_list_t *self, pthread_t tid);
void doubly_linked_list_set_tail(
    const doubly_linked_list_t *self, pthread_t tid);

pthread_t doubly_linked_list_get(const doubly_linked_list_t *self, size_t pos);
pthread_t doubly_linked_list_get_head(const doubly_linked_list_t *self);
pthread_t doubly_linked_list_get_tail(const doubly_linked_list_t *self);

bool doubly_linked_list_insert(
    doubly_linked_list_t *self, size_t pos, pthread_t tid);
bool doubly_linked_list_insert_head(doubly_linked_list_t *self, pthread_t tid);
bool doubly_linked_list_insert_tail(doubly_linked_list_t *self, pthread_t tid);

pthread_t doubly_linked_list_remove(doubly_linked_list_t *self, size_t pos);
pthread_t doubly_linked_list_remove_head(doubly_linked_list_t *self);
pthread_t doubly_linked_list_remove_tail(doubly_linked_list_t *self);

#endif /* DOUBLY_LINKED_LIST_H */
