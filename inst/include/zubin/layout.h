/*
 * zubin/layout.h -- fixed-size record layouts (design 11).
 *
 * A layout is an array of fields, each a type, an element count, a byte
 * offset and a byte order, parsed from one specification string:
 *
 *     <magic:u32 version:u16 flags:u16 count:u64 name:s32
 *     >magic:u32 minor:u16 major:u16
 *     <xyz:f32[3] rgb:u8[3] x1
 *     <ts:i64 price:f64 qty:i32 side:u8 x3 crc:u32.be
 *
 * The grammar (design 11.2):
 *
 *     layout  := [ endian ] field { sep field }
 *     endian  := "<" | ">" | "="            little, big, host; default from the caller
 *     field   := [ name ":" ] type [ "[" count "]" ] [ "." order ]
 *              | "x" width                   padding: never read, written as zeros
 *     type    := u8 i8 u16 i16 u32 i32 u64 i64 f16 bf16 f32 f64 bool
 *              | "b" width                   fixed bytes
 *              | "s" width                   fixed string
 *     order   := "le" | "be"                 multi-byte numeric types only
 *     name    := [A-Za-z_] [A-Za-z0-9_.]*    unique within a layout
 *     count, width := decimal, 1 .. 2^31-1
 *     sep     := whitespace, or one comma with optional whitespace around it
 *
 * The parser allocates nothing: the caller supplies the field array, and
 * names are borrowed pointers into the specification, which the caller
 * keeps alive while it uses the layout. Every error is ZB_ERR_SPEC with the
 * 0-based byte position of the offending token, except a full field array,
 * which is ZB_ERR_LIMIT.
 *
 * Record sizes and offsets are uint32_t and at most 2^31 - 1, which is also
 * what fits an R integer; record counts are size_t everywhere.
 */
#ifndef ZUBIN_LAYOUT_H
#define ZUBIN_LAYOUT_H

#include "detail/portability.h"
#include "status.h"
#include "rw.h"

/* Values are permanent; 17..31 are reserved for variable-length fields and
   bitfields (design 4.4, 11.1). */
typedef enum {
    ZB_U8 = 1, ZB_I8 = 2, ZB_U16 = 3, ZB_I16 = 4, ZB_U32 = 5, ZB_I32 = 6,
    ZB_U64 = 7, ZB_I64 = 8, ZB_F16 = 9, ZB_BF16 = 10, ZB_F32 = 11, ZB_F64 = 12,
    ZB_BOOL = 13, ZB_BYTES = 14, ZB_STR = 15, ZB_PAD = 16
} zb_type;

typedef struct {
    zb_type     type;
    uint32_t    count;      /* array length; 1 for a scalar; the byte width for b, s and x */
    uint32_t    size;       /* bytes the field occupies in a record: elements * width */
    uint32_t    offset;     /* from the start of the record */
    uint8_t     big_endian; /* 0 for every one-byte type and for b, s and x */
    const char *name;       /* borrowed from the spec; NULL when unnamed */
    uint32_t    name_len;
} zb_field;

typedef struct {
    zb_field *fields;       /* caller-supplied storage */
    uint32_t  nfields;
    uint32_t  size;         /* record size, including trailing alignment padding */
    uint32_t  align;        /* 1 when packed; the largest field alignment when aligned */
} zb_layout;

/* The largest record and the largest count or width. */
#define ZB_LAYOUT_MAX ((uint32_t)0x7FFFFFFF)

/* The width in bytes of one element: 1, 2, 4 or 8 for the numeric types and
   bool, and 1 for b, s and x, whose count is their width. 0 for a value
   outside the enumeration. */
ZB_INLINE uint32_t zb_type_width(zb_type t)
{
    switch (t) {
    case ZB_U8: case ZB_I8: case ZB_BOOL: case ZB_BYTES: case ZB_STR: case ZB_PAD: return 1;
    case ZB_U16: case ZB_I16: case ZB_F16: case ZB_BF16: return 2;
    case ZB_U32: case ZB_I32: case ZB_F32: return 4;
    case ZB_U64: case ZB_I64: case ZB_F64: return 8;
    }
    return 0;
}

