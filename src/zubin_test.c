/* The always-compiled test harness (design 16.3). Each zubin_test_ entry
   point drives the headers directly, at sizes, positions and byte orders the
   tests choose, and reports what the C saw. None is exported to R users. */
#include <stdlib.h>

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

/* ---- buf.h and zubin-r.h ---------------------------------------------------- */

SEXP zubin_test_live_buffers(void)
{
    return Rf_ScalarReal((double)zubin_int_live_buffers);
}

/* Appends `chunk` zero bytes at a time until the buffer holds `total`, and
   returns every capacity it grew to, in order: the reallocation count is
   its length. The buffer lives on the stack and nothing below can jump
   until it is released. */
SEXP zubin_test_buf_growth(SEXP chunk, SEXP total)
{
    size_t c, t, caps[128], ncap = 0, i;
    zb_buf b;
    zb_status st = ZB_OK;
    SEXP out;
    if (zubin_int_size(chunk, &c) || zubin_int_size(total, &t) || c == 0) Rf_error("bad sizes");
    zb_buf_init(&b);
    if ((st = zb_buf_alloc(&b, 0, 0))) return zubin_int_status(st, -1);
    while (b.len < t) {
        size_t before = b.cap;
        if ((st = zb_put_zeros(&b, c))) break;
        if (b.cap != before) {
            if (ncap == sizeof caps / sizeof caps[0]) { st = ZB_ERR_LIMIT; break; }
            caps[ncap++] = b.cap;
        }
    }
    zb_buf_release(&b);
    if (st) return zubin_int_status(st, -1);
    out = Rf_allocVector(REALSXP, (R_xlen_t)ncap);
    for (i = 0; i < ncap; i++) REAL(out)[i] = (double)caps[i];
    return out;
}

/* A buffer allocated with (reserve, max), then one zb_put_bytes() per element
   of `puts`, each of a recognisable pattern. Per put: status, len, cap and
   whether ZB_BUF_HIT_LIMIT is set; and the final bytes. */
SEXP zubin_test_buf_cap(SEXP reserve, SEXP max, SEXP puts)
{
    const char *names[] = {"alloc", "status", "len", "cap", "hit_limit", "bytes", ""};
    size_t r, m, biggest = 0;
    R_xlen_t i, n = XLENGTH(puts);
    zb_status st;
    zb_buf *b;
    uint8_t *pattern;
    SEXP ptr, out, status, len, cap, hit;
    if (zubin_int_size(reserve, &r) || zubin_int_size(max, &m) || TYPEOF(puts) != REALSXP)
        Rf_error("bad arguments");
    for (i = 0; i < n; i++) {
        size_t k;
        if (zubin_int_size(Rf_ScalarReal(REAL(puts)[i]), &k)) Rf_error("bad put");
        if (k > biggest) biggest = k;
    }
    out = PROTECT(Rf_mkNamed(VECSXP, names));
    status = Rf_allocVector(INTSXP, n); SET_VECTOR_ELT(out, 1, status);
    len = Rf_allocVector(REALSXP, n);   SET_VECTOR_ELT(out, 2, len);
    cap = Rf_allocVector(REALSXP, n);   SET_VECTOR_ELT(out, 3, cap);
    hit = Rf_allocVector(LGLSXP, n);    SET_VECTOR_ELT(out, 4, hit);
    pattern = (uint8_t *)R_alloc(biggest ? biggest : 1, 1);
    for (i = 0; i < (R_xlen_t)biggest; i++) pattern[i] = (uint8_t)(i % 251);
    ptr = PROTECT(zb_r_buf_new(r, m, &st));
    SET_VECTOR_ELT(out, 0, Rf_mkString(zb_status_string(st)));
    if (st) {
        UNPROTECT(2);
        return out;
    }
    b = zb_r_buf_get(ptr);
    for (i = 0; i < n; i++) {
        INTEGER(status)[i] = (int)zb_put_bytes(b, pattern, (size_t)REAL(puts)[i]);
        REAL(len)[i] = (double)b->len;
        REAL(cap)[i] = (double)b->cap;
        LOGICAL(hit)[i] = (b->flags & ZB_BUF_HIT_LIMIT) != 0;
    }
    SET_VECTOR_ELT(out, 5, zb_r_buf_to_raw(b));
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return out;
}

/* Borrows a copy of `bytes`, resets it, and writes puts of 0xEE into it:
   allowed up to its length, refused beyond. Per put: status and len; and
   the borrowed memory afterwards. */
