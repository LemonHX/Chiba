#include "array_queue.h"
#include "../basic_types.h"
#include "../chiba_testing.h"

TEST_GROUP(array_queue);

// Concurrent test structures
typedef struct {
  chiba_arrayqueue *queue;
  i64 thread_id;
  i64 iterations;
} ThreadArgs;

// Thread functions
void *producer_thread(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;

  for (i64 i = 0; i < args->iterations; i++) {
    i64 value = args->thread_id * 1000000 + i;

    // Keep trying until push succeeds
    while (!chiba_arrayqueue_push(args->queue, (anyptr)value)) {
      // Backoff
      for (volatile int j = 0; j < 100; j++)
        ;
    }
  }

  return NULL;
}

void *consumer_thread(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;
  i64 consumed = 0;

  while (consumed < args->iterations) {
    anyptr value = chiba_arrayqueue_pop(args->queue);
    if (value != NULL) {
      consumed++;
    } else {
      // Backoff
      for (volatile int j = 0; j < 10; j++)
        ;
    }
  }

  return NULL;
}

void *mixed_thread(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;

  for (i64 i = 0; i < args->iterations; i++) {
    // Alternate between push and pop
    if (i % 2 == 0) {
      i64 value = args->thread_id * 1000000 + i;
      chiba_arrayqueue_push(args->queue, (anyptr)value);
    } else {
      chiba_arrayqueue_pop(args->queue);
    }
  }

  return NULL;
}

void *stress_producer(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;

  for (i64 i = 0; i < args->iterations; i++) {
    i64 value = args->thread_id * 10000000 + i;
    while (!chiba_arrayqueue_push(args->queue, (anyptr)value)) {
      // Tight loop - stress test
    }
  }

  return NULL;
}

void *stress_consumer(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;
  i64 consumed = 0;

  while (consumed < args->iterations) {
    anyptr value = chiba_arrayqueue_pop(args->queue);
    if (value != NULL) {
      consumed++;
    }
  }

  return NULL;
}

// Basic functionality tests
TEST_CASE(create_destroy, array_queue, "Create and destroy queue", {
  DESC(create_destroy);

  chiba_arrayqueue *queue = chiba_arrayqueue_new(10);
  ASSERT_NOT_NULL(queue, "Queue should be created successfully");
  ASSERT_EQ(10, chiba_arrayqueue_capacity(queue),
            "Queue capacity should be 10");
  ASSERT_TRUE(chiba_arrayqueue_is_empty(queue), "New queue should be empty");
  ASSERT_EQ(0, chiba_arrayqueue_size(queue), "New queue length should be 0");

  chiba_arrayqueue_drop(queue);
  return 0;
})

TEST_CASE(single_push_pop, array_queue, "Single push and pop", {
  DESC(single_push_pop);

  chiba_arrayqueue *queue = chiba_arrayqueue_new(10);
  ASSERT_NOT_NULL(queue, "Queue should be created");

  anyptr value = (anyptr)0x1234;
  ASSERT_TRUE(chiba_arrayqueue_push(queue, value), "Push should succeed");
  ASSERT_EQ(1, chiba_arrayqueue_size(queue), "Queue length should be 1");
  ASSERT_TRUE(!chiba_arrayqueue_is_empty(queue), "Queue should not be empty");

  anyptr popped = chiba_arrayqueue_pop(queue);
  ASSERT_EQ((i64)value, (i64)popped, "Popped value should match pushed value");
  ASSERT_TRUE(chiba_arrayqueue_is_empty(queue),
              "Queue should be empty after pop");

  chiba_arrayqueue_drop(queue);
  return 0;
})