/* The type's token in the grammar ("u32", "bf16"), with "b", "s" and "x" for
   the width-carrying types; "?" outside the enumeration. Never NULL. */
ZB_INLINE const char *zb_type_name(zb_type t)
{
    switch (t) {
    case ZB_U8: return "u8";     case ZB_I8: return "i8";
    case ZB_U16: return "u16";   case ZB_I16: return "i16";
    case ZB_U32: return "u32";   case ZB_I32: return "i32";
    case ZB_U64: return "u64";   case ZB_I64: return "i64";
    case ZB_F16: return "f16";   case ZB_BF16: return "bf16";
    case ZB_F32: return "f32";   case ZB_F64: return "f64";
    case ZB_BOOL: return "bool"; case ZB_BYTES: return "b";
    case ZB_STR: return "s";     case ZB_PAD: return "x";
    }
    return "?";
}

/* ---- parsing ------------------------------------------------------------------ */

ZB_INLINE int zb_int_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
ZB_INLINE int zb_int_is_digit(char c) { return c >= '0' && c <= '9'; }
ZB_INLINE int zb_int_is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
ZB_INLINE int zb_int_is_name_char(char c)
{
    return zb_int_is_alpha(c) || zb_int_is_digit(c) || c == '_' || c == '.';
}

/* An upper bound on the number of fields in spec: one more than the number
   of separator characters. Size the field array with it. */
ZB_INLINE uint32_t zb_layout_count_fields(const char *spec, size_t n)
{
    size_t i;
    uint32_t k = 1;
    for (i = 0; i < n; i++) {
        if ((zb_int_is_space(spec[i]) || spec[i] == ',') && k < 0xFFFFFFFFu) k++;
    }
    return k;
}

/* Decimal digits at spec[*i] as a count or width in 1 .. 2^31 - 1; advances
   *i past them. Nonzero when there are none, the value is 0, or it is too
   large. */
ZB_INLINE int zb_int_parse_count(const char *spec, size_t n, size_t *i, uint32_t *out)
{
    uint32_t v = 0;
    size_t j = *i;
    if (j >= n || !zb_int_is_digit(spec[j])) return 1;
    for (; j < n && zb_int_is_digit(spec[j]); j++) {
        v = v * 10 + (uint32_t)(spec[j] - '0');
        if (v > ZB_LAYOUT_MAX) return 1;
    }
    if (v == 0) return 1;
    *i = j;
    *out = v;
    return 0;
}

/* The type named by letters spec[a, b) followed by digits spec[b, c). The
   width-carrying types return their width in *width. 0 when unknown. */
ZB_INLINE int zb_int_type_of(const char *spec, size_t a, size_t b, size_t c, uint32_t *width)
{
    const char *l = spec + a;
    size_t nl = b - a, nd = c - b;
    uint32_t bits = 0;
    size_t j;
    if (nl == 1 && (l[0] == 'b' || l[0] == 's' || l[0] == 'x')) {
        size_t k = b;
        if (zb_int_parse_count(spec, c, &k, width) || k != c) return -1;
        return l[0] == 'b' ? ZB_BYTES : l[0] == 's' ? ZB_STR : ZB_PAD;
    }
    if (nl == 4 && memcmp(l, "bool", 4) == 0 && nd == 0) return ZB_BOOL;
    if (nd == 0 || nd > 2) return 0;
    for (j = b; j < c; j++) bits = bits * 10 + (uint32_t)(spec[j] - '0');
    if (nd == 2 && spec[b] == '0') return 0;
    if (nl == 1 && l[0] == 'u') {
        return bits == 8 ? ZB_U8 : bits == 16 ? ZB_U16 : bits == 32 ? ZB_U32 : bits == 64 ? ZB_U64 : 0;
    }
    if (nl == 1 && l[0] == 'i') {
        return bits == 8 ? ZB_I8 : bits == 16 ? ZB_I16 : bits == 32 ? ZB_I32 : bits == 64 ? ZB_I64 : 0;
    }
    if (nl == 1 && l[0] == 'f') return bits == 16 ? ZB_F16 : bits == 32 ? ZB_F32 : bits == 64 ? ZB_F64 : 0;
    if (nl == 2 && l[0] == 'b' && l[1] == 'f') return bits == 16 ? ZB_BF16 : 0;
    return 0;
}

