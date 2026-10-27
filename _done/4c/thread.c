#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void cleanup_handler(void *arg) {
  printf("Cleaning\n");
  free(arg);
}

void *thread_main(void *arg) {
  char *str = (char *)malloc(100);
  memcpy(str, "hello world", sizeof("hello world"));

  pthread_cleanup_push(cleanup_handler, str);
  int oldstate;
  int err = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
  if (err) {
    printf("pthread_setcancelstate failed: %d", err);
  }
  int cntr = 0;
  while (1) {
    printf("str: %s\n", str);
    usleep(20000);
    cntr += 1;
    if (cntr == 200) {

      err = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
      if (err) {
        printf("pthread_setcancelstate failed: %d", err);
      }
    }
  }
  pthread_cleanup_pop(1);
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
