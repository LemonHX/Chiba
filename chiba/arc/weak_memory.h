#pragma once

#include "shared_memory.h"

// Weak pointer
typedef struct {
  CHIBA_control_block *control;
} CHIBA_weak_ptr;

////////////////////////////////////////////////////////////////////////////////
// Weak Pointer API
////////////////////////////////////////////////////////////////////////////////

// Create a weak pointer from shared pointer
UTILS CHIBA_weak_ptr CHIBA_weak_from_shared(const CHIBA_shared_ptr *ptr) {
  CHIBA_weak_ptr weak = {.control = NULL};

  if (!ptr || !ptr->control)
    return weak;

  CHIBA_control_block_inc_weak(ptr->control);
  weak.control = ptr->control;

  return weak;
}

// Clone a weak pointer
UTILS CHIBA_weak_ptr CHIBA_weak_clone(const CHIBA_weak_ptr *ptr) {
  CHIBA_weak_ptr new_weak = {.control = NULL};

  if (!ptr || !ptr->control)
    return new_weak;

  CHIBA_control_block_inc_weak(ptr->control);
  new_weak.control = ptr->control;

  return new_weak;
}

// Drop a weak pointer
UTILS void CHIBA_weak_drop(CHIBA_weak_ptr *ptr) {
  if (!ptr || !ptr->control)
    return;

  CHIBA_control_block_dec_weak(ptr->control);
  ptr->control = NULL;
}

// Upgrade weak pointer to shared pointer (returns null if expired)
UTILS CHIBA_shared_ptr CHIBA_weak_upgrade(const CHIBA_weak_ptr *ptr) {
  CHIBA_shared_ptr shared = {.control = NULL};

  if (!ptr || !ptr->control)
    return shared;

  // Try to increment strong count
  if (CHIBA_control_block_inc_strong(ptr->control)) {
    shared.control = ptr->control;
  }

  return shared;
}

// Check if weak pointer is expired (object destroyed)
UTILS bool CHIBA_weak_is_expired(const CHIBA_weak_ptr *ptr) {
  if (!ptr || !ptr->control)
    return true;

  return atomic_load(&ptr->control->strong_count) == 0;
}

#define chiba_weak_ptr __attribute__((cleanup(CHIBA_weak_drop))) CHIBA_weak_ptr