/* Parses the n bytes of spec into fields[0, max_fields) and *out.
   default_big_endian is the byte order when the spec has no prefix ("=" is
   the host's); align nonzero places each field at its natural alignment
   (design 11.4). On ZB_OK, *out describes the layout and out->fields is
   `fields`. On failure, *out is unchanged, the contents of `fields` are
   unspecified, and *err_pos is the 0-based byte position of the error:
   ZB_ERR_SPEC for a malformed spec, ZB_ERR_LIMIT when it has more than
   max_fields fields (zb_layout_count_fields() never undercounts). */
ZB_INLINE zb_status zb_layout_parse(const char *spec, size_t n,
                                    int default_big_endian, int align,
                                    zb_field *fields, uint32_t max_fields,
                                    zb_layout *out, size_t *err_pos)
{
    size_t i = 0, comma = (size_t)-1;
    uint32_t nf = 0, maxalign = 1;
    uint64_t off = 0;
    int big = default_big_endian ? 1 : 0;

#define ZB_INT_SPEC_FAIL(pos) do { *err_pos = (pos); return ZB_ERR_SPEC; } while (0)

    while (i < n && zb_int_is_space(spec[i])) i++;
    if (i < n && (spec[i] == '<' || spec[i] == '>' || spec[i] == '=')) {
        big = spec[i] == '>' ? 1 : spec[i] == '<' ? 0 : zb_host_big_endian();
        i++;
    }
    for (;;) {
        size_t start, j, tstart, lend, dend;
        uint32_t width = 0, count = 1, elem, a, k;
        uint64_t size;
        int t, fbig;
        const char *name = NULL;
        uint32_t name_len = 0;
        zb_field *f;

        while (i < n && zb_int_is_space(spec[i])) i++;
        if (i == n) {
            if (comma != (size_t)-1) ZB_INT_SPEC_FAIL(comma);   /* a trailing comma */
            if (nf == 0) ZB_INT_SPEC_FAIL(i);                    /* no fields */
            break;
        }
        if (spec[i] == ',') ZB_INT_SPEC_FAIL(i);                 /* an empty field */
        start = i;

        /* name ":" */
        for (j = i; j < n && zb_int_is_name_char(spec[j]); j++) {}
        if (j < n && spec[j] == ':') {
            if (j == i || zb_int_is_digit(spec[i]) || spec[i] == '.') ZB_INT_SPEC_FAIL(i);
            if (j - i > ZB_LAYOUT_MAX) ZB_INT_SPEC_FAIL(i);
            name = spec + i;
            name_len = (uint32_t)(j - i);
            i = j + 1;
        }

        /* type: letters, then digits */
        tstart = i;
        for (lend = i; lend < n && zb_int_is_alpha(spec[lend]); lend++) {}
        for (dend = lend; dend < n && zb_int_is_digit(spec[dend]); dend++) {}
        t = zb_int_type_of(spec, tstart, lend, dend, &width);
        if (t < 0) ZB_INT_SPEC_FAIL(lend);                       /* a zero or overlarge width */
        if (t == 0) ZB_INT_SPEC_FAIL(tstart);                    /* an unknown type */
        i = dend;
        fbig = big;

        /* "[" count "]" */
        if (i < n && spec[i] == '[') {
            if (t == ZB_BYTES || t == ZB_STR || t == ZB_PAD) ZB_INT_SPEC_FAIL(i);
            i++;
            if (zb_int_parse_count(spec, n, &i, &count)) ZB_INT_SPEC_FAIL(i);
            if (i >= n || spec[i] != ']') ZB_INT_SPEC_FAIL(i);
            i++;
        }
        /* "." order */
        if (i < n && spec[i] == '.') {
            if (zb_type_width((zb_type)t) == 1) ZB_INT_SPEC_FAIL(i);
            if (i + 3 > n) ZB_INT_SPEC_FAIL(i);
            if (spec[i + 1] == 'l' && spec[i + 2] == 'e') fbig = 0;
            else if (spec[i + 1] == 'b' && spec[i + 2] == 'e') fbig = 1;
            else ZB_INT_SPEC_FAIL(i);
            i += 3;
        }
        if (i < n && !zb_int_is_space(spec[i]) && spec[i] != ',') ZB_INT_SPEC_FAIL(i);
        if (t == ZB_PAD && name) ZB_INT_SPEC_FAIL(start);

        /* names are unique */
        for (k = 0; name && k < nf; k++) {
            if (fields[k].name && fields[k].name_len == name_len &&
                memcmp(fields[k].name, name, name_len) == 0) ZB_INT_SPEC_FAIL(start);
        }

        /* placement */
        elem = zb_type_width((zb_type)t);
        if (t == ZB_BYTES || t == ZB_STR || t == ZB_PAD) {
            count = width;
            size = width;
        } else {
            size = (uint64_t)count * elem;
        }
        a = align ? elem : 1;   /* 1 for bool, b, s and x */
        off = (off + a - 1) / a * a;
        if (size > ZB_LAYOUT_MAX || off + size > ZB_LAYOUT_MAX) ZB_INT_SPEC_FAIL(start);
        if (a > maxalign) maxalign = a;

        if (nf == max_fields) {
            *err_pos = start;
            return ZB_ERR_LIMIT;
        }
        f = &fields[nf++];
        f->type = (zb_type)t;
        f->count = count;
        f->size = (uint32_t)size;
        f->offset = (uint32_t)off;
        f->big_endian = (uint8_t)(elem > 1 ? fbig : 0);
        f->name = name;
        f->name_len = name_len;
        off += size;

        /* sep: whitespace, or one comma with optional whitespace around it */
        comma = (size_t)-1;
        while (i < n && zb_int_is_space(spec[i])) i++;
        if (i < n && spec[i] == ',') comma = i++;
    }
#undef ZB_INT_SPEC_FAIL

    if (align) {
        off = (off + maxalign - 1) / maxalign * maxalign;
        if (off > ZB_LAYOUT_MAX) {
            *err_pos = n;
            return ZB_ERR_SPEC;
        }
    }
    out->fields = fields;
    out->nfields = nf;
    out->size = (uint32_t)off;
    out->align = align ? maxalign : 1;
    return ZB_OK;
}

