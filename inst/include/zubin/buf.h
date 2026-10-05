/*
 * zubin/buf.h -- the buffer: ownership, limits, checked growth, typed appends
 * (design 9).
 *
 *     zb_buf b;
 *     if (zb_buf_alloc(&b, 4096, 0)) return ZB_ERR_MEMORY;    -- 0: no cap
 *     zb_put_u32le(&b, 0x1A5A4452u);
 *     zb_put_bytes(&b, payload, n);
 *     ... b.data, b.len are yours to read ...
 *     zb_buf_release(&b);
 *
 * Two backings exist: malloc (zb_buf_alloc, growable, owned) and borrowed
 * (zb_buf_borrow, a window onto caller memory: writable up to cap, never
 * grown, never freed). Whoever sets `release` frees `owner`, as in the Arrow
 * C data interface. Ownership and growability are the flags, never the
 * identity of `release`: every translation unit has its own copy of a
 * header-only function, so pointers to it do not compare equal (design 4.5,
 * rule 5).
 *
 * This is the only zubin header that allocates, and it does so only through
 * malloc, realloc and free. There is no bare size arithmetic: every sum and
 * product goes through zb_size_add() and zb_size_mul(), which refuse to wrap.
 * On any failure the buffer is unchanged, apart from ZB_BUF_HIT_LIMIT, which
 * records whether the last failed growth hit the cap (ZB_ERR_LIMIT) or the
 * allocator (ZB_ERR_MEMORY).
 *
 * Typed appends, one per writer of rw.h, each with a vectorised _n twin that
 * reserves once and then loops:
 *
 *     zb_put_u8 zb_put_i8
 *     zb_put_{u16,i16,u32,i32,u64,i64}{le,be}
 *     zb_put_{f16,bf16,f64}{le,be}          (double)
 *     zb_put_f32{le,be}                      (float)
 *     zb_put_u8_n(b, const uint8_t *v, n) ... zb_put_f64be_n(b, const double *v, n)
 *
 * Threads: every function here is pure, or reads and writes only the
 * buffer, cursor or arrays it is passed; no zubin header holds static or
 * global state (tools/check-headers refuses a static object under zubin/).
 * Any function may be called from any thread, as long as no two threads use
 * one buffer, cursor or output array at the same time (design 15).
 */
#ifndef ZUBIN_BUF_H
#define ZUBIN_BUF_H

#include "detail/portability.h"
#include "status.h"
#include "rw.h"

#include <stdlib.h>

/* zb_buf.flags */
#define ZB_BUF_OWNED     0x1u   /* release must be called */
#define ZB_BUF_GROWABLE  0x2u   /* data is a malloc block that realloc may move */
#define ZB_BUF_HIT_LIMIT 0x4u   /* the last failed growth was refused by max */

/* Growth doubles below this capacity and grows by half from it (design 9.3). */
#define ZB_BUF_DOUBLING_LIMIT ((size_t)64 * 1024 * 1024)
/* The smallest capacity a growable buffer allocates. */
#define ZB_BUF_MIN_CAP ((size_t)256)
/* The largest capacity: no C object is larger than PTRDIFF_MAX, and asking
   malloc or realloc for more is refused before it is asked. */
#define ZB_BUF_MAX_CAP ((size_t)PTRDIFF_MAX)

typedef struct zb_buf {
    uint8_t *data;
    size_t   len;      /* bytes in use */
    size_t   cap;      /* bytes allocated or borrowed */
    size_t   max;      /* hard cap on cap; 0 = unlimited */
    uint32_t flags;    /* ZB_BUF_OWNED | ZB_BUF_GROWABLE | ZB_BUF_HIT_LIMIT */
    void   (*release)(struct zb_buf *);   /* NULL for a borrowed buffer */
    void    *owner;    /* opaque: what release frees; the malloc block here */
} zb_buf;

/* ---- checked size arithmetic ------------------------------------------------ */

