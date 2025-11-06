#pragma once
//[Chase and Lev. Dynamic circular work-stealing deque. SPAA 2005.]
// https://dl.acm.org/citation.cfm?id=1073974

#include "../basic_memory.h"
#include "../basic_types.h"
#include "../common_headers.h"

// Internal array structure for the work-stealing queue
typedef struct {
  i64 capacity;       // C in the paper
  i64 mask;           // M in the paper (capacity - 1)
  _Atomic(anyptr) *S; // Array of atomic pointers
} CHIBA_WSQArray;

// Work-stealing queue structure
// Single-producer (owner) multiple-consumer queue
typedef struct {
  _Atomic(i64) top __attribute__((aligned(64)));
  _Atomic(i64) bottom __attribute__((aligned(64)));
  _Atomic(CHIBA_WSQArray *) array;

  // Garbage collection for old arrays
  CHIBA_WSQArray **garbage;
  i32 garbage_count;
  i32 garbage_capacity;
} CHIBA_WSQueue;

// Create a new array with given capacity
PRIVATE CHIBA_WSQArray *chiba_wsqarray_new(i64 capacity) {
  CHIBA_WSQArray *arr =
      (CHIBA_WSQArray *)CHIBA_INTERNAL_malloc(sizeof(CHIBA_WSQArray));
  if (!arr)
    return NULL;

  arr->capacity = capacity;
  arr->mask = capacity - 1;
  arr->S = (_Atomic(anyptr) *)CHIBA_INTERNAL_malloc(sizeof(_Atomic(anyptr)) *
                                                    capacity);

  if (!arr->S) {
    CHIBA_INTERNAL_free(arr);
    return NULL;
  }

  // Initialize all slots to NULL
  for (i64 i = 0; i < capacity; i++) {
    atomic_init(&arr->S[i], NULL);
  }

  return arr;
}

// Destroy an array
PRIVATE void chiba_wsqarray_drop(CHIBA_WSQArray *arr) {
  if (!arr)
    return;
  CHIBA_INTERNAL_free(arr->S);
  CHIBA_INTERNAL_free(arr);
}

// Push element to array at index i
PRIVATE void chiba_wsqarray_push(CHIBA_WSQArray *arr, i64 i, anyptr item) {
  atomic_store_explicit(&arr->S[i & arr->mask], item, memory_order_relaxed);
}

// Pop element from array at index i
PRIVATE anyptr chiba_wsqarray_pop(CHIBA_WSQArray *arr, i64 i) {
  return atomic_load_explicit(&arr->S[i & arr->mask], memory_order_relaxed);
}

// Resize array (double the capacity)
PRIVATE CHIBA_WSQArray *chiba_wsqarray_resize(CHIBA_WSQArray *arr, i64 bottom,
                                              i64 top) {
  CHIBA_WSQArray *new_arr = chiba_wsqarray_new(2 * arr->capacity);
  if (!new_arr)
    return NULL;

  // Copy elements from old array to new array
  for (i64 i = top; i != bottom; ++i) {
    chiba_wsqarray_push(new_arr, i, chiba_wsqarray_pop(arr, i));
  }

  return new_arr;
}

// Create a new work-stealing queue
PRIVATE CHIBA_WSQueue *chiba_wsqueue_new(i64 capacity) {
  // Capacity must be power of 2
  if (capacity <= 0 || (capacity & (capacity - 1)) != 0) {
    return NULL;
  }

  CHIBA_WSQueue *queue =
      (CHIBA_WSQueue *)CHIBA_INTERNAL_malloc(sizeof(CHIBA_WSQueue));
  if (!queue)
    return NULL;

  atomic_init(&queue->top, 0);
  atomic_init(&queue->bottom, 0);

  CHIBA_WSQArray *arr = chiba_wsqarray_new(capacity);
  if (!arr) {
    CHIBA_INTERNAL_free(queue);
    return NULL;
  }

  atomic_init(&queue->array, arr);

  // Initialize garbage collection
  queue->garbage_capacity = 32;
  queue->garbage_count = 0;
  queue->garbage = (CHIBA_WSQArray **)CHIBA_INTERNAL_malloc(
      sizeof(CHIBA_WSQArray *) * queue->garbage_capacity);

  if (!queue->garbage) {
    chiba_wsqarray_drop(arr);
    CHIBA_INTERNAL_free(queue);
    return NULL;
  }

  return queue;
}

// Destroy work-stealing queue
PRIVATE void chiba_wsqueue_drop(CHIBA_WSQueue *queue) {
  if (!queue)
    return;

  // Clean up garbage
  for (i32 i = 0; i < queue->garbage_count; i++) {
    chiba_wsqarray_drop(queue->garbage[i]);
  }
  CHIBA_INTERNAL_free(queue->garbage);

  // Clean up current array
  CHIBA_WSQArray *arr =
      atomic_load_explicit(&queue->array, memory_order_relaxed);
  chiba_wsqarray_drop(arr);

  CHIBA_INTERNAL_free(queue);
}

