#pragma once

#include "../../compiler_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// malloc
#if defined(_MSC_VER)
#include <winsock2.h>
#include <malloc.h>
#define alloca _alloca
#define ssize_t ptrdiff_t
#endif
#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__linux__) || defined(__ANDROID__) || defined(__CYGWIN__) || defined(__GLIBC__)
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

// arg
#include <stdarg.h>


