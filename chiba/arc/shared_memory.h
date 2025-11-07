#pragma once

#include "../basic_memory.h"

// Atomic reference counter for shared ownership
typedef struct {
  _Atomic(i64) strong_count;
  _Atomic(i64) weak_count;

  // comparing the init pointer could tell the type of data
  // i am so fucking clever
  void (*init)(anyptr self, anyptr args);
  void (*drop)(anyptr self);

  u64 _start_data[0]; // Placeholder for data start
} chiba_control_block;

// Shared pointer with atomic reference counting
typedef struct {
  chiba_control_block *control;
} chiba_shared_ptr;

////////////////////////////////////////////////////////////////////////////////
// Control Block Management
////////////////////////////////////////////////////////////////////////////////

// Forward declarations
UTILS void chiba_control_block_dec_weak(chiba_control_block *cb);

UTILS chiba_shared_ptr chiba_shared_null() {
  chiba_shared_ptr ptr = {.control = NULL};
  return ptr;
}

UTILS bool chiba_shared_ptr_is_null(chiba_shared_ptr *ptr) {
  return !ptr || !ptr->control;
}

// Create a new control block
UTILS chiba_control_block *chiba_control_block_new(u64 size, anyptr init_args,
                                                   void (*init)(anyptr, anyptr),
                                                   void (*drop)(anyptr)) {
  chiba_control_block *cb = (chiba_control_block *)CHIBA_INTERNAL_malloc(
      sizeof(chiba_control_block) + size);
  if (!cb)
    return NULL;

  atomic_init(&cb->strong_count, 1);
  atomic_init(&cb->weak_count, 1); // control block holds one weak ref
  cb->drop = drop;
  cb->init = init;

  // invoke init function if provided
  if (cb->init) {
    cb->init(cb->_start_data, init_args);
  }
  return cb;
}

// Increment strong count
UTILS bool chiba_control_block_inc_strong(chiba_control_block *cb) {
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
UTILS bool chiba_control_block_dec_strong(chiba_control_block *cb) {
  if (!cb)
    return false;

  i64 old_count = atomic_fetch_sub(&cb->strong_count, 1);
  if (old_count == 1) {
    // Last strong reference - call drop function
    if (cb->drop) {
      cb->drop(cb->_start_data);
    }
    // Decrement weak count (control block's weak ref)
    chiba_control_block_dec_weak(cb);
    return true;
  }
  return false;
}

// Increment weak count
UTILS void chiba_control_block_inc_weak(chiba_control_block *cb) {
  if (!cb)
    return;
  atomic_fetch_add(&cb->weak_count, 1);
}

// Decrement weak count, destroy control block if reached 0
UTILS void chiba_control_block_dec_weak(chiba_control_block *cb) {
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

// Create a new shared pointer with allocated memory
UTILS chiba_shared_ptr chiba_shared_new(u64 size, anyptr args,
                                        void (*init)(anyptr, anyptr),
                                        void (*drop)(anyptr)) {
  chiba_shared_ptr ptr = {.control = NULL};
  ptr.control = chiba_control_block_new(size, args, init, drop);
  return ptr;
}

// Clone a shared pointer (increment ref count)
UTILS chiba_shared_ptr chiba_shared_clone(const chiba_shared_ptr *ptr) {
  chiba_shared_ptr new_ptr = {.control = NULL};

  if (!ptr || !ptr->control)
    return new_ptr;

  if (chiba_control_block_inc_strong(ptr->control)) {
    new_ptr.control = ptr->control;
  }

  return new_ptr;
}

// Drop a shared pointer (decrement ref count)
UTILS void chiba_shared_drop(chiba_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return;

  chiba_control_block_dec_strong(ptr->control);
  ptr->control = NULL;
}

// Get the data pointer from shared pointer
UTILS anyptr chiba_shared_get(const chiba_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return NULL;

  // Check if strong count is > 0
  if (atomic_load(&ptr->control->strong_count) == 0)
    return NULL;

  return ptr->control->_start_data;
}

// Get strong reference count
UTILS u64 chiba_shared_strong_count(const chiba_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return 0;

  return atomic_load(&ptr->control->strong_count);
}

// Get weak reference count
UTILS u64 chiba_shared_weak_count(const chiba_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return 0;

  return atomic_load(&ptr->control->weak_count);
}

// Check if shared pointer is null
UTILS bool chiba_shared_is_null(const chiba_shared_ptr *ptr) {
  return !ptr || !ptr->control || atomic_load(&ptr->control->strong_count) == 0;
}

// Check if this is the unique owner (strong_count == 1)
UTILS bool chiba_shared_is_unique(const chiba_shared_ptr *ptr) {
  if (!ptr || !ptr->control)
    return false;

  return atomic_load(&ptr->control->strong_count) == 1;
}

// Swap two shared pointers
UTILS void chiba_shared_swap(chiba_shared_ptr *a, chiba_shared_ptr *b) {
  if (!a || !b)
    return;

  chiba_control_block *temp = a->control;
  a->control = b->control;
  b->control = temp;
}

#define chiba_shared_ptr_var(type)                                             \
  __attribute__((cleanup(chiba_shared_drop))) chiba_shared_ptr

#define chiba_shared_ptr_param(type) chiba_shared_ptr