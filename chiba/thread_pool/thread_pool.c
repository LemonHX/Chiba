#include "thread_pool.h"
#include "../utils/backoff.h"
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

//////////////////////////////////////////////////////////////////////////////////
// 辅助函数
//////////////////////////////////////////////////////////////////////////////////

// 获取系统 CPU 核心数
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
// Worker 线程主循环
//////////////////////////////////////////////////////////////////////////////////

PRIVATE void *threadpool_worker_main(void *arg) {
  ThreadPoolWorker *worker = (ThreadPoolWorker *)arg;
  ThreadPool *pool = worker->pool;

  atomic_store(&worker->running, true);

  while (atomic_load(&worker->running)) {
    ThreadPoolTask *task = NULL;

    // 1. 尝试从本地队列 pop 任务
    anyptr task_ptr = chiba_wsqueue_pop(pool->blocking_queue);
    if (task_ptr != NULL) {
      task = (ThreadPoolTask *)task_ptr;
      atomic_store(&worker->idle, false);
    }

    // 2.没有任务，进入空闲状态等待
    if (task == NULL) {
      atomic_store(&worker->idle, true);
      atomic_fetch_add(&pool->idle_workers, 1);

      pthread_mutex_lock(&worker->wakeup_mutex);
      // 再次检查，避免丢失唤醒信号
      if (atomic_load(&pool->total_tasks_pending) == 0 &&
          atomic_load(&worker->running)) {
        pthread_cond_wait(&worker->wakeup_cond, &worker->wakeup_mutex);
      }
      pthread_mutex_unlock(&worker->wakeup_mutex);

      atomic_fetch_sub(&pool->idle_workers, 1);
      continue;
    }

    // 4. 执行任务
    atomic_store(&worker->idle, false);
    threadpool_execute_task(worker, task);
  }

  return NULL;
}

//////////////////////////////////////////////////////////////////////////////////
// 任务执行
//////////////////////////////////////////////////////////////////////////////////

PRIVATE void threadpool_execute_task(ThreadPoolWorker *worker,
                                     ThreadPoolTask *task) {
  if (!task || !task->func || !task->future) {
    if (task) {
      CHIBA_INTERNAL_free(task);
    }
    return;
  }

  chiba_future *future = task->future;

  // 检查是否已取消
  if (chiba_future_is_cancelled(future)) {
    atomic_store(&future->state, FUTURE_CANCELLED);
    atomic_store(&future->error, FUTURE_ERR_CANCELLED);
    CHIBA_INTERNAL_free(task);
    atomic_fetch_sub(&worker->pool->total_tasks_pending, 1);
    return;
  }

  // 设置状态为运行中
  atomic_store(&future->state, FUTURE_RUNNING);
  future->worker_tid = pthread_self();

  // 执行任务函数
  anyptr result = task->func(task->arg, future);

  // 更新 future 状态
  if (chiba_future_is_cancelled(future)) {
    atomic_store(&future->state, FUTURE_CANCELLED);
    atomic_store(&future->error, FUTURE_ERR_CANCELLED);
  } else {
    atomic_store(&future->result, result);
    atomic_store(&future->state, FUTURE_COMPLETED);
    atomic_store(&future->error, FUTURE_ERR_OK);
  }

  // 更新统计信息
  atomic_fetch_sub(&worker->pool->total_tasks_pending, 1);
  atomic_fetch_add(&worker->pool->total_tasks_completed, 1);

  // 释放任务内存
  CHIBA_INTERNAL_free(task);
}

//////////////////////////////////////////////////////////////////////////////////
// 线程池创建和销毁
//////////////////////////////////////////////////////////////////////////////////

PUBLIC ThreadPool *threadpool_create() {
  ThreadPool *pool = (ThreadPool *)CHIBA_INTERNAL_malloc(sizeof(ThreadPool));
  if (!pool)
    return NULL;

  // 获取 CPU 核心数
  i32 cpu_count = threadpool_get_cpu_count();
  pool->num_workers = cpu_count;

  // 初始化队列 (容量 65536)
  pool->blocking_queue = chiba_wsqueue_new(65536);
  if (!pool->blocking_queue) {
    CHIBA_INTERNAL_free(pool);
    return NULL;
  }

  // 初始化统计信息
  atomic_init(&pool->total_tasks_pending, 0);
  atomic_init(&pool->total_tasks_completed, 0);
  atomic_init(&pool->idle_workers, 0);
  atomic_init(&pool->shutting_down, false);

  // 创建 worker 线程
  pool->workers = (ThreadPoolWorker *)CHIBA_INTERNAL_malloc(
      sizeof(ThreadPoolWorker) * pool->num_workers);
  if (!pool->workers) {
    chiba_wsqueue_drop(pool->blocking_queue);
    CHIBA_INTERNAL_free(pool);
    return NULL;
  }

  // 初始化每个 worker
  for (i32 i = 0; i < pool->num_workers; i++) {
    ThreadPoolWorker *worker = &pool->workers[i];
    worker->worker_id = i;
    worker->pool = pool;
    worker->node = NULL; // 目前不使用 NUMA 节点
    atomic_init(&worker->running, true);
    atomic_init(&worker->idle, true);

    // 初始化 worker 的条件变量和互斥锁
    pthread_mutex_init(&worker->wakeup_mutex, NULL);
    pthread_cond_init(&worker->wakeup_cond, NULL);

    if (pthread_create(&worker->thread, NULL, threadpool_worker_main, worker) !=
        0) {
      // 创建线程失败，清理已创建的线程
      pthread_mutex_destroy(&worker->wakeup_mutex);
      pthread_cond_destroy(&worker->wakeup_cond);

      atomic_store(&pool->shutting_down, true);
      for (i32 j = 0; j < i; j++) {
        atomic_store(&pool->workers[j].running, false);
        pthread_cond_signal(&pool->workers[j].wakeup_cond);
      }
      for (i32 j = 0; j < i; j++) {
        pthread_join(pool->workers[j].thread, NULL);
        pthread_mutex_destroy(&pool->workers[j].wakeup_mutex);
        pthread_cond_destroy(&pool->workers[j].wakeup_cond);
      }
      CHIBA_INTERNAL_free(pool->workers);
      chiba_wsqueue_drop(pool->blocking_queue);
      CHIBA_INTERNAL_free(pool);
      return NULL;
    }
  }

  return pool;
}

