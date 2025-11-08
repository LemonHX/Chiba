#pragma once

#include "../coroutine/coroutine.h"

typedef struct chiba_sco_desc {
  anyptr stack;
  i64 stack_size;
  void (*entry)(anyptr udata);
  void (*cleanup)(anyptr stack, i64 stack_size, anyptr udata);
  anyptr udata;
} chiba_sco_desc;

typedef i64 chiba_sco_id_t;

// Starts a new coroutine with the provided description.
void chiba_sco_start(chiba_sco_desc *desc);

// Causes the calling coroutine to relinquish the CPU.
// This operation should be called from a coroutine, otherwise it does nothing.
void chiba_sco_yield(void);

// Get the identifier for the current coroutine.
// This operation should be called from a coroutine, otherwise it returns zero.
chiba_sco_id_t chiba_sco_id(void);

// Pause the current coroutine.
// This operation should be called from a coroutine, otherwise it does nothing.
void chiba_sco_pause(void);

// Resume a paused coroutine.
// If the id is invalid or does not belong to a paused coroutine then this
// operation does nothing.
// Calling chiba_sco_resume(0) is a special case that continues a runloop. See
// the README for an example.
void chiba_sco_resume(chiba_sco_id_t id);

// Returns true if there are any coroutines running, yielding, or paused.
bool chiba_sco_active(void);

// Detach a coroutine from a thread.
// This allows for moving coroutines between threads.
// The coroutine must be currently paused before it can be detached, thus this
// operation cannot be called from the coroutine belonging to the provided id.
// If the id is invalid or does not belong to a paused coroutine then this
// operation does nothing.
void chiba_sco_detach(chiba_sco_id_t id);

// Attach a coroutine to a thread.
// This allows for moving coroutines between threads.
// If the id is invalid or does not belong to a detached coroutine then this
// operation does nothing.
// Once attached, the coroutine will be paused.
void chiba_sco_attach(chiba_sco_id_t id);

// Exit a coroutine early.
// This _will not_ exit the program. Rather, it's for ending the current
// coroutine and quickly switching to the thread's runloop before any other
// scheduled (yielded) coroutines run.
// This operation should be called from a coroutine, otherwise it does nothing.
void chiba_sco_exit(void);

// Returns the user data of the currently running coroutine.
anyptr chiba_sco_udata(void);

// General information and statistics
i64 chiba_sco_info_scheduled(void);
i64 chiba_sco_info_running(void);
i64 chiba_sco_info_paused(void);
i64 chiba_sco_info_detached(void);
const char *chiba_sco_info_method(void);
