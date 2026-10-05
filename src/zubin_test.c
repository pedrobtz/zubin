/* The always-compiled test harness (design 16.3). Each zubin_test_ entry
   point drives the headers directly, at sizes, positions and byte orders the
   tests choose, and reports what the C saw. None is exported to R users. */
#include <stdlib.h>

#include <zubin.h>
#include "zubin_r.h"

/* ---- type tokens ---------------------------------------------------------- */

enum { K_U8, K_I8, K_U16, K_I16, K_U32, K_I32, K_U64, K_I64,
       K_F16, K_BF16, K_F32, K_F64, K_N };

static const char *const k_name[K_N] = {
    "u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f16", "bf16", "f32", "f64"
};
static const int k_width[K_N] = {1, 1, 2, 2, 4, 4, 8, 8, 2, 2, 4, 8};

/* Results that fit an R integer come back as integer; the rest as double,
   except that 64-bit integers come back as their bit pattern in a double,
   which is how bit64's integer64 stores them. */
static int k_is_int(int k)
{
    return k == K_U8 || k == K_I8 || k == K_U16 || k == K_I16 || k == K_I32;
}

static int kind_of(const char *s)
{
    int k;
    for (k = 0; k < K_N; k++) {
        if (strcmp(s, k_name[k]) == 0) return k;
    }
    return -1;
}

static int scalar_kind(SEXP type)
{
    int k;
    if (!Rf_isString(type) || XLENGTH(type) != 1) Rf_error("`type` must be a string");
    k = kind_of(CHAR(STRING_ELT(type, 0)));
    if (k < 0) Rf_error("unknown type");
    return k;
}

static int scalar_big(SEXP endian)
{
    const char *s;
    if (!Rf_isString(endian) || XLENGTH(endian) != 1) Rf_error("`endian` must be a string");
    s = CHAR(STRING_ELT(endian, 0));
    if (strcmp(s, "be") == 0) return 1;
    if (strcmp(s, "le") == 0) return 0;
    Rf_error("`endian` must be \"le\" or \"be\"");
    return 0;
}

static void rd_one(int k, int be, const uint8_t *p, int *iv, double *dv)
{
    switch (k) {
    case K_U8:   *iv = zb_rd_u8(p); break;
    case K_I8:   *iv = zb_rd_i8(p); break;
    case K_U16:  *iv = be ? zb_rd_u16be(p) : zb_rd_u16le(p); break;
    case K_I16:  *iv = be ? zb_rd_i16be(p) : zb_rd_i16le(p); break;
    case K_U32:  *dv = be ? zb_rd_u32be(p) : zb_rd_u32le(p); break;
    case K_I32:  *iv = be ? zb_rd_i32be(p) : zb_rd_i32le(p); break;
    case K_U64:  { uint64_t u = be ? zb_rd_u64be(p) : zb_rd_u64le(p); memcpy(dv, &u, 8); } break;
    case K_I64:  { int64_t v = be ? zb_rd_i64be(p) : zb_rd_i64le(p); memcpy(dv, &v, 8); } break;
    case K_F16:  *dv = be ? zb_rd_f16be(p) : zb_rd_f16le(p); break;
    case K_BF16: *dv = be ? zb_rd_bf16be(p) : zb_rd_bf16le(p); break;
    case K_F32:  *dv = be ? zb_rd_f32be(p) : zb_rd_f32le(p); break;
    case K_F64:  *dv = be ? zb_rd_f64be(p) : zb_rd_f64le(p); break;
    }
}

static void wr_one(int k, int be, uint8_t *p, int iv, double dv)
{
    switch (k) {
    case K_U8:   zb_wr_u8(p, (uint8_t)iv); break;
    case K_I8:   zb_wr_i8(p, (int8_t)iv); break;
    case K_U16:  if (be) zb_wr_u16be(p, (uint16_t)iv); else zb_wr_u16le(p, (uint16_t)iv); break;
    case K_I16:  if (be) zb_wr_i16be(p, (int16_t)iv); else zb_wr_i16le(p, (int16_t)iv); break;
    case K_U32:  if (be) zb_wr_u32be(p, (uint32_t)dv); else zb_wr_u32le(p, (uint32_t)dv); break;
    case K_I32:  if (be) zb_wr_i32be(p, iv); else zb_wr_i32le(p, iv); break;
    case K_U64:  { uint64_t u; memcpy(&u, &dv, 8); if (be) zb_wr_u64be(p, u); else zb_wr_u64le(p, u); } break;
    case K_I64:  { int64_t v; memcpy(&v, &dv, 8); if (be) zb_wr_i64be(p, v); else zb_wr_i64le(p, v); } break;
    case K_F16:  if (be) zb_wr_f16be(p, dv); else zb_wr_f16le(p, dv); break;
    case K_BF16: if (be) zb_wr_bf16be(p, dv); else zb_wr_bf16le(p, dv); break;
    case K_F32:  {
        float f = zb_int_f64_to_f32(dv);
        if (be) zb_wr_f32be(p, f); else zb_wr_f32le(p, f);
    } break;
    case K_F64:  if (be) zb_wr_f64be(p, dv); else zb_wr_f64le(p, dv); break;
    }
}

