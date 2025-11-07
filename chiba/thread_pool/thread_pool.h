#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Thread Pool - work-stealing thread pool
//
// 特性:
// - Work stealing: 线程优先从本地队列取任务,空闲时从其他队列窃取
// - 线程亲和性: 线程绑定到对应 NUMA 节点的 CPU
// - Future 机制: 异步任务返回 future, 支持错误处理
//
// 使用 pthread (POSIX) 和 pthread-win32 (Windows)
//////////////////////////////////////////////////////////////////////////////////

#include "../basic_memory.h"
#include "../basic_types.h"
#include "../common_headers.h"
#include "../concurrency/dequeue.h"
#include "../concurrency/future.h"

PUBLIC void threadpool_await_other_future(chiba_future *self_future,
                                          chiba_future *future);

//////////////////////////////////////////////////////////////////////////////////
// 任务结构
//////////////////////////////////////////////////////////////////////////////////

// 任务函数签名: anyptr task_func(anyptr arg, chiba_future* future)
// - arg: 用户传入的参数
// - future: 任务对应的 future,可用于检查取消状态, 传入时需要为空
// - 返回值: 任务结果,会存储到 future 中
typedef anyptr (*TaskFunc)(anyptr arg, chiba_future *future);

typedef struct ThreadPoolBlockingTask {
  TaskFunc func;        // 任务函数
  anyptr arg;           // 任务参数
  chiba_future *future; // 关联的 future
} ThreadPoolTask;

//////////////////////////////////////////////////////////////////////////////////
// Worker 线程结构
//////////////////////////////////////////////////////////////////////////////////
typedef struct NumaNode NumaNode;
typedef struct ThreadPool ThreadPool;

typedef struct ThreadPoolWorker {
  pthread_t thread; // 线程句柄
  i32 worker_id;    // Worker ID

  // 同步原语
  pthread_mutex_t wakeup_mutex; // 用于唤醒空闲线程的互斥锁
  pthread_cond_t wakeup_cond;   // 用于唤醒空闲线程的条件变量

  // Numa 节点
  NumaNode *node;   // 指向所属 NUMA 节点的指针
  ThreadPool *pool; // 所属线程池

  // 运行状态
  _Atomic(bool) running;
  _Atomic(bool) idle;
} ThreadPoolWorker;

typedef struct ThreadPool {
  // Worker 线程
  ThreadPoolWorker *workers; // worker 线程数组

  // 偷取队列
  // 初始大小 65536
  chiba_wsqueue *blocking_queue;

  i32 worker_count;            // 该节点上的 worker 数量
  i32 *worker_ids;             // Worker ID 列表
  i32 num_workers;             // worker 数量
  _Atomic(bool) shutting_down; // 线程池是否正在关闭
  _Atomic(i32) idle_workers;   // 空闲 worker 数量

  _Atomic(u64) total_tasks_pending;   // 总待处理任务数
  _Atomic(u64) total_tasks_completed; // 总完成任务数
} ThreadPool;

//////////////////////////////////////////////////////////////////////////////////
// 线程池 API
//////////////////////////////////////////////////////////////////////////////////

/**
 * 创建线程池
 * 在每个numa节点上创建worker线程和队列
 * @return 线程池指针,失败返回 NULL
 */
PUBLIC ThreadPool *threadpool_create();

/**
 * 销毁线程池
 * 会等待所有任务完成后再销毁
 */
PUBLIC void threadpool_destroy(ThreadPool **pool);

/**
 * 提交任务到线程池
 * 自动选择合适的队列 (优先当前 NUMA 节点), resize false
 * 如果当前节点的队列满了,则提交到全局队列
 *
 * @param pool 线程池
 * @param func 任务函数
 * @param arg 任务参数
 * @return future 指针,失败返回 NULL
 */
PUBLIC chiba_future *threadpool_submit_blocking_task(ThreadPool *pool,
                                                     TaskFunc func, anyptr arg);

typedef struct threadpool_stats {
  u64 total_tasks_submitted;
  u64 total_tasks_completed;
  i32 total_workers;
  i32 idle_workers;
} threadpool_stats;
/**
 * 获取线程池统计信息
 */
PUBLIC threadpool_stats get_threadpool_stats(ThreadPool *pool);

//////////////////////////////////////////////////////////////////////////////////
// 内部函数声明 (实现在 .c 文件中)
//////////////////////////////////////////////////////////////////////////////////

// Worker 线程主循环
PRIVATE void *threadpool_worker_main(void *arg);

// 执行任务
PRIVATE void threadpool_execute_task(ThreadPoolWorker *worker,
                                     ThreadPoolTask *task);

// 获取系统 CPU 核心数
PRIVATE int threadpool_get_cpu_count(void);
