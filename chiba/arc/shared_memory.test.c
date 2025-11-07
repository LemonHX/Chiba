#include "shared_memory.h"
#include "../chiba_testing.h"

// Test data structure
typedef struct {
  int value;
  bool was_dropped;
} test_data;

// Custom drop function
void test_drop_marker(anyptr *data) {
  if (data && *data) {
    test_data *td = (test_data *)*data;
    td->was_dropped = true;
    CHIBA_INTERNAL_free(*data);
    *data = NULL;
  }
}

// Macros for test convenience
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

TEST_GROUP(shared_memory);

TEST_CASE(basic_creation, shared_memory, "Create and destroy shared pointer", {
  DESC(basic_creation);

  chiba_shared_ptr ptr = NEW_SHARED_TEST(42);

  ASSERT_NOT_NULL(ptr.control, "Control block created");
  ASSERT_EQ(1, CHIBA_shared_strong_count(&ptr), "Strong count is 1");
  ASSERT_EQ(42, GET_TEST_DATA(&ptr)->value, "Data value preserved");

  return 0;
})

TEST_CASE(clone_increases_refcount, shared_memory,
          "Clone increases reference count", {
            DESC(clone_increases_refcount);

            test_data *data = ALLOC_TEST_DATA(100);
            chiba_shared_ptr ptr1 =
                CHIBA_shared_new(data, sizeof(test_data), test_drop_marker);

            ASSERT_EQ(1, CHIBA_shared_strong_count(&ptr1),
                      "Initial count is 1");

            chiba_shared_ptr ptr2 = CHIBA_shared_clone(&ptr1);
            ASSERT_EQ(2, CHIBA_shared_strong_count(&ptr1),
                      "Count is 2 after clone");
            ASSERT_TRUE(GET_TEST_DATA(&ptr1) == GET_TEST_DATA(&ptr2),
                        "Same data pointer");

            return 0;
          })

TEST_CASE(unique_check, shared_memory, "Check if pointer is unique owner", {
  DESC(unique_check);

  chiba_shared_ptr ptr1 = NEW_SHARED_TEST(200);
  ASSERT_TRUE(CHIBA_shared_is_unique(&ptr1), "Unique with 1 ref");

  chiba_shared_ptr ptr2 = CHIBA_shared_clone(&ptr1);
  ASSERT_TRUE(!CHIBA_shared_is_unique(&ptr1), "Not unique with 2 refs");

  return 0;
})

TEST_CASE(swap_pointers, shared_memory, "Swap two shared pointers", {
  DESC(swap_pointers);

  chiba_shared_ptr ptr1 = NEW_SHARED_TEST(111);
  chiba_shared_ptr ptr2 = NEW_SHARED_TEST(222);

  ASSERT_EQ(111, GET_TEST_DATA(&ptr1)->value, "ptr1 has 111");
  ASSERT_EQ(222, GET_TEST_DATA(&ptr2)->value, "ptr2 has 222");

  CHIBA_shared_swap(&ptr1, &ptr2);

  ASSERT_EQ(222, GET_TEST_DATA(&ptr1)->value, "ptr1 now has 222");
  ASSERT_EQ(111, GET_TEST_DATA(&ptr2)->value, "ptr2 now has 111");

  return 0;
})

TEST_CASE(replace_data, shared_memory, "Replace shared pointer data", {
  DESC(replace_data);

  test_data *data1 = ALLOC_TEST_DATA(333);
  chiba_shared_ptr ptr =
      CHIBA_shared_new(data1, sizeof(test_data), test_drop_marker);

  ASSERT_EQ(333, GET_TEST_DATA(&ptr)->value, "Initial value is 333");

  test_data *data2 = ALLOC_TEST_DATA(444);

  // Note: data1 will be freed during replace, so we can't check was_dropped
  // after
  CHIBA_shared_replace(&ptr, data2, sizeof(test_data), test_drop_marker);

  // data1 is now freed, only check the new value
  ASSERT_EQ(444, GET_TEST_DATA(&ptr)->value, "New value is 444");

  return 0;
})