// Check if queue is empty
PRIVATE bool chiba_wsqueue_is_empty(const CHIBA_WSQueue *queue) {
  i64 b = atomic_load_explicit((_Atomic(i64) *)&queue->bottom,
                               memory_order_relaxed);
  i64 t =
      atomic_load_explicit((_Atomic(i64) *)&queue->top, memory_order_relaxed);
  return b <= t;
}

// Get queue size
PRIVATE u64 chiba_wsqueue_size(const CHIBA_WSQueue *queue) {
  i64 b = atomic_load_explicit((_Atomic(i64) *)&queue->bottom,
                               memory_order_relaxed);
  i64 t =
      atomic_load_explicit((_Atomic(i64) *)&queue->top, memory_order_relaxed);
  return (u64)(b >= t ? b - t : 0);
}

// Get queue capacity
PRIVATE i64 chiba_wsqueue_capacity(const CHIBA_WSQueue *queue) {
  CHIBA_WSQArray *arr = atomic_load_explicit(
      (_Atomic(CHIBA_WSQArray *) *)&queue->array, memory_order_relaxed);
  return arr->capacity;
}

// Push item to queue (only owner thread)
PRIVATE bool chiba_wsqueue_push(CHIBA_WSQueue *queue, anyptr item,
                                bool resize_if_full) {
  i64 b = atomic_load_explicit(&queue->bottom, memory_order_relaxed);
  i64 t = atomic_load_explicit(&queue->top, memory_order_acquire);
  CHIBA_WSQArray *arr =
      atomic_load_explicit(&queue->array, memory_order_relaxed);

  // Queue is full, need to resize
  if (arr->capacity - 1 < (b - t)) {
    if (resize_if_full) {
      CHIBA_WSQArray *new_arr = chiba_wsqarray_resize(arr, b, t);
      if (!new_arr)
        return false;

      // Add old array to garbage
      if (queue->garbage_count >= queue->garbage_capacity) {
        // Expand garbage array
        i32 new_capacity = queue->garbage_capacity * 2;
        CHIBA_WSQArray **new_garbage =
            (CHIBA_WSQArray **)CHIBA_INTERNAL_realloc(
                queue->garbage, sizeof(CHIBA_WSQArray *) * new_capacity);
        if (!new_garbage) {
          chiba_wsqarray_drop(new_arr);
          return false;
        }
        queue->garbage = new_garbage;
        queue->garbage_capacity = new_capacity;
      }

      queue->garbage[queue->garbage_count++] = arr;
      arr = new_arr;
      atomic_store_explicit(&queue->array, arr, memory_order_relaxed);
    } else {
      // Queue is full and resizing not allowed
      return false;
    }
  }

  chiba_wsqarray_push(arr, b, item);
  atomic_thread_fence(memory_order_release);
  atomic_store_explicit(&queue->bottom, b + 1, memory_order_relaxed);

  return true;
}

// Pop item from queue (only owner thread)
// Returns NULL if queue is empty or pop failed
PRIVATE anyptr chiba_wsqueue_pop(CHIBA_WSQueue *queue) {
  i64 b = atomic_load_explicit(&queue->bottom, memory_order_relaxed) - 1;
  CHIBA_WSQArray *arr =
      atomic_load_explicit(&queue->array, memory_order_relaxed);
  atomic_store_explicit(&queue->bottom, b, memory_order_relaxed);
  atomic_thread_fence(memory_order_seq_cst);
  i64 t = atomic_load_explicit(&queue->top, memory_order_relaxed);

  anyptr item = NULL;

  if (t <= b) {
    item = chiba_wsqarray_pop(arr, b);
    if (t == b) {
      // The last item just got stolen
      if (!atomic_compare_exchange_strong_explicit(&queue->top, &t, t + 1,
                                                   memory_order_seq_cst,
                                                   memory_order_relaxed)) {
        item = NULL;
      }
      atomic_store_explicit(&queue->bottom, b + 1, memory_order_relaxed);
    }
  } else {
    atomic_store_explicit(&queue->bottom, b + 1, memory_order_relaxed);
  }

  return item;
}

// Steal item from queue (any thread)
// Returns NULL if queue is empty or steal failed
PRIVATE anyptr chiba_wsqueue_steal(CHIBA_WSQueue *queue) {
  i64 t = atomic_load_explicit(&queue->top, memory_order_acquire);
  atomic_thread_fence(memory_order_seq_cst);
  i64 b = atomic_load_explicit(&queue->bottom, memory_order_acquire);

  anyptr item = NULL;

  if (t < b) {
    CHIBA_WSQArray *arr =
        atomic_load_explicit(&queue->array, memory_order_consume);
    item = chiba_wsqarray_pop(arr, t);
    if (!atomic_compare_exchange_strong_explicit(&queue->top, &t, t + 1,
                                                 memory_order_seq_cst,
                                                 memory_order_relaxed)) {
      return NULL;
    }
  }

  return item;
}
