/* The sink form (issue #25): an object built from the input is serialized
   through zb_serialize_to_sink into fixed blocks of an input-chosen size, as
   rdz's pipeline does, and must equal the builder form byte for byte; with
   the header skipped it must equal the builder form minus the header. Needs R. */
#include "rfuzz.h"

#include <stdlib.h>

typedef struct {
    zb_rfuzz_in in;
} job;

typedef struct {
    zb_buf *out;
    uint8_t block[257];
    size_t chunk, used;
} blocks;

static void flush(blocks *s)
{
    if (s->used) FUZZ_CHECK(zb_put_bytes(s->out, s->block, s->used) == ZB_OK);
    s->used = 0;
}

static void put(void *state, const void *p, size_t n)
{
    blocks *s = (blocks *)state;
    const uint8_t *q = (const uint8_t *)p;
    while (n) {
        size_t take = s->chunk - s->used < n ? s->chunk - s->used : n;
        memcpy(s->block + s->used, q, take);
        s->used += take;
        q += take;
        n -= take;
        if (s->used == s->chunk) flush(s);
    }
}

static void run(void *data)
{
    job *j = (job *)data;
    zb_status st;
    uint8_t flags = zb_rfuzz_u8(&j->in);
    int version = flags & 1 ? 3 : 2, xdr = (flags >> 1) & 1, skip = (flags >> 2) & 1;
    size_t chunk = 1 + zb_rfuzz_u8(&j->in) % 256, head;
    SEXP x = PROTECT(zb_rfuzz_object(&j->in, 3));
    SEXP p1 = PROTECT(zb_r_buf_new(0, 0, &st)), p2;
    zb_buf *whole = zb_r_buf_get(p1);
    blocks s;
    FUZZ_CHECK(st == ZB_OK);
    p2 = PROTECT(zb_r_buf_new(0, 0, &st));
    FUZZ_CHECK(st == ZB_OK);
    FUZZ_CHECK(zb_serialize(x, whole, version, xdr, R_NilValue) == ZB_OK);
    memset(&s, 0, sizeof s);
    s.out = zb_r_buf_get(p2);
    s.chunk = chunk;
    zb_serialize_to_sink(x, put, &s, version, xdr, skip);
    flush(&s);
    head = 0;
    if (skip) {
        uint32_t nelen = 0;
        head = 14;
        if (version == 3) {
            if (xdr) nelen = zuf_load_be32(whole->data + 14);
            else memcpy(&nelen, whole->data + 14, 4);
            head = 18 + nelen;
        }
    }
    FUZZ_CHECK(s.out->len + head == whole->len);
    FUZZ_CHECK(memcmp(s.out->data, whole->data + head, s.out->len) == 0);
    zb_r_buf_free(p1);
    zb_r_buf_free(p2);
    UNPROTECT(3);
}

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    if (zb_rfuzz_init()) __builtin_trap();
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    job j;
    FUZZ_CANARY(data, size);
    if (size > 4096) return 0;
    j.in.p = data;
    j.in.n = size;
    j.in.i = 0;
    FUZZ_CHECK(R_ToplevelExec(run, &j));
    return 0;
}
