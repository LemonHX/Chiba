#pragma once

#include "pool.h"

#define err(str) fprintf(stderr, str)

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

PRIVATE volatile i32 threads_keepalive;
PRIVATE volatile i32 threads_on_hold;

/* ========================== STRUCTURES ============================ */

/* Binary semaphore */
typedef struct bsem {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  i32 v;
} bsem;

/* Job */
typedef struct job {
  struct job *prev;            /* pointer to previous job   */
  void (*function)(void *arg); /* function pointer          */
  void *arg;                   /* function's argument       */
} job;

/* Job queue */
typedef struct jobqueue {
  pthread_mutex_t rwmutex; /* used for queue r/w access */
  job *front;              /* pointer to front of queue */
  job *rear;               /* pointer to rear  of queue */
  bsem *has_jobs;          /* flag as binary semaphore  */
  i32 len;                 /* number of jobs in queue   */
} jobqueue;

/* Thread */
typedef struct thread {
  i32 id;            /* friendly id               */
  pthread_t pthread; /* pointer to actual thread  */
  struct chiba_iocpu_workerpool *chiba_iocpu_workerpoolp; /* access to thpool */
} thread;

/* Threadpool */
typedef struct chiba_iocpu_workerpool {
  thread **threads;                 /* pointer to threads        */
  volatile i32 num_threads_alive;   /* threads currently alive   */
  volatile i32 num_threads_working; /* threads currently working */
  pthread_mutex_t thcount_lock;     /* used for thread count etc */
  pthread_cond_t threads_all_idle;  /* signal to chiba_iocpu_workerpoolwait  */
  jobqueue jobqueue;                /* job queue                 */
} chiba_iocpu_workerpool;

/* ========================== PROTOTYPES ============================ */

PRIVATE i32 thread_init(chiba_iocpu_workerpool *chiba_iocpu_workerpoolp,
                        struct thread **thread_p, i32 id);
PRIVATE void *thread_do(struct thread *thread_p);
PRIVATE void thread_destroy(struct thread *thread_p);

PRIVATE i32 jobqueue_init(jobqueue *jobqueue_p);
PRIVATE void jobqueue_clear(jobqueue *jobqueue_p);
PRIVATE void jobqueue_push(jobqueue *jobqueue_p, struct job *newjob_p);
PRIVATE struct job *jobqueue_pull(jobqueue *jobqueue_p);
PRIVATE void jobqueue_destroy(jobqueue *jobqueue_p);

PRIVATE void bsem_init(struct bsem *bsem_p, i32 value);
PRIVATE void bsem_reset(struct bsem *bsem_p);
PRIVATE void bsem_post(struct bsem *bsem_p);
PRIVATE void bsem_post_all(struct bsem *bsem_p);
PRIVATE void bsem_wait(struct bsem *bsem_p);

/* ========================== THREADPOOL ============================ */

/* Initialise thread pool */
struct chiba_iocpu_workerpool *chiba_iocpu_workerpoolinit(i32 num_threads) {

  threads_on_hold = 0;
  threads_keepalive = 1;

  if (num_threads < 0) {
    num_threads = 0;
  }

  /* Make new thread pool */
  chiba_iocpu_workerpool *chiba_iocpu_workerpoolp;
  chiba_iocpu_workerpoolp =
      (struct chiba_iocpu_workerpool *)CHIBA_INTERNAL_malloc(
          sizeof(struct chiba_iocpu_workerpool));
  if (chiba_iocpu_workerpoolp == NULL) {
    err("chiba_iocpu_workerpoolinit(): Could not allocate memory for thread "
        "pool\n");
    return NULL;
  }
  chiba_iocpu_workerpoolp->num_threads_alive = 0;
  chiba_iocpu_workerpoolp->num_threads_working = 0;

  /* Initialise the job queue */
  if (jobqueue_init(&chiba_iocpu_workerpoolp->jobqueue) == -1) {
    err("chiba_iocpu_workerpoolinit(): Could not allocate memory for job "
        "queue\n");
    CHIBA_INTERNAL_free(chiba_iocpu_workerpoolp);
    return NULL;
  }

