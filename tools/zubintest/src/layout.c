/* Translation unit 1: parse a layout and unpack records with its kernels.
   Includes the umbrella header. */
#include <zubin.h>
#include "zubintest.h"

/* Every numeric field of every whole record of x, as doubles, one list
   element per field; or the status's name when the spec is malformed. */
SEXP zt_unpack(SEXP spec, SEXP x)
{
    const char *s = CHAR(STRING_ELT(spec, 0));
    size_t n = strlen(s), pos = 0, len = (size_t)XLENGTH(x), nrec;
    uint32_t nf = zb_layout_count_fields(s, n), j;
    zb_field *fields = (zb_field *)R_alloc(nf, sizeof(zb_field));
    zb_layout l;
    zb_status st = zb_layout_parse(s, n, 0, 0, fields, nf, &l, &pos);
    SEXP out;
    if (st) return Rf_mkString(zb_status_string(st));
    nrec = len / l.size;
    out = PROTECT(Rf_allocVector(VECSXP, l.nfields));
    for (j = 0; j < l.nfields; j++) {
        const zb_field *f = &l.fields[j];
        size_t bad = 0, i;
        SEXP col = Rf_allocVector(REALSXP, (R_xlen_t)(nrec * f->count));
        SET_VECTOR_ELT(out, j, col);
        if (zb_unpack_f64(RAW(x), nrec, l.size, f, REAL(col)) == ZB_OK) continue;
        if (zb_unpack_f64x(RAW(x), nrec, l.size, f, REAL(col), &bad) == ZB_OK) continue;
        {
            int32_t *tmp = (int32_t *)R_alloc(nrec * f->count + 1, sizeof(int32_t));
            if (zb_unpack_i32(RAW(x), nrec, l.size, f, tmp, 1, &bad) == ZB_OK) {
                for (i = 0; i < nrec * f->count; i++) REAL(col)[i] = tmp[i];
                continue;
            }
        }
        SET_VECTOR_ELT(out, j, R_NilValue);   /* b, s, x: not numbers */
    }
    UNPROTECT(1);
    return out;
}
