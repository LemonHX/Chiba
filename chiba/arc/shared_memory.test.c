#include "shared_memory.h"
#include "../chiba_testing.h"

// Test data structure
typedef struct {
  int value;
  bool was_dropped;
} test_data;

// Custom drop function
void test_drop_marker(anyptr data) {
  if (data) {
    test_data *td = (test_data *)data;
    td->was_dropped = true;
  }
}

void TEST_DATA_INIT(test_data *val, anyptr args) {
  val->value = (int)args;
  val->was_dropped = false;
}

#define NEW_SHARED_TEST(val)                                                   \
  chiba_shared_new(sizeof(test_data), (anyptr)(int *)(val),                    \
                   (void (*)(anyptr, anyptr))TEST_DATA_INIT, test_drop_marker)

#define GET_TEST_DATA(ptr) ((test_data *)chiba_shared_get(ptr))

TEST_GROUP(shared_memory);

TEST_CASE(basic_creation, shared_memory, "Create and destroy shared pointer", {
  DESC(basic_creation);

  chiba_shared_ptr_var(test_data) ptr = NEW_SHARED_TEST(42);

  ASSERT_NOT_NULL(ptr.control, "Control block created");
  ASSERT_EQ(1, chiba_shared_strong_count(&ptr), "Strong count is 1");
  ASSERT_EQ(42, GET_TEST_DATA(&ptr)->value, "Data value preserved");

  return 0;
})

TEST_CASE(clone_increases_refcount, shared_memory,
          "Clone increases reference count", {
            DESC(clone_increases_refcount);

            chiba_shared_ptr_var(test_data) ptr1 = NEW_SHARED_TEST(100);

            ASSERT_EQ(1, chiba_shared_strong_count(&ptr1),
                      "Initial count is 1");

            chiba_shared_ptr_var(test_data) ptr2 = chiba_shared_clone(&ptr1);
            ASSERT_EQ(2, chiba_shared_strong_count(&ptr1),
                      "Count is 2 after clone");
            ASSERT_TRUE(GET_TEST_DATA(&ptr1) == GET_TEST_DATA(&ptr2),
                        "Same data pointer");

            return 0;
          })

TEST_CASE(unique_check, shared_memory, "Check if pointer is unique owner", {
  DESC(unique_check);

  chiba_shared_ptr_var(test_data) ptr1 = NEW_SHARED_TEST(200);
  ASSERT_TRUE(chiba_shared_is_unique(&ptr1), "Unique with 1 ref");

  chiba_shared_ptr_var(test_data) ptr2 = chiba_shared_clone(&ptr1);
  ASSERT_TRUE(!chiba_shared_is_unique(&ptr1), "Not unique with 2 refs");

  return 0;
})

TEST_CASE(swap_pointers, shared_memory, "Swap two shared pointers", {
  DESC(swap_pointers);

  chiba_shared_ptr_var(test_data) ptr1 = NEW_SHARED_TEST(111);
  chiba_shared_ptr_var(test_data) ptr2 = NEW_SHARED_TEST(222);

  ASSERT_EQ(111, GET_TEST_DATA(&ptr1)->value, "ptr1 has 111");
  ASSERT_EQ(222, GET_TEST_DATA(&ptr2)->value, "ptr2 has 222");

  chiba_shared_swap(&ptr1, &ptr2);

  ASSERT_EQ(222, GET_TEST_DATA(&ptr1)->value, "ptr1 now has 222");
  ASSERT_EQ(111, GET_TEST_DATA(&ptr2)->value, "ptr2 now has 111");

  return 0;
})

TEST_CASE(null_pointer_handling, shared_memory,
          "Handle NULL pointers gracefully", {
            DESC(null_pointer_handling);

            chiba_shared_ptr null_ptr = {.control = NULL};

            ASSERT_TRUE(chiba_shared_is_null(&null_ptr), "Detected as null");
            ASSERT_EQ(0, chiba_shared_strong_count(&null_ptr), "Zero count");
            ASSERT_NULL(chiba_shared_get(&null_ptr), "Get returns NULL");

            chiba_shared_drop(&null_ptr); // Should not crash

            chiba_shared_ptr_var(test_data) clone =
                chiba_shared_clone(&null_ptr);
            ASSERT_NULL(clone.control, "Clone of null is null");

            return 0;
          })

TEST_CASE(cleanup_attribute, shared_memory, "Test automatic cleanup with scope",
          {
            DESC(cleanup_attribute);

            {
              chiba_shared_ptr_var(test_data) scoped = NEW_SHARED_TEST(555);
              ASSERT_EQ(1, chiba_shared_strong_count(&scoped), "Count is 1");
              ASSERT_EQ(555, GET_TEST_DATA(&scoped)->value, "Data accessible");
              // Auto cleanup on scope exit
            }

            // Data should be dropped after scope exit (cannot access
            // was_dropped after free)

            return 0;
          })

// Thread safety test
typedef struct {
  chiba_shared_ptr *shared;
  int thread_id;
  int iterations;
} thread_test_data;

void *thread_clone_drop(void *arg) {
  thread_test_data *td = (thread_test_data *)arg;

  for (int i = 0; i < td->iterations; i++) {
    chiba_shared_ptr_var(test_data) clone = chiba_shared_clone(td->shared);
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

      chiba_shared_ptr_var(test_data) shared = NEW_SHARED_TEST(999);

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

      ASSERT_EQ(1, chiba_shared_strong_count(&shared), "Count back to 1");
      // Data should still be alive while shared ptr exists
      ASSERT_EQ(999, GET_TEST_DATA(&shared)->value, "Data still accessible");

      chiba_shared_drop(&shared);
      // Data should be dropped (cannot access was_dropped after free)

      return 0;
    })

REGISTER_TEST_GROUP(shared_memory) {
  REGISTER_TEST(basic_creation, shared_memory);
  REGISTER_TEST(clone_increases_refcount, shared_memory);
  REGISTER_TEST(unique_check, shared_memory);
  REGISTER_TEST(swap_pointers, shared_memory);
  REGISTER_TEST(null_pointer_handling, shared_memory);
  REGISTER_TEST(cleanup_attribute, shared_memory);
  REGISTER_TEST(thread_safety, shared_memory);
}

ENABLE_TEST_GROUP(shared_memory);