TEST_CASE(multiple_push_pop, array_queue, "Multiple push and pop operations", {
  DESC(multiple_push_pop);

  chiba_arrayqueue *queue = chiba_arrayqueue_new(5);
  ASSERT_NOT_NULL(queue, "Queue should be created");

  // Push 5 elements
  for (i64 i = 1; i <= 5; i++) {
    ASSERT_TRUE(chiba_arrayqueue_push(queue, (anyptr)i), "Push should succeed");
  }

  ASSERT_EQ(5, chiba_arrayqueue_size(queue), "Queue should have 5 elements");
  ASSERT_TRUE(chiba_arrayqueue_is_full(queue), "Queue should be full");

  // Try to push to full queue
  ASSERT_TRUE(!chiba_arrayqueue_push(queue, (anyptr)6),
              "Push to full queue should fail");

  // Pop all elements
  for (i64 i = 1; i <= 5; i++) {
    anyptr popped = chiba_arrayqueue_pop(queue);
    ASSERT_EQ(i, (i64)popped, "Popped value should match pushed value");
  }

  ASSERT_TRUE(chiba_arrayqueue_is_empty(queue), "Queue should be empty");
  ASSERT_NULL(chiba_arrayqueue_pop(queue),
              "Pop from empty queue should return NULL");

  chiba_arrayqueue_drop(queue);
  return 0;
})

TEST_CASE(wrap_around, array_queue, "Test queue wrap around", {
  DESC(wrap_around);

  chiba_arrayqueue *queue = chiba_arrayqueue_new(3);
  ASSERT_NOT_NULL(queue, "Queue should be created");

  // Fill queue
  for (i64 i = 1; i <= 3; i++) {
    ASSERT_TRUE(chiba_arrayqueue_push(queue, (anyptr)i), "Push should succeed");
  }

  // Pop 2 elements
  ASSERT_EQ(1, (i64)chiba_arrayqueue_pop(queue), "First pop should return 1");
  ASSERT_EQ(2, (i64)chiba_arrayqueue_pop(queue), "Second pop should return 2");

  // Push 2 more (wraps around)
  ASSERT_TRUE(chiba_arrayqueue_push(queue, (anyptr)4),
              "Push after wrap should succeed");
  ASSERT_TRUE(chiba_arrayqueue_push(queue, (anyptr)5),
              "Push after wrap should succeed");

  // Pop remaining
  ASSERT_EQ(3, (i64)chiba_arrayqueue_pop(queue), "Pop should return 3");
  ASSERT_EQ(4, (i64)chiba_arrayqueue_pop(queue), "Pop should return 4");
  ASSERT_EQ(5, (i64)chiba_arrayqueue_pop(queue), "Pop should return 5");
  ASSERT_TRUE(chiba_arrayqueue_is_empty(queue), "Queue should be empty");

  chiba_arrayqueue_drop(queue);
  return 0;
})

TEST_CASE(concurrent_multiple_producers_consumers, array_queue,
          "Multiple producers and consumers", {
            DESC(concurrent_multiple_producers_consumers);

            chiba_arrayqueue *queue = chiba_arrayqueue_new(128);
            ASSERT_NOT_NULL(queue, "Queue should be created");

            const int num_producers = 4;
            const int num_consumers = 4;
            const i64 items_per_producer = 500;

            atomic_int errors = 0;
            pthread_t producers[num_producers];
            pthread_t consumers[num_consumers];
            ThreadArgs prod_args[num_producers];
            ThreadArgs cons_args[num_consumers];

            // Create producers
            for (int i = 0; i < num_producers; i++) {
              prod_args[i].queue = queue;
              prod_args[i].thread_id = i + 1;
              prod_args[i].iterations = items_per_producer;
              pthread_create(&producers[i], NULL, producer_thread,
                             &prod_args[i]);
            }

            // Create consumers
            for (int i = 0; i < num_consumers; i++) {
              cons_args[i].queue = queue;
              cons_args[i].thread_id = i + 100;
              cons_args[i].iterations = items_per_producer;
              pthread_create(&consumers[i], NULL, consumer_thread,
                             &cons_args[i]);
            }

            // Join all threads
            for (int i = 0; i < num_producers; i++) {
              pthread_join(producers[i], NULL);
            }

            for (int i = 0; i < num_consumers; i++) {
              pthread_join(consumers[i], NULL);
            }

            ASSERT_TRUE(chiba_arrayqueue_is_empty(queue),
                        "Queue should be empty after test");
            ASSERT_EQ(0, errors, "No errors should occur");

            chiba_arrayqueue_drop(queue);
            return 0;
          })