/* ---- status.h ----------------------------------------------------------------- */

/* zb_status_string() for each integer code, including ones outside the enum. */
SEXP zubin_test_status_string(SEXP codes)
{
    R_xlen_t i, n = XLENGTH(codes);
    SEXP out = PROTECT(Rf_allocVector(STRSXP, n));
    for (i = 0; i < n; i++) {
        SET_STRING_ELT(out, i, Rf_mkChar(zb_status_string((zb_status)INTEGER(codes)[i])));
    }
    UNPROTECT(1);
    return out;
}

/* ---- rw.h ----------------------------------------------------------------------- */

/* Reads every whole element of `bytes` with zb_rd_<type><endian>. */
SEXP zubin_test_rw(SEXP type, SEXP endian, SEXP bytes)
{
    int k = scalar_kind(type), be = scalar_big(endian), w;
    R_xlen_t i, n;
    const uint8_t *p;
    SEXP out;
    if (TYPEOF(bytes) != RAWSXP) Rf_error("`bytes` must be raw");
    w = k_width[k];
    n = XLENGTH(bytes) / w;
    p = RAW(bytes);
    out = PROTECT(Rf_allocVector(k_is_int(k) ? INTSXP : REALSXP, n));
    for (i = 0; i < n; i++) {
        int iv = 0;
        double dv = 0;
        rd_one(k, be, p + i * w, &iv, &dv);
        if (k_is_int(k)) INTEGER(out)[i] = iv; else REAL(out)[i] = dv;
    }
    UNPROTECT(1);
    return out;
}

/* The inverse: writes each value with zb_wr_<type><endian>. `values` is
   integer for the types zubin_test_rw returns as integer, double otherwise. */
SEXP zubin_test_rw_write(SEXP type, SEXP endian, SEXP values)
{
    int k = scalar_kind(type), be = scalar_big(endian), w;
    R_xlen_t i, n = XLENGTH(values);
    SEXP out;
    if (TYPEOF(values) != (k_is_int(k) ? INTSXP : REALSXP)) Rf_error("`values` has the wrong type");
    w = k_width[k];
    out = PROTECT(Rf_allocVector(RAWSXP, n * w));
    for (i = 0; i < n; i++) {
        if (k_is_int(k)) wr_one(k, be, RAW(out) + i * w, INTEGER(values)[i], 0);
        else wr_one(k, be, RAW(out) + i * w, 0, REAL(values)[i]);
    }
    UNPROTECT(1);
    return out;
}

/* ---- cursor.h ------------------------------------------------------------------- */

/* Runs a plan over a cursor on `bytes`. Each step is a typed read ("u32le",
   "f64be", "u8", ...), or "seek:N", "skip:N", "bytes:N". Reports, per step, the
   status, the position after it, the value read (as a double; NA on failure),
   and whether a failed read left its out-parameter untouched. */
