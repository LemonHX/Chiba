// Chiba Coroutine System - Public API
// Lightweight coroutine implementation for the Chiba runtime

#pragma once
#include "../basic_types.h"

// Coroutine descriptor for starting new coroutines
typedef struct chiba_co_desc {
  anyptr stack;
  u64 stack_size;
  void (*entry)(anyptr ctx);
  void (*defer)(anyptr stack, u64 stack_size, anyptr ctx);
  anyptr ctx;
} chiba_co_desc;

// Opaque coroutine handle
typedef struct chiba_co chiba_co;

// Public API

// Get the current coroutine (returns NULL if not in a coroutine)
PUBLIC chiba_co *chiba_co_current(void);

// Start a new coroutine
PUBLIC void chiba_co_start(chiba_co_desc *desc, bool final);

// Switch to another coroutine
PUBLIC void chiba_co_switch(chiba_co *co, bool final);

// Get the coroutine method name (e.g., "asm,aarch64", "ucontext")
PUBLIC cstr chiba_co_method(anyptr caps);