/* ---- unpack kernels (design 11.5) ---------------------------------------------- */

/* Each kernel reads one field of n records, the first at base and the rest
   every `stride` bytes, into one typed column. Fields are processed
   field-major: one strided loop per field, which the compiler vectorises.
   Array fields (count > 1) are written column-major: element k of record i
   lands at dst[k * n + i], which is R's matrix layout. The caller has
   checked that every record lies inside its buffer; the kernels do not
   bounds-check and never allocate.

   A kernel that can refuse a value returns its status and writes the index
   of the first record holding one to *bad; what it wrote to dst is then
   unspecified. A field of the wrong type for the kernel is ZB_ERR_INVALID. */

/* The element of record i at array position k. */
#define ZB_INT_ELEM(base, i, stride, f, k, w) \
    ((base) + (i) * (stride) + (f)->offset + (size_t)(k) * (w))

/* u8 i8 u16 i16 i32 bool into int32_t. bool reads 1 for any non-zero byte.
   An i32 of -2^31 is ZB_ERR_RANGE unless allow_na, because it is R's
   NA_integer_ and a file holding it would otherwise acquire a missing value
   silently (design 14.3). */
ZB_INLINE zb_status zb_unpack_i32(const uint8_t *base, size_t n, size_t stride,
                                  const zb_field *f, int32_t *dst, int allow_na, size_t *bad)
{
    uint32_t k, w = zb_type_width(f->type);
    size_t i, first = n;
    int be = f->big_endian;
    switch (f->type) {
    case ZB_U8: case ZB_I8: case ZB_BOOL: case ZB_U16: case ZB_I16: case ZB_I32: break;
    default: return ZB_ERR_INVALID;
    }
    for (k = 0; k < f->count; k++) {
        int32_t *d = dst + (size_t)k * n;
        switch (f->type) {
        case ZB_U8:
            for (i = 0; i < n; i++) d[i] = zb_rd_u8(ZB_INT_ELEM(base, i, stride, f, k, w));
            break;
        case ZB_I8:
            for (i = 0; i < n; i++) d[i] = zb_rd_i8(ZB_INT_ELEM(base, i, stride, f, k, w));
            break;
        case ZB_BOOL:
            for (i = 0; i < n; i++) d[i] = zb_rd_u8(ZB_INT_ELEM(base, i, stride, f, k, w)) != 0;
            break;
        case ZB_U16:
            if (be) for (i = 0; i < n; i++) d[i] = zb_rd_u16be(ZB_INT_ELEM(base, i, stride, f, k, w));
            else    for (i = 0; i < n; i++) d[i] = zb_rd_u16le(ZB_INT_ELEM(base, i, stride, f, k, w));
            break;
        case ZB_I16:
            if (be) for (i = 0; i < n; i++) d[i] = zb_rd_i16be(ZB_INT_ELEM(base, i, stride, f, k, w));
            else    for (i = 0; i < n; i++) d[i] = zb_rd_i16le(ZB_INT_ELEM(base, i, stride, f, k, w));
            break;
        default:   /* ZB_I32 */
            if (be) for (i = 0; i < n; i++) d[i] = zb_rd_i32be(ZB_INT_ELEM(base, i, stride, f, k, w));
            else    for (i = 0; i < n; i++) d[i] = zb_rd_i32le(ZB_INT_ELEM(base, i, stride, f, k, w));
            if (!allow_na) {
                for (i = 0; i < first; i++) {
                    if (d[i] == INT32_MIN) { first = i; break; }
                }
            }
            break;
        }
    }
    if (first < n) {
        *bad = first;
        return ZB_ERR_RANGE;
    }
    return ZB_OK;
}

