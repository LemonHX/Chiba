#pragma once

#include "chiba_utils_basic_types.h"

#ifdef __GNUC__
    #define FORCE_STACK_USAGE_ANALYSIS __attribute__((noinline))
    #define GET_STACK_PTR() __builtin_frame_address(0)
#elif defined(_MSC_VER)
    #define FORCE_STACK_USAGE_ANALYSIS __declspec(noinline)
    #define GET_STACK_PTR() _AddressOfReturnAddress()
#else
    #define FORCE_STACK_USAGE_ANALYSIS
    #define GET_STACK_PTR() ((void*)0)
#endif

#define C8API_MAX_STACK_USAGE 1024 * 1024 /* 1MB */

#ifdef DEBUG
    #define STACK_HEAD static void* stack_head; stack_head = GET_STACK_PTR();
    #define STACK_CHECK \
        do { \
            void* current_sp = GET_STACK_PTR(); \
            ptrdiff_t used_stack = (ptrdiff_t)current_sp - (ptrdiff_t)stack_head; \
            printf("%sDebug: %s used stack: %td bytes%s\n", ANSI_GREEN, __func__, used_stack, ANSI_RESET); \
            if (used_stack < 0) used_stack = -used_stack; \
            if (used_stack > C8API_MAX_STACK_USAGE) { \
                fprintf(stderr, "Error: %s stack overflow detected (used %td bytes, max %d bytes)\n", \
                        __func__, used_stack, C8API_MAX_STACK_USAGE); \
                abort(); \
            } \
        } while (0)
#else
    #define STACK_HEAD
    #define STACK_CHECK
#endif

#define C8API_TYPE(name) C8_##name##_func

#define C8API(name, args_type, ret_type, body) \
struct C8_##name##_arg args_type; \
typedef void (*C8_##name##_func)(ret_type* return_var, void* ctx, struct C8_##name##_arg* args); \
PUBLIC FORCE_STACK_USAGE_ANALYSIS void name(ret_type* return_var, void* ctx, struct C8_##name##_arg* args) { \
    STACK_HEAD \
    body \
}


#define ARG(field) (args->field)
#define C8ARG(name) struct C8_##name##_arg
#define MKARG(name, body) ((struct C8_##name##_arg)body)

#define RET(val) \
*(return_var) = (val); \
STACK_CHECK; \
return


