/* A spec and a byte string (design 16.6): every field of every whole record
   is decoded by its kernel, and nothing may read outside the bytes or write
   outside the columns. The input is one flag byte (bit 0: big-endian
   default, bit 1: aligned, bits 2-4: extra stride), the spec up to the
   first newline, then the bytes, which are copied to an exact-size heap
   block so ASan sees any overread. */
#include "fuzz.h"

#include <stdlib.h>

#define MAX_FIELDS 512

static zb_field fields[MAX_FIELDS];

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
        if (f->type == ZB_PAD) continue;
        switch (f->type) {
        case ZB_U8: case ZB_I8: case ZB_U16: case ZB_I16: case ZB_I32: case ZB_BOOL: {
            int32_t *d = (int32_t *)malloc((m ? m : 1) * sizeof *d);
            if (!d) break;
            st = zb_unpack_i32(bytes, n, stride, f, d, 1, &bad);
            FUZZ_CHECK(st == ZB_OK);
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
            if (f->type == ZB_U32) for (i = 0; i < m; i++) FUZZ_CHECK(d[i] >= 0 && d[i] <= 4294967295.0);
            free(d);
            break;
        }
        case ZB_I64: case ZB_U64: {
            int64_t *d = (int64_t *)malloc((m ? m : 1) * sizeof *d);
            double *e = (double *)malloc((m ? m : 1) * sizeof *e);
            if (d && e) {
                st = zb_unpack_i64(bytes, n, stride, f, d, &bad);
                FUZZ_CHECK(st == ZB_OK || (st == ZB_ERR_RANGE && f->type == ZB_U64 && bad < n));
                st = zb_unpack_f64x(bytes, n, stride, f, e, &bad);
                FUZZ_CHECK(st == ZB_OK || (st == ZB_ERR_RANGE && bad < n));
                if (st == ZB_OK) for (i = 0; i < m; i++) FUZZ_CHECK(e[i] <= 9007199254740992.0 && e[i] >= -9007199254740992.0);
            }
            free(d);
            free(e);
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
    }
    free(bytes);
    return 0;
}
