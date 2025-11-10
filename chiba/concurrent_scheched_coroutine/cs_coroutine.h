#pragma once
#include "../basic_memory.h"
#include "../concurrency/aatree.h"
#include "../concurrency/array_queue.h"
#include "../concurrency/dequeue.h"
#include "../scheched_coroutine/scheched_coroutine.h"

typedef i64 chiba_csco_id_t;
typedef i64 chiba_csco_ts_t;
typedef void (*chiba_csco_entry_t)(anyptr ctx);

i32 chiba_csco_start(chiba_csco_entry_t e, anyptr arg);
i32 chiba_csco_yield(void);
i32 chiba_csco_sleep(chiba_csco_ts_t nanosecs);
i32 chiba_csco_sleep_dl(chiba_csco_ts_t deadline);
i32 chiba_csco_join(chiba_csco_id_t id);
i32 chiba_csco_join_dl(chiba_csco_id_t id, chiba_csco_ts_t deadline);
i32 chiba_csco_suspend(void);
i32 chiba_csco_suspend_dl(chiba_csco_ts_t deadline);
i32 chiba_csco_resume(chiba_csco_id_t id);
void chiba_csco_exit(void);

chiba_csco_id_t chiba_csco_getid(void);
chiba_csco_id_t chiba_csco_lastid(void);
chiba_csco_id_t chiba_csco_starterid(void);

#if !defined(__EMSCRIPTEN__)
#define SCO_MULTITHREAD_WORKERS
#endif
