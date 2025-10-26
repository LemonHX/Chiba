#pragma once
#include "../utils/chiba_utils.h"
#include <cstring>

UTILS void fmt(u8 *ss, const char *fmt, ...) {
  u64 len = 0;
  u8 *stack_buffer = (u8 *)alloca(1024);
  if (!ss)
    return;
  if (!fmt)
    return;

  va_list args, args2;
  va_start(args, fmt);

  va_copy(args2, args);
  const int n = vsnprintf((i8 *)NULL, 0U, fmt, args);
  if (n < 0)
    goto done2;
  const ptrdiff_t pos = ss->overwrite ? 0 : ss->len;
  ss->len = pos + n;
  if (ss->len > ss->cap) {
    ss->cap = ss->len + ss->cap / 2;
    ss->data = (char *)realloc(ss->data, (size_t)ss->cap + 1U);
  }
  vsnprintf(ss->data + pos, (size_t)n + 1, fmt, args2);
done2:
  va_end(args2);
done1:
  va_end(args);
}

/*
VER 2.4 API:
void        fmt_print(fmt, ...);
void        fmt_println(fmt, ...);
void        fmt_printd(dst, fmt, ...);
const char* fmt_time(fmt, const struct tm* tm, int MAXLEN);
void        fmt_close(fmt_stream* ss);

  dst - destination, one of:
    FILE* fp        Write to a file
    char* strbuf    Write to a pre-allocated string buffer
    fmt_stream* ss  Write to a string-stream (auto allocated).
                    Set ss->overwrite=1 for overwrite-mode.
                    Call fmt_close(ss) after usage.

  fmt - format string (const char*)
    {}              Auto-detected format. If :MOD is not specified,
                    float will use ".8g" format, and double ".16g".
    {:MODS}         Format modifiers: '<' left align (replaces '-'). Default for
char* and char.
                                      '>' right align. Default for numbers.
                    Other than that MODS can be regular printf() format
modifiers.
    {{  }}  %       Print the '{', '}', and '%' characters.

* C11 or higher required.
* MAX 12 arguments after fmt string.
* Define FMT_IMPLEMENT, STC_IMPLEMENT or i_implement prior to #include in one
translation unit.
* (c) operamint, 2022-2025, MIT License.
-----------------------------------------------------------------------------------
#define i_implement
#include "c11/fmt.h"

int main(void) {
    const double pi = 3.141592653589793;
    const size_t x = 1234567890;
    const char* string = "Hello world";
    const wchar_t* wstr = L"The whole";
    const char z = 'z';
    _Bool flag = 1;
    unsigned char r = 123, g = 214, b = 90, w = 110;
    char buffer[64];

    fmt_print("Color: ({} {} {}), {}\n", r, g, b, flag);
    fmt_println("Wide: {}, {}", wstr, L"wide world");
    fmt_println("{:10} {:10} {:10.2f}", 42ull, 43, pi);
    fmt_println("{:>10} {:>10} {:>10}", z, z, w);
    fmt_printd(stdout, "{:10} {:10} {:10}\n", "Hello", "Mad", "World");
    fmt_printd(stderr, "100%: {:<20} {:.*} {}\n", string, 4, pi, x);
    fmt_printd(buffer, "Precision: {} {:.10} {}", string, pi, x);
    fmt_println("{}", buffer);
    fmt_println("Vector: ({}, {}, {})", 3.2, 3.3, pi);

    fmt_stream ss[1] = {0};
    fmt_printd(ss, "{} {}", "Pi is:", pi);
    fmt_print("{}, len={}, cap={}\n", ss->data, ss->len, ss->cap);
    fmt_printd(ss, "{} {}", ", Pi squared is:", pi*pi);
    fmt_print("{}, len={}, cap={}\n", ss->data, ss->len, ss->cap);
    fmt_close(ss);

    time_t now = time(NULL);
    struct tm t1 = *localtime(&now), t2 = t1;
    t2.tm_hour += 48;
    mktime(&t2);
    fmt_print("Dates: {} and {}\n", fmt_time("%Y-%m-%d %X %Z", &t1, 63),
                                    fmt_time("%Y-%m-%d %X %Z", &t2, 63));
}
*/
#include <assert.h>
#include <stddef.h>
#include <stdio.h> // IWYU pragma: keep
#include <string.h>
#if defined _WIN32
#include <malloc.h>
#elif defined __APPLE__
#include <stdlib.h>
#else
#include <alloca.h>
#endif
#define fmt_OVERLOAD(name, ...)                                                \
  fmt_JOIN(name##_, fmt_NUMARGS(__VA_ARGS__))(__VA_ARGS__)
#define fmt_JOIN(a, b) _fmt_JOIN0(a, b)
#define fmt_NUMARGS(...) _fmt_APPLY_ARG_N((__VA_ARGS__, _fmt_RSEQ_N))
#define _fmt_JOIN0(a, b) a##b
#define _fmt_APPLY_ARG_N(args) _fmt_ARG_N args
#define _fmt_RSEQ_N 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
#define _fmt_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13,     \
                   _14, _15, _16, N, ...)                                      \
  N

