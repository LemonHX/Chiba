#include "cs_coroutine.h"
//////////
// Linked list utilities
//////////

typedef struct chiba_csco chiba_csco;

// typedef struct chiba_csco_link {
//   chiba_csco *prev;
//   chiba_csco *next;
// } chiba_csco_link;

// typedef struct chiba_csco_list {
//   chiba_csco_link head;
//   chiba_csco_link tail;
// } chiba_csco_list;

// UTILS void chiba_csco_remove_from_list(chiba_csco *co) {
//   chiba_csco_link *link = (anyptr)co;
//   ((chiba_csco_link *)link->prev)->next = link->next;
//   ((chiba_csco_link *)link->next)->prev = link->prev;
//   link->next = (chiba_csco *)link;
//   link->prev = (chiba_csco *)link;
// }
// UTILS void chiba_csco_list_init(chiba_csco_list *list) {
//   list->head.prev = NULL;
//   list->head.next = (chiba_csco *)(&list->tail);
//   list->tail.prev = (chiba_csco *)(&list->head);
//   list->tail.next = NULL;
// }
// UTILS void chiba_csco_list_push_back(chiba_csco_list *list, chiba_csco *co) {
//   chiba_csco_remove_from_list(co);
//   chiba_csco_link *link = (anyptr)co;
//   ((chiba_csco_link *)list->tail.prev)->next = (chiba_csco *)link;
//   link->prev = list->tail.prev;
//   link->next = (chiba_csco *)(&list->tail);
//   list->tail.prev = (chiba_csco *)link;
// }
// UTILS chiba_csco *chiba_csco_list_pop_front(chiba_csco_list *list,
//                                             chiba_csco **out_co) {
//   chiba_csco *co = list->head.next;
//   if (unlikely(co == (chiba_csco *)(&list->tail))) {
//     return NULL;
//   }
//   chiba_csco_remove_from_list(co);
//   return co;
// }
// UTILS bool chiba_csco_list_is_empty(chiba_csco_list *list) {
//   return list->head.next == (chiba_csco *)(&list->tail);
// }

//////////
// cleanup stack
//////////
typedef struct chiba_csco_cleanup chiba_csco_cleanup;

typedef struct chiba_csco_cleanup {
  chiba_csco_entry_t func;
  anyptr arg;
  chiba_csco_cleanup *next;
} chiba_csco_cleanup;

//////////
// chiba_csco structure and aatrees for coroutine management
//////////

// 我这个和neco不一样的点：
// 我这个eventdata 和 eventkind 是 anyptr 和 ATOM，而neco是i32 fd 和 i32
// evkind 我这个设计更通用一些，并不是局限于文件事件
// TODO: 是不是还得写一个event handler map？ 让用户可以注册event handler？
typedef struct chiba_csco {
  chiba_csco *prev;
  chiba_csco *next;

  chiba_csco_id_t id;         // coroutine id (sco_id())
  chiba_csco_id_t last_id;    // last coroutine id
  chiba_csco_id_t starter_id; // starter coroutine id

  anyptr stack;             // coroutine stack
  chiba_csco_entry_t entry; // coroutine entry function
  anyptr ctx;
  chiba_csco_cleanup *cleanup; // cancelation cleanup stack

  chiba_csco_ts_t deadline;

  // Event, before and after handles event
  // - evdata should be NULL
  // - evkind should be 0
  anyptr evdata;
  ATOM evkind;

  bool paused;       // coroutine is paused
  bool deadlined;    // coroutine operation was deadlined
  bool suspended;    // coroutine is suspended
  bool mutex_locked; // coroutine holds a mutex lock

  // ===== Links for aatrees =====

  AAT_FIELDS(chiba_csco, all_left, all_right,
             all_level) // For the rt->all comap, which stores all active
                        //  coroutines
  AAT_FIELDS(chiba_csco, dl_left, dl_right,
             dl_level) // Deadline for pause. All paused will have this set to
                       // something.
  AAT_FIELDS(chiba_csco, evleft, evright, evlevel) // Event waiters
} chiba_csco;

///////
// chiba_csco_runtime structure
///////

typedef struct chiba_csco_runtime_info {
  i64 ndeadlines; // total number of paused coroutines
  i64 ntotal;     // total number of coroutines ever created
  i64 nsleepers;
  i64 nlocked;
  i64 nreceivers;
  i64 nsenders;
  i64 nwaitgroupers; // number of waitgroup blocked coroutines
  i64 ncondwaiters;  // number of cond blocked coroutines
  i64 nworkers;      // number of background workers
  i64 nsuspended;    // number of suspended coroutines
} chiba_csco_runtime_info;

