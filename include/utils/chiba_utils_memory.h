#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"

#define get_type_from_buffer(type) \
UTILS type get_##type(const u8 *tab) { \
    type v; \
    memcpy(&v, tab, sizeof(v)); \
    return v; \
}

#define put_type_to_buffer(type) \
UTILS void put_##type(u8 *tab, type val) { \
    memcpy(tab, &val, sizeof(val)); \
}

get_type_from_buffer(u8)
get_type_from_buffer(u16)
get_type_from_buffer(u32)
get_type_from_buffer(u64)
get_type_from_buffer(i8)
get_type_from_buffer(i16)
get_type_from_buffer(i32)
get_type_from_buffer(i64)

put_type_to_buffer(u8)
put_type_to_buffer(u16)
put_type_to_buffer(u32)
put_type_to_buffer(u64)
put_type_to_buffer(i8)
put_type_to_buffer(i16)
put_type_to_buffer(i32)
put_type_to_buffer(i64)

#define container_of_type_at(ptr, type, member) ((type *)((uint8_t *)(ptr) - offsetof(type, member)))