/* a + b into *out, or ZB_ERR_MEMORY (and *out untouched) when it would wrap.
   With zb_size_mul, the only size arithmetic a consumer should write: every
   count, length, offset and sum read from a file goes through one of the
   two before it sizes an allocation or a seek. */
ZB_INLINE zb_status zb_size_add(size_t a, size_t b, size_t *out)
{
    if (a > (size_t)-1 - b) return ZB_ERR_MEMORY;
    *out = a + b;
    return ZB_OK;
}

/* a * b into *out, or ZB_ERR_MEMORY (and *out untouched) when it would wrap. */
ZB_INLINE zb_status zb_size_mul(size_t a, size_t b, size_t *out)
{
    if (a != 0 && b > (size_t)-1 / a) return ZB_ERR_MEMORY;
    *out = a * b;
    return ZB_OK;
}

/* The internal names, kept for the code that uses them. */
ZB_INLINE zb_status zb_int_add(size_t a, size_t b, size_t *out) { return zb_size_add(a, b, out); }
ZB_INLINE zb_status zb_int_mul(size_t a, size_t b, size_t *out) { return zb_size_mul(a, b, out); }

/* The capacity to grow `cap` to so that it holds `need` bytes: double below
   ZB_BUF_DOUBLING_LIMIT, half again from it, never below need or
   ZB_BUF_MIN_CAP, and clamped to max when max is set (the caller has checked
   need <= max) and to ZB_BUF_MAX_CAP. ZB_ERR_MEMORY when need is above
   ZB_BUF_MAX_CAP. */
ZB_INLINE zb_status zb_int_grow(size_t cap, size_t need, size_t max, size_t *out)
{
    size_t next;
    if (need > ZB_BUF_MAX_CAP) return ZB_ERR_MEMORY;
    if (cap < ZB_BUF_DOUBLING_LIMIT) {
        next = cap * 2;   /* cannot wrap: cap is below 64 MiB */
    } else if (zb_int_add(cap, cap / 2, &next)) {
        next = ZB_BUF_MAX_CAP;
    }
    if (next > ZB_BUF_MAX_CAP) next = ZB_BUF_MAX_CAP;
    if (next < need) next = need;
    if (next < ZB_BUF_MIN_CAP) next = ZB_BUF_MIN_CAP;
    if (max && next > max) next = max;
    *out = next;
    return ZB_OK;
}

ZB_INLINE void zb_int_release_malloc(zb_buf *b)
{
    free(b->owner);
}

/* ---- construction and release ------------------------------------------------- */

/* Zeroed and unowned: no storage, nothing to release. */
ZB_INLINE void zb_buf_init(zb_buf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->max = 0;
    b->flags = 0;
    b->release = NULL;
    b->owner = NULL;
}

/* A growable, owned buffer with room for cap bytes, never to grow past max
   (0: unlimited). ZB_ERR_LIMIT when max is set and cap exceeds it,
   ZB_ERR_MEMORY when malloc fails; b is then as zb_buf_init leaves it. */
ZB_INLINE zb_status zb_buf_alloc(zb_buf *b, size_t cap, size_t max)
{
    zb_buf_init(b);
    if (max && cap > max) return ZB_ERR_LIMIT;
    if (cap > ZB_BUF_MAX_CAP) return ZB_ERR_MEMORY;
    if (cap) {
        b->data = (uint8_t *)malloc(cap);
        if (!b->data) return ZB_ERR_MEMORY;
    }
    b->cap = cap;
    b->max = max;
    b->flags = ZB_BUF_OWNED | ZB_BUF_GROWABLE;
    b->release = zb_int_release_malloc;
    b->owner = b->data;
    return ZB_OK;
}

/* A window onto the caller's n bytes at p: len = cap = n, not growable, not
   owned. Reading is always allowed; writing is allowed up to cap (reset
   first), since the memory is the caller's. p may be NULL when n is 0. */
