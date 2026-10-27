#include "storage.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char* what, int err) {
  fprintf(stderr, "%s: %s\n", what, strerror(err));
  abort();
}

static void lock(Node* n) {
  int err = pthread_mutex_lock(&n->sync);
  if (err != 0) {
    fail("pthread_mutex_lock", err);
  }
}

static void unlock(Node* n) {
  int err = pthread_mutex_unlock(&n->sync);
  if (err != 0) {
    fail("pthread_mutex_unlock", err);
  }
}

static Node* new_node(const char* str) {
  int err;
  Node* n = calloc(1, sizeof(Node));
  if (n == NULL) {
    fail("calloc", ENOMEM);
  }
  strncpy(n->value, str, sizeof(n->value) - 1);
  err = pthread_mutex_init(&n->sync, NULL);
  if (err != 0) {
    fail("pthread_mutex_init", err);
  }
  return n;
}

void storage_init(Storage* s) {
  s->first = new_node("");
  s->last = s->first;
}

void storage_add(Storage* s, const char* str) {
  Node* n = new_node(str);
  s->last->next = n;
  s->last = n;
}

void storage_destroy(Storage* s) {
  int err;
  Node* n = s->first;
  while (n != NULL) {
    Node* next = n->next;
    err = pthread_mutex_destroy(&n->sync);
    if (err != 0) {
      fail("pthread_mutex_destroy", err);
    }
    free(n);
    n = next;
  }
  s->first = NULL;
  s->last = NULL;
}

unsigned long storage_scan(Storage* s, int mode) {
  unsigned long count = 0;
  Node* p = s->first;
  Node* a;
  lock(p);
  a = p->next;
  while (a != NULL) {
    lock(a);
    if (p != s->first) {
      size_t la = strlen(p->value);
      size_t lb = strlen(a->value);
      if ((mode == MODE_ASC && la < lb) || (mode == MODE_DESC && la > lb) ||
          (mode == MODE_EQ && la == lb)) {
        count++;
      }
    }
    unlock(p);
    p = a;
    a = p->next;
  }
  unlock(p);
  return count;
}

unsigned long storage_shuffle(Storage* s, unsigned int* seed) {
  unsigned long swaps = 0;
  Node* p = s->first;
  Node* a;
  lock(p);
  a = p->next;
  if (a == NULL) {
    unlock(p);
    return 0;
  }
  lock(a);
  for (;;) {
    Node* b = a->next;
    if (b == NULL) {
      break;
    }
    lock(b);
    if (rand_r(seed) & 1) {
      p->next = b;
      a->next = b->next;
      b->next = a;
      swaps++;
      unlock(p);
      p = b;
    } else {
      unlock(p);
      p = a;
      a = b;
    }
  }
  unlock(p);
  unlock(a);
  return swaps;
}
