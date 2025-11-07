#include "thread_pool.h"

//////////////////////////////////////////////////////////////////////////////////
// Benchmark Configuration
//////////////////////////////////////////////////////////////////////////////////

#define NUM_TASKS 10000
#define TASK_WORK_ITERATIONS 10000

//////////////////////////////////////////////////////////////////////////////////
// Timing Utilities
//////////////////////////////////////////////////////////////////////////////////

typedef struct {
  double elapsed_ms;
  double throughput; // tasks per second
} BenchResult;

static inline double get_time_ms(void) {
#if defined(_WIN32) || defined(_WIN64)
  LARGE_INTEGER frequency, counter;
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  return (counter.QuadPart * 1000.0) / frequency.QuadPart;
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
#endif
}

//////////////////////////////////////////////////////////////////////////////////
// Task Workload
//////////////////////////////////////////////////////////////////////////////////

// Simulate some CPU work
static u64 do_work(u64 seed) {
  u64 result = seed;
  for (int i = 0; i < TASK_WORK_ITERATIONS; i++) {
    result = (result * 1103515245 + 12345) & 0x7fffffff;
  }
  return result;
}

//////////////////////////////////////////////////////////////////////////////////
// Thread Pool Benchmark
//////////////////////////////////////////////////////////////////////////////////

static anyptr threadpool_task(anyptr arg, chiba_shared_ptr future) {
  (void)future;
  u64 seed = (u64)arg;
  u64 result = do_work(seed);
  return (anyptr)result;
}

static BenchResult benchmark_threadpool(void) {
  printf("  Creating thread pool...\n");
  ThreadPool *pool = threadpool_create();
  if (!pool) {
    printf("  ERROR: Failed to create thread pool\n");
    BenchResult result = {.elapsed_ms = -1.0, .throughput = 0.0};
    return result;
  }

  threadpool_stats stats = get_threadpool_stats(pool);
  printf("  Thread pool created with %d workers\n", stats.total_workers);

  printf("  Submitting %d tasks...\n", NUM_TASKS);
  chiba_shared_ptr *futures = (chiba_shared_ptr *)CHIBA_INTERNAL_malloc(
      sizeof(chiba_shared_ptr) * NUM_TASKS);

  double start = get_time_ms();

  // Submit all tasks
  for (int i = 0; i < NUM_TASKS; i++) {
    futures[i] = threadpool_submit_blocking_task(pool, threadpool_task,
                                                 (anyptr)(u64)(i + 1));
    if (!futures[i].control) {
      printf("  ERROR: Failed to submit task %d\n", i);
    }
  }

  // Wait for all tasks to complete
  for (int i = 0; i < NUM_TASKS; i++) {
    while (!chiba_future_is_done(futures[i])) {
      CHIBA_INTERNAL_usleep(100);
    }
    chiba_shared_drop(&futures[i]);
  }

  double end = get_time_ms();
  double elapsed = end - start;

  CHIBA_INTERNAL_free(futures);

  stats = get_threadpool_stats(pool);
  printf("  Completed %llu tasks\n",
         (unsigned long long)stats.total_tasks_completed);

  threadpool_destroy(&pool);

  BenchResult result;
  result.elapsed_ms = elapsed;
  result.throughput = (NUM_TASKS * 1000.0) / elapsed;
  return result;
}

//////////////////////////////////////////////////////////////////////////////////
// Direct pthread Benchmark (create thread per task - realistic worst case)
//////////////////////////////////////////////////////////////////////////////////

typedef struct {
  u64 seed;
} PthreadTaskArgs;

static void *pthread_task_worker(void *arg) {
  PthreadTaskArgs *task = (PthreadTaskArgs *)arg;
  u64 result = do_work(task->seed);
  CHIBA_INTERNAL_free(task);
  return (anyptr)result;
}