SEXP zubin_test_buf_borrow(SEXP bytes, SEXP puts)
{
    const char *names[] = {"status", "len", "bytes", "released", ""};
    R_xlen_t i, n = XLENGTH(puts);
    uint8_t fill[64];
    zb_buf b;
    SEXP copy, out, status, len;
    if (TYPEOF(bytes) != RAWSXP || TYPEOF(puts) != INTSXP) Rf_error("bad arguments");
    memset(fill, 0xEE, sizeof fill);
    out = PROTECT(Rf_mkNamed(VECSXP, names));
    copy = Rf_duplicate(bytes);
    SET_VECTOR_ELT(out, 2, copy);
    status = Rf_allocVector(INTSXP, n); SET_VECTOR_ELT(out, 0, status);
    len = Rf_allocVector(REALSXP, n);   SET_VECTOR_ELT(out, 1, len);
    zb_r_buf_borrow(&b, copy);
    zb_buf_reset(&b);
    for (i = 0; i < n; i++) {
        int k = INTEGER(puts)[i];
        if (k < 0 || k > (int)sizeof fill) Rf_error("bad put");
        INTEGER(status)[i] = (int)zb_put_bytes(&b, fill, (size_t)k);
        REAL(len)[i] = (double)b.len;
    }
    /* releasing a borrowed buffer frees nothing and zeroes the struct */
    zb_buf_release(&b);
    SET_VECTOR_ELT(out, 3, Rf_ScalarLogical(b.data == NULL && b.cap == 0));
    UNPROTECT(1);
    return out;
}

/* Typed appends: zb_put_<type><endian> once per value, or the _n twin once
   for all of them. `values` as for zubin_test_rw_write. */
