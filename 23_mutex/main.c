#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "storage.h"

#define RUN_SECONDS 5
#define MAX_LEN 100
#define SEED 666
#define THREADS 3

static const int SIZES[] = {100, 1000, 10000, 100000};
static const int SIZES_COUNT = sizeof(SIZES) / sizeof(SIZES[0]);

static Storage st;
static atomic_int running;
static atomic_ulong iters[THREADS];
static atomic_ulong pairs[THREADS];
static atomic_ulong swaps[THREADS];

static void* reader(void* arg) {
  int mode = (int)(long)arg;
  while (atomic_load_explicit(&running, memory_order_relaxed)) {
    unsigned long c = storage_scan(&st, mode);
    atomic_store_explicit(&pairs[mode], c, memory_order_relaxed);
    atomic_fetch_add_explicit(&iters[mode], 1, memory_order_relaxed);
  }
  return NULL;
}

static void* swapper(void* arg) {
  int id = (int)(long)arg;
  unsigned int seed = SEED + id;
  while (atomic_load_explicit(&running, memory_order_relaxed)) {
    unsigned long c = storage_shuffle(&st, &seed);
    atomic_fetch_add_explicit(&swaps[id], c, memory_order_relaxed);
  }
  return NULL;
}

static void fill(int n) {
  char buf[MAX_LEN];
  for (int i = 0; i < n; i++) {
    int len = 1 + rand() % (MAX_LEN - 1);
    for (int j = 0; j < len; j++) {
      buf[j] = 'a' + rand() % 26;
    }
    buf[len] = '\0';
    storage_add(&st, buf);
  }
}

int main(void) {
  int err;
  srand(SEED);

  for (int k = 0; k < SIZES_COUNT; k++) {
    pthread_t th[THREADS * 2];

    storage_init(&st);
    fill(SIZES[k]);
    for (int i = 0; i < THREADS; i++) {
      atomic_store_explicit(&iters[i], 0, memory_order_relaxed);
      atomic_store_explicit(&pairs[i], 0, memory_order_relaxed);
      atomic_store_explicit(&swaps[i], 0, memory_order_relaxed);
    }
    atomic_store_explicit(&running, 1, memory_order_relaxed);

    for (long i = 0; i < THREADS; i++) {
      err = pthread_create(&th[i], NULL, reader, (void*)i);
      if (err != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(err));
        return EXIT_FAILURE;
      }
    }
    for (long i = 0; i < THREADS; i++) {
      err = pthread_create(&th[THREADS + i], NULL, swapper, (void*)i);
      if (err != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(err));
        return EXIT_FAILURE;
      }
    }

    sleep(RUN_SECONDS);
    atomic_store_explicit(&running, 0, memory_order_relaxed);

    for (int i = 0; i < THREADS * 2; i++) {
      err = pthread_join(th[i], NULL);
      if (err != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(err));
        return EXIT_FAILURE;
      }
    }

    printf("size=%d\n", SIZES[k]);
    printf("  asc : iterations=%lu pairs=%lu\n", atomic_load(&iters[MODE_ASC]),
           atomic_load(&pairs[MODE_ASC]));
    printf("  desc: iterations=%lu pairs=%lu\n", atomic_load(&iters[MODE_DESC]),
           atomic_load(&pairs[MODE_DESC]));
    printf("  eq  : iterations=%lu pairs=%lu\n", atomic_load(&iters[MODE_EQ]),
           atomic_load(&pairs[MODE_EQ]));
    for (int i = 0; i < THREADS; i++) {
      printf("  swapper %d: swaps=%lu\n", i, atomic_load(&swaps[i]));
    }

    storage_destroy(&st);
  }
  return EXIT_SUCCESS;
}
