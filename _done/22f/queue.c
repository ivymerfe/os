#define _GNU_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#include "queue.h"

void* qmonitor(void* arg) {
  queue_t* q = (queue_t*)arg;
  pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

  printf("qmonitor: [%d %d %d]\n", getpid(), getppid(), gettid());

  while (!atomic_load(&q->stopping)) {
    queue_print_stats(q);
    sleep(1);
  }

  return NULL;
}

queue_t* queue_init(int max_count) {
  int err;

  queue_t* q = malloc(sizeof(queue_t));
  if (!q) {
    printf("Cannot allocate memory for a queue\n");
    abort();
  }

  q->first = NULL;
  q->last = NULL;
  q->max_count = max_count;
  q->count = 0;

  q->add_attempts = q->get_attempts = 0;
  q->add_count = q->get_count = 0;
  atomic_init(&q->stopping, 0);

  err = pthread_mutex_init(&q->mutex, NULL);
  if (err) {
    printf("queue_init: pthread_mutex_init() failed: %s\n", strerror(err));
    free(q);
    abort();
  }

  err = pthread_cond_init(&q->not_full, NULL);
  if (err) {
    printf("queue_init: pthread_cond_init() failed: %s\n", strerror(err));
    free(q);
    abort();
  }

  err = pthread_cond_init(&q->not_empty, NULL);
  if (err) {
    printf("queue_init: pthread_cond_init() failed: %s\n", strerror(err));
    free(q);
    abort();
  }

  err = pthread_create(&q->qmonitor_tid, NULL, qmonitor, q);
  if (err) {
    printf("queue_init: pthread_create() failed: %s\n", strerror(err));
    abort();
  }

  return q;
}

void queue_destroy(queue_t* q) {
  // TODO: It's needed to implement this function
  pthread_mutex_lock(&q->mutex);
  atomic_store(&q->stopping, 1);
  pthread_cond_broadcast(&q->not_full);
  pthread_cond_broadcast(&q->not_empty);
  pthread_mutex_unlock(&q->mutex);

  int err = pthread_cancel(q->qmonitor_tid);
  if (err) {
    printf("queue_destroy: pthread_cancel() failed: %s\n", strerror(err));
    abort();
  }
  err = pthread_join(q->qmonitor_tid, NULL);
  if (err) {
    printf("queue_destroy: pthread_join() failed: %s\n", strerror(err));
    abort();
  }

  while (q->first) {
    qnode_t* tmp = q->first;
    q->first = q->first->next;
    free(tmp);
  }
  pthread_cond_destroy(&q->not_full);
  pthread_cond_destroy(&q->not_empty);
  pthread_mutex_destroy(&q->mutex);
  free(q);
}


int queue_add(queue_t *q, int val) {
	pthread_mutex_lock(&q->mutex);

	q->add_attempts++;

	while (q->count == q->max_count && !atomic_load(&q->stopping))
		pthread_cond_wait(&q->not_full, &q->mutex);

	if (atomic_load(&q->stopping)) {
		pthread_mutex_unlock(&q->mutex);
		return 0;
	}

	qnode_t *new = malloc(sizeof(qnode_t));
	if (!new) {
		printf("Cannot allocate memory for new node\n");
		abort();
	}

	new->val = val;
	new->next = NULL;

	if (!q->first)
		q->first = q->last = new;
	else {
		q->last->next = new;
		q->last = new;
	}

	q->count++;
	q->add_count++;

	pthread_cond_signal(&q->not_empty);
	pthread_mutex_unlock(&q->mutex);
	return 1;
}

int queue_get(queue_t *q, int *val) {
	pthread_mutex_lock(&q->mutex);

	q->get_attempts++;

	while (q->count == 0 && !atomic_load(&q->stopping))
		pthread_cond_wait(&q->not_empty, &q->mutex);

	if (q->count == 0) {
		pthread_mutex_unlock(&q->mutex);
		return 0;
	}

	qnode_t *tmp = q->first;

	*val = tmp->val;
	q->first = tmp->next;
	if (!q->first)
		q->last = NULL;

	free(tmp);
	q->count--;
	q->get_count++;

	pthread_cond_signal(&q->not_full);
	pthread_mutex_unlock(&q->mutex);
	return 1;
}

void queue_print_stats(queue_t* q) {
  printf(
      "queue stats: current size %d; attempts: (%ld %ld %ld); counts (%ld %ld "
      "%ld)\n",
      q->count, q->add_attempts, q->get_attempts,
      q->add_attempts - q->get_attempts, q->add_count, q->get_count,
      q->add_count - q->get_count);
}