SEXP zubin_test_buf_put(SEXP type, SEXP endian, SEXP values, SEXP vectorised)
{
    int k = scalar_kind(type), be = scalar_big(endian), vec = Rf_asLogical(vectorised) == TRUE;
    R_xlen_t i, n = XLENGTH(values);
    zb_status st = ZB_OK;
    zb_buf *b;
    SEXP ptr, out;
    if (TYPEOF(values) != (k_is_int(k) ? INTSXP : REALSXP)) Rf_error("`values` has the wrong type");
    ptr = PROTECT(zb_r_buf_new(0, 0, &st));
    if (st) Rf_error("allocation failed");
    b = zb_r_buf_get(ptr);
    if (!vec) {
        for (i = 0; i < n && st == ZB_OK; i++) {
            int iv = k_is_int(k) ? INTEGER(values)[i] : 0;
            double dv = k_is_int(k) ? 0 : REAL(values)[i];
            uint64_t u;
            memcpy(&u, &dv, 8);
            switch (k) {
            case K_U8:   st = zb_put_u8(b, (uint8_t)iv); break;
            case K_I8:   st = zb_put_i8(b, (int8_t)iv); break;
            case K_U16:  st = be ? zb_put_u16be(b, (uint16_t)iv) : zb_put_u16le(b, (uint16_t)iv); break;
            case K_I16:  st = be ? zb_put_i16be(b, (int16_t)iv) : zb_put_i16le(b, (int16_t)iv); break;
            case K_U32:  st = be ? zb_put_u32be(b, (uint32_t)dv) : zb_put_u32le(b, (uint32_t)dv); break;
            case K_I32:  st = be ? zb_put_i32be(b, iv) : zb_put_i32le(b, iv); break;
            case K_U64:  st = be ? zb_put_u64be(b, u) : zb_put_u64le(b, u); break;
            case K_I64:  { int64_t v; memcpy(&v, &u, 8);
                           st = be ? zb_put_i64be(b, v) : zb_put_i64le(b, v); } break;
            case K_F16:  st = be ? zb_put_f16be(b, dv) : zb_put_f16le(b, dv); break;
            case K_BF16: st = be ? zb_put_bf16be(b, dv) : zb_put_bf16le(b, dv); break;
            case K_F32:  { float f = zb_int_f64_to_f32(dv);
                           st = be ? zb_put_f32be(b, f) : zb_put_f32le(b, f); } break;
            case K_F64:  st = be ? zb_put_f64be(b, dv) : zb_put_f64le(b, dv); break;
            }
        }
    } else {
        size_t m = (size_t)n;
        void *tmp = R_alloc(n ? (size_t)n : 1, 8);
        for (i = 0; i < n; i++) {
            int iv = k_is_int(k) ? INTEGER(values)[i] : 0;
            double dv = k_is_int(k) ? 0 : REAL(values)[i];
            switch (k) {
            case K_U8:   ((uint8_t *)tmp)[i] = (uint8_t)iv; break;
            case K_I8:   ((int8_t *)tmp)[i] = (int8_t)iv; break;
            case K_U16:  ((uint16_t *)tmp)[i] = (uint16_t)iv; break;
            case K_I16:  ((int16_t *)tmp)[i] = (int16_t)iv; break;
            case K_U32:  ((uint32_t *)tmp)[i] = (uint32_t)dv; break;
            case K_I32:  ((int32_t *)tmp)[i] = iv; break;
            case K_U64: case K_I64: memcpy((uint64_t *)tmp + i, &dv, 8); break;
            case K_F32:  ((float *)tmp)[i] = zb_int_f64_to_f32(dv); break;
            default:     ((double *)tmp)[i] = dv; break;
            }
        }
        switch (k) {
        case K_U8:   st = zb_put_u8_n(b, (const uint8_t *)tmp, m); break;
        case K_I8:   st = zb_put_i8_n(b, (const int8_t *)tmp, m); break;
        case K_U16:  st = be ? zb_put_u16be_n(b, (const uint16_t *)tmp, m) : zb_put_u16le_n(b, (const uint16_t *)tmp, m); break;
        case K_I16:  st = be ? zb_put_i16be_n(b, (const int16_t *)tmp, m) : zb_put_i16le_n(b, (const int16_t *)tmp, m); break;
        case K_U32:  st = be ? zb_put_u32be_n(b, (const uint32_t *)tmp, m) : zb_put_u32le_n(b, (const uint32_t *)tmp, m); break;
        case K_I32:  st = be ? zb_put_i32be_n(b, (const int32_t *)tmp, m) : zb_put_i32le_n(b, (const int32_t *)tmp, m); break;
        case K_U64:  st = be ? zb_put_u64be_n(b, (const uint64_t *)tmp, m) : zb_put_u64le_n(b, (const uint64_t *)tmp, m); break;
        case K_I64:  st = be ? zb_put_i64be_n(b, (const int64_t *)tmp, m) : zb_put_i64le_n(b, (const int64_t *)tmp, m); break;
        case K_F16:  st = be ? zb_put_f16be_n(b, (const double *)tmp, m) : zb_put_f16le_n(b, (const double *)tmp, m); break;
        case K_BF16: st = be ? zb_put_bf16be_n(b, (const double *)tmp, m) : zb_put_bf16le_n(b, (const double *)tmp, m); break;
        case K_F32:  st = be ? zb_put_f32be_n(b, (const float *)tmp, m) : zb_put_f32le_n(b, (const float *)tmp, m); break;
        case K_F64:  st = be ? zb_put_f64be_n(b, (const double *)tmp, m) : zb_put_f64le_n(b, (const double *)tmp, m); break;
        }
    }
    if (st) {
        UNPROTECT(1);
        return zubin_int_status(st, -1);
    }
    out = PROTECT(zb_r_buf_to_raw(b));
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return out;
}