/* u32 f16 bf16 f32 f64 into double: every value is exact. */
ZB_INLINE zb_status zb_unpack_f64(const uint8_t *base, size_t n, size_t stride,
                                  const zb_field *f, double *dst)
{
    uint32_t k, w = zb_type_width(f->type);
    size_t i;
    int be = f->big_endian;
    switch (f->type) {
    case ZB_U32: case ZB_F16: case ZB_BF16: case ZB_F32: case ZB_F64: break;
    default: return ZB_ERR_INVALID;
    }
    for (k = 0; k < f->count; k++) {
        double *d = dst + (size_t)k * n;
#define ZB_INT_LOOP(rd) for (i = 0; i < n; i++) d[i] = (double)rd(ZB_INT_ELEM(base, i, stride, f, k, w))
        switch (f->type) {
        case ZB_U32:  if (be) ZB_INT_LOOP(zb_rd_u32be);  else ZB_INT_LOOP(zb_rd_u32le);  break;
        case ZB_F16:  if (be) ZB_INT_LOOP(zb_rd_f16be);  else ZB_INT_LOOP(zb_rd_f16le);  break;
        case ZB_BF16: if (be) ZB_INT_LOOP(zb_rd_bf16be); else ZB_INT_LOOP(zb_rd_bf16le); break;
        case ZB_F32:  if (be) ZB_INT_LOOP(zb_rd_f32be);  else ZB_INT_LOOP(zb_rd_f32le);  break;
        default:      if (be) ZB_INT_LOOP(zb_rd_f64be);  else ZB_INT_LOOP(zb_rd_f64le);  break;
        }
#undef ZB_INT_LOOP
    }
    return ZB_OK;
}

