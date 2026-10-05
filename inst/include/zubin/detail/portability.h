/*
 * zubin/detail/portability.h -- compiler macros, and the zufast version check.
 *
 * Internal: the macros here are outside the compatibility promise of design
 * 4.4. Every zubin header includes this one first.
 */
#ifndef ZUBIN_DETAIL_PORTABILITY_H
#define ZUBIN_DETAIL_PORTABILITY_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zufast/version.h>
#include <zufast/detail/portability.h>

/* LinkingTo is not transitive: a consumer lists `LinkingTo: zubin, zufast`
   (design 4.1). The include above fails first when zufast is missing; this
   catches one that is too old. */
#if !defined(ZUFAST_VERSION_NUMBER) || ZUFAST_VERSION_NUMBER < 100
#  error "zubin needs zufast >= 0.1.0: add zufast to LinkingTo"
#endif

/* Every function defined in a zubin header is static inline (design 4.5). */
#define ZB_INLINE static inline

#if defined(__GNUC__) || defined(__clang__)
#  define ZB_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define ZB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define ZB_LIKELY(x)   (x)
#  define ZB_UNLIKELY(x) (x)
#endif

#define ZB_INT_CAT2(a, b) a##b
#define ZB_INT_CAT(a, b)  ZB_INT_CAT2(a, b)

/* ZB_STATIC_ASSERT(cond, tag): a compile-time assertion usable at file scope
   in C99 and C++11. `tag` must be an identifier unique across the headers. */
#if defined(__cplusplus) && __cplusplus >= 201103L
#  define ZB_STATIC_ASSERT(cond, tag) static_assert(cond, #tag)
#else
#  define ZB_STATIC_ASSERT(cond, tag) \
     typedef char ZB_INT_CAT(zb_int_static_assert_, tag)[(cond) ? 1 : -1]
#endif

#endif /* ZUBIN_DETAIL_PORTABILITY_H */
