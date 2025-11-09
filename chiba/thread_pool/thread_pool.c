#include "thread_pool.h"
#include "thread_pool_jobqueue.h"

/* ========================== STRUCTURES ============================ */
typedef struct chiba_thread chiba_thread;
/* Threadpool */
typedef struct chiba_thread_pool {
  chiba_thread **threads; /* pointer to threads        */

  pthread_mutex_t thcount_lock;     /* used for thread count etc */
  volatile i32 num_threads_alive;   /* threads currently alive   */
  volatile i32 num_threads_working; /* threads currently working */

  pthread_cond_t threads_all_idle; /* signal to chiba_thread_poolwait     */

  chiba_thread_pool_jobqueue *jobqueue; /* job queue                 */
} chiba_thread_pool;

static volatile i32 threads_keepalive;
static volatile i32 threads_on_hold;

/* Thread */
typedef struct chiba_thread {
  i32 id;                  /* friendly id               */
  pthread_t pthread;       /* pointer to actual thread  */
  chiba_thread_pool *pool; /* access to thpool          */
} chiba_thread;

UTILS anyptr thread_do(chiba_thread *thread_p);
UTILS i32 thread_init(chiba_thread_pool *pool, chiba_thread **thread_p,
                      i32 id) {

  *thread_p = (chiba_thread *)CHIBA_INTERNAL_malloc(sizeof(chiba_thread));
  if (*thread_p == NULL) {
    CHIBA_PANIC("Could not allocate memory for thread\n");
    return -1;
  }

  (*thread_p)->pool = pool;
  (*thread_p)->id = id;

  pthread_create(&(*thread_p)->pthread, NULL, (anyptr(*)(anyptr))thread_do,
                 (*thread_p));
  pthread_detach((*thread_p)->pthread);
  return 0;
}

/* Sets the calling thread on hold */
UTILS void thread_hold(i32 sig_id) {
  (void)sig_id;
  threads_on_hold = 1;
  while (threads_on_hold) {
    sleep(1);
  }
}

/* What each thread is doing
 *
 * In principle this is an endless loop. The only time this loop gets
 * interrupted is once chiba_thread_pooldestroy() is invoked or the program
 * exits.
 *
 * @param  thread        thread that will run this function
 * @return nothing
 */
UTILS anyptr thread_do(chiba_thread *thread_p) {

  /* Set thread name for profiling and debugging */

#if defined(__linux__)
  char thread_name[16] = {0};
  snprintf(thread_name, 16,
           "CHIBA_TP_THREAD"
           "-%d",
           thread_p->id);
  /* Use prctl instead to prevent using _GNU_SOURCE flag and implicit
   * declaration */
  prctl(PR_SET_NAME, thread_name);
#elif defined(__APPLE__) && defined(__MACH__)
  char thread_name[16] = {0};
  snprintf(thread_name, 16,
           "CHIBA_TP_THREAD"
           "-%d",
           thread_p->id);
  pthread_setname_np(thread_name);
#elif defined(__FreeBSD__) || defined(__OpenBSD__)
  char thread_name[16] = {0};
  snprintf(thread_name, 16,
           "CHIBA_TP_THREAD"
           "-%d",
           thread_p->id);
  pthread_set_name_np(thread_p->pthread, thread_name);
#else
#endif

  /* Assure all threads have been created before starting serving */
  chiba_thread_pool *pool = thread_p->pool;

  /* Register signal handler */
  // struct sigaction act;
  // sigemptyset(&act.sa_mask);
  // act.sa_flags = SA_ONSTACK;
  // act.sa_handler = thread_hold;
  // if (sigaction(SIGUSR1, &act, NULL) == -1) {
  //   err("thread_do(): cannot handle SIGUSR1");
  // }

  /* Mark thread as alive (initialized) */
  pthread_mutex_lock(&pool->thcount_lock);
  pool->num_threads_alive += 1;
  pthread_mutex_unlock(&pool->thcount_lock);

  while (threads_keepalive) {

    chiba_sem_wait(pool->jobqueue->has_jobs);

    if (threads_keepalive) {
      pthread_mutex_lock(&pool->thcount_lock);
      pool->num_threads_working++;
      pthread_mutex_unlock(&pool->thcount_lock);

      /* Read job from queue and execute it */
      void (*func_buff)(anyptr);
      anyptr arg_buff;
      chiba_thread_pool_job *job_p = jobqueue_pull(pool->jobqueue);
      if (job_p) {
        func_buff = job_p->entry;
        arg_buff = job_p->arg;
        func_buff(arg_buff);
        CHIBA_INTERNAL_free(job_p);
      }

      pthread_mutex_lock(&pool->thcount_lock);
      pool->num_threads_working--;
      if (!pool->num_threads_working) {
        pthread_cond_signal(&pool->threads_all_idle);
      }
      pthread_mutex_unlock(&pool->thcount_lock);
    }
  }
  pthread_mutex_lock(&pool->thcount_lock);
  pool->num_threads_alive--;
  pthread_mutex_unlock(&pool->thcount_lock);
  return NULL;
}