TEST_CASE(null_pointer_handling, shared_memory,
          "Handle NULL pointers gracefully", {
            DESC(null_pointer_handling);

            CHIBA_shared_ptr null_ptr = {.control = NULL};

            ASSERT_TRUE(CHIBA_shared_is_null(&null_ptr), "Detected as null");
            ASSERT_EQ(0, CHIBA_shared_strong_count(&null_ptr), "Zero count");
            ASSERT_NULL(CHIBA_shared_get(&null_ptr), "Get returns NULL");

            CHIBA_shared_drop(&null_ptr); // Should not crash

            chiba_shared_ptr clone = CHIBA_shared_clone(&null_ptr);
            ASSERT_NULL(clone.control, "Clone of null is null");

            return 0;
          })

TEST_CASE(cleanup_attribute, shared_memory, "Test automatic cleanup with scope",
          {
            DESC(cleanup_attribute);

            test_data *data = ALLOC_TEST_DATA(555);

            {
              chiba_shared_ptr scoped =
                  CHIBA_shared_new(data, sizeof(test_data), test_drop_marker);
              ASSERT_EQ(1, CHIBA_shared_strong_count(&scoped), "Count is 1");
              ASSERT_EQ(555, GET_TEST_DATA(&scoped)->value, "Data accessible");
              // Auto cleanup on scope exit
            }

            // Data should be dropped after scope exit (cannot access
            // was_dropped after free)

            return 0;
          })

// Thread safety test
typedef struct {
  CHIBA_shared_ptr *shared;
  int thread_id;
  int iterations;
} thread_test_data;

void *thread_clone_drop(void *arg) {
  thread_test_data *td = (thread_test_data *)arg;

  for (int i = 0; i < td->iterations; i++) {
    chiba_shared_ptr clone = CHIBA_shared_clone(td->shared);
    // Simulate work
    test_data *d = GET_TEST_DATA(&clone);
    if (d) {
      volatile int dummy = d->value;
      (void)dummy;
    }
  }

  return NULL;
}

TEST_CASE(
    thread_safety, shared_memory, "Concurrent clone and drop operations", {
      DESC(thread_safety);

      test_data *data = ALLOC_TEST_DATA(999);
      chiba_shared_ptr shared =
          CHIBA_shared_new(data, sizeof(test_data), test_drop_marker);

      const int NUM_THREADS = 4;
      const int ITERATIONS = 1000;
      pthread_t threads[NUM_THREADS];
      thread_test_data thread_data[NUM_THREADS];

      for (int i = 0; i < NUM_THREADS; i++) {
        thread_data[i].shared = &shared;
        thread_data[i].thread_id = i;
        thread_data[i].iterations = ITERATIONS;
        pthread_create(&threads[i], NULL, thread_clone_drop, &thread_data[i]);
      }

      for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
      }

      ASSERT_EQ(1, CHIBA_shared_strong_count(&shared), "Count back to 1");
      // Data should still be alive while shared ptr exists
      ASSERT_EQ(999, GET_TEST_DATA(&shared)->value, "Data still accessible");

      CHIBA_shared_drop(&shared);
      // Data should be dropped (cannot access was_dropped after free)

      return 0;
    })

REGISTER_TEST_GROUP(shared_memory) {
  REGISTER_TEST(basic_creation, shared_memory);
  REGISTER_TEST(clone_increases_refcount, shared_memory);
  REGISTER_TEST(unique_check, shared_memory);
  REGISTER_TEST(swap_pointers, shared_memory);
  REGISTER_TEST(replace_data, shared_memory);
  REGISTER_TEST(null_pointer_handling, shared_memory);
  REGISTER_TEST(cleanup_attribute, shared_memory);
  REGISTER_TEST(thread_safety, shared_memory);
}

ENABLE_TEST_GROUP(shared_memory);
