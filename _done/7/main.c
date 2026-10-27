#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "uthread.h"

#define CNT 10

static void* worker(void* arg) {
  intptr_t id = (intptr_t)arg;

  for (int i = 0; i < 20; i++) {
    printf("uthread %ld on pthread %lu: step %d\n", (long)id,
           (unsigned long)pthread_self(), i);
    uthread_yield();
  }
  return (void*)(id * 100);
}

static void* slow_worker(void* arg) {
  for (int i = 0; i < 1000; i++) {
    printf("slow worker step: %d\n", i);
    usleep(100000);
    uthread_testcancel();
  }
  return (void*)0;
}


static void* normal_worker(void* arg) {
  for (int i = 0; i < 40; i++) {
    printf("normal worker step: %d\n", i);
    usleep(400000);
    uthread_yield();
  }
  return (void*)0;
}

static void* priority_worker(void* arg) {
  for (int i = 0; i < 40; i++) {
    printf("priority worker step: %d\n", i);
    usleep(400000);
    if (i % 10 == 0) {
      uthread_yield();
    } else {
      uthread_fastyield();
    }
  }
  return (void*)0;
}

int main(void) {
  uthread_t threads[CNT];

  int err = uthread_init(4);
  if (err != 0) {
    printf("uthread_init failed: %d\n", err);
    return 1;
  }

  for (intptr_t i = 0; i < CNT; i++) {
    err = uthread_create(&threads[i], worker, (void*)(i + 1));
    if (err != 0) {
      printf("uthread_create failed: %d\n", err);
      return 1;
    }
  }

  for (int i = 0; i < CNT; i++) {
    void* ret;
    err = uthread_join(threads[i], &ret);
    if (err != 0) {
      printf("uthread_join failed: %d\n", err);
      return 1;
    }
    // printf("uthread %d returned %ld\n", i + 1, (long)(intptr_t)ret);
  }
  uthread_t wthread;
  err = uthread_create(&wthread, slow_worker, 0);
  if (err != 0) {
    printf("uthread_create failed: %d\n", err);
    return 1;
  }
  usleep(1000000);
  uthread_cancel(wthread);
  void* ret;
  err = uthread_join(wthread, &ret);
  if (err != 0) {
    printf("uthread_join failed: %d\n", err);
    return 1;
  }
  printf("joined slow worker: %ld\n", (uint64_t)ret);

  uthread_shutdown();

  err = uthread_init(1);
  if (err != 0) {
    printf("uthread_init failed: %d\n", err);
    return 1;
  }

  uthread_t norm, greedy;
  err = uthread_create(&norm, normal_worker, 0);
  if (err != 0) {
    printf("uthread_create failed: %d\n", err);
    return 1;
  }
  err = uthread_create(&greedy, priority_worker, 0);
  if (err != 0) {
    printf("uthread_create failed: %d\n", err);
    return 1;
  }

  err = uthread_join(norm, &ret);
  if (err != 0) {
    printf("uthread_join failed: %d\n", err);
    return 1;
  }
  err = uthread_join(greedy, ret);
  if (err != 0) {
    printf("uthread_join failed: %d\n", err);
    return 1;
  }

  uthread_shutdown();
  return 0;
}
