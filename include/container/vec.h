#pragma once
#include "../utils/chiba_utils.h"

#define DEFINE_VEC(type)                                                       \
  C8ReflStruct(Vec_##type, (size, u64), (cap, u64), (data, u8 *),              \
               (allocator, MEMORY_ALLOCATOR_HEADER *));

#define C8Vec(type) CHIBA_Vec_##type

DEFINE_VEC(u8);

PUBLIC CHIBA_Vec_u8 New_CHIBA_Vec_u8(MEMORY_ALLOCATOR_HEADER *allocator) {
  return _MK_CHIBA_Vec_u8((CHIBA_Vec_u8){
      .size = 0,
      .cap = 64,
      .data = (u8 *)allocator->alloc(64 * sizeof(u8)),
      .allocator = allocator,
  });
}

PUBLIC void Delete_CHIBA_Vec_u8(CHIBA_Vec_u8 *vec) {
  if (vec) {
    if (vec->data) {
      vec->allocator->free(vec->data);
    }
  }
}
#define NEW_VEC(type, allocator) New_CHIBA_Vec_##type(allocator)

typedef CHIBA_Vec_u8 CHIBA_String;

bool resize_CHIBA_Vec_u8(CHIBA_Vec_u8 *vec, u64 new_cap) {
  u8 *new_data = (u8 *)vec->allocator->realloc(vec->data, new_cap * sizeof(u8));
  if (new_data == NULL) {
    return false;
  }
  vec->data = new_data;
  vec->cap = new_cap;
  return true;
}

bool push_CHIBA_Vec_u8(CHIBA_Vec_u8 *vec, u8 value) {
  if (vec->size >= vec->cap) {
    if (!resize_CHIBA_Vec_u8(vec, vec->cap * 2)) {
      return false;
    }
  }
  vec->data[vec->size++] = value;
  return true;
}

bool append_CHIBA_Vec_u8(CHIBA_Vec_u8 *vec, u8 *data, u64 len) {
  while (vec->size + len > vec->cap) {
    if (!resize_CHIBA_Vec_u8(vec, vec->cap * 2)) {
      return false;
    }
  }
  memcpy(&vec->data[vec->size], data, len * sizeof(u8));
  vec->size += len;
  return true;
}

// _resize
// push
// append
// pop
// push_front
// pop_front
// get
// set
// insert
// remove
// into_iter

void f() {
  CHIBA_Vec_u8 vec DEFER(Delete_CHIBA_Vec_u8) =
      NEW_VEC(u8, &default_memory_allocator);
}