TEST_CASE(concurrent_mixed_operations, array_queue,
          "Mixed concurrent push and pop", {
            DESC(concurrent_mixed_operations);

            chiba_arrayqueue *queue = chiba_arrayqueue_new(256);
            ASSERT_NOT_NULL(queue, "Queue should be created");

            // Pre-fill queue with some elements
            for (i64 i = 0; i < 100; i++) {
              chiba_arrayqueue_push(queue, (anyptr)i);
            }

            const int num_threads = 8;
            const i64 operations_per_thread = 500;

            pthread_t threads[num_threads];
            ThreadArgs args[num_threads];

            for (int i = 0; i < num_threads; i++) {
              args[i].queue = queue;
              args[i].thread_id = i + 1;
              args[i].iterations = operations_per_thread;
              pthread_create(&threads[i], NULL, mixed_thread, &args[i]);
            }

            for (int i = 0; i < num_threads; i++) {
              pthread_join(threads[i], NULL);
            }

            chiba_arrayqueue_drop(queue);
            return 0;
          })

// Thread function for ordered test - each thread pushes 10 items
void *ordered_producer_thread(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;

  for (i64 j = 0; j < 10; j++) {
    // 值从1开始，避免0被当作NULL
    i64 value = args->thread_id * 10 + j + 1;
    while (!chiba_arrayqueue_push(args->queue, (anyptr)value)) {
      // Keep trying
      for (volatile int k = 0; k < 10; k++)
        ;
    }
  }
  printf("Producer %lld done\n", args->thread_id);
  return NULL;
}

// Comparison function for qsort
int compare_i64(const void *a, const void *b) {
  i64 ia = *(const i64 *)a;
  i64 ib = *(const i64 *)b;
  return (ia > ib) - (ia < ib);
}

// Shared data structure for multi-consumer test
typedef struct {
  chiba_arrayqueue *queue;
  i64 *shared_buffer;
  atomic_int *write_index;
  i64 items_to_consume;
} ConsumerSharedArgs;

void *multi_consumer_thread(void *arg) {
  ConsumerSharedArgs *args = (ConsumerSharedArgs *)arg;
  i64 consumed = 0;

  while (consumed < args->items_to_consume) {
    anyptr value = chiba_arrayqueue_pop(args->queue);
    if (value != NULL) {
      // Atomically get write position and increment
      int pos = atomic_fetch_add(args->write_index, 1);
      args->shared_buffer[pos] = (i64)value;
      consumed++;
    } else {
      // Backoff
      for (volatile int k = 0; k < 10; k++)
        ;
    }
  }

  return NULL;
}

void *multi_producer_thread(void *arg) {
  ThreadArgs *args = (ThreadArgs *)arg;

  for (i64 i = 0; i < args->iterations; i++) {
    // Each producer pushes: thread_id*20 + i + 1
    i64 value = args->thread_id * 20 + i + 1;
    while (!chiba_arrayqueue_push(args->queue, (anyptr)value)) {
      for (volatile int k = 0; k < 10; k++)
        ;
    }
  }

  return NULL;
}

