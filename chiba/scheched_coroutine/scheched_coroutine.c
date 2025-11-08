#include "scheched_coroutine.h"
#include "../concurrency/aatree.h"
#include "../utils/backoff.h"

typedef struct chiba_sco_link {
  struct chiba_sco *prev;
  struct chiba_sco *next;
} chiba_sco_link;

typedef struct chiba_sco chiba_sco;

typedef struct chiba_sco {
  union {
    // Linked list
    struct {
      chiba_sco *prev;
      chiba_sco *next;
    };
    // Binary tree (AA-tree)
    struct {
      AAT_FIELDS(chiba_sco, left, right, level)
    };
  };
  chiba_sco_id_t id;
  void *ctx;
  chiba_co *co;
} chiba_sco;

UTILS i32 sco_compare(chiba_sco *a, chiba_sco *b) {
  return a->id < b->id ? -1 : a->id > b->id;
}

AAT_DEF(UTILS, chiba_sco_aat, chiba_sco)
AAT_IMPL(chiba_sco_aat, chiba_sco, left, right, level, sco_compare)

typedef struct chiba_sco_map {
  chiba_sco *roots[CHIBA_SCO_NSHARDS];
  i32 count;
} chiba_sco_map;

#define scp_map_getaat(sco)                                                    \
  (&map->roots[CHIBA_HASH_mix13((sco)->id) & (CHIBA_SCO_NSHARDS - 1)])

UTILS chiba_sco *chiba_sco_map_insert(chiba_sco_map *map, chiba_sco *sco) {
  chiba_sco *prev = chiba_sco_aat_insert(scp_map_getaat(sco), sco);
  if (!prev) {
    map->count++;
  }
  return prev;
}

UTILS chiba_sco *chiba_sco_map_delete(chiba_sco_map *map, chiba_sco *key) {
  chiba_sco *prev = chiba_sco_aat_delete(scp_map_getaat(key), key);
  if (prev) {
    map->count--;
  }
  return prev;
}

typedef struct chiba_sco_list {
  chiba_sco_link head;
  chiba_sco_link tail;
} chiba_sco_list;

////////////////////////////////////////////////////////////////////////////////
// Global and thread-local variables.
////////////////////////////////////////////////////////////////////////////////

PRIVATE THREAD_LOCAL bool chiba_sco_initialized = false;
PRIVATE THREAD_LOCAL i64 chiba_sco_nrunners = 0;
PRIVATE THREAD_LOCAL chiba_sco_list chiba_sco_runners = {0};
PRIVATE THREAD_LOCAL i64 chiba_sco_nyielders = 0;
PRIVATE THREAD_LOCAL chiba_sco_list chiba_sco_yielders = {0};
PRIVATE THREAD_LOCAL chiba_sco *chiba_sco_cur = NULL;
PRIVATE THREAD_LOCAL chiba_sco_map chiba_sco_paused = {0};
PRIVATE THREAD_LOCAL i64 chiba_sco_npaused = 0;
PRIVATE THREAD_LOCAL bool chiba_sco_exit_to_main_requested = false;
PRIVATE THREAD_LOCAL void (*chiba_sco_user_entry)(void *udata);

PRIVATE _Atomic(i64) chiba_sco_next_id = 0;
PRIVATE _Atomic(bool) chiba_sco_locker = 0;
PRIVATE chiba_sco_map chiba_sco_detached = {0};
PRIVATE i64 chiba_sco_ndetached = 0;

UTILS void chiba_sco_lock(void) {
  bool expected = false;
  chiba_backoff b = {.step = 0};
  while (!atomic_compare_exchange_weak(&chiba_sco_locker, &expected, true)) {
    expected = false;
    backoff_snooze(&b);
  }
}

UTILS void chiba_sco_unlock(void) { atomic_store(&chiba_sco_locker, false); }

UTILS void chiba_sco_list_init(chiba_sco_list *list) {
  list->head.prev = NULL;
  list->head.next = (chiba_sco *)&list->tail;
  list->tail.prev = (chiba_sco *)&list->head;
  list->tail.next = NULL;
}

UTILS void chiba_sco_remove_from_list(chiba_sco *co) {
  co->prev->next = co->next;
  co->next->prev = co->prev;
  // Reset to self so subsequent remove/push operations are safe
  co->prev = co;
  co->next = co;
}

UTILS void chiba_sco_list_pop_front(chiba_sco_list *list, chiba_sco **out_co) {
  chiba_sco *co = NULL;
  if (list->head.next != (chiba_sco *)&list->tail) {
    co = list->head.next;
    chiba_sco_remove_from_list(co);
  }
  *out_co = co;
}

UTILS void chiba_sco_list_push_back(chiba_sco_list *list, chiba_sco *co) {
  chiba_sco_remove_from_list(co);
  list->tail.prev->next = co;
  co->prev = list->tail.prev;
  co->next = (chiba_sco *)&list->tail;
  list->tail.prev = co;
}

UTILS void chiba_sco_init(void) {
  if (!chiba_sco_initialized) {
    chiba_sco_list_init(&chiba_sco_runners);
    chiba_sco_list_init(&chiba_sco_yielders);
    chiba_sco_initialized = true;
  }
}

UTILS void chiba_sco_return_to_main(bool final) {
  chiba_sco_cur = NULL;
  chiba_sco_exit_to_main_requested = false;
  chiba_co_switch(0, final);
}