  /* Make threads in pool */
  chiba_iocpu_workerpoolp->threads = (struct thread **)CHIBA_INTERNAL_malloc(
      num_threads * sizeof(struct thread *));
  if (chiba_iocpu_workerpoolp->threads == NULL) {
    err("chiba_iocpu_workerpoolinit(): Could not allocate memory for "
        "threads\n");
    jobqueue_destroy(&chiba_iocpu_workerpoolp->jobqueue);
    CHIBA_INTERNAL_free(chiba_iocpu_workerpoolp);
    return NULL;
  }

  pthread_mutex_init(&(chiba_iocpu_workerpoolp->thcount_lock), NULL);
  pthread_cond_init(&chiba_iocpu_workerpoolp->threads_all_idle, NULL);

  /* Thread init */
  i32 n;
  for (n = 0; n < num_threads; n++) {
    thread_init(chiba_iocpu_workerpoolp, &chiba_iocpu_workerpoolp->threads[n],
                n);
#if CHIBA_IOCPU_WORKER_POOLDEBUG
    printf("CHIBA_IOCPU_WORKER_POOLDEBUG: Created thread %d in pool \n", n);
#endif
  }

  /* Wait for threads to initialize */
  while (chiba_iocpu_workerpoolp->num_threads_alive != num_threads) {
  }

  return chiba_iocpu_workerpoolp;
}

/* Add work to the thread pool */
i32 chiba_iocpu_workerpooladd_work(
    chiba_iocpu_workerpool *chiba_iocpu_workerpoolp, void (*function_p)(void *),
    void *arg_p) {
  job *newjob;

  newjob = (struct job *)CHIBA_INTERNAL_malloc(sizeof(struct job));
  if (newjob == NULL) {
    err("chiba_iocpu_workerpooladd_work(): Could not allocate memory for new "
        "job\n");
    return -1;
  }

  /* add function and argument */
  newjob->function = function_p;
  newjob->arg = arg_p;

  /* add job to queue */
  jobqueue_push(&chiba_iocpu_workerpoolp->jobqueue, newjob);

  return 0;
}

/* Wait until all jobs have finished */
void chiba_iocpu_workerpoolwait(
    chiba_iocpu_workerpool *chiba_iocpu_workerpoolp) {
  pthread_mutex_lock(&chiba_iocpu_workerpoolp->thcount_lock);
  while (chiba_iocpu_workerpoolp->jobqueue.len ||
         chiba_iocpu_workerpoolp->num_threads_working) {
    pthread_cond_wait(&chiba_iocpu_workerpoolp->threads_all_idle,
                      &chiba_iocpu_workerpoolp->thcount_lock);
  }
  pthread_mutex_unlock(&chiba_iocpu_workerpoolp->thcount_lock);
}

/* Destroy the threadpool */
void chiba_iocpu_workerpooldestroy(
    chiba_iocpu_workerpool *chiba_iocpu_workerpoolp) {
  /* No need to destroy if it's NULL */
  if (chiba_iocpu_workerpoolp == NULL)
    return;

  volatile i32 threads_total = chiba_iocpu_workerpoolp->num_threads_alive;

  /* End each thread 's infinite loop */
  threads_keepalive = 0;

  /* Give one second to kill idle threads */
  double TIMEOUT = 1.0;
  time_t start, end;
  double tpassed = 0.0;
  time(&start);
  while (tpassed < TIMEOUT && chiba_iocpu_workerpoolp->num_threads_alive) {
    bsem_post_all(chiba_iocpu_workerpoolp->jobqueue.has_jobs);
    time(&end);
    tpassed = difftime(end, start);
  }

  /* Poll remaining threads */
  while (chiba_iocpu_workerpoolp->num_threads_alive) {
    bsem_post_all(chiba_iocpu_workerpoolp->jobqueue.has_jobs);
    sleep(1);
  }

  /* Job queue cleanup */
  jobqueue_destroy(&chiba_iocpu_workerpoolp->jobqueue);
  /* Deallocs */
  i32 n;
  for (n = 0; n < threads_total; n++) {
    thread_destroy(chiba_iocpu_workerpoolp->threads[n]);
  }
  CHIBA_INTERNAL_free(chiba_iocpu_workerpoolp->threads);
  CHIBA_INTERNAL_free(chiba_iocpu_workerpoolp);
}

/* Pause all threads in threadpool */
void chiba_iocpu_workerpoolpause(
    chiba_iocpu_workerpool *chiba_iocpu_workerpoolp) {
  (void)chiba_iocpu_workerpoolp;
  threads_on_hold = 1;
}