/* The corners no R-level call reaches, each as a named TRUE when it holds. */
SEXP zubin_test_buf_misc(void)
{
    const char *names[] = {"add_overflow", "mul_overflow", "mul_zero", "alloc_over_max",
                           "reserve_overflow", "grow_rule", "put_raw_empty", "detach",
                           "detach_borrowed", "release_twice", "n_overflow", "fill_to_max", "max_cap", ""};
    SEXP out = PROTECT(Rf_mkNamed(LGLSXP, names));
    int *ok = LOGICAL(out);
    size_t r = 0;
    zb_buf b;
    uint8_t *data = NULL, *slot;
    size_t len = 0, g1, g2, g3, g4;

    ok[0] = zb_int_add((size_t)-1, 1, &r) == ZB_ERR_MEMORY && zb_int_add(1, 2, &r) == ZB_OK && r == 3;
    ok[1] = zb_int_mul((size_t)-1 / 2 + 1, 2, &r) == ZB_ERR_MEMORY;
    ok[2] = zb_int_mul(0, (size_t)-1, &r) == ZB_OK && r == 0;

    ok[3] = zb_buf_alloc(&b, 10, 5) == ZB_ERR_LIMIT && b.data == NULL && b.flags == 0;

    zb_buf_alloc(&b, 0, 0);
    zb_put_u8(&b, 1);
    ok[4] = zb_buf_reserve(&b, (size_t)-1) == ZB_ERR_MEMORY && b.len == 1 &&
            !(b.flags & ZB_BUF_HIT_LIMIT);
    zb_buf_release(&b);

    zb_int_grow(0, 1, 0, &g1);
    zb_int_grow(ZB_BUF_DOUBLING_LIMIT / 2, ZB_BUF_DOUBLING_LIMIT / 2 + 1, 0, &g2);
    zb_int_grow(ZB_BUF_DOUBLING_LIMIT, ZB_BUF_DOUBLING_LIMIT + 1, 0, &g3);
    zb_int_grow(1000, 1001, 1500, &g4);
    ok[5] = g1 == ZB_BUF_MIN_CAP && g2 == ZB_BUF_DOUBLING_LIMIT &&
            g3 == ZB_BUF_DOUBLING_LIMIT + ZB_BUF_DOUBLING_LIMIT / 2 && g4 == 1500;

    zb_buf_alloc(&b, 0, 0);
    slot = zb_put_raw(&b, 0);
    ok[6] = slot != NULL && b.len == 0 && b.data == NULL;
    zb_buf_release(&b);

    zb_buf_alloc(&b, 0, 0);
    zb_put_u32be(&b, 0x01020304u);
    ok[7] = zb_buf_detach(&b, &data, &len) == ZB_OK && len == 4 && data && data[0] == 1 &&
            data[3] == 4 && b.data == NULL && b.flags == 0;
    free(data);

    zb_buf_borrow(&b, "abc", 3);
    ok[8] = zb_buf_detach(&b, &data, &len) == ZB_ERR_INVALID && b.len == 3;

    zb_buf_alloc(&b, 16, 0);
    zb_buf_release(&b);
    zb_buf_release(&b);
    ok[9] = b.data == NULL && b.flags == 0;

    zb_buf_alloc(&b, 0, 0);
    ok[10] = zb_put_u64le_n(&b, NULL, (size_t)-1 / 4) == ZB_ERR_MEMORY && b.len == 0;
    zb_buf_release(&b);

    /* the last growth is clamped to max, so the buffer fills to exactly it */
    zb_buf_alloc(&b, 0, 1000);
    while (zb_put_u8(&b, 7) == ZB_OK) {}
    ok[11] = b.len == 1000 && b.cap == 1000 && (b.flags & ZB_BUF_HIT_LIMIT);
    zb_buf_release(&b);

    /* nothing is ever asked of the allocator above PTRDIFF_MAX */
    zb_int_grow(ZB_BUF_DOUBLING_LIMIT * 2, ZB_BUF_MAX_CAP, 0, &g1);
    ok[12] = zb_int_grow(0, ZB_BUF_MAX_CAP + 1, 0, &g2) == ZB_ERR_MEMORY &&
             g1 == ZB_BUF_MAX_CAP &&
             zb_buf_alloc(&b, ZB_BUF_MAX_CAP + 1, 0) == ZB_ERR_MEMORY && b.data == NULL;
    zb_buf_alloc(&b, 0, 0);
    zb_put_u8(&b, 1);
    ok[12] = ok[12] && zb_buf_reserve(&b, ZB_BUF_MAX_CAP) == ZB_ERR_MEMORY && b.len == 1;
    zb_buf_release(&b);

    UNPROTECT(1);
    return out;
}

/* A buffer owned by R, some bytes in it, then an error: the longjmp strands
   it, and only its finalizer can free it. */
SEXP zubin_test_put_then_error(void)
{
    zb_status st;
    SEXP ptr = PROTECT(zb_r_buf_new(1024, 0, &st));
    zb_buf *b = zb_r_buf_get(ptr);
    if (b) zb_put_zeros(b, 1000);
    Rf_error("zubin_test_put_then_error: the planned error");
    UNPROTECT(1);
    return R_NilValue;
}

/* `times` appends of `chunk` bytes into a buffer owned by R, with an
   interrupt check after each and a reset whenever it passes 64 MiB: long
   enough for setTimeLimit() to cut it short with the buffer live. */
