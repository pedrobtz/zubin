/*
 * zubin/cursor.h -- checked sequential reads (design 10).
 *
 *     zb_cur c;
 *     uint32_t magic;
 *     zb_cur_init(&c, data, n);
 *     if (zb_cur_u32le(&c, &magic)) return ZB_ERR_EOF;    -- truncated at c.pos
 *
 * A cursor is a base pointer, a length and a 0-based position, so every error
 * a format reader reports is "at byte c.pos". The cursor never writes through
 * base and never allocates. On ZB_ERR_EOF it is unchanged: the read that
 * failed did not advance it and wrote nothing to its out-parameter.
 *
 * One typed read per reader of rw.h, taking a pointer to the type that reader
 * returns:
 *
 *     zb_cur_u8 zb_cur_i8
 *     zb_cur_u16le zb_cur_u16be zb_cur_i16le zb_cur_i16be
 *     zb_cur_u32le zb_cur_u32be zb_cur_i32le zb_cur_i32be
 *     zb_cur_u64le zb_cur_u64be zb_cur_i64le zb_cur_i64be
 *     zb_cur_f16le zb_cur_f16be zb_cur_bf16le zb_cur_bf16be    (double *)
 *     zb_cur_f32le zb_cur_f32be                                (float *)
 *     zb_cur_f64le zb_cur_f64be                                (double *)
 */
#ifndef ZUBIN_CURSOR_H
#define ZUBIN_CURSOR_H

#include "detail/portability.h"
#include "status.h"
#include "rw.h"

typedef struct {
    const uint8_t *base;
    size_t         len;
    size_t         pos;
} zb_cur;

/* A cursor at position 0 over the n bytes at p. p may be NULL when n is 0. */
ZB_INLINE void zb_cur_init(zb_cur *c, const void *p, size_t n)
{
    c->base = (const uint8_t *)p;
    c->len = n;
    c->pos = 0;
}

/* Bytes between the position and the end. */
ZB_INLINE size_t zb_cur_remaining(const zb_cur *c)
{
    return c->len - c->pos;
}

/* Move to an absolute position; the end itself is a valid position. */
ZB_INLINE zb_status zb_cur_seek(zb_cur *c, size_t pos)
{
    if (pos > c->len) return ZB_ERR_EOF;
    c->pos = pos;
    return ZB_OK;
}

/* Advance by n bytes without reading them. */
ZB_INLINE zb_status zb_cur_skip(zb_cur *c, size_t n)
{
    if (n > c->len - c->pos) return ZB_ERR_EOF;
    c->pos += n;
    return ZB_OK;
}

/* Borrow the next n bytes: *p points into the cursor's memory, and the
   cursor advances past them. */
ZB_INLINE zb_status zb_cur_bytes(zb_cur *c, const uint8_t **p, size_t n)
{
    if (n > c->len - c->pos) return ZB_ERR_EOF;
    /* base may be NULL for an empty cursor, and NULL + 0 is undefined */
    *p = c->base ? c->base + c->pos : c->base;
    c->pos += n;
    return ZB_OK;
}

#define ZB_INT_CUR_READ(name, ctype, width)                                            \
    ZB_INLINE zb_status zb_cur_##name(zb_cur *c, ctype *out)                           \
    {                                                                                  \
        if (c->len - c->pos < (width)) return ZB_ERR_EOF;                              \
        *out = zb_rd_##name(c->base + c->pos);                                         \
        c->pos += (width);                                                             \
        return ZB_OK;                                                                  \
    }

ZB_INT_CUR_READ(u8, uint8_t, 1)
ZB_INT_CUR_READ(i8, int8_t, 1)
ZB_INT_CUR_READ(u16le, uint16_t, 2)
ZB_INT_CUR_READ(u16be, uint16_t, 2)
ZB_INT_CUR_READ(i16le, int16_t, 2)
ZB_INT_CUR_READ(i16be, int16_t, 2)
ZB_INT_CUR_READ(u32le, uint32_t, 4)
ZB_INT_CUR_READ(u32be, uint32_t, 4)
ZB_INT_CUR_READ(i32le, int32_t, 4)
ZB_INT_CUR_READ(i32be, int32_t, 4)
ZB_INT_CUR_READ(u64le, uint64_t, 8)
ZB_INT_CUR_READ(u64be, uint64_t, 8)
ZB_INT_CUR_READ(i64le, int64_t, 8)
ZB_INT_CUR_READ(i64be, int64_t, 8)
ZB_INT_CUR_READ(f16le, double, 2)
ZB_INT_CUR_READ(f16be, double, 2)
ZB_INT_CUR_READ(bf16le, double, 2)
ZB_INT_CUR_READ(bf16be, double, 2)
ZB_INT_CUR_READ(f32le, float, 4)
ZB_INT_CUR_READ(f32be, float, 4)
ZB_INT_CUR_READ(f64le, double, 8)
ZB_INT_CUR_READ(f64be, double, 8)

#endif /* ZUBIN_CURSOR_H */
