#include "weak_memory.h"
#include "../chiba_testing.h"

// Test data
typedef struct {
  int value;
  bool was_dropped;
} test_data;

void test_drop_marker(anyptr *data) {
  if (data && *data) {
    test_data *td = (test_data *)*data;
    td->was_dropped = true;
    CHIBA_INTERNAL_free(*data);
    *data = NULL;
  }
}

// Macros
#define ALLOC_TEST_DATA(val)                                                   \
  ({                                                                           \
    test_data *_d = (test_data *)CHIBA_INTERNAL_malloc(sizeof(test_data));     \
    _d->value = val;                                                           \
    _d->was_dropped = false;                                                   \
    _d;                                                                        \
  })

#define NEW_SHARED_TEST(val)                                                   \
  CHIBA_shared_new(ALLOC_TEST_DATA(val), sizeof(test_data), test_drop_marker)

#define GET_TEST_DATA(ptr) ((test_data *)CHIBA_shared_get(ptr))

TEST_GROUP(weak_memory);

TEST_CASE(weak_from_shared, weak_memory, "Create weak pointer from shared", {
  DESC(weak_from_shared);

  chiba_shared_ptr shared = NEW_SHARED_TEST(555);
  ASSERT_EQ(1, CHIBA_shared_weak_count(&shared), "Initial weak count is 1");

  chiba_weak_ptr weak = CHIBA_weak_from_shared(&shared);
  ASSERT_EQ(2, CHIBA_shared_weak_count(&shared), "Weak count increased to 2");
  ASSERT_TRUE(!CHIBA_weak_is_expired(&weak), "Weak pointer not expired");

  return 0;
})

TEST_CASE(weak_upgrade, weak_memory, "Upgrade weak pointer to shared", {
  DESC(weak_upgrade);

  chiba_shared_ptr shared = NEW_SHARED_TEST(666);
  chiba_weak_ptr weak = CHIBA_weak_from_shared(&shared);

  ASSERT_EQ(1, CHIBA_shared_strong_count(&shared), "Strong count is 1");

  chiba_shared_ptr upgraded = CHIBA_weak_upgrade(&weak);
  ASSERT_NOT_NULL(upgraded.control, "Upgrade succeeded");
  ASSERT_EQ(2, CHIBA_shared_strong_count(&shared), "Strong count is 2");
  ASSERT_EQ(666, GET_TEST_DATA(&upgraded)->value, "Data matches");

  return 0;
})

TEST_CASE(weak_expired, weak_memory, "Weak pointer expires when shared dropped",
          {
            DESC(weak_expired);

            test_data *data = ALLOC_TEST_DATA(777);
            chiba_shared_ptr shared =
                CHIBA_shared_new(data, sizeof(test_data), test_drop_marker);
            chiba_weak_ptr weak = CHIBA_weak_from_shared(&shared);

            ASSERT_TRUE(!CHIBA_weak_is_expired(&weak), "Not expired initially");

            CHIBA_shared_drop(&shared);
            ASSERT_TRUE(CHIBA_weak_is_expired(&weak),
                        "Expired after shared dropped");
            // Data was dropped (cannot access was_dropped after free)

            chiba_shared_ptr failed_upgrade = CHIBA_weak_upgrade(&weak);
            ASSERT_NULL(failed_upgrade.control,
                        "Upgrade fails on expired weak pointer");

            return 0;
          })

TEST_CASE(weak_clone, weak_memory, "Clone weak pointer", {
  DESC(weak_clone);

  chiba_shared_ptr shared = NEW_SHARED_TEST(888);
  chiba_weak_ptr weak1 = CHIBA_weak_from_shared(&shared);

  u64 weak_count_before = CHIBA_shared_weak_count(&shared);
  chiba_weak_ptr weak2 = CHIBA_weak_clone(&weak1);
  u64 weak_count_after = CHIBA_shared_weak_count(&shared);

  ASSERT_EQ(weak_count_before + 1, weak_count_after, "Weak count increased");

  return 0;
})