static BenchResult benchmark_pthread_direct(void) {
  printf("  Creating one pthread per task (worst case)...\n");

  pthread_t *threads =
      (pthread_t *)CHIBA_INTERNAL_malloc(sizeof(pthread_t) * NUM_TASKS);

  double start = get_time_ms();

  // Create one thread per task
  for (int i = 0; i < NUM_TASKS; i++) {
    PthreadTaskArgs *args =
        (PthreadTaskArgs *)CHIBA_INTERNAL_malloc(sizeof(PthreadTaskArgs));
    args->seed = i + 1;
    pthread_create(&threads[i], NULL, pthread_task_worker, args);
  }

  // Wait for all threads
  for (int i = 0; i < NUM_TASKS; i++) {
    pthread_join(threads[i], NULL);
  }

  double end = get_time_ms();
  double elapsed = end - start;

  printf("  Completed %d tasks\n", NUM_TASKS);

  CHIBA_INTERNAL_free(threads);

  BenchResult result;
  result.elapsed_ms = elapsed;
  result.throughput = (NUM_TASKS * 1000.0) / elapsed;
  return result;
}

//////////////////////////////////////////////////////////////////////////////////
// pthread Pool (simulating manual thread pool)
//////////////////////////////////////////////////////////////////////////////////

typedef struct {
  u64 *task_queue;
  _Atomic(int) head;
  _Atomic(int) tail;
  _Atomic(bool) shutdown;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int capacity;
  _Atomic(int) completed;
} SimplePthreadPool;

typedef struct {
  SimplePthreadPool *pool;
  int worker_id;
} SimplePthreadWorkerArgs;

static void *simple_pthread_worker(void *arg) {
  SimplePthreadWorkerArgs *worker_args = (SimplePthreadWorkerArgs *)arg;
  SimplePthreadPool *pool = worker_args->pool;

  while (1) {
    pthread_mutex_lock(&pool->mutex);

    while (atomic_load(&pool->head) == atomic_load(&pool->tail) &&
           !atomic_load(&pool->shutdown)) {
      pthread_cond_wait(&pool->cond, &pool->mutex);
    }

    if (atomic_load(&pool->shutdown) &&
        atomic_load(&pool->head) == atomic_load(&pool->tail)) {
      pthread_mutex_unlock(&pool->mutex);
      break;
    }

    // Get task
    int head = atomic_load(&pool->head);
    if (head != atomic_load(&pool->tail)) {
      u64 task_id = pool->task_queue[head % pool->capacity];
      atomic_store(&pool->head, head + 1);
      pthread_mutex_unlock(&pool->mutex);

      // Do work
      do_work(task_id);
      atomic_fetch_add(&pool->completed, 1);
    } else {
      pthread_mutex_unlock(&pool->mutex);
    }
  }

  return NULL;
}

