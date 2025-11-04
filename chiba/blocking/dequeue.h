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
} WSQArray;

// Work-stealing queue structure
// Single-producer (owner) multiple-consumer queue
typedef struct {
  _Atomic(i64) top __attribute__((aligned(64)));
  _Atomic(i64) bottom __attribute__((aligned(64)));
  _Atomic(WSQArray *) array;

  // Garbage collection for old arrays
  WSQArray **garbage;
  i32 garbage_count;
  i32 garbage_capacity;
} WorkStealingQueue;

// Create a new array with given capacity
PRIVATE WSQArray *wsq_array_new(i64 capacity) {
  WSQArray *arr = (WSQArray *)CHIBA_INTERNAL_malloc(sizeof(WSQArray));
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
PRIVATE void wsq_array_destroy(WSQArray *arr) {
  if (!arr)
    return;
  CHIBA_INTERNAL_free(arr->S);
  CHIBA_INTERNAL_free(arr);
}

// Push element to array at index i
PRIVATE void wsq_array_push(WSQArray *arr, i64 i, anyptr item) {
  atomic_store_explicit(&arr->S[i & arr->mask], item, memory_order_relaxed);
}

// Pop element from array at index i
PRIVATE anyptr wsq_array_pop(WSQArray *arr, i64 i) {
  return atomic_load_explicit(&arr->S[i & arr->mask], memory_order_relaxed);
}

// Resize array (double the capacity)
PRIVATE WSQArray *wsq_array_resize(WSQArray *arr, i64 bottom, i64 top) {
  WSQArray *new_arr = wsq_array_new(2 * arr->capacity);
  if (!new_arr)
    return NULL;

  // Copy elements from old array to new array
  for (i64 i = top; i != bottom; ++i) {
    wsq_array_push(new_arr, i, wsq_array_pop(arr, i));
  }

  return new_arr;
}

// Create a new work-stealing queue
PRIVATE WorkStealingQueue *wsqueue_new(i64 capacity) {
  // Capacity must be power of 2
  if (capacity <= 0 || (capacity & (capacity - 1)) != 0) {
    return NULL;
  }

  WorkStealingQueue *queue =
      (WorkStealingQueue *)CHIBA_INTERNAL_malloc(sizeof(WorkStealingQueue));
  if (!queue)
    return NULL;

  atomic_init(&queue->top, 0);
  atomic_init(&queue->bottom, 0);

  WSQArray *arr = wsq_array_new(capacity);
  if (!arr) {
    CHIBA_INTERNAL_free(queue);
    return NULL;
  }

  atomic_init(&queue->array, arr);

  // Initialize garbage collection
  queue->garbage_capacity = 32;
  queue->garbage_count = 0;
  queue->garbage = (WSQArray **)CHIBA_INTERNAL_malloc(sizeof(WSQArray *) *
                                                      queue->garbage_capacity);

  if (!queue->garbage) {
    wsq_array_destroy(arr);
    CHIBA_INTERNAL_free(queue);
    return NULL;
  }

  return queue;
}

// Destroy work-stealing queue
PRIVATE void wsqueue_destroy(WorkStealingQueue *queue) {
  if (!queue)
    return;

  // Clean up garbage
  for (i32 i = 0; i < queue->garbage_count; i++) {
    wsq_array_destroy(queue->garbage[i]);
  }
  CHIBA_INTERNAL_free(queue->garbage);

  // Clean up current array
  WSQArray *arr = atomic_load_explicit(&queue->array, memory_order_relaxed);
  wsq_array_destroy(arr);

  CHIBA_INTERNAL_free(queue);
}

// Check if queue is empty
PRIVATE bool wsqueue_empty(const WorkStealingQueue *queue) {
  i64 b = atomic_load_explicit((_Atomic(i64) *)&queue->bottom,
                               memory_order_relaxed);
  i64 t =
      atomic_load_explicit((_Atomic(i64) *)&queue->top, memory_order_relaxed);
  return b <= t;
}

// Get queue size
PRIVATE u64 wsqueue_size(const WorkStealingQueue *queue) {
  i64 b = atomic_load_explicit((_Atomic(i64) *)&queue->bottom,
                               memory_order_relaxed);
  i64 t =
      atomic_load_explicit((_Atomic(i64) *)&queue->top, memory_order_relaxed);
  return (u64)(b >= t ? b - t : 0);
}

// Get queue capacity
PRIVATE i64 wsqueue_capacity(const WorkStealingQueue *queue) {
  WSQArray *arr = atomic_load_explicit((_Atomic(WSQArray *) *)&queue->array,
                                       memory_order_relaxed);
  return arr->capacity;
}

// Push item to queue (only owner thread)
PRIVATE bool wsqueue_push(WorkStealingQueue *queue, anyptr item) {
  i64 b = atomic_load_explicit(&queue->bottom, memory_order_relaxed);
  i64 t = atomic_load_explicit(&queue->top, memory_order_acquire);
  WSQArray *arr = atomic_load_explicit(&queue->array, memory_order_relaxed);

  // Queue is full, need to resize
  if (arr->capacity - 1 < (b - t)) {
    WSQArray *new_arr = wsq_array_resize(arr, b, t);
    if (!new_arr)
      return false;

    // Add old array to garbage
    if (queue->garbage_count >= queue->garbage_capacity) {
      // Expand garbage array
      i32 new_capacity = queue->garbage_capacity * 2;
      WSQArray **new_garbage = (WSQArray **)CHIBA_INTERNAL_realloc(
          queue->garbage, sizeof(WSQArray *) * new_capacity);
      if (!new_garbage) {
        wsq_array_destroy(new_arr);
        return false;
      }
      queue->garbage = new_garbage;
      queue->garbage_capacity = new_capacity;
    }

    queue->garbage[queue->garbage_count++] = arr;
    arr = new_arr;
    atomic_store_explicit(&queue->array, arr, memory_order_relaxed);
  }

  wsq_array_push(arr, b, item);
  atomic_thread_fence(memory_order_release);
  atomic_store_explicit(&queue->bottom, b + 1, memory_order_relaxed);

  return true;
}

// Pop item from queue (only owner thread)
// Returns NULL if queue is empty or pop failed
PRIVATE anyptr wsqueue_pop(WorkStealingQueue *queue) {
  i64 b = atomic_load_explicit(&queue->bottom, memory_order_relaxed) - 1;
  WSQArray *arr = atomic_load_explicit(&queue->array, memory_order_relaxed);
  atomic_store_explicit(&queue->bottom, b, memory_order_relaxed);
  atomic_thread_fence(memory_order_seq_cst);
  i64 t = atomic_load_explicit(&queue->top, memory_order_relaxed);

  anyptr item = NULL;

  if (t <= b) {
    item = wsq_array_pop(arr, b);
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
PRIVATE anyptr wsqueue_steal(WorkStealingQueue *queue) {
  i64 t = atomic_load_explicit(&queue->top, memory_order_acquire);
  atomic_thread_fence(memory_order_seq_cst);
  i64 b = atomic_load_explicit(&queue->bottom, memory_order_acquire);

  anyptr item = NULL;

  if (t < b) {
    WSQArray *arr = atomic_load_explicit(&queue->array, memory_order_consume);
    item = wsq_array_pop(arr, t);
    if (!atomic_compare_exchange_strong_explicit(&queue->top, &t, t + 1,
                                                 memory_order_seq_cst,
                                                 memory_order_relaxed)) {
      return NULL;
    }
  }

  return item;
}
