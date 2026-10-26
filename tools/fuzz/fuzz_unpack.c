/* A spec and a byte string (design 16.6): every field of every whole record
   is decoded by its kernel, and nothing may read outside the bytes or write
   outside the columns. Each decoded column is then packed back by the
   matching zb_pack_* kernel into the record bytes with that field zeroed,
   which must give the input again (but for a bool byte other than 0 and 1,
   and a NaN, which the narrower floats quiet), and the input's bytes taken
   as arbitrary doubles and int64s are packed into the field, where only a
   status may come back: the casts those kernels guard are UB that UBSan
   would report. The input is one flag byte (bit 0: big-endian default,
   bit 1: aligned, bits 2-4: extra stride, bit 5: allow_na for the arbitrary
   values), the spec up to the first newline, then the bytes, which are
   copied to an exact-size heap block so ASan sees any overread. */
#include "fuzz.h"

#include <stdlib.h>

#define MAX_FIELDS 512

static zb_field fields[MAX_FIELDS];

/* A copy of the records with field f zeroed, for a kernel to pack into. */
static uint8_t *zeroed(const uint8_t *bytes, size_t len, size_t n, size_t stride, const zb_field *f)
{
    uint8_t *copy = (uint8_t *)malloc(len ? len : 1);
    if (!copy) return NULL;
    if (len) memcpy(copy, bytes, len);
    zb_pack_zeros(copy, n, stride, f);
    return copy;
}

/* The status of a kernel that wrote copy, and whether copy is the input. */
static void check_packed(zb_status st, uint8_t *copy, const uint8_t *bytes, size_t len, int same)
{
    FUZZ_CHECK(st == ZB_OK);
    if (same) FUZZ_CHECK(len == 0 || memcmp(copy, bytes, len) == 0);
    free(copy);
}

static int any_nan(const double *d, size_t m)
{
    size_t i;
    for (i = 0; i < m; i++) if (d[i] != d[i]) return 1;
    return 0;
}

