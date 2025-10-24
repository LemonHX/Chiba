#include "chiba_utils_basic_types.h"
#include "chiba_utils_visibility.h"

// Determine system endianness
UTILS u8 is_be(void) {
    union {
        u16 a;
        u8  b;
    } u = { 0x100 };
    return u.b;
}

#ifndef offsetof
#define offsetof(type, field) ((size_t) &((type *)0)->field)
#endif

#define countof(x) (sizeof(x) / sizeof((x)[0]))
#define endof(x) ((x) + countof(x))
#define INF (1.0/0.0)
#define NEG_INF (-1.0/0.0)

// chiba does not support big-endian systems
BEFORE_START void abort_if_be(void) {
    if (is_be()) {
        fprintf(stderr, "Big-endian systems are not supported\n");
        abort();
    }
}

#define max_of_type(type) \
PRIVATE inline type max_##type(type a, type b) { \
    return (a > b) ? a : b; \
}

#define min_of_type(type) \
PRIVATE inline type min_##type(type a, type b) { \
    return (a < b) ? a : b; \
}

max_of_type(u8)
max_of_type(u16)
max_of_type(u32)
max_of_type(u64)
max_of_type(i8)
max_of_type(i16)
max_of_type(i32)
max_of_type(i64)

min_of_type(u8)
min_of_type(u16)
min_of_type(u32)
min_of_type(u64)
min_of_type(i8)
min_of_type(i16)
min_of_type(i32)
min_of_type(i64)

/* WARNING: undefined if a = 0 */
PRIVATE inline i32 clz32(u32 a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index;
    _BitScanReverse(&index, a);
    return 31 - index;
#else
    return __builtin_clz(a);
#endif
}

/* WARNING: undefined if a = 0 */
UTILS i32 clz64(u64 a)
{
#if defined(_MSC_VER) && !defined(__clang__)
#if INTPTR_MAX == INT64_MAX
    unsigned long index;
    _BitScanReverse64(&index, a);
    return 63 - index;
#else
    if (a >> 32)
        return clz32((unsigned)(a >> 32));
    else
        return clz32((unsigned)a) + 32;
#endif
#else
    return __builtin_clzll(a);
#endif
}

/* WARNING: undefined if a = 0 */
UTILS i32 ctz32(u32 a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index;
    _BitScanForward(&index, a);
    return index;
#else
    return __builtin_ctz(a);
#endif
}

/* WARNING: undefined if a = 0 */
UTILS i32 ctz64(u64 a)
{
#if defined(_MSC_VER) && !defined(__clang__)
    unsigned long index;
    _BitScanForward64(&index, a);
    return index;
#else
    return __builtin_ctzll(a);
#endif
}

UTILS u64 f64_to_u64(f64 d)
{
    union {
        f64 d;
        u64 u;
    } u;
    u.d = d;
    return u.u;
}

UTILS f64 u64_to_f64(u64 uu)
{
    union {
        f64 d;
        u64 u;
    } u;
    u.u = uu;
    return u.d;
}

#ifndef bswap16
PRIVATE inline u16 bswap16(u16 x)
{
    return (x >> 8) | (x << 8);
}
#endif

#ifndef bswap32
PRIVATE inline u32 bswap32(u32 v)
{
    return ((v & 0xff000000) >> 24) | ((v & 0x00ff0000) >>  8) |
        ((v & 0x0000ff00) <<  8) | ((v & 0x000000ff) << 24);
}
#endif

#ifndef bswap64
PRIVATE inline u64 bswap64(u64 v)
{
    return ((v & ((u64)0xff << (7 * 8))) >> (7 * 8)) |
        ((v & ((u64)0xff << (6 * 8))) >> (5 * 8)) |
        ((v & ((u64)0xff << (5 * 8))) >> (3 * 8)) |
        ((v & ((u64)0xff << (4 * 8))) >> (1 * 8)) |
        ((v & ((u64)0xff << (3 * 8))) << (1 * 8)) |
        ((v & ((u64)0xff << (2 * 8))) << (3 * 8)) |
        ((v & ((u64)0xff << (1 * 8))) << (5 * 8)) |
        ((v & ((u64)0xff << (0 * 8))) << (7 * 8));
}
#endif

