/* Translation unit 4: the serialization streams of <zubin-r.h>. */
#include <zubin-r.h>
#include "zubintest.h"

static void count_bytes(void *state, const void *p, size_t n)
{
    (void)p;
    *(size_t *)state += n;
}

/* Serializes x after a 3-byte prefix into a buffer owned by R, reads it back
   from a cursor at the prefix, and measures the headerless stream through a
   sink: list(object, bytes after the prefix, headerless length). A stream
   cut short is the status's name. */
SEXP zt_serial(SEXP x, SEXP cut)
{
    zb_status st;
    zb_cur c;
    size_t body = 0, len;
    SEXP out, back, ptr = PROTECT(zb_r_buf_new(0, 0, &st));
    zb_buf *b = zb_r_buf_get(ptr);
    if (st || zb_put_zeros(b, 3) || zb_serialize(x, b, 3, 1, R_NilValue)) Rf_error("zt_serial: %s", "serialize failed");
    len = b->len - 3;
    if (Rf_asInteger(cut) > 0) b->len -= (size_t)Rf_asInteger(cut);
    zb_cur_init(&c, b->data, b->len);
    zb_cur_seek(&c, 3);
    back = PROTECT(zb_unserialize(&c, R_NilValue, &st));
    if (st) {
        out = Rf_mkString(zb_status_string(st));
        zb_r_buf_free(ptr);
        UNPROTECT(2);
        return out;
    }
    zb_serialize_to_sink(x, count_bytes, &body, 2, 1, 1);
    out = PROTECT(Rf_allocVector(VECSXP, 3));
    SET_VECTOR_ELT(out, 0, back);
    SET_VECTOR_ELT(out, 1, Rf_ScalarReal((double)len));
    SET_VECTOR_ELT(out, 2, Rf_ScalarReal((double)body));
    zb_r_buf_free(ptr);
    UNPROTECT(3);
    return out;
}