/* Frees a thread  */
static void thread_destroy(chiba_thread *thread_p) {
  CHIBA_INTERNAL_free(thread_p);
}

/* ========================== THREADPOOL ============================ */

/* Initialise thread pool */
PUBLIC chiba_thread_pool *chiba_thread_pool_new() {

  threads_on_hold = 0;
  threads_keepalive = 1;

  i32 num_threads = (i32)get_cpu_count();

  /* Make new thread pool */
  chiba_thread_pool *pool;
  pool = (struct chiba_thread_pool *)CHIBA_INTERNAL_malloc(
      sizeof(struct chiba_thread_pool));
  if (pool == NULL) {
    CHIBA_PANIC("Could not allocate memory for thread pool\n");
    return NULL;
  }
  pool->num_threads_alive = 0;
  pool->num_threads_working = 0;

  /* Initialise the job queue */
  pool->jobqueue = jobqueue_new();
  if (!pool->jobqueue) {
    CHIBA_PANIC("Could not allocate memory for job queue\n");
    CHIBA_INTERNAL_free(pool);
    return NULL;
  }

  /* Make threads in pool */
  pool->threads = (chiba_thread **)CHIBA_INTERNAL_malloc(
      num_threads * sizeof(chiba_thread *));
  if (pool->threads == NULL) {
    CHIBA_PANIC("Could not allocate memory for threads\n");
    jobqueue_destroy(pool->jobqueue);
    CHIBA_INTERNAL_free(pool);
    return NULL;
  }

  pthread_cond_init(&pool->threads_all_idle, NULL);

  /* Thread init */
  i32 n;
  for (n = 0; n < num_threads; n++) {
    thread_init(pool, &pool->threads[n], n);
#if THPOOL_DEBUG
    printf("THPOOL_DEBUG: Created thread %d in pool \n", n);
#endif
  }

  /* Wait for threads to initialize */
  while (pool->num_threads_alive != num_threads) {
  }

  return pool;
}

/* Add work to the thread pool */
PUBLIC i32 chiba_thread_pool_add_work(chiba_thread_pool *pool,
                                      void (*function_p)(anyptr),
                                      anyptr arg_p) {
  chiba_thread_pool_job *newjob;

  newjob = (chiba_thread_pool_job *)CHIBA_INTERNAL_malloc(
      sizeof(chiba_thread_pool_job));
  if (newjob == NULL) {
    CHIBA_PANIC("Could not allocate memory for new job\n");
    return -1;
  }

  /* add function and argument */
  newjob->entry = function_p;
  newjob->arg = arg_p;

  /* add job to queue */
  jobqueue_push(pool->jobqueue, newjob);

  return 0;
}

/* Wait until all jobs have finished */
PUBLIC void chiba_thread_pool_wait(chiba_thread_pool *pool) {
  pthread_mutex_lock(&pool->thcount_lock);
  while (jobqueue_is_empty(pool->jobqueue) || pool->num_threads_working) {
    pthread_cond_wait(&pool->threads_all_idle, &pool->thcount_lock);
  }
  pthread_mutex_unlock(&pool->thcount_lock);
}

/* Pause all threads in threadpool */
void chiba_thread_pool_pause(chiba_thread_pool *pool) {
  i32 n;
  for (n = 0; n < pool->num_threads_alive; n++) {
    pthread_kill(pool->threads[n]->pthread, SIGUSR1);
  }
}

/* Resume all threads in threadpool */
PUBLIC void chiba_thread_pool_resume(chiba_thread_pool *pool) {
  // resuming a single threadpool hasn't been
  // implemented yet, meanwhile this suppresses
  // the warnings
  (void)pool;

  threads_on_hold = 0;
}

/* Destroy the threadpool */
PUBLIC void chiba_thread_pool_drop(chiba_thread_pool *pool) {
  /* No need to destroy if it's NULL */
  if (pool == NULL)
    return;

  volatile i32 threads_total = pool->num_threads_alive;

  /* End each thread 's infinite loop */
  threads_keepalive = 0;

  /* Give one second to kill idle threads */
  double TIMEOUT = 1.0;
  time_t start, end;
  double tpassed = 0.0;
  time(&start);
  while (tpassed < TIMEOUT && pool->num_threads_alive) {
    chiba_sem_post_all(pool->jobqueue->has_jobs);
    time(&end);
    tpassed = difftime(end, start);
  }

  /* Poll remaining threads */
  while (pool->num_threads_alive) {
    chiba_sem_post_all(pool->jobqueue->has_jobs);
    sleep(1);
  }

  /* Job queue cleanup */
  jobqueue_destroy(pool->jobqueue);
  /* Deallocs */
  i32 n;
  for (n = 0; n < threads_total; n++) {
    thread_destroy(pool->threads[n]);
  }
  CHIBA_INTERNAL_free(pool->threads);
  CHIBA_INTERNAL_free(pool);
}
