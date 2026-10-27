#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void *thread_main(void *arg) {
  while (1) {
    printf("infinite loop \n");
    // usleep(10000);
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
