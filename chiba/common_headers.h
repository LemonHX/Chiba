#pragma once

#include "compiler_config.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// malloc
#if defined(_MSC_VER)
#include <malloc.h>
#include <winsock2.h>
#define alloca _alloca
#define ssize_t ptrdiff_t
#endif
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__ANDROID__) || defined(__CYGWIN__) ||     \
    defined(__GLIBC__)
#include <malloc.h>
#elif defined(__FreeBSD__)
#include <malloc_np.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

// threading
#if !defined(_WIN32) && !defined(EMSCRIPTEN) && !defined(__wasi__)
#include <errno.h>
#include <pthread.h>
#endif
#if !defined(_WIN32)
#include <limits.h>
#include <unistd.h>
#endif
#if defined(_WIN32)
#include "../third_party/pthread-win32/pthread.h"
#endif
#include <stdatomic.h>
#define CHIBA_IOCPU_WORKER_POOLTHREAD_NAME CHIBA_IOCPU_POOL

// arg
#include <stdarg.h>

// opt
#if defined(_MSC_VER) && !defined(__clang__)
#define likely(x) (x)
#define unlikely(x) (x)
#define force_inline __forceinline
#define no_inline __declspec(noinline)
#define __maybe_unused
#define __attribute__(x)
#define __attribute(x)
#else
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define force_inline inline __attribute__((always_inline))
#define no_inline __attribute__((noinline))
#define __maybe_unused __attribute__((unused))
#endif

// visibility and internal namespacing
#if defined(_WIN32) || defined(_WIN64)
#define PUBLIC __declspec(dllexport)
#else
#define PUBLIC __attribute__((visibility("default")))
#endif
#define PRIVATE static
#define THREAD_LOCAL __thread
#define NOINLINE __attribute__((noinline))
#define NAKED __attribute__((naked))

#define UTILS static inline

#define BEFORE_START __attribute__((constructor)) static
#define AFTER_END __attribute__((destructor)) static

// internal memory allocation functions
#define DEFER(fn) __attribute__((cleanup(fn)))

PRIVATE void *CHIBA_INTERNAL_malloc_func = (void *)malloc;
PRIVATE void *CHIBA_INTERNAL_aligned_alloc_func = (void *)aligned_alloc;
PRIVATE void *CHIBA_INTERNAL_realloc_func = (void *)realloc;
PRIVATE void *CHIBA_INTERNAL_free_func = (void *)free;

UTILS void CHIBA_INTERNAL_set_alloc_funcs(
    void *(*malloc_func)(size_t), void *(*aligned_alloc_func)(size_t, size_t),
    void *(*realloc_func)(void *, size_t), void (*free_func)(void *)) {
  CHIBA_INTERNAL_malloc_func = (void *)malloc_func;
  CHIBA_INTERNAL_aligned_alloc_func = (void *)aligned_alloc_func;
  CHIBA_INTERNAL_realloc_func = (void *)realloc_func;
  CHIBA_INTERNAL_free_func = (void *)free_func;
}

UTILS void *CHIBA_INTERNAL_malloc(size_t size) {
  return ((void *(*)(size_t))CHIBA_INTERNAL_malloc_func)(size);
}

UTILS void *CHIBA_INTERNAL_malloc_aligned(size_t alignment, size_t size) {
  return ((void *(*)(size_t, size_t))CHIBA_INTERNAL_aligned_alloc_func)(
      alignment, size);
}

UTILS void *CHIBA_INTERNAL_realloc(void *ptr, size_t size) {
  return ((void *(*)(void *, size_t))CHIBA_INTERNAL_realloc_func)(ptr, size);
}

UTILS void CHIBA_INTERNAL_free(void *ptr) {
  ((void (*)(void *))CHIBA_INTERNAL_free_func)(ptr);
}