TEST_CASE(four_producers_two_consumers, array_queue,
          "4 producers 2 consumers with shared buffer", {
            DESC(four_producers_two_consumers);

            chiba_arrayqueue *queue = chiba_arrayqueue_new(128);
            ASSERT_NOT_NULL(queue, "Queue should be created");

            const int num_producers = 4;
            const int num_consumers = 2;
            const i64 items_per_producer = 20;
            const i64 total_items = num_producers * items_per_producer; // 80

            // Malloc shared buffer for 80 items
            i64 *shared_buffer =
                (i64 *)CHIBA_INTERNAL_malloc(sizeof(i64) * total_items);
            ASSERT_NOT_NULL(shared_buffer, "Shared buffer should be allocated");

            atomic_int write_index = 0;

            pthread_t producers[num_producers];
            pthread_t consumers[num_consumers];
            ThreadArgs prod_args[num_producers];
            ConsumerSharedArgs cons_args[num_consumers];

            // Create 4 producer threads
            // Producer i will push: i*20+1, i*20+2, ..., i*20+20
            for (int i = 0; i < num_producers; i++) {
              prod_args[i].queue = queue;
              prod_args[i].thread_id = i;
              prod_args[i].iterations = items_per_producer;
              pthread_create(&producers[i], NULL, multi_producer_thread,
                             &prod_args[i]);
            }

            // Create 2 consumer threads, each consumes 40 items
            for (int i = 0; i < num_consumers; i++) {
              cons_args[i].queue = queue;
              cons_args[i].shared_buffer = shared_buffer;
              cons_args[i].write_index = &write_index;
              cons_args[i].items_to_consume = total_items / num_consumers;
              pthread_create(&consumers[i], NULL, multi_consumer_thread,
                             &cons_args[i]);
            }

            // Wait for all threads to finish
            for (int i = 0; i < num_producers; i++) {
              pthread_join(producers[i], NULL);
            }

            for (int i = 0; i < num_consumers; i++) {
              pthread_join(consumers[i], NULL);
            }

            printf("All threads done. Write index: %d\n",
                   atomic_load(&write_index));
            ASSERT_EQ(80, atomic_load(&write_index),
                      "Should have written exactly 80 items");
            ASSERT_TRUE(chiba_arrayqueue_is_empty(queue),
                        "Queue should be empty");

            // Sort the received values
            qsort(shared_buffer, total_items, sizeof(i64), compare_i64);

            // Verify we have 1, 2, 3, ..., 80
            printf("Verifying sorted values...\n");
            for (i64 i = 0; i < total_items; i++) {
              ASSERT_EQ(i + 1, shared_buffer[i],
                        "After sorting, should have sequential values 1-80");
            }

            CHIBA_INTERNAL_free(shared_buffer);
            chiba_arrayqueue_drop(queue);
            return 0;
          })

TEST_CASE(ten_producers_one_consumer_ordered, array_queue,
          "10 producers push ordered values, 1 consumer verifies", {
            DESC(ten_producers_one_consumer_ordered);

            // 队列容量设置大一些，避免生产者阻塞
            chiba_arrayqueue *queue = chiba_arrayqueue_new(200);
            ASSERT_NOT_NULL(queue, "Queue should be created");

            const int num_producers = 10;
            pthread_t producers[num_producers];
            ThreadArgs prod_args[num_producers];

            // Create 10 producer threads
            // Thread i will push: i*10+0, i*10+1, ..., i*10+9
            for (int i = 0; i < num_producers; i++) {
              prod_args[i].queue = queue;
              prod_args[i].thread_id = i;
              prod_args[i].iterations = 10;
              pthread_create(&producers[i], NULL, ordered_producer_thread,
                             &prod_args[i]);
            }

            // Consumer: collect all 100 items
            i64 received[100];
            int count = 0;

            while (count < 100) {
              anyptr value = chiba_arrayqueue_pop(queue);
              if (value != NULL) {
                received[count++] = (i64)value;
                if (count % 10 == 0) {
                  printf("Received %d items so far, queue len: %lld\n", count,
                         chiba_arrayqueue_size(queue));
                }
              } else {
                // 如果队列暂时为空，稍微等待
                for (volatile int k = 0; k < 10; k++)
                  ;
              }
            }

            printf("Finished collecting. Count: %d\n", count);
            ASSERT_EQ(100, count, "Should receive exactly 100 items");
            ASSERT_TRUE(chiba_arrayqueue_is_empty(queue),
                        "Queue should be empty");

            // Sort the received values
            qsort(received, 100, sizeof(i64), compare_i64);

            // Verify we have 1, 2, 3, ..., 100
            for (i64 i = 0; i < 100; i++) {
              ASSERT_EQ(i + 1, received[i],
                        "After sorting, should have sequential values 1-100");
            }

            chiba_arrayqueue_drop(queue);
            return 0;
          })

REGISTER_TEST_GROUP(array_queue) {
  REGISTER_TEST(create_destroy, array_queue);
  REGISTER_TEST(single_push_pop, array_queue);
  REGISTER_TEST(multiple_push_pop, array_queue);
  REGISTER_TEST(wrap_around, array_queue);
  REGISTER_TEST(concurrent_multiple_producers_consumers, array_queue);
  REGISTER_TEST(concurrent_mixed_operations, array_queue);
  REGISTER_TEST(four_producers_two_consumers, array_queue);
  REGISTER_TEST(ten_producers_one_consumer_ordered, array_queue);
}

ENABLE_TEST_GROUP(array_queue);