TEST_CASE(weak_cleanup_attribute, weak_memory,
          "Test weak pointer cleanup attribute", {
            DESC(weak_cleanup_attribute);

            chiba_shared_ptr shared = NEW_SHARED_TEST(2222);

            {
              chiba_weak_ptr weak = CHIBA_weak_from_shared(&shared);
              ASSERT_TRUE(!CHIBA_weak_is_expired(&weak), "Weak not expired");
              // Auto cleanup on scope exit
            }

            ASSERT_EQ(1, CHIBA_shared_weak_count(&shared),
                      "Weak count back to 1");

            return 0;
          })

TEST_CASE(
    multiple_weak_pointers, weak_memory,
    "Multiple weak pointers to same object", {
      DESC(multiple_weak_pointers);

      chiba_shared_ptr shared = NEW_SHARED_TEST(1000);

      chiba_weak_ptr weak1 = CHIBA_weak_from_shared(&shared);
      chiba_weak_ptr weak2 = CHIBA_weak_from_shared(&shared);
      chiba_weak_ptr weak3 = CHIBA_weak_from_shared(&shared);

      ASSERT_EQ(4, CHIBA_shared_weak_count(&shared), "Weak count is 4");

      chiba_shared_ptr up1 = CHIBA_weak_upgrade(&weak1);
      chiba_shared_ptr up2 = CHIBA_weak_upgrade(&weak2);
      chiba_shared_ptr up3 = CHIBA_weak_upgrade(&weak3);

      ASSERT_EQ(4, CHIBA_shared_strong_count(&shared), "Strong count is 4");
      ASSERT_EQ(1000, GET_TEST_DATA(&up1)->value, "All point to same data");
      ASSERT_EQ(1000, GET_TEST_DATA(&up2)->value, "All point to same data");
      ASSERT_EQ(1000, GET_TEST_DATA(&up3)->value, "All point to same data");

      return 0;
    })

// Thread safety test
typedef struct {
  CHIBA_weak_ptr *weak;
  int iterations;
} thread_weak_test_data;

void *thread_upgrade(void *arg) {
  thread_weak_test_data *td = (thread_weak_test_data *)arg;

  for (int i = 0; i < td->iterations; i++) {
    chiba_shared_ptr upgraded = CHIBA_weak_upgrade(td->weak);
    if (upgraded.control) {
      // Simulate work
      test_data *d = GET_TEST_DATA(&upgraded);
      if (d) {
        volatile int dummy = d->value;
        (void)dummy;
      }
    }
  }

  return NULL;
}

TEST_CASE(weak_thread_safety, weak_memory, "Concurrent weak pointer upgrades", {
  DESC(weak_thread_safety);

  chiba_shared_ptr shared = NEW_SHARED_TEST(3333);
  chiba_weak_ptr weak = CHIBA_weak_from_shared(&shared);

  const int NUM_THREADS = 4;
  const int ITERATIONS = 1000;
  pthread_t threads[NUM_THREADS];
  thread_weak_test_data thread_data[NUM_THREADS];

  for (int i = 0; i < NUM_THREADS; i++) {
    thread_data[i].weak = &weak;
    thread_data[i].iterations = ITERATIONS;
    pthread_create(&threads[i], NULL, thread_upgrade, &thread_data[i]);
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  ASSERT_EQ(1, CHIBA_shared_strong_count(&shared), "Strong count back to 1");
  ASSERT_TRUE(!CHIBA_weak_is_expired(&weak), "Weak still valid");

  return 0;
})

REGISTER_TEST_GROUP(weak_memory) {
  REGISTER_TEST(weak_from_shared, weak_memory);
  REGISTER_TEST(weak_upgrade, weak_memory);
  REGISTER_TEST(weak_expired, weak_memory);
  REGISTER_TEST(weak_clone, weak_memory);
  REGISTER_TEST(weak_cleanup_attribute, weak_memory);
  REGISTER_TEST(multiple_weak_pointers, weak_memory);
  REGISTER_TEST(weak_thread_safety, weak_memory);
}

ENABLE_TEST_GROUP(weak_memory);