/* Resume all threads in threadpool */
void chiba_iocpu_workerpoolresume(
    chiba_iocpu_workerpool *chiba_iocpu_workerpoolp) {
  (void)chiba_iocpu_workerpoolp;
  threads_on_hold = 0;
}

i32 chiba_iocpu_workerpoolnum_threads_working(
    chiba_iocpu_workerpool *chiba_iocpu_workerpoolp) {
  return chiba_iocpu_workerpoolp->num_threads_working;
}

/* ============================ THREAD ============================== */

/* Initialize a thread in the thread pool
 *
 * @param thread        address to the pointer of the thread to be created
 * @param id            id to be given to the thread
 * @return 0 on success, -1 otherwise.
 */
PRIVATE i32 thread_init(chiba_iocpu_workerpool *chiba_iocpu_workerpoolp,
                        struct thread **thread_p, i32 id) {

  *thread_p = (struct thread *)CHIBA_INTERNAL_malloc(sizeof(struct thread));
  if (*thread_p == NULL) {
    err("thread_init(): Could not allocate memory for thread\n");
    return -1;
  }

  (*thread_p)->chiba_iocpu_workerpoolp = chiba_iocpu_workerpoolp;
  (*thread_p)->id = id;

  pthread_create(&(*thread_p)->pthread, NULL, (void *(*)(void *))thread_do,
                 (*thread_p));
  pthread_detach((*thread_p)->pthread);
  return 0;
}

/* What each thread is doing
 *
 * In principle this is an endless loop. The only time this loop gets
 * interrupted is once chiba_iocpu_workerpooldestroy() is invoked or the program
 * exits.
 *
 * @param  thread        thread that will run this function
 * @return nothing
 */
PRIVATE void *thread_do(struct thread *thread_p) {

  /* Assure all threads have been created before starting serving */
  chiba_iocpu_workerpool *chiba_iocpu_workerpoolp =
      thread_p->chiba_iocpu_workerpoolp;

  /* Mark thread as alive (initialized) */
  pthread_mutex_lock(&chiba_iocpu_workerpoolp->thcount_lock);
  chiba_iocpu_workerpoolp->num_threads_alive += 1;
  pthread_mutex_unlock(&chiba_iocpu_workerpoolp->thcount_lock);

  while (threads_keepalive) {

    bsem_wait(chiba_iocpu_workerpoolp->jobqueue.has_jobs);

    if (threads_keepalive) {
      while (threads_on_hold) {
        sleep(1);
      }

      pthread_mutex_lock(&chiba_iocpu_workerpoolp->thcount_lock);
      chiba_iocpu_workerpoolp->num_threads_working++;
      pthread_mutex_unlock(&chiba_iocpu_workerpoolp->thcount_lock);

      /* Read job from queue and execute it */
      void (*func_buff)(void *);
      void *arg_buff;
      job *job_p = jobqueue_pull(&chiba_iocpu_workerpoolp->jobqueue);
      if (job_p) {
        func_buff = job_p->function;
        arg_buff = job_p->arg;
        func_buff(arg_buff);
        CHIBA_INTERNAL_free(job_p);
      }

      pthread_mutex_lock(&chiba_iocpu_workerpoolp->thcount_lock);
      chiba_iocpu_workerpoolp->num_threads_working--;
      if (!chiba_iocpu_workerpoolp->num_threads_working) {
        pthread_cond_signal(&chiba_iocpu_workerpoolp->threads_all_idle);
      }
      pthread_mutex_unlock(&chiba_iocpu_workerpoolp->thcount_lock);
    }
  }
  pthread_mutex_lock(&chiba_iocpu_workerpoolp->thcount_lock);
  chiba_iocpu_workerpoolp->num_threads_alive--;
  pthread_mutex_unlock(&chiba_iocpu_workerpoolp->thcount_lock);

  return NULL;
}

/* Frees a thread  */
PRIVATE void thread_destroy(thread *thread_p) { CHIBA_INTERNAL_free(thread_p); }

/* ============================ JOB QUEUE =========================== */

/* Initialize queue */
PRIVATE i32 jobqueue_init(jobqueue *jobqueue_p) {
  jobqueue_p->len = 0;
  jobqueue_p->front = NULL;
  jobqueue_p->rear = NULL;

  jobqueue_p->has_jobs =
      (struct bsem *)CHIBA_INTERNAL_malloc(sizeof(struct bsem));
  if (jobqueue_p->has_jobs == NULL) {
    return -1;
  }

  pthread_mutex_init(&(jobqueue_p->rwmutex), NULL);
  bsem_init(jobqueue_p->has_jobs, 0);

  return 0;
}