ZB_INLINE void zb_buf_borrow(zb_buf *b, const void *p, size_t n)
{
    zb_buf_init(b);
    b->data = (uint8_t *)p;
    b->len = n;
    b->cap = n;
}

/* Calls release when the buffer is owned, then zeroes it. Safe on a zeroed,
   borrowed or already released buffer. */
ZB_INLINE void zb_buf_release(zb_buf *b)
{
    if ((b->flags & ZB_BUF_OWNED) && b->release) b->release(b);
    zb_buf_init(b);
}

/* Empties the buffer and keeps its storage. */
ZB_INLINE void zb_buf_reset(zb_buf *b)
{
    b->len = 0;
}

/* Takes ownership of a block the caller allocated with malloc() (or got back
   from zb_buf_detach()): the buffer holds len bytes of its cap, may grow to
   max (0: unlimited), is OWNED | GROWABLE, and frees the block with free() on
   release. The exact inverse of zb_buf_detach(): detaching again returns the
   same pointer. ZB_ERR_INVALID, and nothing taken over (the block is still
   the caller's), when len > cap, cap > max with max set, cap > ZB_BUF_MAX_CAP,
   or data is NULL with cap nonzero; b is then as zb_buf_init() leaves it. */
ZB_INLINE zb_status zb_buf_adopt(zb_buf *b, uint8_t *data, size_t len, size_t cap, size_t max)
{
    zb_buf_init(b);
    if (len > cap || (max && cap > max) || cap > ZB_BUF_MAX_CAP || (!data && cap)) {
        return ZB_ERR_INVALID;
    }
    b->data = data;
    b->len = len;
    b->cap = cap;
    b->max = max;
    b->flags = ZB_BUF_OWNED | ZB_BUF_GROWABLE;
    b->release = zb_int_release_malloc;
    b->owner = data;
    return ZB_OK;
}

/* Hands the malloc block to the caller, who frees it with free(); b is then
   zeroed. *data is NULL when the buffer never had storage. ZB_ERR_INVALID
   for a buffer that is not owned and growable. */
ZB_INLINE zb_status zb_buf_detach(zb_buf *b, uint8_t **data, size_t *len)
{
    const uint32_t need = ZB_BUF_OWNED | ZB_BUF_GROWABLE;
    if ((b->flags & need) != need) return ZB_ERR_INVALID;
    *data = b->data;
    *len = b->len;
    zb_buf_init(b);
    return ZB_OK;
}

/* ---- growth ---------------------------------------------------------------------- */

/* Room for len + extra bytes; the only function that reallocates.
   ZB_ERR_MEMORY when len + extra overflows or realloc fails, ZB_ERR_LIMIT
   (and ZB_BUF_HIT_LIMIT) when it would pass max, ZB_ERR_INVALID when the
   buffer cannot grow. On failure nothing else changes. */
ZB_INLINE zb_status zb_buf_reserve(zb_buf *b, size_t extra)
{
    size_t need, next;
    uint8_t *p;
    if (zb_int_add(b->len, extra, &need)) {
        b->flags &= ~ZB_BUF_HIT_LIMIT;
        return ZB_ERR_MEMORY;
    }
    if (need <= b->cap) return ZB_OK;
    if (!(b->flags & ZB_BUF_GROWABLE)) return ZB_ERR_INVALID;
    if (b->max && need > b->max) {
        b->flags |= ZB_BUF_HIT_LIMIT;
        return ZB_ERR_LIMIT;
    }
    if (zb_int_grow(b->cap, need, b->max, &next)) {
        b->flags &= ~ZB_BUF_HIT_LIMIT;
        return ZB_ERR_MEMORY;
    }
    p = (uint8_t *)realloc(b->data, next);
    if (!p) {
        b->flags &= ~ZB_BUF_HIT_LIMIT;
        return ZB_ERR_MEMORY;
    }
    b->data = p;
    b->owner = p;
    b->cap = next;
    return ZB_OK;
}

