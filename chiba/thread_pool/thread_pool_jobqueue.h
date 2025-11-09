#pragma once
#include "../basic_memory.h"
#include "../concurrency/array_queue.h"
#include "../utils/chiba_sem.h"
#include "thread_pool.h"

/* Job */
typedef struct chiba_thread_pool_job {
  void (*entry)(anyptr arg); /* function pointer          */
  anyptr arg;                /* function's argument       */
} chiba_thread_pool_job;

/* Job queue */
typedef struct chiba_thread_pool_jobqueue {
  chiba_arrayqueue *jobs; /* job queue                 */
  chiba_sem *has_jobs;    /* flag as binary semaphore  */
} chiba_thread_pool_jobqueue;

/* Initialize queue */
UTILS chiba_thread_pool_jobqueue *jobqueue_new() {
  chiba_thread_pool_jobqueue *jq =
      (chiba_thread_pool_jobqueue *)CHIBA_INTERNAL_malloc(
          sizeof(chiba_thread_pool_jobqueue));
  if (unlikely(!jq))
    goto error;
  jq->jobs = chiba_arrayqueue_new(65536);
  jq->has_jobs = chiba_sem_new();
  if (unlikely(!jq->jobs || !jq->has_jobs))
    goto error;
  return jq;
error:
  CHIBA_PANIC("Failed to initialize jobqueue");
}

UTILS void jobqueue_push(chiba_thread_pool_jobqueue *jobqueue_p,
                         chiba_thread_pool_job *newjob) {
  chiba_arrayqueue_push(jobqueue_p->jobs, newjob);
  chiba_sem_post(jobqueue_p->has_jobs);
}

UTILS chiba_thread_pool_job *
jobqueue_pull(chiba_thread_pool_jobqueue *jobqueue_p) {
  chiba_thread_pool_job *job_p =
      (chiba_thread_pool_job *)chiba_arrayqueue_pop(jobqueue_p->jobs);
  if (unlikely(!job_p))
    return NULL;
  chiba_sem_post(jobqueue_p->has_jobs);
  return job_p;
}

/* Free all queue resources back to the system */
UTILS void jobqueue_destroy(chiba_thread_pool_jobqueue *jobqueue_p) {
  chiba_arrayqueue_drop(jobqueue_p->jobs);
  chiba_sem_drop(jobqueue_p->has_jobs);
  CHIBA_INTERNAL_free(jobqueue_p);
}

UTILS bool jobqueue_is_empty(chiba_thread_pool_jobqueue *jq) {
  return chiba_arrayqueue_size(jq->jobs) > 0;
}