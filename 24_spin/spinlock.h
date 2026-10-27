#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdatomic.h>

typedef struct {
  atomic_flag locked;
} spinlock_t;

static inline void cpu_relax(void) {
  __asm__ volatile("yield" ::: "memory");
}

static inline int spin_init(spinlock_t* l) {
  atomic_flag_clear_explicit(&l->locked, memory_order_release);
  return 0;
}

static inline int spin_lock(spinlock_t* l) {
  while (atomic_flag_test_and_set_explicit(&l->locked, memory_order_acquire)) {
    cpu_relax();
  }
  return 0;
}

static inline int spin_unlock(spinlock_t* l) {
  atomic_flag_clear_explicit(&l->locked, memory_order_release);
  return 0;
}

#endif
