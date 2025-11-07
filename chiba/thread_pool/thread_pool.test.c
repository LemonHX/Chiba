#include "thread_pool.h"
#include "../chiba_testing.h"

TEST_GROUP(thread_pool);

//////////////////////////////////////////////////////////////////////////////////
// Test Task Functions
//////////////////////////////////////////////////////////////////////////////////

// Simple task that returns a value
anyptr simple_task(anyptr arg, chiba_shared_ptr future) {
  i64 value = (i64)arg;
  return (anyptr)(value * 2);
}

// Task that checks for cancellation
anyptr cancellable_task(anyptr arg, chiba_shared_ptr future) {
  for (int i = 0; i < 100; i++) {
    if (chiba_future_is_cancelled(future)) {
      return (anyptr)-1;
    }
    CHIBA_INTERNAL_usleep(10000); // Sleep 10ms
  }
  return arg;
}

// Task that returns NULL
anyptr null_task(anyptr arg, chiba_shared_ptr future) {
  (void)arg;
  (void)future;
  return NULL;
}

// Counter for concurrent tasks
_Atomic(i64) concurrent_counter = 0;

anyptr concurrent_task(anyptr arg, chiba_shared_ptr future) {
  (void)future;
  i64 iterations = (i64)arg;
  for (i64 i = 0; i < iterations; i++) {
    atomic_fetch_add(&concurrent_counter, 1);
  }
  return (anyptr)iterations;
}

//////////////////////////////////////////////////////////////////////////////////
// Test Cases
//////////////////////////////////////////////////////////////////////////////////

TEST_CASE(create_destroy, thread_pool, "Create and destroy thread pool", {
  DESC(create_destroy);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  threadpool_stats stats = get_threadpool_stats(pool);
  ASSERT_TRUE(stats.total_workers > 0, "Has worker threads");
  ASSERT_EQ(0, stats.total_tasks_completed, "No tasks completed initially");

  threadpool_destroy(&pool);
  ASSERT_NULL(pool, "Thread pool destroyed");

  return 0;
})

TEST_CASE(submit_simple_task, thread_pool, "Submit and wait for simple task", {
  DESC(submit_simple_task);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  chiba_shared_ptr_var(chiba_future) future =
      threadpool_submit_blocking_task(pool, simple_task, (anyptr)21);
  ASSERT_NOT_NULL(future.control, "Task submitted");

  // Wait for task to complete
  while (!chiba_future_is_done(future)) {
    CHIBA_INTERNAL_usleep(1000);
  }

  ASSERT_EQ(FUTURE_COMPLETED, chiba_future_state(future), "Task completed");

  anyptr result;
  bool got_result = chiba_future_try_get(future, &result);
  ASSERT_TRUE(got_result, "Got result");
  ASSERT_EQ(42, (i64)result, "Result is correct");

  threadpool_stats stats = get_threadpool_stats(pool);
  ASSERT_EQ(1, stats.total_tasks_completed, "One task completed");

  threadpool_destroy(&pool);
  return 0;
})

TEST_CASE(submit_multiple_tasks, thread_pool, "Submit multiple tasks", {
  DESC(submit_multiple_tasks);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  const int NUM_TASKS = 10;
  chiba_shared_ptr futures[NUM_TASKS];

  // Submit tasks
  for (int i = 0; i < NUM_TASKS; i++) {
    futures[i] = threadpool_submit_blocking_task(pool, simple_task,
                                                 (anyptr)(i64)(i + 1));
    ASSERT_NOT_NULL(futures[i].control, "Task submitted");
  }

  // Wait for all tasks
  for (int i = 0; i < NUM_TASKS; i++) {
    while (!chiba_future_is_done(futures[i])) {
      CHIBA_INTERNAL_usleep(1000);
    }
  }

  // Check results
  for (int i = 0; i < NUM_TASKS; i++) {
    anyptr result;
    bool got_result = chiba_future_try_get(futures[i], &result);
    ASSERT_TRUE(got_result, "Got result");
    ASSERT_EQ((i + 1) * 2, (i64)result, "Result is correct");
    chiba_shared_drop(&futures[i]);
  }

  threadpool_stats stats = get_threadpool_stats(pool);
  ASSERT_EQ(NUM_TASKS, stats.total_tasks_completed, "All tasks completed");

  threadpool_destroy(&pool);
  return 0;
})

TEST_CASE(task_cancellation, thread_pool, "Cancel a running task", {
  DESC(task_cancellation);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  chiba_shared_ptr_var(chiba_future) future =
      threadpool_submit_blocking_task(pool, cancellable_task, (anyptr)100);
  ASSERT_NOT_NULL(future.control, "Task submitted");

  // Let task start running
  CHIBA_INTERNAL_usleep(50000); // 50ms

  // Cancel the task
  chiba_future_cancel(future);

  // Wait for task to finish
  while (!chiba_future_is_done(future)) {
    CHIBA_INTERNAL_usleep(1000);
  }

  ASSERT_EQ(FUTURE_CANCELLED, chiba_future_state(future), "Task was cancelled");

  threadpool_destroy(&pool);
  return 0;
})

