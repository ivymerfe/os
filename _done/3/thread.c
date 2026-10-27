#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#pragma pack(16)
typedef struct {
  char *age;
  int name;
} CoolData;

void *thread_main(void *arg) {
  CoolData *data = (CoolData *)arg;
  printf("name = %d, age = %s\n", data->name, data->age);
  return "hello world";
}

CoolData data = {"hihihi", 4};

int main() {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  pthread_t tid;
  int err = pthread_create(&tid, NULL, thread_main, &data);
  if (err) {
    printf("main: pthread_create() failed: %s\n", strerror(err));
    return -1;
  }
  sleep(1);
  pthread_join(tid, NULL);
  return 0;
}
