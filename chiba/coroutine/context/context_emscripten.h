// Emscripten/WebAssembly coroutine context switching implementation

#pragma once
#include "../../basic_types.h"

#if defined(__EMSCRIPTEN__) && !defined(CHIBA_CO_READY)
#pragma message(                                                               \
    "INFO: Using Emscripten fibers for coroutine context switching, only support single thread")
#define CHIBA_CO_WASM
#define CHIBA_CO_READY
#define CHIBA_CO_METHOD "fibers,emscripten"

#include <emscripten/fiber.h>
#include <string.h>

#ifndef CHIBA_CO_ASYNCIFY_STACK_SIZE
#define CHIBA_CO_ASYNCIFY_STACK_SIZE 4096
#endif

PRIVATE THREAD_LOCAL i8 chiba_co_main_stack[CHIBA_CO_ASYNCIFY_STACK_SIZE];

#endif // __EMSCRIPTEN__
