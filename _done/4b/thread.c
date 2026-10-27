#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int counter = 0;

void *thread_main(void *arg) {
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  while (1) {
    counter += 1;
  }
  return "hello world";
}

int main() {
  pthread_t tid;
  int err = pthread_create(&tid, NULL, thread_main, NULL);
  if (err) {
    printf("main: pthread_create() failed: %s\n", strerror(err));
    return -1;
  }
  sleep(2);
  printf("Cancelling...\n");
  err = pthread_cancel(tid);
  if (err) {
    printf("main: pthread_cancel() failed: %s\n", strerror(err));
    return -1;
  }
  printf("Cancelled\n");
  err = pthread_join(tid, NULL);
  if (err) {
    printf("main: pthread_join() failed: %s\n", strerror(err));
    return -1;
  }
  return 0;
}
