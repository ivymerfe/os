#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void * thread_main(void* arg) {
  pthread_detach(pthread_self());
  printf("I am waiting for 1s\n");
  sleep(1);
  printf("I am done\n");
  return (void*)42;
}


int main() {
  pthread_t tid;
  int err;
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  err = pthread_create(&tid, NULL, thread_main, NULL);
  if (err) {
    printf("main: pthread_create() failed: %s\n", strerror(err));
    return -1;
  }
  printf("Waiting for child\n");
  void* ret;
  err = pthread_join(tid, &ret);
    if (err) {
    printf("main: pthread_join() failed: %s\n", strerror(err));
    return -1;
  }
  printf("main exit, child retunr111111111 %llu\n", (long long)ret);

  return 0;
}
