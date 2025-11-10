#include "thread_pool.h"

void task(void *arg) {
  printf("Thread #%u working on %d\n", (int)pthread_self(), (int)arg);
}

int main() {

  chiba_thread_pool *thpool = chiba_thread_pool_new();

  puts("Adding 40 tasks to threadpool\n");
  int i;
  for (i = 0; i < 40; i++) {
    chiba_thread_pool_add_work(thpool, task, (void *)(uintptr_t)i);
  }

  chiba_thread_pool_wait(thpool);
  puts("\nKilling threadpool\n");
  chiba_thread_pool_drop(thpool);

  return 0;
}