SEXP zubin_test_put_loop(SEXP chunk, SEXP times)
{
    size_t c, t, i;
    zb_status st;
    zb_buf *b;
    SEXP ptr;
    if (zubin_int_size(chunk, &c) || zubin_int_size(times, &t)) Rf_error("bad sizes");
    ptr = PROTECT(zb_r_buf_new(0, 0, &st));
    if (st) Rf_error("allocation failed");
    b = zb_r_buf_get(ptr);
    for (i = 0; i < t; i++) {
        if (zb_put_zeros(b, c)) Rf_error("put failed");
        if (b->len > ((size_t)64 << 20)) zb_buf_reset(b);
        R_CheckUserInterrupt();
    }
    zb_r_buf_free(ptr);
    UNPROTECT(1);
    return Rf_ScalarReal((double)i);
}

/* ---- layout.h -------------------------------------------------------------- */

/* zb_layout_parse() as C sees it: the raw field table (count is the byte
   width for b, s and x), the record size and alignment, or the status and
   error position. max_fields 0 sizes the array with zb_layout_count_fields(). */
SEXP zubin_test_layout(SEXP spec, SEXP big, SEXP align, SEXP max_fields)
{
    const char *names[] = {"status", "position", "type", "type_name", "width", "count", "size",
                           "offset", "big", "name", "nfields", "record_size", "align",
                           "count_bound", ""};
    zb_layout l;
    size_t err = 0;
    uint32_t i, mf = (uint32_t)Rf_asInteger(max_fields);
    zb_status st;
    const char *s = CHAR(STRING_ELT(spec, 0));
    SEXP out = PROTECT(Rf_mkNamed(VECSXP, names));
    l.nfields = 0;
    st = zubin_int_parse(spec, Rf_asLogical(big) == TRUE, Rf_asLogical(align) == TRUE, mf, &l, &err);
    SET_VECTOR_ELT(out, 0, Rf_mkString(zb_status_string(st)));
    SET_VECTOR_ELT(out, 1, Rf_ScalarReal(st ? (double)err : NA_REAL));
    SET_VECTOR_ELT(out, 13, Rf_ScalarReal((double)zb_layout_count_fields(s, strlen(s))));
    if (!st) {
        SEXP type = Rf_allocVector(INTSXP, l.nfields), tname, width, count, size, offset, bigs, name;
        SET_VECTOR_ELT(out, 2, type);
        tname = Rf_allocVector(STRSXP, l.nfields);  SET_VECTOR_ELT(out, 3, tname);
        width = Rf_allocVector(INTSXP, l.nfields);  SET_VECTOR_ELT(out, 4, width);
        count = Rf_allocVector(INTSXP, l.nfields);  SET_VECTOR_ELT(out, 5, count);
        size = Rf_allocVector(INTSXP, l.nfields);   SET_VECTOR_ELT(out, 6, size);
        offset = Rf_allocVector(INTSXP, l.nfields); SET_VECTOR_ELT(out, 7, offset);
        bigs = Rf_allocVector(INTSXP, l.nfields);   SET_VECTOR_ELT(out, 8, bigs);
        name = Rf_allocVector(STRSXP, l.nfields);   SET_VECTOR_ELT(out, 9, name);
        for (i = 0; i < l.nfields; i++) {
            const zb_field *f = &l.fields[i];
            INTEGER(type)[i] = (int)f->type;
            SET_STRING_ELT(tname, i, Rf_mkChar(zb_type_name(f->type)));
            INTEGER(width)[i] = (int)zb_type_width(f->type);
            INTEGER(count)[i] = (int)f->count;
            INTEGER(size)[i] = (int)f->size;
            INTEGER(offset)[i] = (int)f->offset;
            INTEGER(bigs)[i] = f->big_endian;
            SET_STRING_ELT(name, i, f->name ? Rf_mkCharLen(f->name, (int)f->name_len) : NA_STRING);
        }
        SET_VECTOR_ELT(out, 10, Rf_ScalarInteger((int)l.nfields));
        SET_VECTOR_ELT(out, 11, Rf_ScalarInteger((int)l.size));
        SET_VECTOR_ELT(out, 12, Rf_ScalarInteger((int)l.align));
    }
    UNPROTECT(1);
    return out;
}

/* The oracle for align = TRUE (design 11.4): structs compiled here, by the
   same compiler as the package, each with the spec that should describe it.
   Returns, per struct, the spec, offsetof() of each member and sizeof(). */
#include <stddef.h>

