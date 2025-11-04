#pragma once
#include "../basic_memory.h"
#include "../common_headers.h"
#include "../utils/backoff.h"

// http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue

typedef u64 u64;
typedef _Atomic(u64) atomic_u64;

// A slot in a queue
typedef struct {
  // The current stamp
  // If the stamp equals the tail, this node will be next written to.
  // If it equals head + 1, this node will be next read from.
  atomic_u64 stamp;

  // The value in this slot
  anyptr value;
} ArrayQueueSlot;

// A bounded multi-producer multi-consumer queue
typedef struct {
  // The head of the queue (cache-line aligned)
  atomic_u64 head __attribute__((aligned(64)));

  // The tail of the queue (cache-line aligned)
  atomic_u64 tail __attribute__((aligned(64)));

  // The buffer holding slots
  ArrayQueueSlot *buffer;

  // Capacity of the buffer
  u64 capacity;

  // A stamp with the value of { lap: 1, index: 0 }
  u64 one_lap;
} ArrayQueue;

// Creates a new bounded queue with the given capacity
// Returns NULL if capacity is zero or allocation fails
PRIVATE ArrayQueue *array_queue_new(u64 cap) {
  if (cap == 0) {
    fprintf(stderr, "array_queue_new: capacity must be non-zero\n");
    return NULL;
  }

  ArrayQueue *queue = (ArrayQueue *)CHIBA_INTERNAL_malloc(sizeof(ArrayQueue));
  if (!queue)
    return NULL;

  // Allocate buffer
  queue->buffer =
      (ArrayQueueSlot *)CHIBA_INTERNAL_malloc(sizeof(ArrayQueueSlot) * cap);
  if (!queue->buffer) {
    CHIBA_INTERNAL_free(queue);
    return NULL;
  }

  queue->capacity = cap;

  // Initialize slots with stamps { lap: 0, index: i }
  for (u64 i = 0; i < cap; i++) {
    atomic_init(&queue->buffer[i].stamp, i);
    queue->buffer[i].value = NULL;
  }

  // One lap is the smallest power of two greater than cap
  queue->one_lap = cap + 1;

  // Round up to next power of two
  queue->one_lap--;
  queue->one_lap |= queue->one_lap >> 1;
  queue->one_lap |= queue->one_lap >> 2;
  queue->one_lap |= queue->one_lap >> 4;
  queue->one_lap |= queue->one_lap >> 8;
  queue->one_lap |= queue->one_lap >> 16;
  queue->one_lap |= queue->one_lap >> 32;
  queue->one_lap++;

  // Head is initialized to { lap: 0, index: 0 }
  atomic_init(&queue->head, 0);
  // Tail is initialized to { lap: 0, index: 0 }
  atomic_init(&queue->tail, 0);

  return queue;
}

// Helper function type for push_or_else
typedef bool (*PushElseFunc)(anyptr value, u64 tail, u64 new_tail,
                             ArrayQueueSlot *slot, ArrayQueue *queue,
                             anyptr *out_value);

// Internal push_or_else implementation
PRIVATE bool array_queue_push_or_else(ArrayQueue *queue, anyptr value,
                                      PushElseFunc f, anyptr *out_value) {
  chiba_backoff backoff = {.step = 0};
  u64 tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);

  while (1) {
    // Deconstruct the tail
    u64 index = tail & (queue->one_lap - 1);
    u64 lap = tail & ~(queue->one_lap - 1);

    u64 new_tail;
    if (index + 1 < queue->capacity) {
      // Same lap, incremented index
      new_tail = tail + 1;
    } else {
      // One lap forward, index wraps around to zero
      new_tail = lap + queue->one_lap;
    }

    // Inspect the corresponding slot
    ArrayQueueSlot *slot = &queue->buffer[index];
    u64 stamp = atomic_load_explicit(&slot->stamp, memory_order_acquire);

    // If the tail and the stamp match, we may attempt to push
    if (tail == stamp) {
      // Try moving the tail
      if (atomic_compare_exchange_weak_explicit(&queue->tail, &tail, new_tail,
                                                memory_order_seq_cst,
                                                memory_order_relaxed)) {
        // Write the value into the slot and update the stamp
        slot->value = value;
        atomic_store_explicit(&slot->stamp, tail + 1, memory_order_release);
        return true;
      }
      backoff_spin(&backoff);
    } else if (stamp + queue->one_lap == tail + 1) {
      atomic_thread_fence(memory_order_seq_cst);
      if (!f(value, tail, new_tail, slot, queue, &value)) {
        if (out_value)
          *out_value = value;
        return false;
      }
      backoff_spin(&backoff);
      tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    } else {
      // Snooze because we need to wait for the stamp to get updated
      backoff_snooze(&backoff);
      tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);
    }
  }
}

