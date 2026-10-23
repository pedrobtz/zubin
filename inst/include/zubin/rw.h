/*
 * zubin/rw.h -- typed reads and writes at any alignment and byte order (design 8).
 *
 * zufast's bits.h does the integer loads and stores and the binary16 and
 * bfloat16 conversions; this header adds one name per (type, byte order) on
 * top, which is what a layout kernel, a cursor and a buffer append call:
 *
 *     zb_rd_u8   zb_rd_i8                       one byte, no order
 *     zb_rd_u16le zb_rd_u16be  zb_rd_i16le zb_rd_i16be
 *     zb_rd_u32le zb_rd_u32be  zb_rd_i32le zb_rd_i32be
 *     zb_rd_u64le zb_rd_u64be  zb_rd_i64le zb_rd_i64be
 *     zb_rd_f16le zb_rd_f16be  zb_rd_bf16le zb_rd_bf16be    widened to double
 *     zb_rd_f32le zb_rd_f32be                    float
 *     zb_rd_f64le zb_rd_f64be                    double
 *     zb_rd_u128le zb_rd_u128be                  zb_u128, the bits of u128 and i128
 *
 * and a zb_wr_ twin for each, taking the same C type the reader returns.
 * For a width known only at run time, 1 to 8 bytes (a format whose head
 * says how wide the integer after it is):
 *
 *     zb_rd_uintle(p, w) zb_rd_uintbe(p, w)      zero-extended to uint64_t
 *     zb_rd_intle(p, w)  zb_rd_intbe(p, w)       sign-extended to int64_t
 *     zb_wr_uintle(p, w, v) zb_wr_uintbe(p, w, v)   the low w bytes of v
 *
 * zb_f64_to_f32() is the double to float narrowing every f32 writer uses.
 *
 * Every access goes through memcpy inside zufast, so a pointer at any offset
 * is safe on strict-alignment targets and under UBSan. Signed values are
 * two's complement in the bytes and in the C type (int32_t and friends are
 * required to be), so a reader reinterprets the bits and a writer stores
 * them; neither depends on implementation-defined conversions.
 *
 * The f16 and bf16 writers narrow from double to float and then to the half
 * format, each step rounding to nearest with ties to even. A double that
 * lies exactly halfway between two halves only after the first step is
 * rounded twice; the tests pin that behaviour rather than claim single
 * rounding (design 8).
 *
 * Threads: every function here is pure, or reads and writes only the
 * buffer, cursor or arrays it is passed; no zubin header holds static or
 * global state (tools/check-headers refuses a static object under zubin/).
 * Any function may be called from any thread, as long as no two threads use
 * one buffer, cursor or output array at the same time (design 15).
 */
#ifndef ZUBIN_RW_H
#define ZUBIN_RW_H

#include "detail/portability.h"
#include <zufast/bits.h>

ZB_STATIC_ASSERT(sizeof(float) == 4, rw_float_is_binary32);
ZB_STATIC_ASSERT(sizeof(double) == 8, rw_double_is_binary64);

/* ---- internal bit casts --------------------------------------------------- */

ZB_INLINE float    zb_int_bits_f32(uint32_t b) { float v;   memcpy(&v, &b, 4); return v; }
ZB_INLINE uint32_t zb_int_f32_bits(float v)    { uint32_t b; memcpy(&b, &v, 4); return b; }
ZB_INLINE double   zb_int_bits_f64(uint64_t b) { double v;  memcpy(&v, &b, 8); return v; }
ZB_INLINE uint64_t zb_int_f64_bits(double v)   { uint64_t b; memcpy(&b, &v, 8); return b; }
ZB_INLINE int16_t  zb_int_u16_i16(uint16_t u)  { int16_t v; memcpy(&v, &u, 2); return v; }
ZB_INLINE int32_t  zb_int_u32_i32(uint32_t u)  { int32_t v; memcpy(&v, &u, 4); return v; }
ZB_INLINE int64_t  zb_int_u64_i64(uint64_t u)  { int64_t v; memcpy(&v, &u, 8); return v; }

