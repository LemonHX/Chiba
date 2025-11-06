#include "dequeue.h"
#include "../basic_types.h"
#include "../chiba_testing.h"
TEST_GROUP(dequeue);

// Simple deque implementation for verification
typedef struct {
  i64 *data;
  i32 front;
  i32 back;
  i32 capacity;
} SimpleDeque;

PRIVATE SimpleDeque *simple_deque_new(i32 capacity) {
  SimpleDeque *deq = (SimpleDeque *)CHIBA_INTERNAL_malloc(sizeof(SimpleDeque));
  deq->data = (i64 *)CHIBA_INTERNAL_malloc(sizeof(i64) * capacity);
  deq->front = 0;
  deq->back = 0;
  deq->capacity = capacity;
  return deq;
}

PRIVATE void simple_deque_destroy(SimpleDeque *deq) {
  CHIBA_INTERNAL_free(deq->data);
  CHIBA_INTERNAL_free(deq);
}

PRIVATE void simple_deque_push_back(SimpleDeque *deq, i64 value) {
  if (deq->back >= deq->capacity) {
    deq->capacity *= 2;
    deq->data =
        (i64 *)CHIBA_INTERNAL_realloc(deq->data, sizeof(i64) * deq->capacity);
  }
  deq->data[deq->back++] = value;
}

PRIVATE i64 simple_deque_pop_back(SimpleDeque *deq) {
  return deq->data[--deq->back];
}

PRIVATE i64 simple_deque_pop_front(SimpleDeque *deq) {
  return deq->data[deq->front++];
}

PRIVATE i32 simple_deque_size(const SimpleDeque *deq) {
  return deq->back - deq->front;
}

PRIVATE bool simple_deque_empty(const SimpleDeque *deq) {
  return deq->front >= deq->back;
}

// Test owner thread operations
TEST_CASE(owner_operations, dequeue, "Test owner push/pop operations", {
  DESC(owner_operations);

  CHIBA_WSQueue *queue = chiba_wsqueue_new(2);
  ASSERT_NOT_NULL(queue, "Queue should be created");
  ASSERT_EQ(2, chiba_wsqueue_capacity(queue), "Initial capacity should be 2");
  ASSERT_TRUE(chiba_wsqueue_is_empty(queue), "Queue should be empty");

  SimpleDeque *gold = simple_deque_new(16);

  // Test with increasing sizes
  for (i32 i = 2; i <= (1 << 10); i <<= 1) {
    ASSERT_TRUE(chiba_wsqueue_is_empty(queue),
                "Queue should be empty at start");

    // Push items
    for (i32 j = 0; j < i; j++) {
      ASSERT_TRUE(chiba_wsqueue_push(queue, (anyptr)(i64)(j + 1), true),
                  "Push should succeed");
    }

    // Pop items (LIFO order)
    for (i32 j = 0; j < i; j++) {
      anyptr item = chiba_wsqueue_pop(queue);
      ASSERT_NOT_NULL(item, "Pop should return non-NULL");
      ASSERT_EQ(i - j, (i64)item, "Popped item should match (LIFO)");
    }
    ASSERT_NULL(chiba_wsqueue_pop(queue),
                "Pop from empty queue should return NULL");

    ASSERT_TRUE(chiba_wsqueue_is_empty(queue),
                "Queue should be empty after pops");

    // Push and steal (FIFO order)
    for (i32 j = 0; j < i; j++) {
      ASSERT_TRUE(chiba_wsqueue_push(queue, (anyptr)(i64)(j + 1), true),
                  "Push should succeed");
    }

    for (i32 j = 0; j < i; j++) {
      anyptr item = chiba_wsqueue_steal(queue);
      ASSERT_NOT_NULL(item, "Steal should return non-NULL");
      ASSERT_EQ(j + 1, (i64)item, "Stolen item should match (FIFO)");
    }
    ASSERT_NULL(chiba_wsqueue_pop(queue),
                "Pop from empty queue should return NULL");

    ASSERT_TRUE(chiba_wsqueue_is_empty(queue),
                "Queue should be empty after steals");

    printf("   Tested size %d successfully\n", i);
  }

  simple_deque_destroy(gold);
  chiba_wsqueue_drop(queue);
  return 0;
})