SEXP zubin_test_cursor(SEXP bytes, SEXP plan)
{
    const char *names[] = {"status", "pos", "value", "untouched", ""};
    R_xlen_t i, n;
    zb_cur c;
    SEXP out, status, pos, value, untouched;
    if (TYPEOF(bytes) != RAWSXP) Rf_error("`bytes` must be raw");
    if (!Rf_isString(plan)) Rf_error("`plan` must be character");
    n = XLENGTH(plan);
    out = PROTECT(Rf_mkNamed(VECSXP, names));
    status = Rf_allocVector(INTSXP, n);
    SET_VECTOR_ELT(out, 0, status);
    pos = Rf_allocVector(REALSXP, n);
    SET_VECTOR_ELT(out, 1, pos);
    value = Rf_allocVector(REALSXP, n);
    SET_VECTOR_ELT(out, 2, value);
    untouched = Rf_allocVector(LGLSXP, n);
    SET_VECTOR_ELT(out, 3, untouched);

    /* XLENGTH 0 gives RAW() a non-NULL pointer; pass NULL to cover that case. */
    zb_cur_init(&c, XLENGTH(bytes) ? RAW(bytes) : NULL, (size_t)XLENGTH(bytes));
    for (i = 0; i < n; i++) {
        const char *step = CHAR(STRING_ELT(plan, i));
        zb_status st = ZB_OK;
        double v = NA_REAL;
        int same = NA_LOGICAL;
        if (strncmp(step, "seek:", 5) == 0) {
            st = zb_cur_seek(&c, (size_t)strtod(step + 5, NULL));
        } else if (strncmp(step, "skip:", 5) == 0) {
            st = zb_cur_skip(&c, (size_t)strtod(step + 5, NULL));
        } else if (strncmp(step, "bytes:", 6) == 0) {
            const uint8_t *sentinel = (const uint8_t *)&c, *p = sentinel;
            st = zb_cur_bytes(&c, &p, (size_t)strtod(step + 6, NULL));
            if (st == ZB_OK) v = p ? (double)(p - c.base) : 0;
            else same = p == sentinel;
        } else {
            /* the read's out-parameter starts as a byte pattern no read
               could leave behind by accident, and is compared afterwards */
            union { uint8_t u8; int8_t i8; uint16_t u16; int16_t i16; uint32_t u32;
                    int32_t i32; uint64_t u64; int64_t i64; float f32; double f64;
                    unsigned char raw[8]; } o, fresh;
            size_t len = strlen(step);
            int be = len > 2 && strcmp(step + len - 2, "be") == 0;
            char base[8] = {0};
            int k;
            memset(&o, 0xA5, sizeof o);
            memset(&fresh, 0xA5, sizeof fresh);
            if (len > 2 && (be || strcmp(step + len - 2, "le") == 0)) {
                if (len - 2 >= sizeof base) Rf_error("unknown step");
                memcpy(base, step, len - 2);
            } else {
                if (len >= sizeof base) Rf_error("unknown step");
                memcpy(base, step, len);
            }
            k = kind_of(base);
            switch (k) {
            case K_U8:   st = zb_cur_u8(&c, &o.u8); v = o.u8; break;
            case K_I8:   st = zb_cur_i8(&c, &o.i8); v = o.i8; break;
            case K_U16:  st = be ? zb_cur_u16be(&c, &o.u16) : zb_cur_u16le(&c, &o.u16); v = o.u16; break;
            case K_I16:  st = be ? zb_cur_i16be(&c, &o.i16) : zb_cur_i16le(&c, &o.i16); v = o.i16; break;
            case K_U32:  st = be ? zb_cur_u32be(&c, &o.u32) : zb_cur_u32le(&c, &o.u32); v = o.u32; break;
            case K_I32:  st = be ? zb_cur_i32be(&c, &o.i32) : zb_cur_i32le(&c, &o.i32); v = o.i32; break;
            case K_U64:  st = be ? zb_cur_u64be(&c, &o.u64) : zb_cur_u64le(&c, &o.u64); v = (double)o.u64; break;
            case K_I64:  st = be ? zb_cur_i64be(&c, &o.i64) : zb_cur_i64le(&c, &o.i64); v = (double)o.i64; break;
            case K_F16:  st = be ? zb_cur_f16be(&c, &o.f64) : zb_cur_f16le(&c, &o.f64); v = o.f64; break;
            case K_BF16: st = be ? zb_cur_bf16be(&c, &o.f64) : zb_cur_bf16le(&c, &o.f64); v = o.f64; break;
            case K_F32:  st = be ? zb_cur_f32be(&c, &o.f32) : zb_cur_f32le(&c, &o.f32); v = o.f32; break;
            case K_F64:  st = be ? zb_cur_f64be(&c, &o.f64) : zb_cur_f64le(&c, &o.f64); v = o.f64; break;
            default: Rf_error("unknown step");
            }
            if (st != ZB_OK) {
                v = NA_REAL;
                same = memcmp(&o, &fresh, sizeof o) == 0;
            }
        }
        INTEGER(status)[i] = (int)st;
        REAL(pos)[i] = (double)c.pos;
        REAL(value)[i] = v;
        LOGICAL(untouched)[i] = same;
    }
    UNPROTECT(1);
    return out;
}