PUBLIC void threadpool_destroy(ThreadPool **pool_ptr) {
  if (!pool_ptr || !*pool_ptr)
    return;

  ThreadPool *pool = *pool_ptr;

  // 设置关闭标志
  atomic_store(&pool->shutting_down, true);

  // 等待所有任务完成
  while (atomic_load(&pool->total_tasks_pending) > 0) {
    // 唤醒所有线程处理剩余任务
    for (i32 i = 0; i < pool->num_workers; i++) {
      pthread_cond_signal(&pool->workers[i].wakeup_cond);
    }
    usleep(1000); // 等待 1ms
  }

  // 停止所有 worker 线程
  for (i32 i = 0; i < pool->num_workers; i++) {
    atomic_store(&pool->workers[i].running, false);
  }

  // 唤醒所有等待的线程
  for (i32 i = 0; i < pool->num_workers; i++) {
    pthread_cond_signal(&pool->workers[i].wakeup_cond);
  }

  // 等待所有线程退出
  for (i32 i = 0; i < pool->num_workers; i++) {
    pthread_join(pool->workers[i].thread, NULL);
    pthread_mutex_destroy(&pool->workers[i].wakeup_mutex);
    pthread_cond_destroy(&pool->workers[i].wakeup_cond);
  }

  // 清理资源
  CHIBA_INTERNAL_free(pool->workers);
  chiba_wsqueue_drop(pool->blocking_queue);
  CHIBA_INTERNAL_free(pool);

  *pool_ptr = NULL;
}

//////////////////////////////////////////////////////////////////////////////////
// 任务提交
//////////////////////////////////////////////////////////////////////////////////

PUBLIC chiba_future *
threadpool_submit_blocking_task(ThreadPool *pool, TaskFunc func, anyptr arg) {
  if (!pool || !func)
    return NULL;

  // 检查线程池是否正在关闭
  if (atomic_load(&pool->shutting_down)) {
    return NULL;
  }

  // 创建 future
  chiba_future *future = chiba_future_init();
  if (!future)
    return NULL;

  // 创建任务
  ThreadPoolTask *task =
      (ThreadPoolTask *)CHIBA_INTERNAL_malloc(sizeof(ThreadPoolTask));
  if (!task) {
    chiba_future_drop(&future);
    return NULL;
  }

  task->func = func;
  task->arg = arg;
  task->future = future;

  // 提交任务到队列
  if (!chiba_wsqueue_push(pool->blocking_queue, (anyptr)task, true)) {
    CHIBA_INTERNAL_free(task);
    chiba_future_drop(&future);
    return NULL;
  }

  // 更新统计信息
  atomic_fetch_add(&pool->total_tasks_pending, 1);

  // 唤醒一个空闲线程（简单轮询策略）
  if (atomic_load(&pool->idle_workers) > 0) {
    for (i32 i = 0; i < pool->num_workers; i++) {
      if (atomic_load(&pool->workers[i].idle)) {
        pthread_cond_signal(&pool->workers[i].wakeup_cond);
        break;
      }
    }
  }

  return future;
}

//////////////////////////////////////////////////////////////////////////////////
// 统计信息
//////////////////////////////////////////////////////////////////////////////////

PUBLIC threadpool_stats get_threadpool_stats(ThreadPool *pool) {
  threadpool_stats stats;
  stats.total_tasks_submitted = atomic_load(&pool->total_tasks_pending) +
                                atomic_load(&pool->total_tasks_completed);
  stats.total_tasks_completed = atomic_load(&pool->total_tasks_completed);
  stats.total_workers = pool->num_workers;
  stats.idle_workers = atomic_load(&pool->idle_workers);

  return stats;
}

//////////////////////////////////////////////////////////////////////////////////
// Future 等待其他 Future
//////////////////////////////////////////////////////////////////////////////////

PUBLIC void threadpool_await_other_future(chiba_future *self_future,
                                          chiba_future *future) {
  if (!self_future || !future)
    return;

  // 轮询等待，同时检查自己是否被取消
  while (!chiba_future_is_done(future)) {
    if (chiba_future_is_cancelled(self_future)) {
      chiba_future_cancel(future);
      return;
    }
    chiba_backoff backoff = {.step = 0};
    backoff_snooze(&backoff);
  }
}
