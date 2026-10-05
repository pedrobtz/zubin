/* Calls every public function: must compile warning-free as C99 and C++11
   (design 16.1). Each stage adds the calls for the headers it lands. */
#include <zubin.h>

#define RW(name, ctype, width)                                   \
    do {                                                         \
        ctype v = zb_rd_##name(buf);                             \
        zb_wr_##name(buf + (width), v);                          \
        acc += (int)buf[(width)];                                \
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

    /* zufast/utf8.h, re-exported by the umbrella */
    acc += zuf_utf8_valid((const char *)buf, sizeof buf);

    return acc;
}