/* ---- appends ---------------------------------------------------------------------- */

/* Reserves n bytes, advances len past them, and returns the slot to write
   them into; NULL on failure (see zb_buf_reserve; the buffer is unchanged).
   For n = 0 the result is never NULL but must not be written through. */
ZB_INLINE uint8_t *zb_put_raw(zb_buf *b, size_t n)
{
    uint8_t *slot;
    if (zb_buf_reserve(b, n)) return NULL;
    /* n == 0 on a buffer with no storage: any non-NULL pointer will do, and
       one into the struct keeps the header free of static state */
    if (!b->data) return (uint8_t *)b;
    slot = b->data + b->len;
    b->len += n;
    return slot;
}

ZB_INLINE zb_status zb_put_bytes(zb_buf *b, const void *p, size_t n)
{
    zb_status st = zb_buf_reserve(b, n);
    if (st) return st;
    if (n) {
        memcpy(b->data + b->len, p, n);
        b->len += n;
    }
    return ZB_OK;
}

ZB_INLINE zb_status zb_put_zeros(zb_buf *b, size_t n)
{
    zb_status st = zb_buf_reserve(b, n);
    if (st) return st;
    if (n) {
        memset(b->data + b->len, 0, n);
        b->len += n;
    }
    return ZB_OK;
}

#define ZB_INT_PUT(name, ctype, width)                                                 \
    ZB_INLINE zb_status zb_put_##name(zb_buf *b, ctype v)                              \
    {                                                                                  \
        zb_status st = zb_buf_reserve(b, (width));                                     \
        if (st) return st;                                                             \
        zb_wr_##name(b->data + b->len, v);                                             \
        b->len += (width);                                                             \
        return ZB_OK;                                                                  \
    }                                                                                  \
    ZB_INLINE zb_status zb_put_##name##_n(zb_buf *b, const ctype *v, size_t n)         \
    {                                                                                  \
        size_t i, total;                                                               \
        uint8_t *p;                                                                    \
        zb_status st = zb_int_mul(n, (width), &total);                                 \
        if (st) return st;                                                             \
        st = zb_buf_reserve(b, total);                                                 \
        if (st) return st;                                                             \
        if (!n) return ZB_OK;                                                          \
        p = b->data + b->len;                                                          \
        for (i = 0; i < n; i++) zb_wr_##name(p + i * (width), v[i]);                   \
        b->len += total;                                                               \
        return ZB_OK;                                                                  \
    }

ZB_INT_PUT(u8, uint8_t, 1)
ZB_INT_PUT(i8, int8_t, 1)
ZB_INT_PUT(u16le, uint16_t, 2)
ZB_INT_PUT(u16be, uint16_t, 2)
ZB_INT_PUT(i16le, int16_t, 2)
ZB_INT_PUT(i16be, int16_t, 2)
ZB_INT_PUT(u32le, uint32_t, 4)
ZB_INT_PUT(u32be, uint32_t, 4)
ZB_INT_PUT(i32le, int32_t, 4)
ZB_INT_PUT(i32be, int32_t, 4)
ZB_INT_PUT(u64le, uint64_t, 8)
ZB_INT_PUT(u64be, uint64_t, 8)
ZB_INT_PUT(i64le, int64_t, 8)
ZB_INT_PUT(i64be, int64_t, 8)
ZB_INT_PUT(f16le, double, 2)
ZB_INT_PUT(f16be, double, 2)
ZB_INT_PUT(bf16le, double, 2)
ZB_INT_PUT(bf16be, double, 2)
ZB_INT_PUT(f32le, float, 4)
ZB_INT_PUT(f32be, float, 4)
ZB_INT_PUT(f64le, double, 8)
ZB_INT_PUT(f64be, double, 8)

#endif /* ZUBIN_BUF_H */
