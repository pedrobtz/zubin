/* <zubin-r.h> against R's own headers, calling every function it adds
   (design 16.1, third probe). R's headers need C11 or later and use C23
   extensions, so this probe is compiled as gnu17 without -Wpedantic. */
#define R_NO_REMAP
#include <zubin-r.h>

SEXP zb_probe_r(SEXP raw);
SEXP zb_probe_r(SEXP raw)
{
    zb_status st;
    zb_buf view;
    SEXP out, ptr = PROTECT(zb_r_buf_new(64, 0, &st));
    zb_buf *b = zb_r_buf_get(ptr);
    zb_r_buf_borrow(&view, raw);
    if (b) zb_put_bytes(b, view.data, view.len);
    out = PROTECT(b ? zb_r_buf_to_raw(b) : Rf_allocVector(RAWSXP, 0));
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return out;
}

/* The serialization streams (Stage 10). */
static void zb_probe_sink(void *state, const void *p, size_t n)
{
    (void)p;
    *(size_t *)state += n;
}

SEXP zb_probe_r_serial(SEXP x);
SEXP zb_probe_r_serial(SEXP x)
{
    zb_status st;
    zb_cur c;
    size_t counted = 0;
    zb_sink_fn fn = zb_probe_sink;
    SEXP out, ptr = PROTECT(zb_r_buf_new(0, 0, &st));
    zb_buf *b = zb_r_buf_get(ptr);
    if (b && zb_serialize(x, b, 3, 1, R_NilValue) == ZB_OK) {
        zb_cur_init(&c, b->data, b->len);
        out = PROTECT(zb_unserialize(&c, R_NilValue, &st));
    } else {
        out = PROTECT(R_NilValue);
    }
    zb_serialize_to_sink(x, fn, &counted, 2, 0, 1);
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return counted ? out : R_NilValue;
}

/* Foreign buffers (Stage 12). */
static void zb_probe_release(void *data, void *ptr, size_t len)
{
    (void)data;
    (void)len;
    free(ptr);
}

SEXP zb_probe_r_extbuf(void);
SEXP zb_probe_r_extbuf(void)
{
    zb_status st;
    SEXP h = PROTECT(zb_r_extbuf_new(R_NilValue, &st));
    void *p = malloc(8);
    zb_release_fn fn = zb_probe_release;
    if (st || !p || zb_r_extbuf_adopt(h, p, 8, fn, NULL) != ZB_OK) free(p);
    st = zb_r_extbuf_ptr(h) && zb_r_extbuf_len(h) == 8 ? ZB_OK : ZB_ERR_INVALID;
    zb_r_extbuf_release(h);
    UNPROTECT(1);
    return Rf_ScalarInteger((int)st);
}
