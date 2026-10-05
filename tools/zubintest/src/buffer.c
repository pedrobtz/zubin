/* Translation unit 3: a buffer owned by R, through <zubin-r.h>. */
#include <zubin-r.h>
#include "zubintest.h"

/* Appends the doubles in `values` as f32be, then borrows the raw vector
   `tail` and appends its bytes, and returns the buffer's bytes. The buffer is
   owned by an external pointer from before it exists, so the error below
   cannot leak it. */
SEXP zt_build(SEXP values, SEXP tail)
{
    zb_status st;
    zb_buf view, *b;
    R_xlen_t i;
    SEXP out, ptr = PROTECT(zb_r_buf_new(16, 1 << 20, &st));
    if (st) Rf_error("zt_build: %s", zb_status_string(st));
    b = zb_r_buf_get(ptr);
    for (i = 0; i < XLENGTH(values); i++) {
        if (zb_put_f32be(b, (float)REAL(values)[i])) Rf_error("zt_build: put failed");
    }
    zb_r_buf_borrow(&view, tail);
    if (zb_put_bytes(b, view.data, view.len)) Rf_error("zt_build: the cap was reached");
    out = PROTECT(zb_r_buf_to_raw(b));
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return out;
}
