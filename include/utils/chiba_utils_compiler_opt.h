#pragma once

#if defined(_MSC_VER) && !defined(__clang__)
#  define likely(x)       (x)
#  define unlikely(x)     (x)
#  define force_inline __forceinline
#  define no_inline __declspec(noinline)
#  define __maybe_unused
#  define __attribute__(x)
#  define __attribute(x)
#else
#  define likely(x)       __builtin_expect(!!(x), 1)
#  define unlikely(x)     __builtin_expect(!!(x), 0)
#  define force_inline inline __attribute__((always_inline))
#  define no_inline __attribute__((noinline))
#  define __maybe_unused __attribute__((unused))
#endif