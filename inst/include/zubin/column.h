/*
 * zubin/column.h -- bitmaps, validity and byte order for columnar buffers
 * (design 11.6).
 *
 * A columnar format (Arrow, Parquet, a database's binary protocol) keeps
 * one buffer per column and marks missing values in a separate bitmap
 * rather than with a sentinel. This header is what turns such a column into
 * the arrays the layout kernels and R use, and back:
 *
 *     zb_bit_get  zb_bit_set              one bit, least significant first
 *     zb_bit_get_msb  zb_bit_set_msb      one bit, most significant first
 *     zb_bitmap_bytes  zb_bitmap_count    sizes and population counts
 *     zb_unpack_bits  zb_pack_bits        bit-packed booleans <-> int32_t 0/1
 *     zb_validity_apply[_i32|_f64]        a cleared bit -> a missing value
 *     zb_validity_from[_i32|_f64]         a missing value -> a cleared bit
 *     zb_swap_copy                        reverse the bytes of every element
 *
 * Bit i of a bitmap is bit (i % 8) of byte i / 8, least significant first:
 * Arrow's and Parquet's order. The _msb functions take the other order,
 * which PNG and the ISO 8583 bitmap use. Every function takes a bit
 * offset, since an Arrow array may start inside its buffers.
 *
 * A validity bitmap has a set bit for a present value and a cleared bit
 * for a missing one, and a NULL bitmap means that every value is present,
 * as in Arrow. The missing-value markers are R's, as everywhere in zubin
 * (design 14.3): INT32_MIN for int32_t, R's NA_real_ for double (a NaN
 * whose low word is 1954; any other NaN is a value), and for any other
 * width a marker the caller passes.
 *
 * Nothing here allocates, and nothing bounds-checks: the caller has sized
 * every buffer, zb_bitmap_bytes() giving a bitmap's.
 *
 * Threads: every function here is pure, or reads and writes only the
 * buffer, cursor or arrays it is passed; no zubin header holds static or
 * global state (tools/check-headers refuses a static object under zubin/).
 * Any function may be called from any thread, as long as no two threads use
 * one buffer, cursor or output array at the same time (design 15).
 */
#ifndef ZUBIN_COLUMN_H
#define ZUBIN_COLUMN_H

#include "detail/portability.h"
#include "status.h"
#include "rw.h"

/* The bits of R's NA_real_; a NaN whose low 32 bits are 1954 is NA. */
#define ZB_NA_F64_BITS ((uint64_t)0x7FF00000000007A2u)

/* ---- bits ------------------------------------------------------------------------ */

ZB_INLINE int zb_bit_get(const uint8_t *bm, size_t i)
{
    return (bm[i >> 3] >> (i & 7)) & 1;
}
ZB_INLINE void zb_bit_set(uint8_t *bm, size_t i, int v)
{
    uint8_t m = (uint8_t)(1u << (i & 7));
    if (v) bm[i >> 3] = (uint8_t)(bm[i >> 3] | m);
    else   bm[i >> 3] = (uint8_t)(bm[i >> 3] & ~m);
}
ZB_INLINE int zb_bit_get_msb(const uint8_t *bm, size_t i)
{
    return (bm[i >> 3] >> (7 - (i & 7))) & 1;
}
ZB_INLINE void zb_bit_set_msb(uint8_t *bm, size_t i, int v)
{
    uint8_t m = (uint8_t)(0x80u >> (i & 7));
    if (v) bm[i >> 3] = (uint8_t)(bm[i >> 3] | m);
    else   bm[i >> 3] = (uint8_t)(bm[i >> 3] & ~m);
}

/* The bytes a bitmap of nbits bits occupies; never overflows. */
ZB_INLINE size_t zb_bitmap_bytes(size_t nbits)
{
    return nbits / 8 + (nbits % 8 != 0);
}

/* The number of set bits in [offset, offset + n): whole bytes at a time
   once aligned. Bit order does not matter for a count of whole bytes, but
   the partial bytes at either end are counted least significant first. */
ZB_INLINE size_t zb_bitmap_count(const uint8_t *bm, size_t offset, size_t n)
{
    size_t c = 0, i = offset, end = offset + n;
    while (i < end && (i & 7)) c += (size_t)zb_bit_get(bm, i++);
    for (; i + 8 <= end; i += 8) {
        unsigned b = bm[i >> 3];
        b = b - ((b >> 1) & 0x55u);
        b = (b & 0x33u) + ((b >> 2) & 0x33u);
        c += (b + (b >> 4)) & 0x0Fu;
    }
    while (i < end) c += (size_t)zb_bit_get(bm, i++);
    return c;
}

/* ---- bit-packed booleans --------------------------------------------------------- */

/* n bits from offset into 0 and 1: an R logical's int values. */
ZB_INLINE void zb_unpack_bits(const uint8_t *bm, size_t offset, size_t n, int32_t *dst)
{
    size_t i;
    for (i = 0; i < n; i++) dst[i] = zb_bit_get(bm, offset + i);
}

