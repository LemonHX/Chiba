#pragma once
#include "basic_types.h"

extern anyptr CHIBA_INTERNAL_malloc_func;
extern anyptr CHIBA_INTERNAL_aligned_alloc_func;
extern anyptr CHIBA_INTERNAL_realloc_func;
extern anyptr CHIBA_INTERNAL_free_func;

UTILS void CHIBA_INTERNAL_set_alloc_funcs(
    anyptr (*malloc_func)(size_t), anyptr (*aligned_alloc_func)(size_t, size_t),
    anyptr (*realloc_func)(anyptr, size_t), void (*free_func)(anyptr)) {
  CHIBA_INTERNAL_malloc_func = (anyptr)malloc_func;
  CHIBA_INTERNAL_aligned_alloc_func = (anyptr)aligned_alloc_func;
  CHIBA_INTERNAL_realloc_func = (anyptr)realloc_func;
  CHIBA_INTERNAL_free_func = (anyptr)free_func;
}

UTILS anyptr CHIBA_INTERNAL_malloc(size_t size) {
  return ((anyptr(*)(size_t))CHIBA_INTERNAL_malloc_func)(size);
}

UTILS anyptr CHIBA_INTERNAL_malloc_aligned(size_t alignment, size_t size) {
  return ((anyptr(*)(size_t, size_t))CHIBA_INTERNAL_aligned_alloc_func)(
      alignment, size);
}

UTILS anyptr CHIBA_INTERNAL_realloc(anyptr ptr, size_t size) {
  return ((anyptr(*)(anyptr, size_t))CHIBA_INTERNAL_realloc_func)(ptr, size);
}

UTILS void CHIBA_INTERNAL_free(anyptr ptr) {
  if (unlikely(!ptr))
    return;
  ((void (*)(anyptr))CHIBA_INTERNAL_free_func)(ptr);
}

#define CHIBA_PANIC(fmt, ...)                                                  \
  do {                                                                         \
    fprintf(stderr, "CHIBA PANIC at %s:%d in %s(): " fmt "\n", __FILE__,       \
            __LINE__, __func__, ##__VA_ARGS__);                                \
    abort();                                                                   \
  } while (0)

#include "utils/murmurhash3.h"