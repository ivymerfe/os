#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "mythread.h"

void* my_thread_func(void* arg) {
  char* msg = (char*)arg;
  printf("tid = %d msg = %s\n", gettid(), msg);
  sleep(1);
  return (void*)msg;
}

void* cancel_target_func(void* arg) {
  (void)arg;
  printf("tid = %d cancel target running\n", gettid());
  sleep(5);
  printf("tid = %d cancel target finished (should not print)\n", gettid());
  return NULL;
}

int main() {
  mythread_t t1, t2, t3, t4;
  void* ret;

  if (mythread_create(&t1, my_thread_func, "hello") != 0) {
    printf("create  error 1");
    return 1;
  }
  if (mythread_create(&t2, my_thread_func, "joined thread") != 0) {
    printf("create  error 2");
    return 1;
  }
  if (mythread_create(&t3, my_thread_func, "detached thread") != 0) {
    printf("create  error 3");
    return 1;
  }
  if (mythread_create(&t4, cancel_target_func, NULL) != 0) {
    printf("create  error 4");
    return 1;
  }

  if (mythread_join(t1, &ret) != 0) {
    printf("join error");
    return 2;
  }
  printf("t1 joined, retval = %s\n", (char*)ret);

  if (mythread_join(t2, &ret) != 0) {
    printf("join error");
    return 3;
  }
  printf("t2 joined, retval = %s\n", (char*)ret);

  if (mythread_detach(t3) != 0) {
    printf("detach error");
    return 4;
  }

  sleep(1);
  if (mythread_cancel(t4) != 0) {
    printf("cancel error");
    return 5;
  }
  if (mythread_join(t4, &ret)) {
    printf("join error");
    return 6;
  }
  printf("t4 joined after cancel, retval = %s\n",
         ret == MYTHREAD_CANCELED ? "canceled" : "unexpected");
  if (mythread_join(t3, &ret) != 0) {
    printf("failed to join detached thread\n");
    return 0;
  }
  return 0;
}
