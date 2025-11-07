#pragma once

#include "shared_memory.h"

// Weak pointer
typedef struct {
  chiba_control_block *control;
} chiba_weak_ptr;

////////////////////////////////////////////////////////////////////////////////
// Weak Pointer API
////////////////////////////////////////////////////////////////////////////////

// Create a weak pointer from shared pointer
UTILS chiba_weak_ptr chiba_weak_from_shared(const chiba_shared_ptr *ptr) {
  chiba_weak_ptr weak = {.control = NULL};

  if (!ptr || !ptr->control)
    return weak;

  chiba_control_block_inc_weak(ptr->control);
  weak.control = ptr->control;

  return weak;
}

// Clone a weak pointer
UTILS chiba_weak_ptr chiba_weak_clone(const chiba_weak_ptr *ptr) {
  chiba_weak_ptr new_weak = {.control = NULL};

  if (!ptr || !ptr->control)
    return new_weak;

  chiba_control_block_inc_weak(ptr->control);
  new_weak.control = ptr->control;

  return new_weak;
}

// Drop a weak pointer
UTILS void chiba_weak_drop(chiba_weak_ptr *ptr) {
  if (!ptr || !ptr->control)
    return;

  chiba_control_block_dec_weak(ptr->control);
  ptr->control = NULL;
}

// Upgrade weak pointer to shared pointer (returns null if expired)
UTILS chiba_shared_ptr chiba_weak_upgrade(const chiba_weak_ptr *ptr) {
  chiba_shared_ptr shared = {.control = NULL};

  if (!ptr || !ptr->control)
    return shared;

  // Try to increment strong count
  if (chiba_control_block_inc_strong(ptr->control)) {
    shared.control = ptr->control;
  }

  return shared;
}

// Check if weak pointer is expired (object destroyed)
UTILS bool chiba_weak_is_expired(const chiba_weak_ptr *ptr) {
  if (!ptr || !ptr->control)
    return true;

  return atomic_load(&ptr->control->strong_count) == 0;
}

#define chiba_weak_ptr_var(type)                                               \
  __attribute__((cleanup(chiba_weak_drop))) chiba_weak_ptr

#define chiba_weak_ptr_param(type) chiba_weak_ptr