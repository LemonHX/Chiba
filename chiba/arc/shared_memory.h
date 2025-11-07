#pragma once

#include "../basic_memory.h"

// Atomic reference counter for shared ownership
typedef struct {
  _Atomic(i64) strong_count;
  _Atomic(i64) weak_count;
  void (*drop)(anyptr *);
  anyptr data;
  u64 size;
} CHIBA_control_block;

// Shared pointer with atomic reference counting
typedef struct {
  CHIBA_control_block *control;
} CHIBA_shared_ptr;

////////////////////////////////////////////////////////////////////////////////
// Control Block Management
////////////////////////////////////////////////////////////////////////////////

// Forward declarations
UTILS void CHIBA_control_block_dec_weak(CHIBA_control_block *cb);

// Create a new control block
UTILS CHIBA_control_block *CHIBA_control_block_new(anyptr data, u64 size,
                                                   void (*drop)(anyptr *)) {
  CHIBA_control_block *cb =
      (CHIBA_control_block *)CHIBA_INTERNAL_malloc(sizeof(CHIBA_control_block));
  if (!cb)
    return NULL;

  atomic_init(&cb->strong_count, 1);
  atomic_init(&cb->weak_count, 1); // control block holds one weak ref
  cb->drop = drop;
  cb->data = data;
  cb->size = size;

  return cb;
}

// Increment strong count
UTILS bool CHIBA_control_block_inc_strong(CHIBA_control_block *cb) {
  if (!cb)
    return false;

  i64 old_count = atomic_load(&cb->strong_count);
  if (old_count == 0)
    return false; // Object already destroyed

  // Try to increment, but fail if count reached 0
  while (old_count > 0) {
    if (atomic_compare_exchange_weak(&cb->strong_count, &old_count,
                                     old_count + 1)) {
      return true;
    }
  }
  return false;
}

// Decrement strong count, return true if reached 0
UTILS bool CHIBA_control_block_dec_strong(CHIBA_control_block *cb) {
  if (!cb)
    return false;

  i64 old_count = atomic_fetch_sub(&cb->strong_count, 1);
  if (old_count == 1) {
    // Last strong reference - call drop function
    if (cb->drop && cb->data) {
      cb->drop(&cb->data);
    }
    // Decrement weak count (control block's weak ref)
    CHIBA_control_block_dec_weak(cb);
    return true;
  }
  return false;
}

// Increment weak count
UTILS void CHIBA_control_block_inc_weak(CHIBA_control_block *cb) {
  if (!cb)
    return;
  atomic_fetch_add(&cb->weak_count, 1);
}

// Decrement weak count, destroy control block if reached 0
UTILS void CHIBA_control_block_dec_weak(CHIBA_control_block *cb) {
  if (!cb)
    return;

  i64 old_count = atomic_fetch_sub(&cb->weak_count, 1);
  if (old_count == 1) {
    // Last weak reference - free control block
    CHIBA_INTERNAL_free(cb);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Shared Pointer Functions
////////////////////////////////////////////////////////////////////////////////

// Create a new shared pointer with data
UTILS CHIBA_shared_ptr CHIBA_shared_new(anyptr data, u64 size,
                                        void (*drop)(anyptr *)) {
  CHIBA_shared_ptr ptr = {.control = NULL};

  if (!data) {
    return ptr;
  }

  ptr.control = CHIBA_control_block_new(data, size, drop);
  return ptr;
}

// Create a new shared pointer with allocated memory
UTILS CHIBA_shared_ptr CHIBA_shared_new_with_alloc(u64 size,
                                                   void (*drop)(anyptr *)) {
  anyptr data = CHIBA_INTERNAL_malloc(size);
  if (!data) {
    CHIBA_shared_ptr ptr = {.control = NULL};
    return ptr;
  }

  return CHIBA_shared_new(data, size, drop);
}

// Clone a shared pointer (increment ref count)
UTILS CHIBA_shared_ptr CHIBA_shared_clone(const CHIBA_shared_ptr *ptr) {
  CHIBA_shared_ptr new_ptr = {.control = NULL};

  if (!ptr || !ptr->control)
    return new_ptr;

  if (CHIBA_control_block_inc_strong(ptr->control)) {
    new_ptr.control = ptr->control;
  }

  return new_ptr;
}

// Drop a shared pointer (decrement ref count)
UTILS void CHIBA_shared_drop(CHIBA_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return;

  CHIBA_control_block_dec_strong(ptr->control);
  ptr->control = NULL;
}

// Get the data pointer from shared pointer
UTILS anyptr CHIBA_shared_get(const CHIBA_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return NULL;

  // Check if strong count is > 0
  if (atomic_load(&ptr->control->strong_count) == 0)
    return NULL;

  return ptr->control->data;
}

// Get the size of the shared data
UTILS u64 CHIBA_shared_size(const CHIBA_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return 0;

  return ptr->control->size;
}

// Get strong reference count
UTILS u64 CHIBA_shared_strong_count(const CHIBA_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return 0;

  return atomic_load(&ptr->control->strong_count);
}

// Get weak reference count
UTILS u64 CHIBA_shared_weak_count(const CHIBA_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return 0;

  return atomic_load(&ptr->control->weak_count);
}

// Check if shared pointer is null
UTILS bool CHIBA_shared_is_null(const CHIBA_shared_ptr *ptr) {
  return !ptr || !ptr->control || atomic_load(&ptr->control->strong_count) == 0;
}

// Check if this is the unique owner (strong_count == 1)
UTILS bool CHIBA_shared_is_unique(const CHIBA_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return false;

  return atomic_load(&ptr->control->strong_count) == 1;
}

// Swap two shared pointers
UTILS void CHIBA_shared_swap(CHIBA_shared_ptr *a, CHIBA_shared_ptr *b) {
  if (!a || !b)
    return;

  CHIBA_control_block *temp = a->control;
  a->control = b->control;
  b->control = temp;
}

// Replace shared pointer with new data
UTILS void CHIBA_shared_replace(CHIBA_shared_ptr *ptr, anyptr data, u64 size,
                                void (*drop)(anyptr *)) {
  CHIBA_shared_drop(ptr);
  *ptr = CHIBA_shared_new(data, size, drop);
}

// Compare data pointers
UTILS bool CHIBA_shared_ptr_eq(const CHIBA_shared_ptr *a,
                               const CHIBA_shared_ptr *b) {
  anyptr data_a = CHIBA_shared_get(a);
  anyptr data_b = CHIBA_shared_get(b);
  return data_a == data_b;
}

#define chiba_shared_ptr                                                       \
  __attribute__((cleanup(CHIBA_shared_drop))) CHIBA_shared_ptr
