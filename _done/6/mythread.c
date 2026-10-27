#define _GNU_SOURCE
#include "mythread.h"

#include <errno.h>
#include <linux/futex.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

static _Atomic int g_handler_installed = 0;

static _Atomic(struct stack_node*) g_stack_free_list = NULL;
static long g_page_size_cache = 0;

static long page_size(void) {
  if (!g_page_size_cache) {
    g_page_size_cache = sysconf(_SC_PAGESIZE);
  }
  return g_page_size_cache;
}

static void stack_cache_push(void* stack_base) {
  struct stack_node* node = malloc(sizeof(*node));
  if (!node) {
    munmap(stack_base, STACK_SIZE + page_size());
    return;
  }
  node->stack_base = stack_base;
  struct stack_node* old_head = atomic_load(&g_stack_free_list);
  do {
    node->next = old_head;
  } while (!atomic_compare_exchange_weak(&g_stack_free_list, &old_head, node));
}

static void* stack_cache_pop(void) {
  struct stack_node* old_head = atomic_load(&g_stack_free_list);
  while (old_head) {
    if (atomic_compare_exchange_weak(&g_stack_free_list, &old_head,
                                     old_head->next)) {
      void* stack_base = old_head->stack_base;
      free(old_head);
      return stack_base;
    }
  }
  return NULL;
}

static void* stack_alloc(void) {
  void* stack_base = stack_cache_pop();
  if (stack_base) {
    return stack_base;
  }

  long guard = page_size();
  void* mem = mmap(NULL, STACK_SIZE + guard, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  if (mem == MAP_FAILED) {
    return NULL;
  }

  if (madvise(mem, guard, MADV_GUARD_INSTALL) == -1) {
    perror("guard page setup failed");
    munmap(mem, STACK_SIZE + guard);
    return NULL;
  }
  return mem;
}

static int thread_entry(void* raw) {
  struct mythread_tcb* tcb = raw;
  tcb->tid = gettid();
  void* ret = tcb->start_routine(tcb->arg);
  tcb->retval = ret;

  if (atomic_load(&tcb->detached)) {
    void* stack_base = tcb->stack_base;
    free(tcb);
    stack_cache_push(stack_base);
  }
  syscall(SYS_exit, 0);
  return 0;
}

static void cancel_signal_handler(int sig) {
  (void)sig;
  syscall(SYS_exit, 0);
}

static void ensure_cancel_handler(void) {
  int expected = 0;
  if (atomic_compare_exchange_strong(&g_handler_installed, &expected, 1)) {
    struct sigaction sa;
    sa.sa_handler = cancel_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(MYTHREAD_CANCEL_SIG, &sa, NULL);
  }
}

int mythread_create(mythread_t* thread,
                    void* (*start_routine)(void*),
                    void* arg) {
  ensure_cancel_handler();
  void* stack_base = stack_alloc();
  if (!stack_base) {
    perror("stack_alloc failed");
    return -1;
  }

  struct mythread_tcb* tcb = malloc(sizeof(*tcb));
  if (!tcb) {
    stack_cache_push(stack_base);
    return -1;
  }
  tcb->futex_word = 1;
  tcb->retval = NULL;
  tcb->start_routine = start_routine;
  tcb->arg = arg;
  tcb->stack_base = stack_base;
  tcb->detached = 0;
  tcb->cancelled = 0;

  void* stack_top = (char*)stack_base + page_size() + STACK_SIZE;
  int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD |
              CLONE_CHILD_CLEARTID;

  pid_t tid =
      clone(thread_entry, stack_top, flags, tcb, NULL, NULL, &tcb->futex_word);
  if (tid == -1) {
    perror("clone failed");
    free(tcb);
    stack_cache_push(stack_base);
    return -1;
  }
  tcb->tid = tid;
  *thread = tcb;
  return 0;
}

int mythread_join(mythread_t thread, void** retval) {
  if (!thread) {
    return -1;
  }
  if (atomic_load(&thread->detached)) {
    return -1;
  }
  while (atomic_load(&thread->futex_word) != 0) {
    long r =
        syscall(SYS_futex, &thread->futex_word, FUTEX_WAIT, 1, NULL, NULL, 0);
    if (r == -1 && errno != EAGAIN && errno != EINTR) {
      break;
    }
  }

  if (retval) {
    *retval = thread->cancelled ? MYTHREAD_CANCELED : thread->retval;
  }

  stack_cache_push(thread->stack_base);
  free(thread);
  return 0;
}

int mythread_detach(mythread_t thread) {
  if (!thread) {
    return -1;
  }
  int already_exited =
      atomic_load(&thread->futex_word) == 0;
  atomic_store(&thread->detached, 1);
  if (already_exited) {
    stack_cache_push(thread->stack_base);
    free(thread);
  }
  return 0;
}

int mythread_cancel(mythread_t thread) {
  if (!thread) {
    return -1;
  }
  atomic_store(&thread->cancelled, 1);
  int result = syscall(SYS_tgkill, getpid(), thread->tid, MYTHREAD_CANCEL_SIG);
  return result == -1 ? -1 : 0;
}