#if defined FMT_NDEBUG || defined STC_NDEBUG || defined NDEBUG
#define fmt_OK(exp) (void)(exp)
#else
#define fmt_OK(exp) assert(exp)
#endif

typedef struct {
  char *data;
  ptrdiff_t cap, len;
  bool overwrite;
} fmt_stream;

#if defined FMT_STATIC || defined STC_STATIC || defined i_static
#define FMT_API static
#define FMT_DEF static
#elif defined FMT_IMPLEMENT || defined STC_IMPLEMENT || defined i_implement
#define FMT_API extern
#define FMT_DEF
#else
#define FMT_API
#endif

struct tm;
FMT_API const char *_fmt_time(const char *fmt, const struct tm *tm, char *buf,
                              int bufsize);
FMT_API void fmt_close(fmt_stream *ss);
FMT_API int _fmt_parse(char *p, int nargs, const char *fmt, ...);
FMT_API void _fmt_sprint(fmt_stream *, const char *fmt, ...);
#define _fmt_init(_fs, _fmt, fmt, nargs)                                       \
  const char *_fmt = fmt;                                                      \
  char *_fs =                                                                  \
      (char *)alloca(strlen(_fmt) + nargs * 3 + 1) // "{}" => "%.16g" => +3

#define fmt_print(...) fmt_printd(stdout, __VA_ARGS__)
#define fmt_println(...) fmt_printd((fmt_stream *)0, __VA_ARGS__)
#define fmt_printd(...) fmt_OVERLOAD(fmt_printd, __VA_ARGS__)
#define fmt_time(fmt, tm, MAXLEN)                                              \
  _fmt_time(fmt, tm, (char[MAXLEN + 1]){0}, MAXLEN + 1)

#define fmt_sv "{:.*s}"
#define fmt_svarg(sv) (int)(sv).size, (sv).buf

/* Primary function. */
#define fmt_printd_2(to, fmt)                                                  \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 0);                                              \
    int _n = _fmt_parse(_fs, 0, _fmt);                                         \
    fmt_OK(_n == 0);                                                           \
    _fmt_fn(to)(to, _fmt);                                                     \
  } while (0)
#define fmt_printd_3(to, fmt, a)                                               \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 1);                                              \
    int _n = _fmt_parse(_fs, 1, _fmt, _fc(a));                                 \
    fmt_OK(_n == 1);                                                           \
    _fmt_fn(to)(to, _fs, a);                                                   \
  } while (0)
#define fmt_printd_4(to, fmt, a, b)                                            \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 2);                                              \
    int _n = _fmt_parse(_fs, 2, _fmt, _fc(a), _fc(b));                         \
    fmt_OK(_n == 2);                                                           \
    _fmt_fn(to)(to, _fs, a, b);                                                \
  } while (0)
#define fmt_printd_5(to, fmt, a, b, c)                                         \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 3);                                              \
    int _n = _fmt_parse(_fs, 3, _fmt, _fc(a), _fc(b), _fc(c));                 \
    fmt_OK(_n == 3);                                                           \
    _fmt_fn(to)(to, _fs, a, b, c);                                             \
  } while (0)
#define fmt_printd_6(to, fmt, a, b, c, d)                                      \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 4);                                              \
    int _n = _fmt_parse(_fs, 4, _fmt, _fc(a), _fc(b), _fc(c), _fc(d));         \
    fmt_OK(_n == 4);                                                           \
    _fmt_fn(to)(to, _fs, a, b, c, d);                                          \
  } while (0)
#define fmt_printd_7(to, fmt, a, b, c, d, e)                                   \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 5);                                              \
    int _n = _fmt_parse(_fs, 5, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e)); \
    fmt_OK(_n == 5);                                                           \
    _fmt_fn(to)(to, _fs, a, b, c, d, e);                                       \
  } while (0)
