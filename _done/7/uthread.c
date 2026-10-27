#include "uthread.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <ucontext.h>

enum { UT_READY, UT_RUNNING, UT_FINISHED };
enum { ACT_NONE, ACT_YIELD, ACT_FASTYIELD, ACT_EXIT };

struct uthread_impl {
  ucontext_t ctx;
  void* stack;
  void* (*fn)(void*);
  void* arg;
  void* retval;
  int state;
  int action;
  _Atomic int cancel;
  int started;
  struct uthread_impl* next;
};

struct queue {
  struct uthread_impl *head, *tail;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t work_cv = PTHREAD_COND_INITIALIZER;
static pthread_cond_t done_cv = PTHREAD_COND_INITIALIZER;
static struct queue normal_q, fast_q;
static pthread_t* workers;
static int nworkers;
static int stopping;

struct stack_node {
  struct stack_node* next;
};

static pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;
static struct stack_node* cache_head;
static int stack_cache_size;

static __thread struct uthread_impl* current;
static __thread ucontext_t sched_ctx;

__attribute__((noinline)) static struct uthread_impl* get_current(void) {
  return current;
}

__attribute__((noinline)) static ucontext_t* get_sched(void) {
  return &sched_ctx;
}

static void* stack_get(void) {
  struct stack_node* n;

  pthread_mutex_lock(&cache_lock);
  n = cache_head;
  if (n) {
    cache_head = n->next;
    stack_cache_size--;
  }
  pthread_mutex_unlock(&cache_lock);
  return n ? (void*)n : malloc(STACK_SIZE);
}

static void stack_put(void* stack) {
  struct stack_node* n = stack;

  pthread_mutex_lock(&cache_lock);
  if (stack_cache_size < STACK_CACHE_SIZE) {
    n->next = cache_head;
    cache_head = n;
    stack_cache_size++;
    pthread_mutex_unlock(&cache_lock);
    return;
  }
  pthread_mutex_unlock(&cache_lock);
  free(stack);
}

static void free_stack_cache(void) {
  pthread_mutex_lock(&cache_lock);
  while (cache_head) {
    struct stack_node* n = cache_head;
    cache_head = n->next;
    free(n);
  }
  stack_cache_size = 0;
  pthread_mutex_unlock(&cache_lock);
}

static void enqueue(struct queue* q, struct uthread_impl* t) {
  t->next = NULL;
  t->state = UT_READY;
  if (q->tail) {
    q->tail->next = t;
  } else {
    q->head = t;
  }
  q->tail = t;
}

static struct uthread_impl* dequeue(void) {
  struct queue* q = fast_q.head ? &fast_q : &normal_q;
  struct uthread_impl* t = q->head;
  if (t) {
    q->head = t->next;
    if (!q->head) {
      q->tail = NULL;
    }
    t->next = NULL;
  }
  return t;
}

static int is_cancelled(struct uthread_impl* t) {
  return atomic_load(&t->cancel);
}

static void* worker_main(void* unused) {
  (void)unused;
  for (;;) {
    struct uthread_impl* t;

    pthread_mutex_lock(&lock);
    while (!fast_q.head && !normal_q.head && !stopping) {
      pthread_cond_wait(&work_cv, &lock);
    }
    t = dequeue();
    if (!t) {
      pthread_mutex_unlock(&lock);
      break;
    }
    if (!t->started && is_cancelled(t)) {
      t->retval = UTHREAD_CANCELED;
      t->state = UT_FINISHED;
      pthread_cond_broadcast(&done_cv);
      pthread_mutex_unlock(&lock);
      continue;
    }
    t->started = 1;
    t->state = UT_RUNNING;
    pthread_mutex_unlock(&lock);

    t->action = ACT_NONE;
    current = t;
    swapcontext(&sched_ctx, &t->ctx);
    current = NULL;

    pthread_mutex_lock(&lock);
    if (t->action == ACT_YIELD) {
      enqueue(&normal_q, t);
      pthread_cond_signal(&work_cv);
    } else if (t->action == ACT_FASTYIELD) {
      enqueue(&fast_q, t);
      pthread_cond_signal(&work_cv);
    } else {
      t->state = UT_FINISHED;
      pthread_cond_broadcast(&done_cv);
    }
    pthread_mutex_unlock(&lock);
  }
  return NULL;
}

int uthread_init(int n) {
  if (n < 1 || workers) {
    return -1;
  }
  workers = calloc(n, sizeof(*workers));
  if (!workers) {
    return -1;
  }
  nworkers = n;
  stopping = 0;
  for (int i = 0; i < n; i++) {
    if (pthread_create(&workers[i], NULL, worker_main, NULL) != 0) {
      nworkers = i;
      uthread_shutdown();
      return -1;
    }
  }
  return 0;
}

void uthread_shutdown(void) {
  pthread_mutex_lock(&lock);
  stopping = 1;
  pthread_cond_broadcast(&work_cv);
  pthread_mutex_unlock(&lock);
  for (int i = 0; i < nworkers; i++) {
    pthread_join(workers[i], NULL);
  }
  free(workers);
  workers = NULL;
  nworkers = 0;
  free_stack_cache();
}

void uthread_exit(void* retval) {
  struct uthread_impl* t = get_current();

  t->retval = retval;
  t->action = ACT_EXIT;
  swapcontext(&t->ctx, get_sched());
}

void uthread_testcancel(void) {
  struct uthread_impl* t = get_current();

  if (t && is_cancelled(t)) {
    uthread_exit(UTHREAD_CANCELED);
  }
}

static void trampoline(void) {
  struct uthread_impl* t = get_current();
  uthread_testcancel();
  uthread_exit(t->fn(t->arg));
}

int uthread_create(uthread_t* thread,
                   void* (*start_routine)(void*),
                   void* arg) {
  struct uthread_impl* t = calloc(1, sizeof(*t));

  if (!t) {
    return -1;
  }
  t->stack = stack_get();
  if (!t->stack) {
    free(t);
    return -1;
  }
  t->fn = start_routine;
  t->arg = arg;
  getcontext(&t->ctx);
  t->ctx.uc_stack.ss_sp = t->stack;
  t->ctx.uc_stack.ss_size = STACK_SIZE;
  t->ctx.uc_link = NULL;
  makecontext(&t->ctx, trampoline, 0);

  pthread_mutex_lock(&lock);
  enqueue(&normal_q, t);
  pthread_cond_signal(&work_cv);
  pthread_mutex_unlock(&lock);

  thread->impl = t;
  return 0;
}

static void do_yield(int action) {
  struct uthread_impl* t = get_current();

  if (!t) {
    return;
  }
  uthread_testcancel();
  t->action = action;
  swapcontext(&t->ctx, get_sched());
  uthread_testcancel();
}

void uthread_yield(void) {
  do_yield(ACT_YIELD);
}

void uthread_fastyield(void) {
  do_yield(ACT_FASTYIELD);
}

int uthread_cancel(uthread_t thread) {
  struct uthread_impl* t = thread.impl;

  if (!t) {
    return -1;
  }
  atomic_store(&t->cancel, 1);
  return 0;
}

int uthread_join(uthread_t thread, void** retval) {
  struct uthread_impl* t = thread.impl;

  if (!t || t == get_current()) {
    return -1;
  }

  if (get_current()) {
    for (;;) {
      int done;
      pthread_mutex_lock(&lock);
      done = t->state == UT_FINISHED;
      pthread_mutex_unlock(&lock);
      if (done) {
        break;
      }
      uthread_yield();
    }
  } else {
    pthread_mutex_lock(&lock);
    while (t->state != UT_FINISHED) {
      pthread_cond_wait(&done_cv, &lock);
    }
    pthread_mutex_unlock(&lock);
  }

  if (retval) {
    *retval = t->retval;
  }
  stack_put(t->stack);
  free(t);
  return 0;
}