struct zb_s1  { uint8_t a; uint32_t b; uint16_t c; };
struct zb_s2  { uint8_t a; double b; uint8_t c; };
struct zb_s3  { uint16_t a; uint8_t b; int64_t c; int8_t d; };
struct zb_s4  { float xyz[3]; uint8_t rgb[3]; };
struct zb_s5  { char name[5]; int32_t id; char tag[3]; };
struct zb_s6  { uint8_t flag; uint16_t h[2]; uint8_t g; };
struct zb_s7  { int64_t ts; double price; int32_t qty; uint8_t side; };
struct zb_s8  { uint8_t a; uint8_t b; uint8_t c; };
struct zb_s9  { uint32_t a; uint8_t b; };
struct zb_s10 { uint8_t a; uint64_t b[2]; uint16_t c; };
struct zb_s11 { uint8_t raw[7]; uint16_t v; };
struct zb_s12 { int16_t a; float b; uint8_t c; double d; uint8_t e; };

#define ZB_OFFS(...) { __VA_ARGS__ }
typedef struct { const char *spec; int n; size_t off[6]; size_t size; int has8; } zb_struct_case;

SEXP zubin_test_struct_offsets(void)
{
    const zb_struct_case cases[] = {
        {"a:u8 b:u32 c:u16", 3, ZB_OFFS(offsetof(struct zb_s1, a), offsetof(struct zb_s1, b), offsetof(struct zb_s1, c)), sizeof(struct zb_s1), 0},
        {"a:u8 b:f64 c:u8", 3, ZB_OFFS(offsetof(struct zb_s2, a), offsetof(struct zb_s2, b), offsetof(struct zb_s2, c)), sizeof(struct zb_s2), 1},
        {"a:u16 b:u8 c:i64 d:i8", 4, ZB_OFFS(offsetof(struct zb_s3, a), offsetof(struct zb_s3, b), offsetof(struct zb_s3, c), offsetof(struct zb_s3, d)), sizeof(struct zb_s3), 1},
        {"xyz:f32[3] rgb:u8[3]", 2, ZB_OFFS(offsetof(struct zb_s4, xyz), offsetof(struct zb_s4, rgb)), sizeof(struct zb_s4), 0},
        {"name:s5 id:i32 tag:b3", 3, ZB_OFFS(offsetof(struct zb_s5, name), offsetof(struct zb_s5, id), offsetof(struct zb_s5, tag)), sizeof(struct zb_s5), 0},
        {"flag:bool h:f16[2] g:u8", 3, ZB_OFFS(offsetof(struct zb_s6, flag), offsetof(struct zb_s6, h), offsetof(struct zb_s6, g)), sizeof(struct zb_s6), 0},
        {"ts:i64 price:f64 qty:i32 side:u8", 4, ZB_OFFS(offsetof(struct zb_s7, ts), offsetof(struct zb_s7, price), offsetof(struct zb_s7, qty), offsetof(struct zb_s7, side)), sizeof(struct zb_s7), 1},
        {"a:u8 b:u8 c:u8", 3, ZB_OFFS(offsetof(struct zb_s8, a), offsetof(struct zb_s8, b), offsetof(struct zb_s8, c)), sizeof(struct zb_s8), 0},
        {"a:u32 b:u8", 2, ZB_OFFS(offsetof(struct zb_s9, a), offsetof(struct zb_s9, b)), sizeof(struct zb_s9), 0},
        {"a:u8 b:u64[2] c:u16", 3, ZB_OFFS(offsetof(struct zb_s10, a), offsetof(struct zb_s10, b), offsetof(struct zb_s10, c)), sizeof(struct zb_s10), 1},
        {"raw:b7 v:bf16", 2, ZB_OFFS(offsetof(struct zb_s11, raw), offsetof(struct zb_s11, v)), sizeof(struct zb_s11), 0},
        {"a:i16 b:f32 c:u8 d:f64 e:u8", 5, ZB_OFFS(offsetof(struct zb_s12, a), offsetof(struct zb_s12, b), offsetof(struct zb_s12, c), offsetof(struct zb_s12, d), offsetof(struct zb_s12, e)), sizeof(struct zb_s12), 1},
    };
    const int ncase = (int)(sizeof cases / sizeof cases[0]);
    const char *names[] = {"spec", "offsets", "size", "has8", ""};
    int i, j;
    SEXP out = PROTECT(Rf_allocVector(VECSXP, ncase));
    for (i = 0; i < ncase; i++) {
        SEXP one = Rf_mkNamed(VECSXP, names), offs;
        SET_VECTOR_ELT(out, i, one);
        SET_VECTOR_ELT(one, 0, Rf_mkString(cases[i].spec));
        offs = Rf_allocVector(INTSXP, cases[i].n);
        SET_VECTOR_ELT(one, 1, offs);
        for (j = 0; j < cases[i].n; j++) INTEGER(offs)[j] = (int)cases[i].off[j];
        SET_VECTOR_ELT(one, 2, Rf_ScalarInteger((int)cases[i].size));
        SET_VECTOR_ELT(one, 3, Rf_ScalarLogical(cases[i].has8));
    }
    UNPROTECT(1);
    return out;
}

