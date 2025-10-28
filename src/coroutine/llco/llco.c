// https://github.com/tidwall/llco
//
// Copyright (c) 2024 Joshua J Baker.
// This software is available as a choice of Public Domain or MIT-0.

#ifdef _FORTIFY_SOURCE
#define LLCO_FORTIFY_SOURCE _FORTIFY_SOURCE
// Disable __longjmp_chk validation so that we can jump between stacks.
#pragma push_macro("_FORTIFY_SOURCE")
#undef _FORTIFY_SOURCE
#include <setjmp.h>
#define _FORTIFY_SOURCE LLCO_FORTIFY_SOURCE
#undef LLCO_FORTIFY_SOURCE
#pragma pop_macro("_FORTIFY_SOURCE")
#endif

#ifndef LLCO_STATIC
#include "llco.h"
#else
#include <stdbool.h>
#include <stddef.h>
#define LLCO_MINSTACKSIZE 16384
#define LLCO_EXTERN static
struct llco_desc {
  void *stack;
  size_t stack_size;
  void (*entry)(void *udata);
  void (*cleanup)(void *stack, size_t stack_size, void *udata);
  void *udata;
};
struct llco_symbol {
  void *cfa;
  void *ip;
  const char *fname;
  void *fbase;
  const char *sname;
  void *saddr;
};
#endif

#include <stdlib.h>

#ifdef LLCO_VALGRIND
#include <valgrind/valgrind.h>
#endif

#ifndef LLCO_EXTERN
#define LLCO_EXTERN
#endif

#if defined(__GNUC__)
#ifdef noinline
#define LLCO_NOINLINE noinline
#else
#define LLCO_NOINLINE __attribute__((noinline))
#endif
#ifdef noreturn
#define LLCO_NORETURN noreturn
#else
#define LLCO_NORETURN __attribute__((noreturn))
#endif
#else
#define LLCO_NOINLINE
#define LLCO_NORETURN
#endif

#if defined(_MSC_VER)
#define __thread __declspec(thread)
#endif

static void llco_entry(void *arg);

LLCO_NORETURN
static void llco_exit(void) { _Exit(0); }

#ifdef LLCO_ASM
#error LLCO_ASM must not be defined
#endif

#if defined(__COSMOCC__) && !defined(LLCO_NOASM)
// Cosmopolitan has issues with asm code
#define LLCO_NOASM
#endif

// Passing the entry function into assembly requires casting the function
// pointer to an object pointer, which is forbidden in the ISO C spec but
// allowed in posix. Ignore the warning attributed to this  requirement when
// the -pedantic compiler flag is provide.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

////////////////////////////////////////////////////////////////////////////////
// Below is various assembly code adapted from the Lua Coco [MIT] and Minicoro
// [MIT-0] projects by Mike Pall and Eduardo Bart respectively.
////////////////////////////////////////////////////////////////////////////////

