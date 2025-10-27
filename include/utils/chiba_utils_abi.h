#pragma once

#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"

#ifdef __GNUC__
#define FORCE_STACK_USAGE_ANALYSIS __attribute__((noinline))
#define GET_STACK_PTR() __builtin_frame_address(0)
#elif defined(_MSC_VER)
#define FORCE_STACK_USAGE_ANALYSIS __declspec(noinline)
#define GET_STACK_PTR() _AddressOfReturnAddress()
#else
#define FORCE_STACK_USAGE_ANALYSIS
#define GET_STACK_PTR() ((void *)0)
#endif


#ifdef DEBUG
#define STACK_HEAD                                                             \
  static void *stack_head;                                                     \
  stack_head = GET_STACK_PTR();
#define STACK_CHECK                                                            \
  do {                                                                         \
    void *current_sp = GET_STACK_PTR();                                        \
    ptrdiff_t used_stack = (ptrdiff_t)current_sp - (ptrdiff_t)stack_head;      \
    printf("%sDebug: %s used stack: %td bytes%s\n", ANSI_GREEN, __func__,      \
           used_stack, ANSI_RESET);                                            \
    if (used_stack < 0)                                                        \
      used_stack = -used_stack;                                                \
    if (used_stack > API_MAX_STACK_USAGE) {                                    \
      fprintf(stderr,                                                          \
              "Error: %s stack overflow detected (used %td bytes, max %d "     \
              "bytes)\n",                                                      \
              __func__, used_stack, API_MAX_STACK_USAGE);                      \
      abort();                                                                 \
    }                                                                          \
  } while (0)
#else
#define STACK_HEAD
#define STACK_CHECK
#endif

typedef struct APIError {
  cstr msg;
  cstr file;
  u32 line;
} APIError;

typedef struct APIResult {
  bool ok;
  APIError err;
} APIResult;

#define API_TYPE(name) API_##name##_func

#define API(name, args_type, ret_type, body)                                   \
  struct __attribute__((aligned(8))) API_##name##_arg args_type;               \
  typedef APIResult (*API_##name##_func)(ret_type * return_var, void *ctx,     \
                                         void *self,                           \
                                         struct API_##name##_arg *args);       \
  PUBLIC FORCE_STACK_USAGE_ANALYSIS APIResult name(                            \
      ret_type *return_var, void *ctx, void *self,                             \
      struct API_##name##_arg *args) {                                         \
    STACK_HEAD                                                                 \
    body                                                                       \
  }

typedef APIResult (*API_FUNC_PTR)(void *return_var, void *ctx, void *self,
                                  void *args);

#define ARG(field) (args->field)
#define ARG_TY(name) struct API_##name##_arg
#define MKARG(name, body) ((struct API_##name##_arg)body)

#define RET(val)                                                               \
  do {                                                                         \
    STACK_CHECK;                                                               \
    *(return_var) = (val);                                                     \
    return (APIResult){.ok = true, .err = {0}};                                \
  } while (0)

#define RETRES(val)                                                            \
  do {                                                                         \
    STACK_CHECK;                                                               \
    return val;                                                                \
  } while (0)

#define THROW(errmsg)                                                          \
  do {                                                                         \
    STACK_CHECK;                                                               \
    return (APIResult){                                                        \
        .ok = false,                                                           \
        .err = {.msg = errmsg, .file = __FILE__, .line = __LINE__}};           \
  } while (0)