/* Each kernel driven directly over `bytes`: n records (every whole one when
   n < 0), `stride` apart (the record size when stride < 0), with no R glue
   in between. Per value field: the kernel's output as C sees it (int32,
   double, int64 bits in a double, or raw bytes) and its status and *bad;
   for i32 the strict (allow_na = 0) run's status, for i64/u64 also the
   exact-double kernel's output. */
SEXP zubin_test_unpack_kernel(SEXP bytes, SEXP spec, SEXP n, SEXP stride)
{
    const char *names[] = {"type", "values", "status", "bad", "values2", "status2", "bad2", ""};
    zb_layout l;
    size_t err = 0, rs, nrec, len = (size_t)XLENGTH(bytes);
    uint32_t j;
    zb_status st = zubin_int_parse(spec, 0, 0, 0, &l, &err);
    const uint8_t *base = len ? RAW(bytes) : NULL;
    SEXP out;
    if (st) Rf_error("bad spec");
    rs = Rf_asReal(stride) < 0 ? l.size : (size_t)Rf_asReal(stride);
    if (Rf_asReal(n) < 0) nrec = len < l.size ? 0 : (len - l.size) / rs + 1;
    else nrec = (size_t)Rf_asReal(n);
    if (nrec && (nrec - 1) * rs + l.size > len) Rf_error("records do not fit");
    out = PROTECT(Rf_allocVector(VECSXP, l.nfields));
    for (j = 0; j < l.nfields; j++) {
        const zb_field *f = &l.fields[j];
        size_t m = nrec * f->count, bad = (size_t)-1, bad2 = (size_t)-1;
        zb_status s1 = ZB_OK, s2 = ZB_OK;
        SEXP one = Rf_mkNamed(VECSXP, names), v = R_NilValue, v2 = R_NilValue;
        SET_VECTOR_ELT(out, j, one);
        SET_VECTOR_ELT(one, 0, Rf_mkString(zb_type_name(f->type)));
        switch (f->type) {
        case ZB_U8: case ZB_I8: case ZB_U16: case ZB_I16: case ZB_I32: case ZB_BOOL:
            v = Rf_allocVector(INTSXP, (R_xlen_t)m);
            SET_VECTOR_ELT(one, 1, v);
            s1 = zb_unpack_i32(base, nrec, rs, f, INTEGER(v), 1, &bad);
            {
                int32_t *tmp = (int32_t *)R_alloc(m ? m : 1, sizeof(int32_t));
                s2 = zb_unpack_i32(base, nrec, rs, f, tmp, 0, &bad2);
            }
            break;
        case ZB_U32: case ZB_F16: case ZB_BF16: case ZB_F32: case ZB_F64:
            v = Rf_allocVector(REALSXP, (R_xlen_t)m);
            SET_VECTOR_ELT(one, 1, v);
            s1 = zb_unpack_f64(base, nrec, rs, f, REAL(v));
            break;
        case ZB_I64: case ZB_U64:
            v = Rf_allocVector(REALSXP, (R_xlen_t)m);
            SET_VECTOR_ELT(one, 1, v);
            s1 = zb_unpack_i64(base, nrec, rs, f, (int64_t *)(void *)REAL(v), &bad);
            v2 = Rf_allocVector(REALSXP, (R_xlen_t)m);
            SET_VECTOR_ELT(one, 4, v2);
            s2 = zb_unpack_f64x(base, nrec, rs, f, REAL(v2), &bad2);
            break;
        case ZB_BYTES: case ZB_STR:
            v = Rf_allocVector(RAWSXP, (R_xlen_t)(nrec * f->size));
            SET_VECTOR_ELT(one, 1, v);
            s1 = zb_unpack_bytes(base, nrec, rs, f, RAW(v));
            break;
        default:
            break;
        }
        SET_VECTOR_ELT(one, 2, Rf_mkString(zb_status_string(s1)));
        SET_VECTOR_ELT(one, 3, Rf_ScalarReal(bad == (size_t)-1 ? NA_REAL : (double)bad));
        SET_VECTOR_ELT(one, 5, Rf_mkString(zb_status_string(s2)));
        SET_VECTOR_ELT(one, 6, Rf_ScalarReal(bad2 == (size_t)-1 ? NA_REAL : (double)bad2));
    }
    UNPROTECT(1);
    return out;
}

