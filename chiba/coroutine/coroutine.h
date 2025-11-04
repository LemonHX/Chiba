// Chiba Coroutine System - Public API
// Lightweight coroutine implementation for the Chiba runtime
// Adapted from llco (https://github.com/tidwall/llco)
// Copyright (c) 2024 Joshua J Baker.

#pragma once
#include "../basic_types.h"

// Coroutine descriptor for starting new coroutines
struct CHIBA_co_desc {
  anyptr stack;
  u64 stack_size;
  void (*entry)(anyptr ctx);
  void (*defer)(anyptr stack, u64 stack_size, anyptr ctx);
  anyptr ctx;
};

// Opaque coroutine handle
struct CHIBA_co;

// Public API

// Get the current coroutine (returns NULL if not in a coroutine)
PUBLIC struct CHIBA_co *CHIBA_co_current(void);

// Start a new coroutine
PUBLIC void CHIBA_co_start(struct CHIBA_co_desc *desc, bool final);

// Switch to another coroutine
PUBLIC void CHIBA_co_switch(struct CHIBA_co *co, bool final);

// Get the coroutine method name (e.g., "asm,aarch64", "ucontext")
PUBLIC cstr CHIBA_co_method(anyptr caps);