UTILS void chiba_sco_switch(bool resumed_from_main, bool final) {
  if (chiba_sco_nrunners == 0) {
    // No more runners.
    if (chiba_sco_nyielders == 0 || chiba_sco_exit_to_main_requested ||
        (!resumed_from_main && chiba_sco_npaused > 0)) {
      chiba_sco_return_to_main(final);
      return;
    }
    // Convert the yielders to runners
    chiba_sco_runners.head.next = chiba_sco_yielders.head.next;
    chiba_sco_runners.head.next->prev = (chiba_sco *)&chiba_sco_runners.head;
    chiba_sco_runners.tail.prev = chiba_sco_yielders.tail.prev;
    chiba_sco_runners.tail.prev->next = (chiba_sco *)&chiba_sco_runners.tail;
    chiba_sco_yielders.head.next = (chiba_sco *)&chiba_sco_yielders.tail;
    chiba_sco_yielders.tail.prev = (chiba_sco *)&chiba_sco_yielders.head;
    chiba_sco_nrunners = chiba_sco_nyielders;
    chiba_sco_nyielders = 0;
  }
  chiba_sco_list_pop_front(&chiba_sco_runners, &chiba_sco_cur);
  chiba_sco_nrunners--;
  chiba_co_switch(chiba_sco_cur->co, final);
}

UTILS void chiba_sco_entry(anyptr ctx) {
  // Initialize a new coroutine on the user's stack.
  chiba_sco scostk = {0};
  chiba_sco *co = &scostk;
  co->co = chiba_co_current();
  co->id = atomic_fetch_add(&chiba_sco_next_id, 1) + 1;
  co->ctx = ctx;
  co->prev = co;
  co->next = co;
  if (chiba_sco_cur) {
    // Reschedule the coroutine that started this one immediately after
    // all running coroutines, but before any yielding coroutines, and
    // continue running the started coroutine.
    chiba_sco_list_push_back(&chiba_sco_runners, chiba_sco_cur);
    chiba_sco_nrunners++;
  }
  chiba_sco_cur = co;
  if (chiba_sco_user_entry) {
    chiba_sco_user_entry(ctx);
  }
  // This coroutine is finished. Switch to the next coroutine.
  chiba_sco_switch(false, true);
}

PUBLIC void chiba_sco_exit(void) {
  if (chiba_sco_cur) {
    chiba_sco_exit_to_main_requested = true;
    chiba_sco_switch(false, true);
  }
}

PUBLIC void chiba_sco_start(chiba_sco_desc *desc) {
  chiba_sco_init();
  chiba_co_desc co_desc = {
      .entry = chiba_sco_entry,
      .defer = desc->defer,
      .stack = desc->stack,
      .stack_size = desc->stack_size,
      .ctx = desc->ctx,
  };
  chiba_sco_user_entry = desc->entry;
  chiba_co_start(&co_desc, false);
}

PUBLIC chiba_sco_id_t chiba_sco_id(void) {
  return chiba_sco_cur ? chiba_sco_cur->id : 0;
}

PUBLIC void chiba_sco_yield(void) {
  if (chiba_sco_cur) {
    chiba_sco_list_push_back(&chiba_sco_yielders, chiba_sco_cur);
    chiba_sco_nyielders++;
    chiba_sco_switch(false, false);
  }
}

PUBLIC void chiba_sco_pause(void) {
  if (chiba_sco_cur) {
    chiba_sco_map_insert(&chiba_sco_paused, chiba_sco_cur);
    chiba_sco_npaused++;
    chiba_sco_switch(false, false);
  }
}

PUBLIC void chiba_sco_resume(chiba_sco_id_t id) {
  chiba_sco_init();
  if (id == 0 && !chiba_sco_cur) {
    // Resuming from main
    chiba_sco_switch(true, false);
  } else {
    // Resuming from coroutine
    struct chiba_sco *co =
        chiba_sco_map_delete(&chiba_sco_paused, &(struct chiba_sco){.id = id});
    if (co) {
      chiba_sco_npaused--;
      co->prev = co;
      co->next = co;
      chiba_sco_list_push_back(&chiba_sco_yielders, co);
      chiba_sco_nyielders++;
      chiba_sco_yield();
    }
  }
}

PUBLIC void chiba_sco_detach(chiba_sco_id_t id) {
  struct chiba_sco *co =
      chiba_sco_map_delete(&chiba_sco_paused, &(struct chiba_sco){.id = id});
  if (co) {
    chiba_sco_npaused--;
    chiba_sco_lock();
    chiba_sco_map_insert(&chiba_sco_detached, co);
    chiba_sco_ndetached++;
    chiba_sco_unlock();
  }
}

PUBLIC void chiba_sco_attach(chiba_sco_id_t id) {
  chiba_sco_lock();
  struct chiba_sco *co =
      chiba_sco_map_delete(&chiba_sco_detached, &(struct chiba_sco){.id = id});
  if (co) {
    chiba_sco_ndetached--;
  }
  chiba_sco_unlock();
  if (co) {
    chiba_sco_map_insert(&chiba_sco_paused, co);
    chiba_sco_npaused++;
  }
}

PUBLIC void *chiba_sco_ctx(void) {
  return chiba_sco_cur ? chiba_sco_cur->ctx : NULL;
}

PUBLIC chiba_sco_info chiba_sco_info_all(void) {
  chiba_sco_info info = {
      .scheduled = chiba_sco_nyielders,
      .running = chiba_sco_nrunners + (chiba_sco_cur ? 1 : 0),
      .paused = chiba_sco_npaused,
      .detached = chiba_sco_ndetached,
      .method = chiba_co_method(0),
  };
  return info;
}

PUBLIC bool chiba_sco_active(void) {
  return (chiba_sco_nyielders + chiba_sco_npaused + chiba_sco_nrunners +
          !!chiba_sco_cur) > 0;
}
