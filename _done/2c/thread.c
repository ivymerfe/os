#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void *thread(void *arg) {
  printf("I am waiting for 1s\n");
  sleep(1);
  printf("I am done\n");
  return "hello world";
}

int main() {
  pthread_t tid;
  int err;

  err = pthread_create(&tid, NULL, thread, NULL);
  if (err) {
    printf("main: pthread_create() failed: %s\n", strerror(err));
    return -1;
  }
  printf("Waiting for child\n");
  void *ret;
  err = pthread_join(tid, &ret);
  if (err) {
    printf("main: pthread_join() failed: %s\n", strerror(err));
    return -1;
  }
  printf("main exit, child retuned: %s\n", (const char *)ret);

  return 0;
}
