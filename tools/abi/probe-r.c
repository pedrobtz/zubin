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