/*
Lua Coco (coco.luajit.org)
Copyright (C) 2004-2016 Mike Pall. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "llco_asm.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

////////////////////////////////////////////////////////////////////////////////
// ASM with stackjmp activated
////////////////////////////////////////////////////////////////////////////////
#if defined(LLCO_READY) && defined(LLCO_STACKJMP)
LLCO_NOINLINE LLCO_NORETURN static void
llco_stackjmp(void *stack, size_t stack_size, void (*entry)(void *arg)) {
  struct llco_asmctx ctx = {0};
  llco_asmctx_make(&ctx, stack, stack_size, 0);
  struct llco_asmctx ctx0 = {0};
  _llco_asm_switch(&ctx0, &ctx);
  llco_exit();
}
#endif

////////////////////////////////////////////////////////////////////////////////
// Webassembly Fibers
////////////////////////////////////////////////////////////////////////////////
#if defined(__EMSCRIPTEN__) && !defined(LLCO_READY)

#endif

////////////////////////////////////////////////////////////////////////////////
// Ucontext
////////////////////////////////////////////////////////////////////////////////
#if !defined(LLCO_READY)

#endif // Ucontext

#if defined(LLCO_STACKJMP)
#include <setjmp.h>
#ifdef _WIN32
// For reasons outside of my understanding, Windows does not allow for jumping
// between stacks using the setjmp/longjmp mechanism.
#error Windows stackjmp not supported
#endif
#endif

////////////////////////////////////////////////////////////////////////////////
// llco switching code
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>

struct llco {
  struct llco_desc desc;
#if defined(LLCO_STACKJMP)
  jmp_buf buf;
#elif defined(LLCO_ASM)
  struct llco_asmctx ctx;
#elif defined(LLCO_WASM)
  emscripten_fiber_t fiber;
#endif
#ifdef LLCO_VALGRIND
  int valgrind_stack_id;
#endif
#if defined(__GNUC__)
  void *uw_stop_ip; // record of the last unwind ip.
#endif
};

#ifdef LLCO_VALGRIND
static __thread unsigned int llco_valgrind_stack_id = 0;
static __thread unsigned int llco_cleanup_valgrind_stack_id = 0;
#endif

static __thread struct llco llco_thread = {0};
static __thread struct llco *llco_cur = NULL;
static __thread struct llco_desc llco_desc;
static __thread volatile bool llco_cleanup_needed = false;
static __thread volatile struct llco_desc llco_cleanup_desc;
static __thread volatile bool llco_cleanup_active = false;

#define llco_cleanup_guard()                                                   \
  {                                                                            \
    if (llco_cleanup_active) {                                                 \
      fprintf(stderr, "%s not available during cleanup\n", __func__);          \
      abort();                                                                 \
    }                                                                          \
  }

static void llco_cleanup_last(void) {
  if (llco_cleanup_needed) {
    if (llco_cleanup_desc.cleanup) {
      llco_cleanup_active = true;
#ifdef LLCO_VALGRIND
      VALGRIND_STACK_DEREGISTER(llco_cleanup_valgrind_stack_id);
#endif
      llco_cleanup_desc.cleanup(llco_cleanup_desc.stack,
                                llco_cleanup_desc.stack_size,
                                llco_cleanup_desc.udata);
      llco_cleanup_active = false;
    }
    llco_cleanup_needed = false;
  }
}

LLCO_NOINLINE
static void llco_entry_wrap(void *arg) {
  llco_cleanup_last();
#if defined(LLCO_WASM)
  llco_cur = arg;
  llco_cur->desc = llco_desc;
#else
  (void)arg;
  struct llco self = {.desc = llco_desc};
  llco_cur = &self;
#endif
#ifdef LLCO_VALGRIND
  llco_cur->valgrind_stack_id = llco_valgrind_stack_id;
#endif
#if defined(__GNUC__) && !defined(__EMSCRIPTEN__)
  llco_cur->uw_stop_ip = __builtin_return_address(0);
#endif
  llco_cur->desc.entry(llco_cur->desc.udata);
}

LLCO_NOINLINE LLCO_NORETURN static void llco_entry(void *arg) {
  llco_entry_wrap(arg);
  llco_exit();
}

LLCO_NOINLINE
static void llco_switch1(struct llco *from, struct llco *to, void *stack,
                         size_t stack_size) {
#ifdef LLCO_VALGRIND
  llco_valgrind_stack_id = VALGRIND_STACK_REGISTER(stack, stack + stack_size);
#endif
#if defined(LLCO_STACKJMP)
  if (to) {
    if (!_setjmp(from->buf)) {
      _longjmp(to->buf, 1);
    }
  } else {
    if (!_setjmp(from->buf)) {
      llco_stackjmp(stack, stack_size, llco_entry);
    }
  }
#elif defined(LLCO_ASM)
  if (to) {
    _llco_asm_switch(&from->ctx, &to->ctx);
  } else {
    struct llco_asmctx ctx = {0};
    llco_asmctx_make(&ctx, stack, stack_size, 0);
    _llco_asm_switch(&from->ctx, &ctx);
  }
#elif defined(LLCO_WASM)
  if (to) {
    emscripten_fiber_swap(&from->fiber, &to->fiber);
  } else {
    if (from == &llco_thread) {
      emscripten_fiber_init_from_current_context(&from->fiber, llco_main_stack,
                                                 LLCO_ASYNCIFY_STACK_SIZE);
    }
    stack_size -= LLCO_ASYNCIFY_STACK_SIZE;
    char *astack = ((char *)stack) + stack_size;
    size_t astack_size = LLCO_ASYNCIFY_STACK_SIZE - sizeof(struct llco);
    struct llco *self = (void *)(astack + astack_size);
    memset(self, 0, sizeof(struct llco));
    emscripten_fiber_init(&self->fiber, llco_entry, self, stack, stack_size,
                          astack, astack_size);
    emscripten_fiber_swap(&from->fiber, &self->fiber);
  }
#elif defined(LLCO_WINDOWS)
  // Unsupported
#endif
}

static void llco_switch0(struct llco_desc *desc, struct llco *co, bool final) {
  struct llco *from = llco_cur ? llco_cur : &llco_thread;
  struct llco *to = desc ? NULL : co ? co : &llco_thread;
  if (from != to) {
    if (final) {
      llco_cleanup_needed = true;
      llco_cleanup_desc = from->desc;
#ifdef LLCO_VALGRIND
      llco_cleanup_valgrind_stack_id = from->valgrind_stack_id;
#endif
    }
    if (desc) {
      llco_desc = *desc;
      llco_switch1(from, 0, desc->stack, desc->stack_size);
    } else {
      llco_cur = to;
      llco_switch1(from, to, 0, 0);
    }
    llco_cleanup_last();
  }
}

////////////////////////////////////////////////////////////////////////////////
// Exported methods
////////////////////////////////////////////////////////////////////////////////

// Start a new coroutine.
LLCO_EXTERN
void llco_start(struct llco_desc *desc, bool final) {
  if (!desc || desc->stack_size < LLCO_MINSTACKSIZE) {
    fprintf(stderr, "stack too small\n");
    abort();
  }
  llco_cleanup_guard();
  llco_switch0(desc, 0, final);
}

// Switch to another coroutine.
LLCO_EXTERN
void llco_switch(struct llco *co, bool final) {
#if defined(LLCO_ASM)
  // fast track context switch. Saves a few nanoseconds by checking the
  // exception condition first.
  if (!llco_cleanup_active && llco_cur && co && llco_cur != co && !final) {
    struct llco *from = llco_cur;
    llco_cur = co;
    _llco_asm_switch(&from->ctx, &co->ctx);
    llco_cleanup_last();
    return;
  }
#endif
  llco_cleanup_guard();
  llco_switch0(0, co, final);
}

// Return the current coroutine or NULL if not currently running in a
// coroutine.
LLCO_EXTERN
struct llco *llco_current(void) {
  llco_cleanup_guard();
  return llco_cur == &llco_thread ? 0 : llco_cur;
}

// Returns a string that indicates which coroutine method is being used by
// the program. Such as "asm" or "ucontext", etc.
LLCO_EXTERN
const char *llco_method(void *caps) {
  (void)caps;
  return LLCO_METHOD
#ifdef LLCO_STACKJMP
      ",stackjmp"
#endif
      ;
}

#if defined(__GNUC__) && !defined(__EMSCRIPTEN__) && !defined(_WIN32) &&       \
    !defined(LLCO_NOUNWIND) && !defined(__COSMOCC__)

#include <dlfcn.h>
#include <string.h>
#include <unwind.h>

struct llco_dlinfo {
  const char *dli_fname; /* Pathname of shared object */
  void *dli_fbase;       /* Base address of shared object */
  const char *dli_sname; /* Name of nearest symbol */
  void *dli_saddr;       /* Address of nearest symbol */
};

