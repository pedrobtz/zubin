/* Calls every public function: must compile warning-free as C99 and C++11
   (design 16.1). Each stage adds the calls for the headers it lands. */
#include <zubin.h>

#define RW(name, ctype, width)                                   \
    do {                                                         \
        ctype v = zb_rd_##name(buf);                             \
        zb_wr_##name(buf + (width), v);                          \
        acc += (int)buf[(width)];                                \
    } while (0)

#define PUT(name, ctype)                                         \
    do {                                                         \
        ctype v[2] = {1, 2};                                     \
        acc += (int)zb_put_##name(&b, v[0]);                     \
        acc += (int)zb_put_##name##_n(&b, v, 2);                 \
    } while (0)

#define CUR(name, ctype)                                         \
    do {                                                         \
        ctype v;                                                 \
        acc += (int)zb_cur_##name(&c, &v);                       \
    } while (0)

int zb_probe_all(void);
int zb_probe_all(void)
{
    int acc = 0;
    unsigned char buf[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    /* version.h */
    acc += ZUBIN_VERSION_NUMBER >= 0;
    acc += (int)sizeof(ZUBIN_VERSION);
    acc += ZUBIN_VERSION_MAJOR + ZUBIN_VERSION_MINOR + ZUBIN_VERSION_PATCH;

    /* status.h */
    {
        zb_status s = ZB_OK;
        acc += (int)zb_status_string(s)[0];
        acc += (int)zb_status_string(ZB_ERR_INVALID)[0] + (int)zb_status_string(ZB_ERR_EOF)[0];
        acc += (int)zb_status_string(ZB_ERR_RANGE)[0] + (int)zb_status_string(ZB_ERR_MEMORY)[0];
        acc += (int)zb_status_string(ZB_ERR_LIMIT)[0] + (int)zb_status_string(ZB_ERR_SPEC)[0];
        acc += (int)zb_status_string(ZB_ERR_NA)[0];
    }

    /* rw.h */
    acc += zb_host_big_endian();
    RW(u8, uint8_t, 1);       RW(i8, int8_t, 1);
    RW(u16le, uint16_t, 2);   RW(u16be, uint16_t, 2);
    RW(i16le, int16_t, 2);    RW(i16be, int16_t, 2);
    RW(u32le, uint32_t, 4);   RW(u32be, uint32_t, 4);
    RW(i32le, int32_t, 4);    RW(i32be, int32_t, 4);
    RW(u64le, uint64_t, 8);   RW(u64be, uint64_t, 8);
    RW(i64le, int64_t, 8);    RW(i64be, int64_t, 8);
    RW(f16le, double, 2);     RW(f16be, double, 2);
    RW(bf16le, double, 2);    RW(bf16be, double, 2);
    RW(f32le, float, 4);      RW(f32be, float, 4);
    RW(f64le, double, 8);     RW(f64be, double, 8);

    /* cursor.h */
    {
        zb_cur c;
        const uint8_t *p = NULL;
        zb_cur_init(&c, buf, sizeof buf);
        acc += (int)zb_cur_remaining(&c);
        acc += (int)zb_cur_seek(&c, 1);
        acc += (int)zb_cur_skip(&c, 1);
        acc += (int)zb_cur_bytes(&c, &p, 1);
        acc += p ? (int)p[0] : 0;
        acc += (int)zb_cur_seek(&c, 0);
        CUR(u8, uint8_t);       CUR(i8, int8_t);
        CUR(u16le, uint16_t);   CUR(u16be, uint16_t);
        CUR(i16le, int16_t);    CUR(i16be, int16_t);
        CUR(u32le, uint32_t);   CUR(u32be, uint32_t);
        CUR(i32le, int32_t);    CUR(i32be, int32_t);
        CUR(u64le, uint64_t);   CUR(u64be, uint64_t);
        CUR(i64le, int64_t);    CUR(i64be, int64_t);
        CUR(f16le, double);     CUR(f16be, double);
        CUR(bf16le, double);    CUR(bf16be, double);
        CUR(f32le, float);      CUR(f32be, float);
        CUR(f64le, double);     CUR(f64be, double);
    }

    /* buf.h */
    {
        zb_buf b;
        uint8_t *data = NULL, *slot;
        size_t len = 0, r = 0;
        zb_buf_init(&b);
        acc += (int)zb_buf_alloc(&b, 16, 1024);
        acc += (int)zb_buf_reserve(&b, 32);
        acc += (int)zb_put_bytes(&b, buf, 4);
        acc += (int)zb_put_zeros(&b, 4);
        slot = zb_put_raw(&b, 2);
        if (slot) slot[0] = slot[1] = 0;
        PUT(u8, uint8_t);       PUT(i8, int8_t);
        PUT(u16le, uint16_t);   PUT(u16be, uint16_t);
        PUT(i16le, int16_t);    PUT(i16be, int16_t);
        PUT(u32le, uint32_t);   PUT(u32be, uint32_t);
        PUT(i32le, int32_t);    PUT(i32be, int32_t);
        PUT(u64le, uint64_t);   PUT(u64be, uint64_t);
        PUT(i64le, int64_t);    PUT(i64be, int64_t);
        PUT(f16le, double);     PUT(f16be, double);
        PUT(bf16le, double);    PUT(bf16be, double);
        PUT(f32le, float);      PUT(f32be, float);
        PUT(f64le, double);     PUT(f64be, double);
        acc += (int)(b.flags & (ZB_BUF_OWNED | ZB_BUF_GROWABLE | ZB_BUF_HIT_LIMIT));
        zb_buf_reset(&b);
        acc += (int)zb_buf_detach(&b, &data, &len);
        free(data);
        zb_buf_release(&b);
        zb_buf_borrow(&b, buf, sizeof buf);
        zb_buf_release(&b);
        acc += (int)zb_int_add(1, 2, &r) + (int)zb_int_mul(3, 4, &r) + (int)r;
        acc += (int)(ZB_BUF_DOUBLING_LIMIT > ZB_BUF_MIN_CAP);
    }

    /* zufast/utf8.h, re-exported by the umbrella */
    acc += zuf_utf8_valid((const char *)buf, sizeof buf);

    return acc;
}
