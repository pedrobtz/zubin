/* The R side of the R fuzz targets: embedding, and an object builder that
   turns input bytes into every kind of value serialization treats
   differently. */
#include "rfuzz.h"

#include <stdlib.h>

int zb_rfuzz_init(void)
{
    static int done = 0;
    char *argv[] = {"R", "--vanilla", "--silent", "--no-echo"};
    if (done) return 0;
    if (!getenv("R_HOME")) return 1;
    R_SignalHandlers = 0;
    Rf_initEmbeddedR(4, argv);
    R_CStackLimit = (uintptr_t)-1;
    done = 1;
    return 0;
}

uint8_t zb_rfuzz_u8(zb_rfuzz_in *in)
{
    return in->i < in->n ? in->p[in->i++] : 0;
}

static size_t small_len(zb_rfuzz_in *in)
{
    return zb_rfuzz_u8(in) % 9;
}

SEXP zb_rfuzz_object(zb_rfuzz_in *in, int depth)
{
    uint8_t op = zb_rfuzz_u8(in);
    size_t i, n = small_len(in);
    SEXP x;
    switch (op % 11) {
    case 0:
        return R_NilValue;
    case 1:
        x = PROTECT(Rf_allocVector(INTSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) {
            uint32_t v = 0;
            int k;
            for (k = 0; k < 4; k++) v = v << 8 | zb_rfuzz_u8(in);
            memcpy(&INTEGER(x)[i], &v, 4);
        }
        break;
    case 2:   /* any bit pattern: NA, NaN payloads, -0, subnormals */
        x = PROTECT(Rf_allocVector(REALSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) {
            uint64_t v = 0;
            int k;
            for (k = 0; k < 8; k++) v = v << 8 | zb_rfuzz_u8(in);
            memcpy(&REAL(x)[i], &v, 8);
        }
        break;
    case 3:
        x = PROTECT(Rf_allocVector(LGLSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) {
            uint8_t b = zb_rfuzz_u8(in) % 3;
            LOGICAL(x)[i] = b == 2 ? NA_LOGICAL : b;
        }
        break;
    case 4:   /* strings in every encoding, NA, and bytes that are not UTF-8 */
        x = PROTECT(Rf_allocVector(STRSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) {
            char buf[16];
            size_t k, m = zb_rfuzz_u8(in) % sizeof buf;
            uint8_t enc = zb_rfuzz_u8(in) % 5;
            for (k = 0; k < m; k++) {
                uint8_t c = zb_rfuzz_u8(in);
                buf[k] = (char)(c ? c : 'z');   /* no embedded NUL */
            }
            if (enc == 4) {
                SET_STRING_ELT(x, (R_xlen_t)i, NA_STRING);
            } else {
                cetype_t ce = enc == 0 ? CE_NATIVE : enc == 1 ? CE_UTF8 : enc == 2 ? CE_LATIN1 : CE_BYTES;
                SET_STRING_ELT(x, (R_xlen_t)i, Rf_mkCharLenCE(buf, (int)m, ce));
            }
        }
        break;
    case 5:
        x = PROTECT(Rf_allocVector(RAWSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) RAW(x)[i] = zb_rfuzz_u8(in);
        break;
    case 6:
        x = PROTECT(Rf_allocVector(CPLXSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) {
            COMPLEX(x)[i].r = (double)(int8_t)zb_rfuzz_u8(in);
            COMPLEX(x)[i].i = zb_rfuzz_u8(in) / 7.0;
        }
        break;
    case 7:   /* a compact sequence (ALTREP) */
        x = PROTECT(Rf_eval(Rf_lang3(Rf_install(":"), Rf_ScalarInteger(1),
                                     Rf_ScalarInteger(1 + (int)n * 1000)), R_BaseEnv));
        break;
    case 8:
        x = PROTECT(Rf_install(n ? "zb_sym" : "another_sym"));
        break;
    default:  /* a list, nested */
        x = PROTECT(Rf_allocVector(VECSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) {
            SET_VECTOR_ELT(x, (R_xlen_t)i, depth > 0 ? zb_rfuzz_object(in, depth - 1) : R_NilValue);
        }
        break;
    }
    /* sometimes names, sometimes a class attribute */
    if (TYPEOF(x) != SYMSXP && TYPEOF(x) != NILSXP && n && (zb_rfuzz_u8(in) & 1) && !ALTREP(x)) {
        SEXP nm = PROTECT(Rf_allocVector(STRSXP, (R_xlen_t)n));
        for (i = 0; i < n; i++) SET_STRING_ELT(nm, (R_xlen_t)i, Rf_mkChar(i % 2 ? "a" : "b"));
        Rf_setAttrib(x, R_NamesSymbol, nm);
        Rf_setAttrib(x, Rf_install("zb_attr"), Rf_ScalarInteger((int)n));
        UNPROTECT(1);
    }
    UNPROTECT(1);
    return x;
}
