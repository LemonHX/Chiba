#pragma once

// disable testing/logging/etc ANSI colors if NO_COLOR is defined
#ifndef NO_COLOR
#define ANSI_BOLD "\033[1m"
#define ANSI_RESET "\033[0m"
#define ANSI_RED "\033[31m"
#define ANSI_GREEN "\033[32m"
#define ANSI_BLUE "\033[34m"
#define ANSI_CYAN "\033[36m"
#else
#define ANSI_BOLD ""
#define ANSI_RESET ""
#define ANSI_RED ""
#define ANSI_GREEN ""
#define ANSI_BLUE ""
#define ANSI_CYAN ""
#endif

#ifdef NDEBUG
#define RELEASE
#else
#define DEBUG
#endif

#define API_MAX_STACK_USAGE 1024 * 1024 /* 1MB */

// please change if you are using risv64 and using 39 bits virtual address space
#define PTR_BIT 48

// Passing the entry function into assembly requires casting the function
// pointer to an object pointer, which is forbidden in the ISO C spec but
// allowed in posix. Ignore the warning attributed to this  requirement when
// the -pedantic compiler flag is provide.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

// Minimum stack size for coroutines (16 KB)
#define CHIBA_CO_MINSTACKSIZE 16384
// #define CHIBA_CO_NOASM // Disable assembly implementations

#define COROUTINE_SINGLECORE_IMPL // enable single-core coroutine implementation

// binary or lib

#define CHIBA_BINARY
// #define CHIBA_LIB