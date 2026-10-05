/* An input-driven sequence of appends, reserves and resets on a buffer with
   a tiny cap (design 16.6). After every step len <= cap, cap <= max, the
   bytes equal a shadow copy kept by plain memcpy, a failed step changed
   neither, and ZB_BUF_HIT_LIMIT is set exactly when the last failed growth
   was refused by the cap. The first byte is the cap (1-255; 0 means none). */
#include "fuzz.h"

#include <stdlib.h>

#define SHADOW 65536

static uint8_t shadow[SHADOW];

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    zb_buf b;
    size_t i = 1, slen = 0, max;
    FUZZ_CANARY(data, size);
    if (size < 1) return 0;
    max = data[0];
    if (zb_buf_alloc(&b, 0, max) != ZB_OK) return 0;
    while (i < size) {
        uint8_t op = data[i++] & 15, k = i < size ? data[i++] : 0;
        size_t before = b.len, cap = b.cap;
        uint8_t tmp[16 * 8];
        zb_status st = ZB_OK;
        size_t add = 0, req;   /* bytes len grows by; bytes the step asks room for */
        memset(tmp, k, sizeof tmp);
        switch (op) {
        case 0: st = zb_put_u8(&b, k); add = 1; break;
        case 1: st = zb_put_u32le(&b, 0x01020304u * k); add = 4; break;
        case 2: st = zb_put_f64be(&b, (double)k / 3); add = 8; break;
        case 3: { double v[16]; size_t j, m = k % 17;
                  for (j = 0; j < 16; j++) v[j] = (double)j - k;
                  st = zb_put_f16le_n(&b, v, m); add = 2 * m; } break;
        case 4: { int64_t v[16]; size_t j, m = k % 17;
                  for (j = 0; j < 16; j++) v[j] = (int64_t)j * k - 99;
                  st = zb_put_i64be_n(&b, v, m); add = 8 * m; } break;
        case 5: st = zb_put_bytes(&b, tmp, k % 100); add = k % 100; break;
        case 6: st = zb_put_zeros(&b, k % 100); add = k % 100; break;
        case 7: st = zb_buf_reserve(&b, k); add = 0; break;   /* req is k, below */
        case 8: zb_buf_reset(&b); slen = 0; before = 0; break;
        case 9: { uint8_t *p = zb_put_raw(&b, k % 40); st = p ? ZB_OK : ZB_ERR_LIMIT;
                  if (p && k % 40) memset(p, 0xEE, k % 40); add = k % 40; } break;
        case 10: st = zb_put_bf16be(&b, -(double)k); add = 2; break;
        case 11: st = zb_put_i8_n(&b, (const int8_t *)tmp, k % 33); add = k % 33; break;
        default: st = zb_put_u16be(&b, (uint16_t)(k * 257)); add = 2; break;
        }
        req = op == 7 ? k : add;
        FUZZ_CHECK(b.len <= b.cap);
        FUZZ_CHECK(b.max == max);
        if (max) FUZZ_CHECK(b.cap <= max);
        if (st) {
            FUZZ_CHECK(st == ZB_ERR_LIMIT);
            FUZZ_CHECK(b.len == before && b.cap == cap);
            FUZZ_CHECK(b.flags & ZB_BUF_HIT_LIMIT);
            FUZZ_CHECK(max && before + req > max);
        } else if (op != 7 && op != 8) {
            FUZZ_CHECK(b.len == before + add);
            if (slen + add > SHADOW) break;
            /* the shadow takes the new bytes from the buffer, then the old
               ones must still match: an append never disturbs what was there */
            memcpy(shadow + slen, b.data + before, add);
            slen += add;
        }
        FUZZ_CHECK(slen == b.len);
        FUZZ_CHECK(slen == 0 || memcmp(shadow, b.data, slen) == 0);
        if (op == 9 && st == ZB_OK && add) FUZZ_CHECK(b.data[b.len - 1] == 0xEE);
        if (op == 6 && st == ZB_OK && add) FUZZ_CHECK(b.data[b.len - 1] == 0);
    }
    zb_buf_release(&b);
    FUZZ_CHECK(b.data == NULL && b.len == 0 && b.flags == 0);
    return 0;
}
