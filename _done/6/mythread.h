#pragma once

#include <sched.h>

#define STACK_SIZE (8 * 1024 * 1024)
#define MYTHREAD_CANCEL_SIG SIGRTMIN
#define MYTHREAD_CANCELED ((void*)-1)

struct mythread_tcb {
  _Atomic int futex_word;
  void* retval;
  void* (*start_routine)(void*);
  void* arg;
  void* stack_base;
  pid_t tid;
  _Atomic int detached;
  _Atomic int cancelled;
};

struct stack_node {
  void* stack_base;
  struct stack_node* next;
};

typedef struct mythread_tcb* mythread_t;

int mythread_create(mythread_t* thread,
                    void* (*start_routine)(void*),
                    void* arg);
int mythread_join(mythread_t thread, void** retval);
int mythread_detach(mythread_t thread);
int mythread_cancel(mythread_t thread);
