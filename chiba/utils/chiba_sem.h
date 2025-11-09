#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Semaphore
//////////////////////////////////////////////////////////////////////////////////

#include "../basic_memory.h"

typedef struct chiba_sem {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  u32 value;
} chiba_sem;

UTILS int chiba_sem_init(chiba_sem *sem, u32 value);
UTILS chiba_sem *chiba_sem_new() {
  chiba_sem *sem = (chiba_sem *)CHIBA_INTERNAL_malloc(sizeof(chiba_sem));
  if (unlikely(!sem))
    goto error;
  if (chiba_sem_init(sem, 0) != 0)
    goto error;
  return sem;
error:
  CHIBA_PANIC("Failed to create semaphore");
}

UTILS int chiba_sem_init(chiba_sem *sem, u32 value) {
  sem->value = value;
  if (pthread_mutex_init(&sem->mutex, NULL) != 0)
    return -1;
  if (pthread_cond_init(&sem->cond, NULL) != 0) {
    pthread_mutex_destroy(&sem->mutex);
    return -1;
  }
  return 0;
}

UTILS void chiba_sem_drop(chiba_sem *sem) {
  pthread_cond_destroy(&sem->cond);
  pthread_mutex_destroy(&sem->mutex);
}

UTILS void chiba_sem_wait(chiba_sem *sem) {
  pthread_mutex_lock(&sem->mutex);
  while (sem->value == 0) {
    pthread_cond_wait(&sem->cond, &sem->mutex);
  }
  sem->value--;
  pthread_mutex_unlock(&sem->mutex);
}

UTILS void chiba_sem_post(chiba_sem *sem) {
  pthread_mutex_lock(&sem->mutex);
  sem->value++;
  pthread_cond_signal(&sem->cond);
  pthread_mutex_unlock(&sem->mutex);
}

UTILS void chiba_sem_post_all(chiba_sem *sem) {
  pthread_mutex_lock(&sem->mutex);
  sem->value++;
  pthread_cond_broadcast(&sem->cond);
  pthread_mutex_unlock(&sem->mutex);
}
