#pragma once

////////////////////////////////////////////////////////////////////////////////
// Chiba Fiber Scheduler - Public API
////////////////////////////////////////////////////////////////////////////////

#include "../arc/arc.h" // ARC: chiba_shared_ptr
#include "../basic_types.h"
#include "../common_headers.h"
#include "../context/context.h"

// Forward declarations keep public API light-weight for users pulling headers
// only.
// Opaque fiber object managed via ARC (see chiba/arc/shared_memory.h).
typedef struct chiba_fiber chiba_fiber;
typedef struct chiba_fiber_scheduler chiba_fiber_scheduler;

////////////////////////////////////////////////////////////////////////////////
// Constants
////////////////////////////////////////////////////////////////////////////////

// Default stack size for fibers (internal policy; subject to change).
#define CHIBA_FIBER_DEFAULT_STACK_SIZE (64 * 1024)

// User supplied entry point executed within a fiber context.
typedef anyptr (*chiba_fiber_start_fn)(anyptr arg);

////////////////////////////////////////////////////////////////////////////////
// Scheduler lifecycle
////////////////////////////////////////////////////////////////////////////////

PUBLIC int
chiba_fiber_scheduler_create(struct chiba_fiber_scheduler **out_sched,
                             i32 num_workers);
PUBLIC void chiba_fiber_scheduler_destroy(struct chiba_fiber_scheduler *sched);
PUBLIC int chiba_fiber_scheduler_run(struct chiba_fiber_scheduler *sched);
PUBLIC void
chiba_fiber_scheduler_request_stop(struct chiba_fiber_scheduler *sched);

////////////////////////////////////////////////////////////////////////////////
// Fiber lifecycle
////////////////////////////////////////////////////////////////////////////////

// ARC-based fiber handles. Functions take/return chiba_shared_ptr as in
// future.h.
PUBLIC chiba_shared_ptr_param(chiba_fiber)
    chiba_fiber_create(struct chiba_fiber_scheduler *sched,
                       chiba_fiber_start_fn fn, anyptr arg);

PUBLIC chiba_shared_ptr_param(chiba_fiber)
    chiba_fiber_spawn(struct chiba_fiber_scheduler *sched,
                      chiba_fiber_start_fn fn, anyptr arg);

PUBLIC int chiba_fiber_detach(chiba_shared_ptr_param(chiba_fiber) fiber);
PUBLIC int chiba_fiber_join(chiba_shared_ptr_param(chiba_fiber) fiber,
                            anyptr *ret_ptr);
PUBLIC void chiba_fiber_exit(anyptr ret_value) __attribute__((noreturn));
PUBLIC void chiba_fiber_yield(void);
// Returns a handle (ARC cloned) to the current fiber, or null handle if none.
PUBLIC chiba_shared_ptr_param(chiba_fiber) chiba_fiber_current(void);