/* Clear the queue */
PRIVATE void jobqueue_clear(jobqueue *jobqueue_p) {

  while (jobqueue_p->len) {
    CHIBA_INTERNAL_free(jobqueue_pull(jobqueue_p));
  }

  jobqueue_p->front = NULL;
  jobqueue_p->rear = NULL;
  bsem_reset(jobqueue_p->has_jobs);
  jobqueue_p->len = 0;
}

/* Add (allocated) job to queue
 */
PRIVATE void jobqueue_push(jobqueue *jobqueue_p, struct job *newjob) {

  pthread_mutex_lock(&jobqueue_p->rwmutex);
  newjob->prev = NULL;

  switch (jobqueue_p->len) {

  case 0: /* if no jobs in queue */
    jobqueue_p->front = newjob;
    jobqueue_p->rear = newjob;
    break;

  default: /* if jobs in queue */
    jobqueue_p->rear->prev = newjob;
    jobqueue_p->rear = newjob;
  }
  jobqueue_p->len++;

  bsem_post(jobqueue_p->has_jobs);
  pthread_mutex_unlock(&jobqueue_p->rwmutex);
}

/* Get first job from queue(removes it from queue)
 * Notice: Caller MUST hold a mutex
 */
PRIVATE struct job *jobqueue_pull(jobqueue *jobqueue_p) {

  pthread_mutex_lock(&jobqueue_p->rwmutex);
  job *job_p = jobqueue_p->front;

  switch (jobqueue_p->len) {

  case 0: /* if no jobs in queue */
    break;

  case 1: /* if one job in queue */
    jobqueue_p->front = NULL;
    jobqueue_p->rear = NULL;
    jobqueue_p->len = 0;
    break;

  default: /* if >1 jobs in queue */
    jobqueue_p->front = job_p->prev;
    jobqueue_p->len--;
    /* more than one job in queue -> post it */
    bsem_post(jobqueue_p->has_jobs);
  }

  pthread_mutex_unlock(&jobqueue_p->rwmutex);
  return job_p;
}

/* Free all queue resources back to the system */
PRIVATE void jobqueue_destroy(jobqueue *jobqueue_p) {
  jobqueue_clear(jobqueue_p);
  CHIBA_INTERNAL_free(jobqueue_p->has_jobs);
}

/* ======================== SYNCHRONISATION ========================= */

/* Init semaphore to 1 or 0 */
PRIVATE void bsem_init(bsem *bsem_p, i32 value) {
  if (value < 0 || value > 1) {
    err("bsem_init(): Binary semaphore can take only values 1 or 0");
    exit(1);
  }
  pthread_mutex_init(&(bsem_p->mutex), NULL);
  pthread_cond_init(&(bsem_p->cond), NULL);
  bsem_p->v = value;
}

/* Reset semaphore to 0 */
PRIVATE void bsem_reset(bsem *bsem_p) {
  pthread_mutex_destroy(&(bsem_p->mutex));
  pthread_cond_destroy(&(bsem_p->cond));
  bsem_init(bsem_p, 0);
}

/* Post to at least one thread */
PRIVATE void bsem_post(bsem *bsem_p) {
  pthread_mutex_lock(&bsem_p->mutex);
  bsem_p->v = 1;
  pthread_cond_signal(&bsem_p->cond);
  pthread_mutex_unlock(&bsem_p->mutex);
}

/* Post to all threads */
PRIVATE void bsem_post_all(bsem *bsem_p) {
  pthread_mutex_lock(&bsem_p->mutex);
  bsem_p->v = 1;
  pthread_cond_broadcast(&bsem_p->cond);
  pthread_mutex_unlock(&bsem_p->mutex);
}

/* Wait on semaphore until semaphore has value 0 */
PRIVATE void bsem_wait(bsem *bsem_p) {
  pthread_mutex_lock(&bsem_p->mutex);
  while (bsem_p->v != 1) {
    pthread_cond_wait(&bsem_p->cond, &bsem_p->mutex);
  }
  bsem_p->v = 0;
  pthread_mutex_unlock(&bsem_p->mutex);
}
