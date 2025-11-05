#pragma once

//////////////////////////////////////////////////////////////////////////////////
// Chiba Thread Pool - NUMA-aware work-stealing thread pool
//
// 特性:
// - NUMA 感知: 每个 NUMA 节点一个队列
// - Work stealing: 线程优先从本地队列取任务,空闲时从其他队列窃取
// - 线程亲和性: 线程绑定到对应 NUMA 节点的 CPU
// - Future 机制: 异步任务返回 future,支持错误处理
// - 自动配置: 默认 CPU 核心数个线程,每队列 65536 容量
//
// 使用 pthread (POSIX) 和 pthread-win32 (Windows)
//////////////////////////////////////////////////////////////////////////////////

#include "../basic_memory.h"
#include "../basic_types.h"
#include "../common_headers.h"
#include "dequeue.h"

//////////////////////////////////////////////////////////////////////////////////
// Future 状态和错误码
//////////////////////////////////////////////////////////////////////////////////

typedef enum {
  FUTURE_PENDING = 0,   // 任务待执行
  FUTURE_RUNNING = 1,   // 任务执行中
  FUTURE_COMPLETED = 2, // 任务完成
  FUTURE_ERROR = 3,     // 任务出错
  FUTURE_CANCELLED = 4  // 任务被取消
} FutureState;

typedef enum {
  FUTURE_ERR_OK = 0,            // 无错误
  FUTURE_ERR_THREAD_DIED = 1,   // 线程挂掉
  FUTURE_ERR_TIMEOUT = 2,       // 超时
  FUTURE_ERR_CANCELLED = 3,     // 被取消
  FUTURE_ERR_POOL_SHUTDOWN = 4, // 线程池关闭
  FUTURE_ERR_OUT_OF_MEMORY = 5  // 内存不足
} FutureError;

//////////////////////////////////////////////////////////////////////////////////
// Future 结构
//////////////////////////////////////////////////////////////////////////////////

typedef struct ThreadPoolFuture {
  // 状态
  _Atomic(FutureState) state;
  _Atomic(FutureError) error;

  // 结果存储 (void* 可存任意数据)
  _Atomic(anyptr) result;

  // 同步原语
  pthread_mutex_t mutex;
  pthread_cond_t cond;

  // 引用计数 (用于自动释放)
  _Atomic(i64) refcount;

  // 任务执行的线程 ID (用于检测线程挂掉)
  _Atomic(pthread_t) worker_tid;

  // 取消标志
  _Atomic(bool) cancel_requested;
} ThreadPoolFuture;

//////////////////////////////////////////////////////////////////////////////////
// 任务结构
//////////////////////////////////////////////////////////////////////////////////

// 任务函数签名: anyptr task_func(anyptr arg, ThreadPoolFuture* future)
// - arg: 用户传入的参数
// - future: 任务对应的 future,可用于检查取消状态
// - 返回值: 任务结果,会存储到 future 中
typedef anyptr (*TaskFunc)(anyptr arg, ThreadPoolFuture *future);

typedef struct ThreadPoolTask {
  TaskFunc func;            // 任务函数
  anyptr arg;               // 任务参数
  ThreadPoolFuture *future; // 关联的 future
} ThreadPoolTask;

//////////////////////////////////////////////////////////////////////////////////
// Worker 线程结构
//////////////////////////////////////////////////////////////////////////////////

typedef struct ThreadPoolWorker {
  pthread_t thread;        // 线程句柄
  i32 worker_id;           // Worker ID
  i32 numa_node;           // 所属 NUMA 节点
  struct ThreadPool *pool; // 所属线程池

  // 线程本地队列 (work-stealing deque)
  WorkStealingQueue *local_queue;

  // 统计信息
  _Atomic(u64) tasks_executed;
  _Atomic(u64) tasks_stolen;
  _Atomic(u64) steal_attempts;

  // 运行状态
  _Atomic(bool) running;
  _Atomic(bool) idle;
} ThreadPoolWorker;

//////////////////////////////////////////////////////////////////////////////////
// NUMA 节点结构
//////////////////////////////////////////////////////////////////////////////////

typedef struct NumaNode {
  i32 node_id;                // NUMA 节点 ID
  i32 worker_count;           // 该节点上的 worker 数量
  i32 *worker_ids;            // Worker ID 列表
  _Atomic(u64) pending_tasks; // 待处理任务数 (用于负载均衡)
} NumaNode;

//////////////////////////////////////////////////////////////////////////////////
// 线程池结构
//////////////////////////////////////////////////////////////////////////////////

