/* The serialization round trip (issue #25): an object built from the input
   is serialized into a buffer owned by R behind a prefix, then unserialized
   from a cursor at the prefix, and must be identical() to what went in, with
   the cursor just past the stream. Then every truncation of the stream the
   input picks must be ZB_ERR_EOF with the cursor unchanged. Needs R. */
#include "rfuzz.h"

typedef struct {
    zb_rfuzz_in in;
} job;

static void run(void *data)
{
    job *j = (job *)data;
    zb_status st;
    zb_cur c;
    uint8_t flags = zb_rfuzz_u8(&j->in), prefix = zb_rfuzz_u8(&j->in) % 17;
    int version = flags & 1 ? 3 : 2, xdr = (flags >> 1) & 1;
    SEXP x = PROTECT(zb_rfuzz_object(&j->in, 3));
    SEXP ptr = PROTECT(zb_r_buf_new(0, 0, &st));
    zb_buf *b = zb_r_buf_get(ptr);
    SEXP back;
    size_t len, cut;
    FUZZ_CHECK(st == ZB_OK && b);
    FUZZ_CHECK(zb_put_zeros(b, prefix) == ZB_OK);
    FUZZ_CHECK(zb_serialize(x, b, version, xdr, R_NilValue) == ZB_OK);
    len = b->len;
    zb_cur_init(&c, b->data, len);
    zb_cur_seek(&c, prefix);
    back = PROTECT(zb_unserialize(&c, R_NilValue, &st));
    FUZZ_CHECK(st == ZB_OK);
    FUZZ_CHECK(c.pos == len);
    FUZZ_CHECK(R_compute_identical(x, back, 16));
    /* a truncated stream: EOF, nothing advanced */
    cut = prefix + (len > prefix ? ((size_t)zb_rfuzz_u8(&j->in) * 7919u) % (len - prefix) : 0);
    zb_cur_init(&c, b->data, cut);
    zb_cur_seek(&c, prefix);
    back = zb_unserialize(&c, R_NilValue, &st);
    FUZZ_CHECK(st == ZB_ERR_EOF && back == R_NilValue && c.pos == prefix);
    zb_r_buf_free(ptr);
    UNPROTECT(3);
}

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    if (zb_rfuzz_init()) __builtin_trap();   /* R_HOME is not set */
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
    /* R_ToplevelExec catches any R error: none is expected, so one fails the run */
    FUZZ_CHECK(R_ToplevelExec(run, &j));
    return 0;
}