// Test mixed operations
TEST_CASE(mixed_operations, dequeue, "Test mixed push/pop/steal operations", {
  DESC(mixed_operations);

  CHIBA_WSQueue *queue = chiba_wsqueue_new(2);
  ASSERT_NOT_NULL(queue, "Queue should be created");

  SimpleDeque *gold = simple_deque_new(1024);

  srand((unsigned)time(NULL));

  // Perform 1000 random operations
  for (i32 i = 0; i < 1000; i++) {
    i32 dice = rand() % 3;

    if (dice == 0) {
      // Push
      chiba_wsqueue_push(queue, (anyptr)(i64)(i + 1), true);
      simple_deque_push_back(gold, i + 1);
    } else if (dice == 1) {
      // Pop back
      anyptr item = chiba_wsqueue_pop(queue);
      if (simple_deque_empty(gold)) {
        ASSERT_NULL(item, "Pop should return NULL when gold is empty");
      } else {
        ASSERT_NOT_NULL(item, "Pop should return non-NULL when gold has items");
        i64 gold_val = simple_deque_pop_back(gold);
        ASSERT_EQ(gold_val, (i64)item, "Popped value should match gold");
      }
    } else {
      // Steal front
      anyptr item = chiba_wsqueue_steal(queue);
      if (simple_deque_empty(gold)) {
        ASSERT_NULL(item, "Steal should return NULL when gold is empty");
      } else {
        ASSERT_NOT_NULL(item,
                        "Steal should return non-NULL when gold has items");
        i64 gold_val = simple_deque_pop_front(gold);
        ASSERT_EQ(gold_val, (i64)item, "Stolen value should match gold");
      }
    }

    ASSERT_EQ(simple_deque_size(gold), (i64)chiba_wsqueue_size(queue),
              "Queue size should match gold size");
  }

  // Drain remaining items
  while (!chiba_wsqueue_is_empty(queue)) {
    anyptr item = chiba_wsqueue_pop(queue);
    ASSERT_NOT_NULL(item, "Item should not be NULL");
    i64 gold_val = simple_deque_pop_back(gold);
    ASSERT_EQ(gold_val, (i64)item, "Remaining items should match");
  }

  ASSERT_TRUE(simple_deque_empty(gold), "Gold should be empty");

  simple_deque_destroy(gold);
  chiba_wsqueue_drop(queue);
  return 0;
})

// Thread data for concurrent tests
// Per-consumer deque to store stolen items
typedef struct {
  i64 *data;
  atomic_int size;
  i32 capacity;
} ConsumerDeque;

typedef struct {
  CHIBA_WSQueue *queue;
  ConsumerDeque *cdeqs;
  atomic_int *pdeq_size;
  i32 N;
  i32 consumer_id;
  i32 target_count;
} ConsumerArgs;

void *consumer_thread(void *arg) {
  ConsumerArgs *args = (ConsumerArgs *)arg;
  while (1) {
    // Calculate total stolen
    i32 total = atomic_load(args->pdeq_size);
    for (i32 j = 0; j < args->N; j++) {
      total += atomic_load(&args->cdeqs[j].size);
    }
    if (total >= args->target_count) {
      break;
    }

    // 25% chance to steal
    if (rand() % 4 == 0) {
      anyptr item = chiba_wsqueue_steal(args->queue);
      if (item != NULL) {
        // sleep 10ms
        struct timespec ts;
        ts.tv_sec = 0;         // 0 seconds
        ts.tv_nsec = 10000000; // 10 million nanoseconds (0.01 seconds)
        nanosleep(&ts, NULL);
        i32 idx = atomic_fetch_add(&args->cdeqs[args->consumer_id].size, 1);
        args->cdeqs[args->consumer_id].data[idx] = (i64)item;
      }
    }
  }
  return NULL;
}

