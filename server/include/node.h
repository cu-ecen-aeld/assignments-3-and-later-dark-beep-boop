#ifndef NODE_H
#define NODE_H

#include <bits/pthreadtypes.h>
#include <stdbool.h>

struct node
{
  pthread_t tid;
  struct node *next;
  struct node *prev;
};
typedef struct node node_t;

bool node_initialize(node_t *self);
void node_finalize(node_t *self);

node_t *node_new(void);
void node_destroy(node_t *self);

void node_set(node_t *self, pthread_t tid);
pthread_t node_get(const node_t *self);
node_t *node_next(const node_t *self);
node_t *node_prev(const node_t *self);
void node_link_next(node_t *self, node_t *next);
void node_link_prev(node_t *self, node_t *prev);
void node_unlink_next(node_t *self);
void node_unlink_prev(node_t *self);
bool node_has_next(const node_t *self);
bool node_has_prev(const node_t *self);

#endif /* NODE_H */