// Helper for regular push
PRIVATE bool array_queue_push_helper(anyptr value, u64 tail, u64 new_tail,
                                     ArrayQueueSlot *slot, ArrayQueue *queue,
                                     anyptr *out_value) {
  (void)new_tail;
  (void)slot;

  u64 head = atomic_load_explicit(&queue->head, memory_order_relaxed);

  // If the head lags one lap behind the tail, the queue is full
  if (head + queue->one_lap == tail) {
    *out_value = value;
    return false;
  }
  return true;
}

// Attempts to push an element into the queue
// Returns true on success, false if the queue is full
PRIVATE bool array_queue_push(ArrayQueue *queue, anyptr value) {
  return array_queue_push_or_else(queue, value, array_queue_push_helper, NULL);
}

// Attempts to pop an element from the queue
// Returns NULL if the queue is empty
PRIVATE anyptr array_queue_pop(ArrayQueue *queue) {
  chiba_backoff backoff = {.step = 0};
  u64 head = atomic_load_explicit(&queue->head, memory_order_relaxed);

  while (1) {
    // Deconstruct the head
    u64 index = head & (queue->one_lap - 1);
    u64 lap = head & ~(queue->one_lap - 1);

    // Inspect the corresponding slot
    ArrayQueueSlot *slot = &queue->buffer[index];
    u64 stamp = atomic_load_explicit(&slot->stamp, memory_order_acquire);

    // If the stamp is ahead of the head by 1, we may attempt to pop
    if (head + 1 == stamp) {
      u64 neww;
      if (index + 1 < queue->capacity) {
        // Same lap, incremented index
        neww = head + 1;
      } else {
        // One lap forward, index wraps around to zero
        neww = lap + queue->one_lap;
      }

      // Try moving the head
      if (atomic_compare_exchange_weak_explicit(&queue->head, &head, neww,
                                                memory_order_seq_cst,
                                                memory_order_relaxed)) {
        // Read the value from the slot and update the stamp
        anyptr msg = slot->value;
        atomic_store_explicit(&slot->stamp, head + queue->one_lap,
                              memory_order_release);
        return msg;
      }
      backoff_spin(&backoff);
    } else if (stamp == head) {
      atomic_thread_fence(memory_order_seq_cst);
      u64 tail = atomic_load_explicit(&queue->tail, memory_order_relaxed);

      // If the tail equals the head, the queue is empty
      if (tail == head) {
        return NULL;
      }

      backoff_spin(&backoff);
      head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    } else {
      // Snooze because we need to wait for the stamp to get updated
      backoff_snooze(&backoff);
      head = atomic_load_explicit(&queue->head, memory_order_relaxed);
    }
  }
}

// Returns the capacity of the queue
UTILS u64 array_queue_capacity(const ArrayQueue *queue) {
  return queue->capacity;
}

// Returns true if the queue is empty
PRIVATE bool array_queue_is_empty(const ArrayQueue *queue) {
  u64 head =
      atomic_load_explicit((atomic_u64 *)&queue->head, memory_order_seq_cst);
  u64 tail =
      atomic_load_explicit((atomic_u64 *)&queue->tail, memory_order_seq_cst);
  return tail == head;
}

// Returns true if the queue is full
PRIVATE bool array_queue_is_full(const ArrayQueue *queue) {
  u64 tail =
      atomic_load_explicit((atomic_u64 *)&queue->tail, memory_order_seq_cst);
  u64 head =
      atomic_load_explicit((atomic_u64 *)&queue->head, memory_order_seq_cst);
  return head + queue->one_lap == tail;
}

// Returns the number of elements in the queue
PRIVATE u64 array_queue_len(const ArrayQueue *queue) {
  while (1) {
    // Load the tail, then load the head
    u64 tail =
        atomic_load_explicit((atomic_u64 *)&queue->tail, memory_order_seq_cst);
    u64 head =
        atomic_load_explicit((atomic_u64 *)&queue->head, memory_order_seq_cst);

    // If the tail didn't change, we've got consistent values to work with
    if (atomic_load_explicit((atomic_u64 *)&queue->tail,
                             memory_order_seq_cst) == tail) {
      u64 hix = head & (queue->one_lap - 1);
      u64 tix = tail & (queue->one_lap - 1);

      if (hix < tix) {
        return tix - hix;
      } else if (hix > tix) {
        return queue->capacity - hix + tix;
      } else if (tail == head) {
        return 0;
      } else {
        return queue->capacity;
      }
    }
  }
}

// Destroys the queue and frees all allocated memory
// Note: Does not free the contained pointers - caller must handle that
PRIVATE void array_queue_destroy(ArrayQueue *queue) {
  if (!queue)
    return;
  CHIBA_INTERNAL_free(queue->buffer);
  CHIBA_INTERNAL_free(queue);
}
