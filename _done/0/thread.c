#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

typedef void *(*__start_routine)(void *);

int main() {
  printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
  pthread_t tid;
  int err;
  err = pthread_create(&tid, NULL, (__start_routine)main, NULL);
  if (err) {
    printf("main: pthread_create() failed: %s\n", strerror(err));
    return -1;
  }
  pthread_join(tid, NULL);
  return 0;
}