/* n int32_t values into bits from offset: 0 clears, any other value sets,
   and INT32_MIN (R's NA) is ZB_ERR_NA at *bad, the bits before it written.
   A boolean column's missing values are its validity bitmap's business
   (zb_validity_from_i32). */
ZB_INLINE zb_status zb_pack_bits(uint8_t *bm, size_t offset, size_t n, const int32_t *src,
                                 size_t *bad)
{
    size_t i;
    for (i = 0; i < n; i++) {
        if (src[i] == INT32_MIN) {
            *bad = i;
            return ZB_ERR_NA;
        }
        zb_bit_set(bm, offset + i, src[i] != 0);
    }
    return ZB_OK;
}

/* ---- validity -------------------------------------------------------------------- */

/* Writes the width-byte marker `na` over every element of dst (n of them,
   width bytes apart) whose bit in valid, from offset, is cleared. Returns
   the number of missing values; 0 at once for a NULL bitmap. */
ZB_INLINE size_t zb_validity_apply(const uint8_t *valid, size_t offset, size_t n, void *dst,
                                   size_t width, const void *na)
{
    size_t i, nulls = 0;
    uint8_t *d = (uint8_t *)dst;
    if (!valid) return 0;
    for (i = 0; i < n; i++) {
        if (!zb_bit_get(valid, offset + i)) {
            memcpy(d + i * width, na, width);
            nulls++;
        }
    }
    return nulls;
}

ZB_INLINE size_t zb_validity_apply_i32(const uint8_t *valid, size_t offset, size_t n,
                                       int32_t *dst)
{
    size_t i, nulls = 0;
    if (!valid) return 0;
    for (i = 0; i < n; i++) {
        if (!zb_bit_get(valid, offset + i)) {
            dst[i] = INT32_MIN;
            nulls++;
        }
    }
    return nulls;
}

ZB_INLINE size_t zb_validity_apply_f64(const uint8_t *valid, size_t offset, size_t n,
                                       double *dst)
{
    size_t i, nulls = 0;
    double na = zb_int_bits_f64(ZB_NA_F64_BITS);
    if (!valid) return 0;
    for (i = 0; i < n; i++) {
        if (!zb_bit_get(valid, offset + i)) {
            dst[i] = na;
            nulls++;
        }
    }
    return nulls;
}

/* Sets bit offset + i of valid when element i of src (n of them, width
   bytes apart) differs from the width-byte marker `na`, and clears it when
   it equals it. Returns the number of missing values. */
ZB_INLINE size_t zb_validity_from(const void *src, size_t n, size_t width, const void *na,
                                  uint8_t *valid, size_t offset)
{
    size_t i, nulls = 0;
    const uint8_t *s = (const uint8_t *)src;
    for (i = 0; i < n; i++) {
        int miss = memcmp(s + i * width, na, width) == 0;
        zb_bit_set(valid, offset + i, !miss);
        nulls += (size_t)miss;
    }
    return nulls;
}

ZB_INLINE size_t zb_validity_from_i32(const int32_t *src, size_t n, uint8_t *valid,
                                      size_t offset)
{
    size_t i, nulls = 0;
    for (i = 0; i < n; i++) {
        int miss = src[i] == INT32_MIN;
        zb_bit_set(valid, offset + i, !miss);
        nulls += (size_t)miss;
    }
    return nulls;
}

/* NA_real_ is missing; NaN is a value, as Arrow keeps them apart. */
ZB_INLINE size_t zb_validity_from_f64(const double *src, size_t n, uint8_t *valid,
                                      size_t offset)
{
    size_t i, nulls = 0;
    for (i = 0; i < n; i++) {
        uint64_t b = zb_int_f64_bits(src[i]);
        int miss = src[i] != src[i] && (b & 0xFFFFFFFFu) == 1954;
        zb_bit_set(valid, offset + i, !miss);
        nulls += (size_t)miss;
    }
    return nulls;
}

/* ---- byte order ------------------------------------------------------------------ */

/* Copies n elements of width bytes from src to dst, reversing the bytes of
   each: a column in the other byte order becomes the host's. dst may be
   src (in place); any other overlap is undefined. A 16-byte element is
   reversed whole, which is right for a 128-bit integer. Width 1 copies. */
ZB_INLINE void zb_swap_copy(void *dst, const void *src, size_t n, size_t width)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i, j;
    if (width <= 1) {
        if (d != s && n) memmove(d, s, n);
        return;
    }
    for (i = 0; i < n; i++, d += width, s += width) {
        for (j = 0; j < width / 2; j++) {
            uint8_t t = s[j];
            d[j] = s[width - 1 - j];
            d[width - 1 - j] = t;
        }
        if (width % 2) d[width / 2] = s[width / 2];
    }
}

#endif /* ZUBIN_COLUMN_H */
