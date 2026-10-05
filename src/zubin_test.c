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
    out = zb_r_buf_to_raw(b);
    zb_r_buf_free(ptr);
    UNPROTECT(1);
    return out;
}

/* The corners no R-level call reaches, each as a named TRUE when it holds. */
SEXP zubin_test_buf_misc(void)
{
    const char *names[] = {"add_overflow", "mul_overflow", "mul_zero", "alloc_over_max",
                           "reserve_overflow", "grow_rule", "put_raw_empty", "detach",
                           "detach_borrowed", "release_twice", "n_overflow", "fill_to_max", ""};
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
