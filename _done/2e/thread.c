#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void *thread_main(void *arg) {
  printf("my tid: %d\n", gettid());
  // pthread_detach(pthread_self());
  return "hello world";
}

int main() {
  pthread_attr_t attr;

  int err = pthread_attr_init(&attr);
  if (err) {
    printf("main: pthread_attr_init() failed: %s\n", strerror(err));
    return -1;
  }
  err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (err) {
    printf("main: pthread_attr_setdetachstate() failed: %s\n", strerror(err));
    return -1;
  }

  while (1) {
    pthread_t tid;
    err = pthread_create(&tid, NULL, thread_main, NULL);
    if (err) {
      printf("main: pthread_create() failed: %s\n", strerror(err));
      return -1;
    }
  }
  return 0;
}
