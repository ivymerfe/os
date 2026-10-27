#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

void print_maps() {
  FILE *fp = fopen("/proc/self/maps", "r");
  if (fp == NULL) {
    perror("fopen /proc/self/maps failed");
    return;
  }
  printf("<maps>\n");
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), fp) != NULL) {
    fputs(buffer, stdout);
  }
  printf("</maps>\n");
  fclose(fp);
}

void *mythread(void *arg) {
  printf("mythread [%d %d %d]: Hello from mythread!\n", getpid(), getppid(),
         gettid());
  return NULL;
}

int IAMGLOBAL = 11;

void *threadfunc(void *arg) {
  int local = 111;
  static int local_static = 1111;
  const int local_const = 11111;
  local += 1;
  IAMGLOBAL += 1;

  printf("pid=%d, ppid=%d, pthread_self=%lu gettid=%d local=%p local_static=%p "
         "local_const=%p global=%p local_value=%d global_value=%d\n",
         getpid(), getppid(), pthread_self(), gettid(), &local, &local_static,
         &local_const, &IAMGLOBAL, local, IAMGLOBAL);
  return NULL;
}

int main() {
  pthread_t tid;
  int err;

  err = pthread_create(&tid, NULL, mythread, NULL);
  if (err) {
    printf("main: pthread_create() failed: %s\n", strerror(err));
    return -1;
  }
  printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());
  // pthread_exit(0);
  err = pthread_join(tid, NULL);
  if (err) {
    printf("main: pthread_join() failed: %s\n", strerror(err));
    return -1;
  }

  print_maps();
  pthread_t tids[5];
  for (int i = 0; i < 5; i++) {
    err = pthread_create(&tids[i], NULL, threadfunc, NULL);
    if (err) {
      printf("main: pthread_create() failed: %s\n", strerror(err));
      return -1;
    }
    printf("created thread %d -> %lu\n", i, tids[i]);
  }
  print_maps();
  for (int i = 0; i < 5; i++) {
    err = pthread_join(tids[i], NULL);
    if (err) {
      printf("main: pthread_join() failed: %s\n", strerror(err));
      return -1;
    }
  }
  print_maps();
  return 0;
}