#if defined(__linux__) && !defined(_GNU_SOURCE)
int dladdr(const void *, void *);
#endif

static void llco_getsymbol(struct _Unwind_Context *uwc,
                           struct llco_symbol *sym) {
  memset(sym, 0, sizeof(struct llco_symbol));
  sym->cfa = (void *)_Unwind_GetCFA(uwc);
  int ip_before; /* unused */
  sym->ip = (void *)_Unwind_GetIPInfo(uwc, &ip_before);
  struct llco_dlinfo dlinfo = {0};
  if (sym->ip && dladdr(sym->ip, (void *)&dlinfo)) {
    sym->fname = dlinfo.dli_fname;
    sym->fbase = dlinfo.dli_fbase;
    sym->sname = dlinfo.dli_sname;
    sym->saddr = dlinfo.dli_saddr;
  }
}

struct llco_unwind_context {
  void *udata;
  void *start_ip;
  bool started;
  int nsymbols;
  int nsymbols_actual;
  struct llco_symbol last;
  bool (*func)(struct llco_symbol *, void *);
  void *unwind_addr;
};

static _Unwind_Reason_Code llco_func(struct _Unwind_Context *uwc, void *ptr) {
  struct llco_unwind_context *ctx = ptr;

  struct llco *cur = llco_current();
  if (cur && !cur->uw_stop_ip) {
    return _URC_END_OF_STACK;
  }
  struct llco_symbol sym;
  llco_getsymbol(uwc, &sym);
  if (ctx->start_ip && !ctx->started && sym.ip != ctx->start_ip) {
    return _URC_NO_REASON;
  }
  ctx->started = true;
  if (!sym.ip || (cur && sym.ip == cur->uw_stop_ip)) {
    return _URC_END_OF_STACK;
  }
  ctx->nsymbols++;
  if (!cur) {
    ctx->nsymbols_actual++;
    if (ctx->func && !ctx->func(&sym, ctx->udata)) {
      return _URC_END_OF_STACK;
    }
  } else {
    if (ctx->nsymbols > 1) {
      ctx->nsymbols_actual++;
      if (ctx->func && !ctx->func(&ctx->last, ctx->udata)) {
        return _URC_END_OF_STACK;
      }
    }
    ctx->last = sym;
  }
  return _URC_NO_REASON;
}

LLCO_EXTERN
int llco_unwind(bool (*func)(struct llco_symbol *sym, void *udata),
                void *udata) {
  struct llco_unwind_context ctx = {
#if defined(__GNUC__) && !defined(__EMSCRIPTEN__)
      .start_ip = __builtin_return_address(0),
#endif
      .func = func,
      .udata = udata};
  _Unwind_Backtrace(llco_func, &ctx);
  return ctx.nsymbols_actual;
}

#else

LLCO_EXTERN
int llco_unwind(bool (*func)(struct llco_symbol *sym, void *udata),
                void *udata) {
  (void)func;
  (void)udata;
  /* Unsupported */
  return 0;
}

#endif