static BenchResult benchmark_pthread_pool(void) {
  int num_threads = get_cpu_count();
  printf("  Creating simple pthread pool with %d workers...\n", num_threads);

  SimplePthreadPool pool;
  pool.capacity = NUM_TASKS * 2;
  pool.task_queue = (u64 *)CHIBA_INTERNAL_malloc(sizeof(u64) * pool.capacity);
  atomic_init(&pool.head, 0);
  atomic_init(&pool.tail, 0);
  atomic_init(&pool.shutdown, false);
  atomic_init(&pool.completed, 0);
  pthread_mutex_init(&pool.mutex, NULL);
  pthread_cond_init(&pool.cond, NULL);

  pthread_t *threads =
      (pthread_t *)CHIBA_INTERNAL_malloc(sizeof(pthread_t) * num_threads);
  SimplePthreadWorkerArgs *args =
      (SimplePthreadWorkerArgs *)CHIBA_INTERNAL_malloc(
          sizeof(SimplePthreadWorkerArgs) * num_threads);

  // Create worker threads
  for (int i = 0; i < num_threads; i++) {
    args[i].pool = &pool;
    args[i].worker_id = i;
    pthread_create(&threads[i], NULL, simple_pthread_worker, &args[i]);
  }

  double start = get_time_ms();

  // Submit tasks
  for (int i = 0; i < NUM_TASKS; i++) {
    pthread_mutex_lock(&pool.mutex);
    int tail = atomic_load(&pool.tail);
    pool.task_queue[tail % pool.capacity] = i + 1;
    atomic_store(&pool.tail, tail + 1);
    pthread_cond_signal(&pool.cond);
    pthread_mutex_unlock(&pool.mutex);
  }

  // Wait for completion
  while (atomic_load(&pool.completed) < NUM_TASKS) {
    CHIBA_INTERNAL_usleep(100);
  }

  double end = get_time_ms();
  double elapsed = end - start;

  // Shutdown
  atomic_store(&pool.shutdown, true);
  pthread_cond_broadcast(&pool.cond);

  for (int i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  printf("  Completed %d tasks\n", atomic_load(&pool.completed));

  pthread_mutex_destroy(&pool.mutex);
  pthread_cond_destroy(&pool.cond);
  CHIBA_INTERNAL_free(pool.task_queue);
  CHIBA_INTERNAL_free(threads);
  CHIBA_INTERNAL_free(args);

  BenchResult result;
  result.elapsed_ms = elapsed;
  result.throughput = (NUM_TASKS * 1000.0) / elapsed;
  return result;
}

//////////////////////////////////////////////////////////////////////////////////
// CPU count helper (copied from thread_pool.c for pthread benchmark)
//////////////////////////////////////////////////////////////////////////////////

PRIVATE int threadpool_get_cpu_count(void) {
#if defined(_WIN32) || defined(_WIN64)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors < 4 ? 4 : sysinfo.dwNumberOfProcessors;
#else
  long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  return (nprocs > 0) ? (int)nprocs : 4;
#endif
}

//////////////////////////////////////////////////////////////////////////////////
// Main Benchmark Runner
//////////////////////////////////////////////////////////////////////////////////

int main(void) {
  printf("========================================\n");
  printf("  Thread Pool Benchmark\n");
  printf("========================================\n");
  printf("  Tasks: %d\n", NUM_TASKS);
  printf("  Work per task: %d iterations\n", TASK_WORK_ITERATIONS);
  printf("  CPU cores: %d\n", threadpool_get_cpu_count());
  printf("\n");

  // Warm up
  printf("Warming up...\n");
  for (int i = 0; i < 100; i++) {
    do_work(i);
  }
  printf("\n");

  // Benchmark 1: Thread Pool
  printf("========================================\n");
  printf("Benchmark 1: Chiba Thread Pool\n");
  printf("========================================\n");
  BenchResult pool_result = benchmark_threadpool();
  printf("  Time: %.2f ms\n", pool_result.elapsed_ms);
  printf("  Throughput: %.2f tasks/sec\n", pool_result.throughput);
  printf("\n");

  // Benchmark 2: Direct pthread (one thread per task)
  printf("========================================\n");
  printf("Benchmark 2: Direct pthread (one thread per task)\n");
  printf("========================================\n");
  BenchResult pthread_result = benchmark_pthread_direct();
  printf("  Time: %.2f ms\n", pthread_result.elapsed_ms);
  printf("  Throughput: %.2f tasks/sec\n", pthread_result.throughput);
  printf("\n");

  // Benchmark 3: Simple pthread pool
  printf("========================================\n");
  printf("Benchmark 3: Simple pthread pool (mutex queue)\n");
  printf("========================================\n");
  BenchResult simple_pool_result = benchmark_pthread_pool();
  printf("  Time: %.2f ms\n", simple_pool_result.elapsed_ms);
  printf("  Throughput: %.2f tasks/sec\n", simple_pool_result.throughput);
  printf("\n");

  // Summary
  printf("========================================\n");
  printf("Summary\n");
  printf("========================================\n");
  printf("Chiba Thread Pool:        %.2f ms (%.2f tasks/sec)\n",
         pool_result.elapsed_ms, pool_result.throughput);
  printf("Direct pthread:           %.2f ms (%.2f tasks/sec)\n",
         pthread_result.elapsed_ms, pthread_result.throughput);
  printf("Simple pthread pool:      %.2f ms (%.2f tasks/sec)\n",
         simple_pool_result.elapsed_ms, simple_pool_result.throughput);
  printf("\n");

  double pool_vs_direct = pool_result.elapsed_ms / pthread_result.elapsed_ms;
  double pool_vs_simple =
      pool_result.elapsed_ms / simple_pool_result.elapsed_ms;

  printf("Speed factor:\n");
  printf("  Chiba vs Direct pthread:     %.2fx %s\n", pool_vs_direct,
         pool_vs_direct < 1.0 ? "(faster)" : "(slower)");
  printf("  Chiba vs Simple pool:        %.2fx %s\n", pool_vs_simple,
         pool_vs_simple < 1.0 ? "(faster)" : "(slower)");
  printf("========================================\n");

  return 0;
}
