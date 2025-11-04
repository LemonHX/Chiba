#include "basic_memory.h"

anyptr CHIBA_INTERNAL_malloc_func = (anyptr)malloc;
anyptr CHIBA_INTERNAL_aligned_alloc_func = (anyptr)aligned_alloc;
anyptr CHIBA_INTERNAL_realloc_func = (anyptr)realloc;
anyptr CHIBA_INTERNAL_free_func = (anyptr)free;