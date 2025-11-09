#pragma once
#include "../basic_memory.h"
#include "../concurrency/array_queue.h"
#include "../utils/chiba_sem.h"

typedef struct chiba_thread_pool chiba_thread_pool;

chiba_thread_pool *chiba_thread_pool_new();
i32 chiba_thread_pool_add_work(chiba_thread_pool *, void (*function_p)(anyptr),
                               anyptr arg_p);
void chiba_thread_pool_wait(chiba_thread_pool *);
void chiba_thread_pool_pause(chiba_thread_pool *);
void chiba_thread_pool_resume(chiba_thread_pool *);
void chiba_thread_pool_drop(chiba_thread_pool *);
