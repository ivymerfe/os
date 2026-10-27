#pragma once

#define STACK_SIZE (64 * 1024)
#define STACK_CACHE_SIZE 64
#define UTHREAD_CANCELED ((void*)-1)

struct uthread_impl;

typedef struct {
  struct uthread_impl* impl;
} uthread_t;

int uthread_init(int nworkers);
void uthread_shutdown(void);
int uthread_create(uthread_t* thread, void* (*start_routine)(void*), void* arg);
void uthread_yield(void);
void uthread_fastyield(void);
void uthread_exit(void* retval);
int uthread_join(uthread_t thread, void** retval);
int uthread_cancel(uthread_t thread);
void uthread_testcancel(void);
