#pragma once
#include <define.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../libc-base/math/bits.h"
#include "../libc-base/thread/spin.h"
#define streq(s1, s2)                                                                              \
  ({                                                                                               \
    cstr _s1 = (s1), _s2 = (s2);                                                                   \
    (_s1 && _s2) ? strcmp(_s1, _s2) == 0 : _s1 == _s2;                                             \
  })

#define strneq(s1, s2, n)                                                                          \
  ({                                                                                               \
    cstr _s1 = (s1), _s2 = (s2);                                                                   \
    (_s1 && _s2) ? strncmp(_s1, _s2, n) == 0 : _s1 == _s2;                                         \
  })

#define xstreq(s1, s2)                                                                             \
  ({                                                                                               \
    xstr _s1 = (s1), _s2 = (s2);                                                                   \
    (_s1 && _s2) ? (_s1->hash == _s2->hash ? xstrcmp(_s1, _s2) == 0 : false) : _s1 == _s2;         \
  })

#define memeq(s1, s2, n)                                                                           \
  ({                                                                                               \
    const void *_s1 = (const void *)(s1), *_s2 = (const void *)(s2);                               \
    (_s1 && _s2) ? memcmp(_s1, _s2, n) == 0 : _s1 == _s2;                                          \
  })
static inline void *xmalloc(size_t size) {
  void *ptr = malloc(size);
  if (ptr == null) {
    fprintf(stderr, "Out of memory: %zu bytes\n", size);
    abort();
  }
  return ptr;
}