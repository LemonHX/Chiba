#pragma once

#include "../arc/arc.h"
#include "../basic_memory.h"

//////////////////////////////////////////////////////////////////////////////////
// Future 状态和错误码
//////////////////////////////////////////////////////////////////////////////////

typedef enum {
  FUTURE_PENDING = 0,   // 任务待执行
  FUTURE_RUNNING = 1,   // 任务执行中
  FUTURE_COMPLETED = 2, // 任务完成
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

typedef struct chiba_future {
  // 状态
  _Atomic(FutureState) state;
  _Atomic(FutureError) error;
  _Atomic(anyptr) result;

  // 任务执行的线程 ID (用于检测线程挂掉)
  pthread_t worker_tid;

} chiba_future;

//////////////////////////////////////////////////////////////////////////////////
// Future API
//////////////////////////////////////////////////////////////////////////////////

UTILS void _chiba_future_init(anyptr self, anyptr args) {
  (void)args;
  chiba_future *future = (chiba_future *)self;
  atomic_init(&future->state, FUTURE_PENDING);
  atomic_init(&future->error, FUTURE_ERR_OK);
  atomic_init(&future->result, NULL);
  future->worker_tid = 0;
}

UTILS chiba_shared_ptr_param(chiba_future) chiba_future_init() {
  return chiba_shared_new(sizeof(chiba_future), NULL, _chiba_future_init, NULL);
}

// ========== PUBLIC API ==========

/**
 * 尝试获取 future 结果 (非阻塞)
 * @param result_out 输出结果指针
 * @return true 已完成且有结果, false 未完成或出错
 */
UTILS bool chiba_future_try_get(chiba_shared_ptr_param(chiba_future) ptr,
                                anyptr *result_out) {
  chiba_future *future = (chiba_future *)chiba_shared_get(&ptr);
  if (!future || !result_out)
    return false;

  FutureState state = atomic_load(&future->state);
  if (state == FUTURE_COMPLETED) {
    *result_out = atomic_load(&future->result);
    return true;
  }

  return false;
}

/**
 * 请求取消任务
 * 注意: 只是设置取消标志,任务函数需要主动检查
 */
UTILS void chiba_future_cancel(chiba_shared_ptr_param(chiba_future) ptr) {
  chiba_future *future = (chiba_future *)chiba_shared_get(&ptr);
  if (!future)
    return;

  atomic_store(&future->state, FUTURE_CANCELLED);
}

/**
 * 检查任务是否被取消 (任务函数内调用)
 */
UTILS bool chiba_future_is_cancelled(chiba_shared_ptr_param(chiba_future) ptr) {
  chiba_future *future = (chiba_future *)chiba_shared_get(&ptr);
  if (!future)
    return false;

  return atomic_load(&future->state) == FUTURE_CANCELLED;
}

/**
 * 获取 future 状态
 */
UTILS FutureState chiba_future_state(chiba_shared_ptr_param(chiba_future) ptr) {
  chiba_future *future = (chiba_future *)chiba_shared_get(&ptr);
  if (!future)
    return FUTURE_PENDING;

  return atomic_load(&future->state);
}

/**
 * 获取 future 错误码
 */
UTILS FutureError chiba_future_error(chiba_shared_ptr_param(chiba_future) ptr) {
  chiba_future *future = (chiba_future *)chiba_shared_get(&ptr);
  if (!future)
    return FUTURE_ERR_OK;

  return atomic_load(&future->error);
}

/**
 * 判断 future 是否完成 (成功或失败)
 */
UTILS bool chiba_future_is_done(chiba_shared_ptr_param(chiba_future) ptr) {
  chiba_future *future = (chiba_future *)chiba_shared_get(&ptr);
  if (!future)
    return false;

  FutureState state = atomic_load(&future->state);
  return state == FUTURE_COMPLETED || state == FUTURE_CANCELLED;
}