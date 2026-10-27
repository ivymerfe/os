#ifndef MUTEX_H
#define MUTEX_H

#include <errno.h>
#include <linux/futex.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef struct {
  atomic_int state;
} mutex_t;

static inline int futex_wait(atomic_int* addr, int val) {
  long r = syscall(SYS_futex, addr, FUTEX_WAIT_PRIVATE, val, NULL, NULL, 0);
  if (r == -1 && errno != EAGAIN && errno != EINTR) {
    return errno;
  }
  return 0;
}

static inline int futex_wake(atomic_int* addr, int count) {
  long r = syscall(SYS_futex, addr, FUTEX_WAKE_PRIVATE, count, NULL, NULL, 0);
  if (r == -1) {
    return errno;
  }
  return 0;
}

static inline int mutex_init(mutex_t* m) {
  atomic_store_explicit(&m->state, 0, memory_order_release);
  return 0;
}

static inline int mutex_lock(mutex_t* m) {
  int err;
  int c = 0;
  if (atomic_compare_exchange_strong_explicit(
          &m->state, &c, 1, memory_order_acquire, memory_order_relaxed)) {
    return 0;
  }
  if (c != 2) {
    c = atomic_exchange_explicit(&m->state, 2, memory_order_acquire);
  }
  while (c != 0) {
    err = futex_wait(&m->state, 2);
    if (err != 0) {
      return err;
    }
    c = atomic_exchange_explicit(&m->state, 2, memory_order_acquire);
  }
  return 0;
}

static inline int mutex_unlock(mutex_t* m) {
  int old = atomic_exchange_explicit(&m->state, 0, memory_order_release);
  if (old == 2) {
    return futex_wake(&m->state, 1);
  }
  return 0;
}

#endif