typedef struct chiba_csco_runtime {
  chiba_arrayqueue cancellist; // waiting coroutines
  chiba_arrayqueue joinlist;   // waiting coroutines

  chiba_csco_runtime_info info; // runtime info

  chiba_csco_evmap evwaiters; // coroutines waiting on events (aat root)
  i64 nevwaiters;

  // list of coroutines waiting to be resumed by the scheduler
  i32 nresumers;
  chiba_arrayqueue resumers;

  anyptr runtime_user_info_ext;

#ifdef SCO_MULTITHREAD_WORKERS
  struct worker *worker;
  pthread_mutex_t iomu;
  chiba_arrayqueue iolist;
  i64 niowaiters;
#endif
} chiba_csco_runtime;

///////
// AATree definitions and implementations
///////
UTILS i32 chiba_csco_all_compare(chiba_csco *a, chiba_csco *b) {
  return a->id < b->id ? -1 : a->id > b->id;
}
AAT_DEF(UTILS, chiba_csco_all_aat, chiba_csco)
AAT_IMPL(chiba_csco_all_aat, chiba_csco, all_left, all_right, all_level,
         chiba_csco_all_compare)

UTILS i32 chiba_csco_dl_compare(chiba_csco *a, chiba_csco *b) {
  return a->deadline < b->deadline   ? -1
         : a->deadline > b->deadline ? 1
         : a->id < b->id             ? -1
                                     : a->id > b->id;
}
AAT_DEF(UTILS, chiba_csco_dl_aat, chiba_csco)
AAT_IMPL(chiba_csco_dl_aat, chiba_csco, dl_left, dl_right, dl_level,
         chiba_csco_dl_compare)

UTILS i32 chiba_csco_ev_compare(chiba_csco *a, chiba_csco *b) {
  // order by evfd, evkind, id
  return a->evdata < b->evdata   ? -1
         : a->evdata > b->evdata ? 1
         : a->evkind < b->evkind ? -1
         : a->evkind > b->evkind ? 1
         : a->id < b->id         ? -1
                                 : a->id > b->id;
}

AAT_DEF(UTILS, chiba_csco_ev_aat, chiba_csco)
AAT_IMPL(chiba_csco_ev_aat, chiba_csco, evleft, evright, evlevel,
         chiba_csco_ev_compare)

//////////
// maps
//////////

typedef struct chiba_csco_map {
  chiba_csco *roots[CHIBA_SCO_NSHARDS];
  i32 count;
} chiba_csco_map;

#define csco_map_getaat(sco)                                                   \
  (&map->roots[CHIBA_HASH_mix13((sco)->id) & (CHIBA_SCO_NSHARDS - 1)])

UTILS chiba_csco *chiba_csco_map_insert(chiba_csco_map *map, chiba_csco *sco) {
  chiba_csco *prev = chiba_csco_all_aat_insert(csco_map_getaat(sco), sco);
  map->count++;
  return prev;
}

UTILS chiba_csco *chiba_csco_map_search(chiba_csco_map *map, chiba_csco *key) {
  return chiba_csco_all_aat_search(csco_map_getaat(key), key);
}

UTILS chiba_csco *chiba_csco_map_delete(chiba_csco_map *map, chiba_csco *key) {
  chiba_csco *prev = chiba_csco_all_aat_delete(csco_map_getaat(key), key);
  map->count--;
  return prev;
}

typedef struct chiba_csco_evmap {
  chiba_csco *roots[CHIBA_SCO_NSHARDS];
  i32 count;
} chiba_csco_evmap;

#define csco_evmap_getaat(sco)                                                 \
  (&map->roots[CHIBA_HASH_mix13((u64)(sco)->evdata) & (CHIBA_SCO_NSHARDS - 1)])

UTILS chiba_csco *chiba_csco_evmap_insert(chiba_csco_evmap *map,
                                          chiba_csco *sco) {
  chiba_csco *prev = chiba_csco_ev_aat_insert(csco_evmap_getaat(sco), sco);
  map->count++;
  return prev;
}

UTILS chiba_csco *chiba_csco_evmap_iter(chiba_csco_evmap *map,
                                        chiba_csco *key) {
  return chiba_csco_ev_aat_iter(csco_evmap_getaat(key), key);
}

UTILS chiba_csco *chiba_csco_evmap_next(chiba_csco_evmap *map,
                                        chiba_csco *key) {
  return chiba_csco_ev_aat_next(csco_evmap_getaat(key), key);
}

UTILS chiba_csco *chiba_csco_evmap_delete(chiba_csco_evmap *map,
                                          chiba_csco *key) {
  chiba_csco *prev = chiba_csco_ev_aat_delete(csco_evmap_getaat(key), key);
  map->count--;
  return prev;
}