/* double to float, rounding to nearest even, without the undefined
   behaviour of converting a finite double beyond FLT_MAX: those round to
   FLT_MAX below the halfway point to 2^128 and to infinity from it (FLT_MAX
   has an odd significand, so the tie goes up). A NaN stays a NaN. What
   zb_wr_f32le() is handed when the value starts as a double. */
ZB_INLINE float zb_f64_to_f32(double v)
{
    if (v > 3.4028234663852886e38) {
        return v >= 3.4028235677973366e38 ? zb_int_bits_f32(0x7F800000u)
                                          : zb_int_bits_f32(0x7F7FFFFFu);
    }
    if (v < -3.4028234663852886e38) {
        return v <= -3.4028235677973366e38 ? zb_int_bits_f32(0xFF800000u)
                                           : zb_int_bits_f32(0xFF7FFFFFu);
    }
    return (float)v;
}

/* The internal name it had before it was public, kept for the callers that
   used it. */
ZB_INLINE float zb_int_f64_to_f32(double v) { return zb_f64_to_f32(v); }

/* ---- host byte order -------------------------------------------------------- */

/* 1 on a big-endian host, 0 on a little-endian one; what "native" and the
   "=" layout prefix resolve to (design 14.2). */
ZB_INLINE int zb_host_big_endian(void)
{
    const uint16_t one = 1;
    unsigned char b[2];
    memcpy(b, &one, 2);
    return b[0] == 0;
}

/* ---- one byte ----------------------------------------------------------------- */

ZB_INLINE uint8_t zb_rd_u8(const void *p) { uint8_t v; memcpy(&v, p, 1); return v; }
ZB_INLINE int8_t  zb_rd_i8(const void *p) { int8_t v;  memcpy(&v, p, 1); return v; }
ZB_INLINE void    zb_wr_u8(void *p, uint8_t v) { memcpy(p, &v, 1); }
ZB_INLINE void    zb_wr_i8(void *p, int8_t v)  { memcpy(p, &v, 1); }

/* ---- integers ------------------------------------------------------------------- */

