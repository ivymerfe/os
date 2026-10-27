#ifndef STORAGE_H
#define STORAGE_H

#include "mutex.h"

enum { MODE_ASC, MODE_DESC, MODE_EQ };

typedef struct _Node {
  char value[100];
  struct _Node* next;
  mutex_t sync;
} Node;

typedef struct _Storage {
  Node* first;
  Node* last;
} Storage;

void storage_init(Storage* s);
void storage_add(Storage* s, const char* str);
void storage_destroy(Storage* s);
unsigned long storage_scan(Storage* s, int mode);
unsigned long storage_shuffle(Storage* s, unsigned int* seed);

#endif