/* The i-th 8 bytes of the input, cycled, as a 64-bit pattern. */
static uint64_t pattern(const uint8_t *bytes, size_t len, size_t i)
{
    uint64_t u = 0;
    size_t k;
    for (k = 0; k < 8; k++) u = u << 8 | bytes[(i * 8 + k) % len];
    return u;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    const uint8_t *nl;
    size_t spec_len, len, n, stride, pos = 0, i;
    uint8_t flags, *bytes;
    zb_layout l;
    uint32_t j;
    FUZZ_CANARY(data, size);
    if (size < 2 || size > 8192) return 0;
    flags = data[0];
    nl = (const uint8_t *)memchr(data + 1, '\n', size - 1);
    if (!nl) return 0;
    spec_len = (size_t)(nl - (data + 1));
    if (zb_layout_parse((const char *)data + 1, spec_len, flags & 1, (flags >> 1) & 1, fields,
                        MAX_FIELDS, &l, &pos) != ZB_OK) return 0;
    len = size - spec_len - 2;
    bytes = (uint8_t *)malloc(len ? len : 1);
    if (!bytes) return 0;
    if (len) memcpy(bytes, nl + 1, len);
    stride = (size_t)l.size + ((flags >> 2) & 7);
    n = len < l.size ? 0 : (len - l.size) / stride + 1;
    FUZZ_CHECK(n == 0 || (n - 1) * stride + l.size <= len);

    for (j = 0; j < l.nfields; j++) {
        const zb_field *f = &l.fields[j];
        size_t m = n * f->count, bad = (size_t)-1;
        zb_status st;
        uint8_t *copy;
        if (f->type == ZB_PAD) continue;
        switch (f->type) {
        case ZB_U8: case ZB_I8: case ZB_U16: case ZB_I16: case ZB_I32: case ZB_BOOL: {
            int32_t *d = (int32_t *)malloc((m ? m : 1) * sizeof *d);
            if (!d) break;
            st = zb_unpack_i32(bytes, n, stride, f, d, 1, &bad);
            FUZZ_CHECK(st == ZB_OK);
            if ((copy = zeroed(bytes, len, n, stride, f)))
                check_packed(zb_pack_i32(copy, n, stride, f, d, 1, &bad), copy, bytes, len,
                             f->type != ZB_BOOL);
            st = zb_unpack_i32(bytes, n, stride, f, d, 0, &bad);
            FUZZ_CHECK(st == ZB_OK || (st == ZB_ERR_RANGE && f->type == ZB_I32 && bad < n));
            if (f->type == ZB_BOOL) for (i = 0; i < m; i++) FUZZ_CHECK(d[i] == 0 || d[i] == 1);
            if (f->type == ZB_U8) for (i = 0; i < m; i++) FUZZ_CHECK(d[i] >= 0 && d[i] <= 255);
            free(d);
            break;
        }
        case ZB_U32: case ZB_F16: case ZB_BF16: case ZB_F32: case ZB_F64: {
            double *d = (double *)malloc((m ? m : 1) * sizeof *d);
            if (!d) break;
            FUZZ_CHECK(zb_unpack_f64(bytes, n, stride, f, d) == ZB_OK);
            if ((copy = zeroed(bytes, len, n, stride, f)))
                check_packed(zb_pack_f64(copy, n, stride, f, d, 0, &bad), copy, bytes, len,
                             !any_nan(d, m));
            if (f->type == ZB_U32) for (i = 0; i < m; i++) FUZZ_CHECK(d[i] >= 0 && d[i] <= 4294967295.0);
            free(d);
            break;
        }
        case ZB_I64: case ZB_U64: case ZB_I128: case ZB_U128: {
            int64_t *d = (int64_t *)malloc((m ? m : 1) * sizeof *d);
            double *e = (double *)malloc((m ? m : 1) * sizeof *e);
            if (d && e) {
                st = zb_unpack_i64(bytes, n, stride, f, d, &bad);
                FUZZ_CHECK(st == ZB_OK || (st == ZB_ERR_RANGE && f->type != ZB_I64 && bad < n));
                if (st == ZB_OK && (copy = zeroed(bytes, len, n, stride, f))) {
                    /* INT64_MIN is NA, which only i64 takes back */
                    int na = 0;
                    for (i = 0; i < m; i++) na |= d[i] == INT64_MIN;
                    st = zb_pack_i64(copy, n, stride, f, d, 1, &bad);
                    if (na && f->type != ZB_I64) {
                        FUZZ_CHECK(st == ZB_ERR_NA && bad < n);
                        free(copy);
                    } else {
                        check_packed(st, copy, bytes, len, 1);
                    }
                }
                st = zb_unpack_f64x(bytes, n, stride, f, e, &bad);
                FUZZ_CHECK(st == ZB_OK || (st == ZB_ERR_RANGE && bad < n));
                if (st == ZB_OK) for (i = 0; i < m; i++) FUZZ_CHECK(e[i] <= 9007199254740992.0 && e[i] >= -9007199254740992.0);
                if (st == ZB_OK && (copy = zeroed(bytes, len, n, stride, f)))
                    check_packed(zb_pack_f64(copy, n, stride, f, e, 0, &bad), copy, bytes, len, 1);
            }
            free(d);
            free(e);
            break;
        }
        case ZB_C64: case ZB_C128: {
            double *d = (double *)malloc((m ? 2 * m : 1) * sizeof *d);
            if (!d) break;
            FUZZ_CHECK(zb_unpack_cplx(bytes, n, stride, f, d) == ZB_OK);
            if ((copy = zeroed(bytes, len, n, stride, f)))
                check_packed(zb_pack_cplx(copy, n, stride, f, d), copy, bytes, len,
                             !any_nan(d, 2 * m));
            free(d);
            break;
        }
        case ZB_BYTES: case ZB_STR: {
            uint8_t *d = (uint8_t *)malloc(n * f->size ? n * f->size : 1);
            if (!d) break;
            FUZZ_CHECK(zb_unpack_bytes(bytes, n, stride, f, d) == ZB_OK);
            for (i = 0; i < n; i++) FUZZ_CHECK(memcmp(d + i * f->size, bytes + i * stride + f->offset, f->size) == 0);
            if (f->type == ZB_STR) {
                for (i = 0; i < n; i++) {
                    const char *s = (const char *)d + i * f->size;
                    const char *z = (const char *)memchr(s, 0, f->size);
                    (void)zuf_utf8_valid(s, z ? (size_t)(z - s) : f->size);
                }
            }
            free(d);
            break;
        }
        default:
            FUZZ_CHECK(0);
        }
        /* arbitrary doubles and int64s into a numeric field: a status, and
           the index of a value refused inside the records */
        if (f->type != ZB_BYTES && f->type != ZB_STR && m) {
            double *a = (double *)malloc(2 * m * sizeof *a);
            int64_t *b = (int64_t *)malloc(m * sizeof *b);
            int allow = (flags >> 5) & 1;
            copy = zeroed(bytes, len, n, stride, f);
            if (a && b && copy) {
                for (i = 0; i < 2 * m; i++) {
                    uint64_t u = pattern(bytes, len, i);
                    memcpy(&a[i], &u, sizeof u);
                    if (i < m) memcpy(&b[i], &u, sizeof u);
                }
                if (f->type == ZB_C64 || f->type == ZB_C128) {
                    FUZZ_CHECK(zb_pack_cplx(copy, n, stride, f, a) == ZB_OK);
                } else {
                    bad = (size_t)-1;
                    st = zb_pack_f64(copy, n, stride, f, a, allow, &bad);
                    FUZZ_CHECK(st == ZB_OK || st == ZB_ERR_INVALID || bad < n);
                    bad = (size_t)-1;
                    st = zb_pack_i64(copy, n, stride, f, b, allow, &bad);
                    FUZZ_CHECK(st == ZB_OK || st == ZB_ERR_INVALID || bad < n);
                }
            }
            free(a);
            free(b);
            free(copy);
        }
        /* every value field also round-trips through native order: unpack,
           zero the field, pack, and the record bytes are as they were */
        {
            size_t nb = f->type == ZB_BYTES || f->type == ZB_STR ? n * f->size
                                                                 : m * zb_type_width(f->type);
            uint8_t *nat = (uint8_t *)malloc(nb ? nb : 1);
            uint8_t *copy = (uint8_t *)malloc(len ? len : 1);
            if (nat && copy) {
                FUZZ_CHECK(zb_unpack_native(bytes, n, stride, f, nat) == ZB_OK);
                if (len) memcpy(copy, bytes, len);
                zb_pack_zeros(copy, n, stride, f);
                FUZZ_CHECK(zb_pack_native(copy, n, stride, f, nat) == ZB_OK);
                FUZZ_CHECK(len == 0 || memcmp(copy, bytes, len) == 0);
            }
            free(nat);
            free(copy);
        }
    }
    free(bytes);
    return 0;
}