// Test with N thieves
PRIVATE int test_n_thieves(i32 N, i32 item_count) {
  CHIBA_WSQueue *queue = chiba_wsqueue_new(2);
  if (!queue)
    return -1;

  // Per-consumer deques to store stolen items
  typedef struct {
    i64 *data;
    atomic_int size;
    i32 capacity;
  } ConsumerDeque;

  ConsumerDeque *cdeqs =
      (ConsumerDeque *)CHIBA_INTERNAL_malloc(sizeof(ConsumerDeque) * N);
  for (i32 i = 0; i < N; i++) {
    cdeqs[i].data = (i64 *)CHIBA_INTERNAL_malloc(sizeof(i64) * item_count);
    atomic_init(&cdeqs[i].size, 0);
    cdeqs[i].capacity = item_count;
  }

  // Producer's deque for items it pops
  i64 *pdeq = (i64 *)CHIBA_INTERNAL_malloc(sizeof(i64) * item_count);
  atomic_int pdeq_size = 0;

  atomic_int p = 0; // Number of items pushed by producer

  pthread_t *thieves =
      (pthread_t *)CHIBA_INTERNAL_malloc(sizeof(pthread_t) * N);

  // Calculate total stolen items
  typedef struct {
    CHIBA_WSQueue *queue;
    ConsumerDeque *cdeqs;
    atomic_int *pdeq_size;
    i32 N;
    i32 consumer_id;
    i32 target_count;
  } ConsumerArgs;

  ConsumerArgs *consumer_args =
      (ConsumerArgs *)CHIBA_INTERNAL_malloc(sizeof(ConsumerArgs) * N);

  // Create consumer threads
  for (i32 i = 0; i < N; i++) {
    consumer_args[i].queue = queue;
    consumer_args[i].cdeqs = cdeqs;
    consumer_args[i].pdeq_size = &pdeq_size;
    consumer_args[i].N = N;
    consumer_args[i].consumer_id = i;
    consumer_args[i].target_count = item_count;

    pthread_create(&thieves[i], NULL, consumer_thread, &consumer_args[i]);
  }

  // Producer thread (main thread)
  while (atomic_load(&p) < item_count) {
    i32 dice = rand() % 4;
    if (dice == 0) {
      // 25% chance to push
      i32 val = atomic_fetch_add(&p, 1);
      if (val < item_count) {
        chiba_wsqueue_push(queue, (anyptr)(i64)(val + 1), true);
      }
    } else if (dice == 1) {
      // 25% chance to pop
      anyptr item = chiba_wsqueue_pop(queue);
      if (item != NULL) {
        i32 idx = atomic_fetch_add(&pdeq_size, 1);
        pdeq[idx] = (i64)item;
      }
    }
  }

  // Wait for consumers
  for (i32 i = 0; i < N; i++) {
    pthread_join(thieves[i], NULL);
  }

  // Verify queue is empty
  ASSERT_TRUE(chiba_wsqueue_is_empty(queue),
              "Queue should be empty after test");

  // Collect all values into a set
  bool *seen = (bool *)CHIBA_INTERNAL_malloc(sizeof(bool) * (item_count + 1));
  for (i32 i = 0; i <= item_count; i++) {
    seen[i] = false;
  }

  // Add consumer stolen items
  for (i32 i = 0; i < N; i++) {
    i32 size = atomic_load(&cdeqs[i].size);
    for (i32 j = 0; j < size; j++) {
      i64 val = cdeqs[i].data[j];
      if (val > 0 && val <= item_count) {
        if (seen[val]) {
          printf("Duplicate value: %lld\n", val);
        }
        seen[val] = true;
      }
    }
  }

  // Add producer popped items
  i32 psize = atomic_load(&pdeq_size);
  for (i32 i = 0; i < psize; i++) {
    i64 val = pdeq[i];
    if (val > 0 && val <= item_count) {
      if (seen[val]) {
        printf("Duplicate value: %lld\n", val);
      }
      seen[val] = true;
    }
  }

  // Check all items are present
  i32 result = 0;
  for (i32 i = 1; i <= item_count; i++) {
    if (!seen[i]) {
      printf("Missing value: %d\n", i);
      result = -1;
    }
  }

  // Cleanup
  CHIBA_INTERNAL_free(seen);
  for (i32 i = 0; i < N; i++) {
    CHIBA_INTERNAL_free(cdeqs[i].data);
  }
  CHIBA_INTERNAL_free(cdeqs);
  CHIBA_INTERNAL_free(pdeq);
  CHIBA_INTERNAL_free(thieves);
  CHIBA_INTERNAL_free(consumer_args);
  chiba_wsqueue_drop(queue);

  return result;
}

TEST_CASE(one_thief, dequeue, "Test with 1 thief thread", {
  DESC(one_thief);
  ASSERT_EQ(0, test_n_thieves(1, 2560), "Test with 1 thief should succeed");
  return 0;
})

TEST_CASE(two_thieves, dequeue, "Test with 2 thief threads", {
  DESC(two_thieves);
  ASSERT_EQ(0, test_n_thieves(2, 2560), "Test with 2 thieves should succeed");
  return 0;
})

TEST_CASE(four_thieves, dequeue, "Test with 4 thief threads", {
  DESC(four_thieves);
  ASSERT_EQ(0, test_n_thieves(4, 2560), "Test with 4 thieves should succeed");
  return 0;
})

TEST_CASE(eight_thieves, dequeue, "Test with 8 thief threads", {
  DESC(eight_thieves);
  ASSERT_EQ(0, test_n_thieves(8, 2560), "Test with 8 thieves should succeed");
  return 0;
})

TEST_CASE(sixteen_thieves, dequeue, "Test with 16 thief threads", {
  DESC(sixteen_thieves);
  ASSERT_EQ(0, test_n_thieves(16, 2560), "Test with 16 thieves should succeed");
  return 0;
})

REGISTER_TEST_GROUP(dequeue) {
  REGISTER_TEST(owner_operations, dequeue);
  REGISTER_TEST(mixed_operations, dequeue);
  REGISTER_TEST(one_thief, dequeue);
  REGISTER_TEST(two_thieves, dequeue);
  REGISTER_TEST(four_thieves, dequeue);
  REGISTER_TEST(eight_thieves, dequeue);
  REGISTER_TEST(sixteen_thieves, dequeue);
}

ENABLE_TEST_GROUP(dequeue);
