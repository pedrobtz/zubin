/* Translation unit 2: a checked sequential read of a header. Includes one
   area header, then the umbrella, the two ways the README allows. */
#include <zubin/cursor.h>
#include <zubin.h>
#include "zubintest.h"

/* magic:u32be version:u16le count:u32le name:6 bytes; returns c(magic,
   version, count, the position after the header), or the status's name and
   the position where the read failed. */
SEXP zt_header(SEXP x)
{
    zb_cur c;
    uint32_t magic = 0, count = 0;
    uint16_t version = 0;
    const uint8_t *name = NULL;
    zb_status st;
    SEXP out;
    zb_cur_init(&c, XLENGTH(x) ? RAW(x) : NULL, (size_t)XLENGTH(x));
    if ((st = zb_cur_u32be(&c, &magic)) || (st = zb_cur_u16le(&c, &version)) ||
        (st = zb_cur_u32le(&c, &count)) || (st = zb_cur_bytes(&c, &name, 6))) {
        out = PROTECT(Rf_allocVector(STRSXP, 2));
        SET_STRING_ELT(out, 0, Rf_mkChar(zb_status_string(st)));
        SET_STRING_ELT(out, 1, Rf_mkChar(c.pos == 0 ? "0" : c.pos == 4 ? "4" : c.pos == 6 ? "6" : "10"));
        UNPROTECT(1);
        return out;
    }
    out = Rf_allocVector(REALSXP, 4);
    REAL(out)[0] = magic;
    REAL(out)[1] = version;
    REAL(out)[2] = count;
    REAL(out)[3] = (double)c.pos;
    return out;
}