#define fmt_printd_8(to, fmt, a, b, c, d, e, f)                                \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 6);                                              \
    int _n = _fmt_parse(_fs, 6, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e),  \
                        _fc(f));                                               \
    fmt_OK(_n == 6);                                                           \
    _fmt_fn(to)(to, _fs, a, b, c, d, e, f);                                    \
  } while (0)
#define fmt_printd_9(to, fmt, a, b, c, d, e, f, g)                             \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 7);                                              \
    int _n = _fmt_parse(_fs, 7, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e),  \
                        _fc(f), _fc(g));                                       \
    fmt_OK(_n == 7);                                                           \
    _fmt_fn(to)(to, _fs, a, b, c, d, e, f, g);                                 \
  } while (0)
#define fmt_printd_10(to, fmt, a, b, c, d, e, f, g, h)                         \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 8);                                              \
    int _n = _fmt_parse(_fs, 8, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e),  \
                        _fc(f), _fc(g), _fc(h));                               \
    fmt_OK(_n == 8);                                                           \
    _fmt_fn(to)(to, _fs, a, b, c, d, e, f, g, h);                              \
  } while (0)
#define fmt_printd_11(to, fmt, a, b, c, d, e, f, g, h, i)                      \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 9);                                              \
    int _n = _fmt_parse(_fs, 9, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e),  \
                        _fc(f), _fc(g), _fc(h), _fc(i));                       \
    fmt_OK(_n == 9);                                                           \
    _fmt_fn(to)(to, _fs, a, b, c, d, e, f, g, h, i);                           \
  } while (0)
#define fmt_printd_12(to, fmt, a, b, c, d, e, f, g, h, i, j)                   \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 10);                                             \
    int _n = _fmt_parse(_fs, 10, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e), \
                        _fc(f), _fc(g), _fc(h), _fc(i), _fc(j));               \
    fmt_OK(_n == 10);                                                          \
    _fmt_fn(to)(to, _fs, a, b, c, d, e, f, g, h, i, j);                        \
  } while (0)
#define fmt_printd_13(to, fmt, a, b, c, d, e, f, g, h, i, j, k)                \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 11);                                             \
    int _n = _fmt_parse(_fs, 11, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e), \
                        _fc(f), _fc(g), _fc(h), _fc(i), _fc(j), _fc(k));       \
    fmt_OK(_n == 11);                                                          \
    _fmt_fn(to)(to, _fs, a, b, c, d, e, f, g, h, i, j, k);                     \
  } while (0)
#define fmt_printd_14(to, fmt, a, b, c, d, e, f, g, h, i, j, k, l)             \
  do {                                                                         \
    _fmt_init(_fs, _fmt, fmt, 12);                                             \
    int _n =                                                                   \
        _fmt_parse(_fs, 12, _fmt, _fc(a), _fc(b), _fc(c), _fc(d), _fc(e),      \
                   _fc(f), _fc(g), _fc(h), _fc(i), _fc(j), _fc(k), _fc(l));    \
    fmt_OK(_n == 12);                                                          \
    _fmt_fn(to)(to, _fs, a, b, c, d, e, f, g, h, i, j, k, l);                  \
  } while (0)

#define _fmt_fn(x)                                                             \
  _Generic((x), FILE *: fprintf, char *: sprintf, fmt_stream *: _fmt_sprint)

#if defined(_MSC_VER) && !defined(__clang__)
#define _signed_char_hhd
#else
#define _signed_char_hhd signed char : "c",
#endif

#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER)
#define FMT_UNUSED __attribute__((unused))
#else
#define FMT_UNUSED
#endif

UTILS void unsigned_to_string(i8 *dest, u64 value) {
  if (value == 0) {
    dest[0] = '0';
    dest[1] = '\0';
    return;
  }

  i8 *end = dest;
  while (value > 0) {
    *end++ = '0' + (i8)(value % 10);
    value /= 10;
  }
  *end = '\0';

  // Reverse the string
  i8 *start = dest;
  end--;
  while (start < end) {
    i8 temp = *start;
    *start = *end;
    *end = temp;
    start++;
    end--;
  }
}

UTILS void signed_to_string(i8 *dest, i64 value) {
  i8 buffer[33];
  if (value < 0) {
    buffer[0] = '-';
    unsigned_to_string(buffer + 1, (u64)(-value));
  }
  strcpy(dest, buffer);
}

UTILS void str_to_string(i8 *dest, str src) { memcpy(dest, src.data, src.len); }

#define _fc(ret, x)                                                            \
  _Generic((x), str: str_to_string, u64: unsigned_to_string)(ret, x)
void f() {
  i8 buf[10];
  _fc(buf, STR("hello"));
}