#define ZB_INT_RW_INT(bits, ord)                                                       \
    ZB_INLINE uint##bits##_t zb_rd_u##bits##ord(const void *p)                         \
    { return zuf_load_##ord##bits(p); }                                                \
    ZB_INLINE int##bits##_t zb_rd_i##bits##ord(const void *p)                          \
    { return zb_int_u##bits##_i##bits(zuf_load_##ord##bits(p)); }                      \
    ZB_INLINE void zb_wr_u##bits##ord(void *p, uint##bits##_t v)                       \
    { zuf_store_##ord##bits(p, v); }                                                   \
    ZB_INLINE void zb_wr_i##bits##ord(void *p, int##bits##_t v)                        \
    { zuf_store_##ord##bits(p, (uint##bits##_t)v); }

ZB_INT_RW_INT(16, le)
ZB_INT_RW_INT(16, be)
ZB_INT_RW_INT(32, le)
ZB_INT_RW_INT(32, be)
ZB_INT_RW_INT(64, le)
ZB_INT_RW_INT(64, be)

/* ---- floating point ------------------------------------------------------------ */

#define ZB_INT_RW_FLOAT(ord)                                                           \
    ZB_INLINE double zb_rd_f16##ord(const void *p)                                     \
    { return (double)zuf_f16_to_f32(zuf_load_##ord##16(p)); }                          \
    ZB_INLINE void zb_wr_f16##ord(void *p, double v)                                   \
    { zuf_store_##ord##16(p, zuf_f32_to_f16(zb_f64_to_f32(v))); }                  \
    ZB_INLINE double zb_rd_bf16##ord(const void *p)                                    \
    { return (double)zuf_bf16_to_f32(zuf_load_##ord##16(p)); }                         \
    ZB_INLINE void zb_wr_bf16##ord(void *p, double v)                                  \
    { zuf_store_##ord##16(p, zuf_f32_to_bf16(zb_f64_to_f32(v))); }                 \
    ZB_INLINE float zb_rd_f32##ord(const void *p)                                      \
    { return zb_int_bits_f32(zuf_load_##ord##32(p)); }                                 \
    ZB_INLINE void zb_wr_f32##ord(void *p, float v)                                    \
    { zuf_store_##ord##32(p, zb_int_f32_bits(v)); }                                    \
    ZB_INLINE double zb_rd_f64##ord(const void *p)                                     \
    { return zb_int_bits_f64(zuf_load_##ord##64(p)); }                                 \
    ZB_INLINE void zb_wr_f64##ord(void *p, double v)                                   \
    { zuf_store_##ord##64(p, zb_int_f64_bits(v)); }

ZB_INT_RW_FLOAT(le)
ZB_INT_RW_FLOAT(be)

/* ---- 128 bits ------------------------------------------------------------------- */

/* The 16 bytes of a u128 or an i128 (two's complement), as two native
   words: lo the least significant. A container, not an arithmetic type:
   zufast's wide.h and zuint do arithmetic. lo first is also the order of
   zuint's payload and of an Arrow buffer on a little-endian host. */
typedef struct {
    uint64_t lo;
    uint64_t hi;
} zb_u128;

ZB_INLINE zb_u128 zb_rd_u128le(const void *p)
{
    zb_u128 v;
    v.lo = zuf_load_le64(p);
    v.hi = zuf_load_le64((const unsigned char *)p + 8);
    return v;
}
ZB_INLINE zb_u128 zb_rd_u128be(const void *p)
{
    zb_u128 v;
    v.hi = zuf_load_be64(p);
    v.lo = zuf_load_be64((const unsigned char *)p + 8);
    return v;
}
ZB_INLINE void zb_wr_u128le(void *p, zb_u128 v)
{
    zuf_store_le64(p, v.lo);
    zuf_store_le64((unsigned char *)p + 8, v.hi);
}
ZB_INLINE void zb_wr_u128be(void *p, zb_u128 v)
{
    zuf_store_be64(p, v.hi);
    zuf_store_be64((unsigned char *)p + 8, v.lo);
}

/* ---- a width chosen at run time -------------------------------------------------- */

/* w is 1 to 8; any other width reads 0 and writes nothing, so a caller
   checks the width it took from the data before it gets here. */
ZB_INLINE uint64_t zb_rd_uintle(const void *p, unsigned w)
{
    const unsigned char *b = (const unsigned char *)p;
    uint64_t v = 0;
    unsigned k;
    if (w < 1 || w > 8) return 0;
    for (k = w; k-- > 0;) v = v << 8 | b[k];
    return v;
}
ZB_INLINE uint64_t zb_rd_uintbe(const void *p, unsigned w)
{
    const unsigned char *b = (const unsigned char *)p;
    uint64_t v = 0;
    unsigned k;
    if (w < 1 || w > 8) return 0;
    for (k = 0; k < w; k++) v = v << 8 | b[k];
    return v;
}
/* The w-byte value with its top bit as the sign. */
ZB_INLINE int64_t zb_int_sign_extend(uint64_t u, unsigned w)
{
    uint64_t sign;
    if (w < 1 || w >= 8) return zb_int_u64_i64(u);
    sign = (uint64_t)1 << (8 * w - 1);
    return zb_int_u64_i64((u ^ sign) - sign);
}
ZB_INLINE int64_t zb_rd_intle(const void *p, unsigned w) { return zb_int_sign_extend(zb_rd_uintle(p, w), w); }
ZB_INLINE int64_t zb_rd_intbe(const void *p, unsigned w) { return zb_int_sign_extend(zb_rd_uintbe(p, w), w); }
ZB_INLINE void zb_wr_uintle(void *p, unsigned w, uint64_t v)
{
    unsigned char *b = (unsigned char *)p;
    unsigned k;
    if (w < 1 || w > 8) return;
    for (k = 0; k < w; k++, v >>= 8) b[k] = (unsigned char)(v & 0xFF);
}
ZB_INLINE void zb_wr_uintbe(void *p, unsigned w, uint64_t v)
{
    unsigned char *b = (unsigned char *)p;
    unsigned k;
    if (w < 1 || w > 8) return;
    for (k = w; k-- > 0; v >>= 8) b[k] = (unsigned char)(v & 0xFF);
}

#endif /* ZUBIN_RW_H */
