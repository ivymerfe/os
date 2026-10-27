#ifndef MUTEX_H
#define MUTEX_H

#include <errno.h>
#include <linux/futex.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>

typedef struct {
  atomic_int locked;
  atomic_int waiters;
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
  atomic_store_explicit(&m->locked, 0, memory_order_release);
  atomic_store_explicit(&m->waiters, 0, memory_order_release);
  return 0;
}

static inline int mutex_lock(mutex_t* m) {
  int err;
  int expected = 0;
  if (atomic_compare_exchange_strong_explicit(&m->locked, &expected, 1,
                                              memory_order_acquire,
                                              memory_order_relaxed)) {
    return 0;
  }
  atomic_fetch_add_explicit(&m->waiters, 1, memory_order_seq_cst);
  for (;;) {
    expected = 0;
    if (atomic_compare_exchange_strong_explicit(&m->locked, &expected, 1,
                                                memory_order_acquire,
                                                memory_order_relaxed)) {
      break;
    }
    err = futex_wait(&m->locked, 1);
    if (err != 0) {
      atomic_fetch_sub_explicit(&m->waiters, 1, memory_order_seq_cst);
      return err;
    }
  }
  atomic_fetch_sub_explicit(&m->waiters, 1, memory_order_seq_cst);
  return 0;
}

static inline int mutex_unlock(mutex_t* m) {
  atomic_exchange_explicit(&m->locked, 0, memory_order_seq_cst);
  if (atomic_load_explicit(&m->waiters, memory_order_seq_cst) > 0) {
    return futex_wake(&m->locked, 1);
  }
  return 0;
}

#endif