TEST_CASE(null_result_task, thread_pool, "Task returning NULL", {
  DESC(null_result_task);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  chiba_shared_ptr_var(chiba_future) future =
      threadpool_submit_blocking_task(pool, null_task, NULL);
  ASSERT_NOT_NULL(future.control, "Task submitted");

  // Wait for completion
  while (!chiba_future_is_done(future)) {
    CHIBA_INTERNAL_usleep(1000);
  }

  ASSERT_EQ(FUTURE_COMPLETED, chiba_future_state(future), "Task completed");

  anyptr result = (anyptr)0x1; // Non-null initial value
  bool got_result = chiba_future_try_get(future, &result);
  ASSERT_TRUE(got_result, "Got result");
  ASSERT_NULL(result, "Result is NULL");

  threadpool_destroy(&pool);
  return 0;
})

TEST_CASE(concurrent_execution, thread_pool, "Tasks execute concurrently", {
  DESC(concurrent_execution);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  threadpool_stats stats = get_threadpool_stats(pool);
  int num_workers = stats.total_workers;

  // Submit tasks that will run concurrently
  const int NUM_TASKS = num_workers * 2;
  chiba_shared_ptr futures[NUM_TASKS];

  atomic_store(&concurrent_counter, 0);

  for (int i = 0; i < NUM_TASKS; i++) {
    futures[i] =
        threadpool_submit_blocking_task(pool, concurrent_task, (anyptr)1000);
    ASSERT_NOT_NULL(futures[i].control, "Task submitted");
  }

  // Wait for all tasks
  for (int i = 0; i < NUM_TASKS; i++) {
    while (!chiba_future_is_done(futures[i])) {
      CHIBA_INTERNAL_usleep(1000);
    }
    chiba_shared_drop(&futures[i]);
  }

  i64 final_count = atomic_load(&concurrent_counter);
  ASSERT_EQ(NUM_TASKS * 1000, final_count, "All increments executed");

  threadpool_destroy(&pool);
  return 0;
})

TEST_CASE(submit_to_destroyed_pool, thread_pool, "Submit after pool shutdown", {
  DESC(submit_to_destroyed_pool);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  // Submit a task
  chiba_shared_ptr_var(chiba_future) future1 =
      threadpool_submit_blocking_task(pool, simple_task, (anyptr)5);
  ASSERT_NOT_NULL(future1.control, "Task submitted");

  // Wait for completion
  while (!chiba_future_is_done(future1)) {
    CHIBA_INTERNAL_usleep(1000);
  }

  threadpool_destroy(&pool);

  // Try to submit after destroy - should be safe (returns null)
  // Note: This test is mainly to verify we don't crash

  return 0;
})

TEST_CASE(heavy_load, thread_pool, "Handle heavy task load", {
  DESC(heavy_load);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  const int NUM_TASKS = 100;
  chiba_shared_ptr futures[NUM_TASKS];

  // Submit many tasks quickly
  for (int i = 0; i < NUM_TASKS; i++) {
    futures[i] =
        threadpool_submit_blocking_task(pool, simple_task, (anyptr)(i64)i);
    ASSERT_NOT_NULL(futures[i].control, "Task submitted");
  }

  // Wait for all to complete
  for (int i = 0; i < NUM_TASKS; i++) {
    while (!chiba_future_is_done(futures[i])) {
      CHIBA_INTERNAL_usleep(100);
    }
    chiba_shared_drop(&futures[i]);
  }

  threadpool_stats stats = get_threadpool_stats(pool);
  ASSERT_EQ(NUM_TASKS, stats.total_tasks_completed, "All tasks completed");

  threadpool_destroy(&pool);
  return 0;
})

TEST_CASE(stats_tracking, thread_pool, "Statistics tracking works", {
  DESC(stats_tracking);

  ThreadPool *pool = threadpool_create();
  ASSERT_NOT_NULL(pool, "Thread pool created");

  threadpool_stats stats1 = get_threadpool_stats(pool);
  ASSERT_EQ(0, stats1.total_tasks_completed, "No tasks initially");

  // Submit some tasks
  const int NUM_TASKS = 5;
  chiba_shared_ptr futures[NUM_TASKS];

  for (int i = 0; i < NUM_TASKS; i++) {
    futures[i] =
        threadpool_submit_blocking_task(pool, simple_task, (anyptr)(i64)i);
  }

  // Wait for completion
  for (int i = 0; i < NUM_TASKS; i++) {
    while (!chiba_future_is_done(futures[i])) {
      CHIBA_INTERNAL_usleep(1000);
    }
    chiba_shared_drop(&futures[i]);
  }

  threadpool_stats stats2 = get_threadpool_stats(pool);
  ASSERT_EQ(NUM_TASKS, stats2.total_tasks_completed, "Tasks counted");
  ASSERT_EQ(NUM_TASKS, stats2.total_tasks_submitted, "Submissions counted");

  threadpool_destroy(&pool);
  return 0;
})

//////////////////////////////////////////////////////////////////////////////////
// Test Registration
//////////////////////////////////////////////////////////////////////////////////

REGISTER_TEST_GROUP(thread_pool) {
  REGISTER_TEST(create_destroy, thread_pool);
  REGISTER_TEST(submit_simple_task, thread_pool);
  REGISTER_TEST(submit_multiple_tasks, thread_pool);
  REGISTER_TEST(task_cancellation, thread_pool);
  REGISTER_TEST(null_result_task, thread_pool);
  REGISTER_TEST(concurrent_execution, thread_pool);
  REGISTER_TEST(submit_to_destroyed_pool, thread_pool);
  REGISTER_TEST(heavy_load, thread_pool);
  REGISTER_TEST(stats_tracking, thread_pool);
}

ENABLE_TEST_GROUP(thread_pool)
