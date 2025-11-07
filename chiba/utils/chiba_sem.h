#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Semaphore
//////////////////////////////////////////////////////////////////////////////////

#include "../basic_types.h"
#include <assert.h>

#if defined(__APPLE__) || defined(__MACH__)
#define CHIBA_SEM_MACOS
#elif defined(_WIN32) || defined(_WIN64)
#define CHIBA_SEM_WIN
#else
#define CHIBA_SEM_POSIX
#endif

#ifdef CHIBA_SEM_POSIX
#include <errno.h>
#include <semaphore.h>

typedef struct chiba_sem {
  sem_t handle;
} chiba_sem;

UTILS int chiba_sem_init(chiba_sem *sem, u32 value) {
  int ret = sem_init(&sem->handle, 0, value);
  return (ret == 0) ? 0 : -errno;
}

UTILS void chiba_sem_fini(chiba_sem *sem) {
  int ret = sem_destroy(&sem->handle);
  (void)ret;
  assert(ret == 0);
}

UTILS void chiba_sem_wait(chiba_sem *sem) {
  int ret;
  do {
    ret = sem_wait(&sem->handle);
    assert(ret == 0 || (ret == -1 && errno == EINTR));
  } while (ret != 0);
}

UTILS void chiba_sem_post(chiba_sem *sem) {
  int ret = sem_post(&sem->handle);
  (void)ret;
  assert(ret == 0);
}

#elif defined(CHIBA_SEM_MACOS)
typedef struct chiba_sem {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  u32 value;
} chiba_sem;

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

UTILS void chiba_sem_fini(chiba_sem *sem) {
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

#elif defined(CHIBA_SEM_WIN)
typedef struct chiba_sem {
  HANDLE handle;
} chiba_sem;

UTILS int chiba_sem_init(chiba_sem *sem, u32 value) {
  sem->handle = CreateSemaphore(NULL, value, 0x7FFFFFFF, NULL);
  return sem->handle ? 0 : -1;
}

UTILS void chiba_sem_fini(chiba_sem *sem) { CloseHandle(sem->handle); }

UTILS void chiba_sem_wait(chiba_sem *sem) {
  WaitForSingleObject(sem->handle, INFINITE);
}

UTILS void chiba_sem_post(chiba_sem *sem) {
  ReleaseSemaphore(sem->handle, 1, NULL);
}

#endif
