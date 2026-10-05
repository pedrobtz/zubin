/*
 * Shared by the libFuzzer targets in tools/fuzz (design 16.6). Each target
 * compiles the zubin headers directly, with no R, under ASan and UBSan.
 *
 * FUZZ_CHECK fails the run on a broken invariant. FUZZ_CANARY, compiled in
 * with -DZB_FUZZ_CANARY, writes out of bounds on the input "ZB-CANARY":
 * running a canary build on that input must crash, which proves the
 * instrumentation is live (roadmap, principle 7).
 */
#ifndef ZB_FUZZ_H
#define ZB_FUZZ_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zubin.h>

#define FUZZ_CHECK(cond) do { if (!(cond)) __builtin_trap(); } while (0)

#ifdef ZB_FUZZ_CANARY
#  define FUZZ_CANARY(data, size)                                        \
     do {                                                                \
         if ((size) == 9 && memcmp((data), "ZB-CANARY", 9) == 0) {       \
             volatile char small[4];                                     \
             volatile char *volatile p = small;                          \
             volatile size_t i = (size);                                 \
             p[i] = 1; /* past the end: ASan's stack-buffer-overflow */  \
         }                                                               \
     } while (0)
#else
#  define FUZZ_CANARY(data, size) ((void)0)
#endif

int zb_fuzz_support(void);

#endif
