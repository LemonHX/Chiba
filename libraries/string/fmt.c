
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

PUBLIC void _fmt_sprint(fmt_stream *ss, const char *fmt, ...) {
  va_list args, args2;
  va_start(args, fmt);
  if (ss == NULL) {
    vprintf(fmt, args);
    putchar('\n');
    goto done1;
  }
  va_copy(args2, args);
  const int n = vsnprintf(NULL, 0U, fmt, args);
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

PUBLIC int _fmt_parse(char *p, int nargs, const char *fmt, ...) {
  char *arg, *p0, ch;
  int n = 0, empty;
  va_list args;
  va_start(args, fmt);
  do {
    switch ((ch = *fmt)) {
    case '%':
      *p++ = '%';
      break;
    case '}':
      if (*++fmt == '}')
        break; /* ok */
      n = 99;
      continue;
    case '{':
      if (*++fmt == '{')
        break; /* ok */
      if (++n > nargs)
        continue;
      if (*fmt != ':' && *fmt != '}')
        n = 99;
      fmt += (*fmt == ':');
      empty = *fmt == '}';
      arg = va_arg(args, char *);
      *p++ = '%', p0 = p;
      while (1)
        switch (*fmt) {
        case '\0':
          n = 99; /* FALLTHRU */
        case '}':
          goto done;
        case '<':
          *p++ = '-', ++fmt;
          break;
        case '>':
          p0 = NULL; /* FALLTHRU */
        case '-':
          ++fmt;
          break;
        case '*':
          if (++n <= nargs)
            arg = va_arg(args, char *); /* FALLTHRU */
        default:
          *p++ = *fmt++;
        }
    done:
      switch (*arg) {
      case 'g':
        if (empty)
          memcpy(p, ".8", 2), p += 2;
        break;
      case '@':
        ++arg;
        if (empty)
          memcpy(p, ".16", 3), p += 3;
        break;
      }
      if (strchr("csdioxXufFeEaAgGnp", fmt[-1]) == NULL)
        while (*arg)
          *p++ = *arg++;
      if (p0 && (p[-1] == 's' || p[-1] == 'c')) /* left-align str */
        memmove(p0 + 1, p0, (size_t)(p++ - p0)), *p0 = '-';
      fmt += *fmt == '}';
      continue;
    }
    *p++ = *fmt++;
  } while (ch);
  va_end(args);
  return n;
}