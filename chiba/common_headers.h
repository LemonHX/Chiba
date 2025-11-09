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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// threading
#if !defined(_WIN32)
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#elif defined(_WIN32)
#include "../third_party/pthread-win32/pthread.h"
#endif

#include <stdatomic.h>
#define CHIBA_IOCPU_POOL
#include <stdalign.h>

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

#define UTILS static __attribute__((always_inline)) inline

#define BEFORE_START __attribute__((constructor)) static
#define AFTER_END __attribute__((destructor)) static

// 获取系统 CPU 核心数
UTILS int get_cpu_count(void) {
#if defined(_WIN32) || defined(_WIN64)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  return sysinfo.dwNumberOfProcessors < 1 ? 1 : sysinfo.dwNumberOfProcessors;
#elif !defined(__EMSCRIPTEN__)
  long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  return (nprocs > 0) ? (int)nprocs : 1;
#else
  return 8; // 假设 8 核
#endif
}

UTILS void CHIBA_INTERNAL_usleep(long long microseconds) {
#if defined(_WIN32) || defined(_WIN64)
  Sleep((DWORD)(microseconds / 1000));
#elif !defined(__EMSCRIPTEN__)
  struct timespec req, rem;
  req.tv_sec = microseconds / 1000000;
  req.tv_nsec = (microseconds % 1000000) * 1000;
  nanosleep(&req, &rem);
#else
  emscripten_sleep((int)(microseconds / 1000));
#endif
}

UTILS unsigned long long CHIBA_HASH_mix13(unsigned long long key) {
  key ^= (key >> 30);
  key *= 0xbf58476d1ce4e5b9ULL;
  key ^= (key >> 27);
  key *= 0x94d049bb133111ebULL;
  key ^= (key >> 31);
  return key;
}