typedef struct ThreadPool {
  // Worker 线程
  ThreadPoolWorker *workers;
  i32 num_workers;

  // NUMA 节点
  NumaNode *numa_nodes;
  i32 num_numa_nodes;

  // 全局状态
  _Atomic(bool) shutdown;             // 关闭标志
  _Atomic(u64) total_tasks_pending;   // 总待处理任务数
  _Atomic(u64) total_tasks_completed; // 总完成任务数

  // 用于唤醒空闲线程的条件变量
  pthread_mutex_t wakeup_mutex;
  pthread_cond_t wakeup_cond;
  _Atomic(i32) idle_workers; // 空闲 worker 数量

  // 配置
  i32 queue_capacity; // 每个队列容量 (默认 65536)
} ThreadPool;

//////////////////////////////////////////////////////////////////////////////////
// Future API
//////////////////////////////////////////////////////////////////////////////////

/**
 * 创建 future (内部使用)
 */
PRIVATE ThreadPoolFuture *future_create(void);

/**
 * 克隆 future
 */
PUBLIC ThreadPoolFuture *future_clone(ThreadPoolFuture *future);

/**
 * 减少 future 引用计数,为 0 时释放
 */
PUBLIC void future_release(ThreadPoolFuture **future);

/**
 * 尝试获取 future 结果 (非阻塞)
 * @param result_out 输出结果指针
 * @return true 已完成且有结果, false 未完成或出错
 */
PUBLIC bool future_try_get(ThreadPoolFuture *future, anyptr *result_out);

/**
 * 请求取消任务
 * 注意: 只是设置取消标志,任务函数需要主动检查
 */
PUBLIC void future_cancel(ThreadPoolFuture *future);

/**
 * 检查任务是否被取消 (任务函数内调用)
 */
PUBLIC bool future_is_cancelled(ThreadPoolFuture *future);

/**
 * 获取 future 状态
 */
PUBLIC FutureState future_state(ThreadPoolFuture *future);

/**
 * 获取 future 错误码
 */
PUBLIC FutureError future_error(ThreadPoolFuture *future);

/**
 * 判断 future 是否完成 (成功或失败)
 */
PUBLIC bool future_is_done(ThreadPoolFuture *future);

//////////////////////////////////////////////////////////////////////////////////
// 线程池 API
//////////////////////////////////////////////////////////////////////////////////

/**
 * 创建线程池
 * @param num_threads 线程数量, 0 表示使用 CPU 核心数
 * @param queue_capacity 每个队列容量, 0 表示使用默认值 65536
 * @return 线程池指针,失败返回 NULL
 */
PUBLIC ThreadPool *threadpool_create(i32 num_threads, i32 queue_capacity);

/**
 * 销毁线程池
 * 会等待所有任务完成后再销毁
 */
PUBLIC void threadpool_destroy(ThreadPool **pool);

/**
 * 提交任务到线程池
 * 自动选择合适的队列 (优先当前 NUMA 节点)
 *
 * @param pool 线程池
 * @param func 任务函数
 * @param arg 任务参数
 * @return future 指针,失败返回 NULL
 */
PUBLIC ThreadPoolFuture *threadpool_submit(ThreadPool *pool, TaskFunc func,
                                           anyptr arg);

/**
 * 获取当前 NUMA 节点 ID
 * 用于优化任务提交位置
 * @return NUMA 节点 ID, 失败返回 -1
 */
PUBLIC i32 threadpool_current_numa_node(void);

/**
 * 获取线程池统计信息
 */
PUBLIC void threadpool_stats(ThreadPool *pool, u64 *pending, u64 *completed,
                             i32 *idle_workers);

//////////////////////////////////////////////////////////////////////////////////
// 内部函数声明 (实现在 .c 文件中)
//////////////////////////////////////////////////////////////////////////////////

// Worker 线程主循环
PRIVATE void *threadpool_worker_main(void *arg);

// 从指定 worker 的队列窃取任务
PRIVATE ThreadPoolTask *threadpool_steal_task(ThreadPool *pool, i32 thief_id);

// 执行任务
PRIVATE void threadpool_execute_task(ThreadPoolWorker *worker,
                                     ThreadPoolTask *task);

// 设置线程 CPU 亲和性
PRIVATE bool threadpool_set_affinity(pthread_t thread, i32 numa_node);

// 分配 NUMA 节点的 CPU 核心
PRIVATE void threadpool_assign_workers_to_numa(ThreadPool *pool);