/* ---- serialization streams ------------------------------------------------- */

/* A sink in the shape rdz's pipeline has: bytes go into a fixed block of
   `chunk` bytes, and each full block is flushed to a buffer owned by R. The
   result must equal the builder form whatever the chunk size. */
typedef struct {
    zb_buf *out;
    uint8_t *block;
    size_t chunk, used, flushes;
    zb_status st;
} test_block_sink;

static void test_block_flush(test_block_sink *s)
{
    if (!s->used) return;
    if (!s->st) s->st = zb_put_bytes(s->out, s->block, s->used);
    s->used = 0;
    s->flushes++;
}

static void test_block_put(void *state, const void *p, size_t n)
{
    test_block_sink *s = (test_block_sink *)state;
    const uint8_t *q = (const uint8_t *)p;
    while (n) {
        size_t take = s->chunk - s->used < n ? s->chunk - s->used : n;
        memcpy(s->block + s->used, q, take);
        s->used += take;
        q += take;
        n -= take;
        if (s->used == s->chunk) test_block_flush(s);
    }
}

SEXP zubin_test_sink(SEXP x, SEXP version, SEXP xdr, SEXP skip, SEXP chunk)
{
    zb_status st;
    test_block_sink s;
    size_t k;
    SEXP ptr, out;
    if (zubin_int_size(chunk, &k) || k == 0) Rf_error("bad chunk");
    ptr = PROTECT(zb_r_buf_new(0, 0, &st));
    if (st) Rf_error("allocation failed");
    memset(&s, 0, sizeof s);
    s.out = zb_r_buf_get(ptr);
    s.chunk = k;
    s.block = (uint8_t *)R_alloc(k, 1);
    zb_serialize_to_sink(x, test_block_put, &s, Rf_asInteger(version),
                         Rf_asLogical(xdr) == TRUE, Rf_asLogical(skip) == TRUE);
    test_block_flush(&s);
    if (s.st) Rf_error("put failed");
    out = PROTECT(zb_r_buf_to_raw(s.out));
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return out;
}

/* zb_serialize into a fresh buffer owned by R: when the refhook errors or
   an interrupt lands in it, the buffer is stranded and only its finalizer
   frees it, which test-lifetime.R counts. */
SEXP zubin_test_serialize_owned(SEXP x, SEXP refhook)
{
    zb_status st;
    SEXP ptr = PROTECT(zb_r_buf_new(0, 0, &st)), out;
    if (st) Rf_error("allocation failed");
    st = zb_serialize(x, zb_r_buf_get(ptr), 3, 1, refhook);
    if (st) Rf_error("serialize failed: %s", zb_status_string(st));
    out = PROTECT(zb_r_buf_to_raw(zb_r_buf_get(ptr)));
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return out;
}

/* zb_unserialize from a cursor at `offset`: the value, the status, and the
   cursor's position after (unchanged on failure). */
SEXP zubin_test_unserialize_cursor(SEXP bytes, SEXP offset)
{
    const char *names[] = {"value", "status", "pos", ""};
    zb_cur c;
    zb_status st;
    size_t off;
    SEXP out, v;
    if (TYPEOF(bytes) != RAWSXP || zubin_int_size(offset, &off)) Rf_error("bad arguments");
    zb_cur_init(&c, XLENGTH(bytes) ? RAW(bytes) : NULL, (size_t)XLENGTH(bytes));
    if (zb_cur_seek(&c, off)) Rf_error("offset past the end");
    v = PROTECT(zb_unserialize(&c, R_NilValue, &st));
    out = PROTECT(Rf_mkNamed(VECSXP, names));
    SET_VECTOR_ELT(out, 0, v);
    SET_VECTOR_ELT(out, 1, Rf_mkString(zb_status_string(st)));
    SET_VECTOR_ELT(out, 2, Rf_ScalarReal((double)c.pos));
    UNPROTECT(2);
    return out;
}