/* i64 into int64_t; u64 too while it is below 2^63, ZB_ERR_RANGE from it. */
ZB_INLINE zb_status zb_unpack_i64(const uint8_t *base, size_t n, size_t stride,
                                  const zb_field *f, int64_t *dst, size_t *bad)
{
    uint32_t k;
    size_t i, first = n;
    int be = f->big_endian;
    if (f->type != ZB_I64 && f->type != ZB_U64) return ZB_ERR_INVALID;
    for (k = 0; k < f->count; k++) {
        int64_t *d = dst + (size_t)k * n;
        if (f->type == ZB_I64) {
            if (be) for (i = 0; i < n; i++) d[i] = zb_rd_i64be(ZB_INT_ELEM(base, i, stride, f, k, 8));
            else    for (i = 0; i < n; i++) d[i] = zb_rd_i64le(ZB_INT_ELEM(base, i, stride, f, k, 8));
        } else {
            for (i = 0; i < n; i++) {
                const uint8_t *p = ZB_INT_ELEM(base, i, stride, f, k, 8);
                uint64_t u = be ? zb_rd_u64be(p) : zb_rd_u64le(p);
                if (u > (uint64_t)INT64_MAX) {
                    if (i < first) first = i;
                    break;
                }
                d[i] = (int64_t)u;
            }
        }
    }
    if (first < n) {
        *bad = first;
        return ZB_ERR_RANGE;
    }
    return ZB_OK;
}

/* i64 u64 into double, exactly: a value above 2^53 in magnitude is
   ZB_ERR_RANGE rather than rounded (design 14.4). */
ZB_INLINE zb_status zb_unpack_f64x(const uint8_t *base, size_t n, size_t stride,
                                   const zb_field *f, double *dst, size_t *bad)
{
    const uint64_t lim = (uint64_t)1 << 53;
    uint32_t k;
    size_t i, first = n;
    int be = f->big_endian;
    if (f->type != ZB_I64 && f->type != ZB_U64) return ZB_ERR_INVALID;
    for (k = 0; k < f->count; k++) {
        double *d = dst + (size_t)k * n;
        for (i = 0; i < n; i++) {
            const uint8_t *p = ZB_INT_ELEM(base, i, stride, f, k, 8);
            uint64_t u = be ? zb_rd_u64be(p) : zb_rd_u64le(p);
            if (f->type == ZB_I64) {
                int64_t v = zb_int_u64_i64(u);
                uint64_t mag = v < 0 ? (uint64_t)0 - u : u;
                if (mag > lim) { if (i < first) first = i; break; }
                d[i] = (double)v;
            } else {
                if (u > lim) { if (i < first) first = i; break; }
                d[i] = (double)u;
            }
        }
    }
    if (first < n) {
        *bad = first;
        return ZB_ERR_RANGE;
    }
    return ZB_OK;
}

/* b and s: each record's field bytes, contiguous, record i at dst + i * size. */
ZB_INLINE zb_status zb_unpack_bytes(const uint8_t *base, size_t n, size_t stride,
                                    const zb_field *f, uint8_t *dst)
{
    size_t i;
    if (f->type != ZB_BYTES && f->type != ZB_STR) return ZB_ERR_INVALID;
    for (i = 0; i < n; i++) memcpy(dst + i * f->size, base + i * stride + f->offset, f->size);
    return ZB_OK;
}

#endif /* ZUBIN_LAYOUT_H */
