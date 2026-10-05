/* The .Call wrappers behind R/ (design 15). They include <zubin.h> like any
   consumer, so CRAN's instrumented builds exercise the headers through the
   package's own tests. */
#include <limits.h>
#include <stdio.h>
#include <math.h>

#include "zubin_r.h"
#include <zufast/hash.h>

long zubin_int_live_buffers = 0;

/* A failure as R sees it: the status's enumerator name, classed
   zubin_status, with the 0-based index of the offending element when there
   is one. R/conditions.R turns it into a condition; C never raises. */
SEXP zubin_int_status(zb_status st, R_xlen_t index)
{
    SEXP out = PROTECT(Rf_mkString(zb_status_string(st)));
    SEXP cls = PROTECT(Rf_mkString("zubin_status"));
    Rf_setAttrib(out, R_ClassSymbol, cls);
    if (index >= 0) {
        SEXP v = PROTECT(Rf_ScalarReal((double)index));
        Rf_setAttrib(out, Rf_install("index"), v);
        UNPROTECT(1);
    }
    UNPROTECT(2);
    return out;
}

/* A failure in a field: the status name (a zb_status name, or the R-only
   "ZUBIN_ERR_ENCODING"), the 0-based record index, and the field's 0-based
   position in the layout, padding included; R names the field from it. */
SEXP zubin_int_failure(const char *name, R_xlen_t index, int field)
{
    SEXP out = PROTECT(Rf_mkString(name));
    SEXP cls = PROTECT(Rf_mkString("zubin_status"));
    SEXP idx = PROTECT(Rf_ScalarReal((double)index));
    SEXP fld = PROTECT(Rf_ScalarInteger(field));
    Rf_setAttrib(out, R_ClassSymbol, cls);
    Rf_setAttrib(out, Rf_install("index"), idx);
    Rf_setAttrib(out, Rf_install("field"), fld);
    UNPROTECT(4);
    return out;
}

/* A byte count from R: a whole, non-negative double below 2^53 (or an
   integer). R validated it already; this is the second check native safety
   depends on (design 13). Returns 0 on success. */
int zubin_int_size(SEXP x, size_t *out)
{
    double v;
    if (XLENGTH(x) != 1) return 1;
    if (TYPEOF(x) == INTSXP) {
        if (INTEGER(x)[0] == NA_INTEGER || INTEGER(x)[0] < 0) return 1;
        *out = (size_t)INTEGER(x)[0];
        return 0;
    }
    if (TYPEOF(x) != REALSXP) return 1;
    v = REAL(x)[0];
    if (!(v >= 0) || v > 9007199254740992.0 || v != floor(v)) return 1;
    if (v > (double)((size_t)-1 / 2)) return 1;
    *out = (size_t)v;
    return 0;
}

SEXP zubin_info(void)
{
    const char *names[] = {"version", "version_major", "version_minor", "version_patch",
                           "zufast", "endian", "compiler", "build", ""};
    const char *build_names[] = {"c_standard", "optimized", "ndebug", "fortify_source", ""};
    char v[32];
    SEXP build;
    SEXP out = PROTECT(Rf_mkNamed(VECSXP, names));
    SET_VECTOR_ELT(out, 0, Rf_mkString(ZUBIN_VERSION));
    SET_VECTOR_ELT(out, 1, Rf_ScalarInteger(ZUBIN_VERSION_MAJOR));
    SET_VECTOR_ELT(out, 2, Rf_ScalarInteger(ZUBIN_VERSION_MINOR));
    SET_VECTOR_ELT(out, 3, Rf_ScalarInteger(ZUBIN_VERSION_PATCH));
    SET_VECTOR_ELT(out, 4, Rf_mkString(ZUFAST_VERSION));
    SET_VECTOR_ELT(out, 5, Rf_mkString(zb_host_big_endian() ? "big" : "little"));
#if defined(__clang__)
    SET_VECTOR_ELT(out, 6, Rf_mkString("clang " __clang_version__));
#elif defined(__GNUC__)
    SET_VECTOR_ELT(out, 6, Rf_mkString("gcc " __VERSION__));
#else
    SET_VECTOR_ELT(out, 6, Rf_mkString("unknown"));
#endif
    /* The flags that change the generated code, as the preprocessor saw them
       when this file was compiled. */
    build = Rf_mkNamed(STRSXP, build_names);
    SET_VECTOR_ELT(out, 7, build);
#if defined(__STDC_VERSION__)
    snprintf(v, sizeof v, "%ld", (long)__STDC_VERSION__);
#else
    snprintf(v, sizeof v, "C89");
#endif
    SET_STRING_ELT(build, 0, Rf_mkChar(v));
#if defined(__OPTIMIZE__)
    SET_STRING_ELT(build, 1, Rf_mkChar("true"));
#else
    SET_STRING_ELT(build, 1, Rf_mkChar("false"));
#endif
#if defined(NDEBUG)
    SET_STRING_ELT(build, 2, Rf_mkChar("true"));
#else
    SET_STRING_ELT(build, 2, Rf_mkChar("false"));
#endif
#if defined(_FORTIFY_SOURCE)
    snprintf(v, sizeof v, "%d", (int)_FORTIFY_SOURCE);
#else
    snprintf(v, sizeof v, "0");
#endif
    SET_STRING_ELT(build, 3, Rf_mkChar(v));
    UNPROTECT(1);
    return out;
}

/* ---- the builder (design 13.5) --------------------------------------------- */

#define ZUBIN_INTERRUPT_BYTES ((size_t)64 * 1024 * 1024)

static zb_buf *builder(SEXP ptr)
{
    return zb_r_buf_get(ptr);
}

/* max is Inf for no cap. */
SEXP zubin_builder_new(SEXP reserve, SEXP max)
{
    size_t r, m = 0;
    zb_status st;
    SEXP ptr;
    if (zubin_int_size(reserve, &r)) return zubin_int_status(ZB_ERR_INVALID, -1);
    if (!(TYPEOF(max) == REALSXP && XLENGTH(max) == 1 && REAL(max)[0] == R_PosInf)) {
        if (zubin_int_size(max, &m) || m == 0) return zubin_int_status(ZB_ERR_INVALID, -1);
    }
    ptr = zb_r_buf_new(r, m, &st);
    if (st) return zubin_int_status(st, -1);
    return ptr;
}

/* c(len, cap, max), max Inf when there is none. */
SEXP zubin_builder_state(SEXP ptr)
{
    zb_buf *b = builder(ptr);
    SEXP out;
    if (!b) return zubin_int_status(ZB_ERR_INVALID, -1);
    out = Rf_allocVector(REALSXP, 3);
    REAL(out)[0] = (double)b->len;
    REAL(out)[1] = (double)b->cap;
    REAL(out)[2] = b->max ? (double)b->max : R_PosInf;
    return out;
}

/* Appends a raw vector. The whole length is reserved first and len advances
   only once every chunk is copied, so an interrupt between chunks (every
   64 MiB, design 14.7) leaves the builder as it was. */
SEXP zubin_builder_put_raw(SEXP ptr, SEXP x)
{
    zb_buf *b = builder(ptr);
    size_t n, done = 0;
    zb_status st;
    if (!b || TYPEOF(x) != RAWSXP) return zubin_int_status(ZB_ERR_INVALID, -1);
    n = (size_t)XLENGTH(x);
    st = zb_buf_reserve(b, n);
    if (st) return zubin_int_status(st, -1);
    while (done < n) {
        size_t step = n - done < ZUBIN_INTERRUPT_BYTES ? n - done : ZUBIN_INTERRUPT_BYTES;
        if (done) R_CheckUserInterrupt();
        memcpy(b->data + b->len + done, RAW(x) + done, step);
        done += step;
    }
    b->len += n;
    return R_NilValue;
}

/* Appends strings: width < 0 is "z" (the UTF-8 bytes and a NUL), width >= 0
   is "s<width>" (NUL-padded to width; longer is ZB_ERR_RANGE). NA is
   ZB_ERR_NA. Everything is checked before anything is written. */
SEXP zubin_builder_put_str(SEXP ptr, SEXP x, SEXP width)
{
    zb_buf *b = builder(ptr);
    R_xlen_t i, n;
    size_t total = 0, w = 0;
    int z;
    zb_status st;
    uint8_t *slot;
    const void *vmax = vmaxget();
    if (!b || TYPEOF(x) != STRSXP || TYPEOF(width) != INTSXP || XLENGTH(width) != 1)
        return zubin_int_status(ZB_ERR_INVALID, -1);
    z = INTEGER(width)[0] < 0;
    if (!z) w = (size_t)INTEGER(width)[0];
    n = XLENGTH(x);
    for (i = 0; i < n; i++) {
        SEXP s = STRING_ELT(x, i);
        size_t len;
        if (s == NA_STRING) return zubin_int_status(ZB_ERR_NA, i);
        len = strlen(Rf_translateCharUTF8(s));
        vmaxset(vmax);
        if (z) {
            if (zb_int_add(total, len, &total) || zb_int_add(total, 1, &total))
                return zubin_int_status(ZB_ERR_MEMORY, -1);
        } else if (len > w) {
            return zubin_int_status(ZB_ERR_RANGE, i);
        }
    }
    if (!z && zb_int_mul((size_t)n, w, &total)) return zubin_int_status(ZB_ERR_MEMORY, -1);
    slot = zb_put_raw(b, total);
    if (!slot) {
        st = zb_buf_reserve(b, total);
        return zubin_int_status(st ? st : ZB_ERR_MEMORY, -1);
    }
    for (i = 0; i < n; i++) {
        const char *c = Rf_translateCharUTF8(STRING_ELT(x, i));
        size_t len = strlen(c);
        memcpy(slot, c, len);
        if (z) {
            slot[len] = 0;
            slot += len + 1;
        } else {
            memset(slot + len, 0, w - len);
            slot += w;
        }
        vmaxset(vmax);
    }
    return R_NilValue;
}

SEXP zubin_builder_reserve(SEXP ptr, SEXP n)
{
    zb_buf *b = builder(ptr);
    size_t extra;
    zb_status st;
    if (!b || zubin_int_size(n, &extra)) return zubin_int_status(ZB_ERR_INVALID, -1);
    st = zb_buf_reserve(b, extra);
    return st ? zubin_int_status(st, -1) : R_NilValue;
}

SEXP zubin_builder_reset(SEXP ptr)
{
    zb_buf *b = builder(ptr);
    if (!b) return zubin_int_status(ZB_ERR_INVALID, -1);
    zb_buf_reset(b);
    return R_NilValue;
}

/* The bytes as a new raw vector; with reset = TRUE the builder is then
   emptied and keeps its capacity (bin_take), otherwise unchanged (as.raw). */
SEXP zubin_builder_take(SEXP ptr, SEXP reset)
{
    zb_buf *b = builder(ptr);
    SEXP out;
    if (!b) return zubin_int_status(ZB_ERR_INVALID, -1);
    out = zb_r_buf_to_raw(b);
    if (Rf_asLogical(reset) == TRUE) zb_buf_reset(b);
    return out;
}

/* ---- layouts (design 11, 13.2) ---------------------------------------------- */

/* Parses a spec held in an R string into fields R_alloc()ed here, so they
   live until the .Call returns. max_fields 0 sizes the array from the spec. */
zb_status zubin_int_parse(SEXP spec, int big, int align, uint32_t max_fields,
                          zb_layout *out, size_t *err_pos)
{
    const char *s;
    size_t n;
    zb_field *fields;
    if (TYPEOF(spec) != STRSXP || XLENGTH(spec) != 1 || STRING_ELT(spec, 0) == NA_STRING) {
        *err_pos = 0;
        return ZB_ERR_INVALID;
    }
    s = CHAR(STRING_ELT(spec, 0));
    n = strlen(s);
    if (!max_fields) max_fields = zb_layout_count_fields(s, n);
    fields = (zb_field *)R_alloc(max_fields, sizeof(zb_field));
    return zb_layout_parse(s, n, big, align, fields, max_fields, out, err_pos);
}

/* The field table of a spec: type codes, element counts (1 for b, s and x,
   whose width is their size), sizes, offsets, byte order (NA for one-byte
   types), names (NA when unnamed) and each name's byte position; then the
   record size and alignment. On failure, a status with `position`. */
SEXP zubin_layout_parse(SEXP spec, SEXP big, SEXP align)
{
    const char *names[] = {"type", "count", "size", "offset", "big", "name", "name_pos",
                           "record_size", "align", ""};
    zb_layout l;
    size_t err = 0;
    uint32_t i;
    zb_status st;
    SEXP out, type, count, size, offset, bigs, name, pos;
    const char *base;
    st = zubin_int_parse(spec, Rf_asLogical(big) == TRUE, Rf_asLogical(align) == TRUE, 0, &l, &err);
    if (st) {
        SEXP pos;
        out = PROTECT(zubin_int_status(st, -1));
        pos = PROTECT(Rf_ScalarReal((double)err));
        Rf_setAttrib(out, Rf_install("position"), pos);
        UNPROTECT(2);
        return out;
    }
    base = CHAR(STRING_ELT(spec, 0));
    out = PROTECT(Rf_mkNamed(VECSXP, names));
    type = Rf_allocVector(INTSXP, l.nfields);   SET_VECTOR_ELT(out, 0, type);
    count = Rf_allocVector(INTSXP, l.nfields);  SET_VECTOR_ELT(out, 1, count);
    size = Rf_allocVector(INTSXP, l.nfields);   SET_VECTOR_ELT(out, 2, size);
    offset = Rf_allocVector(INTSXP, l.nfields); SET_VECTOR_ELT(out, 3, offset);
    bigs = Rf_allocVector(LGLSXP, l.nfields);   SET_VECTOR_ELT(out, 4, bigs);
    name = Rf_allocVector(STRSXP, l.nfields);   SET_VECTOR_ELT(out, 5, name);
    pos = Rf_allocVector(INTSXP, l.nfields);    SET_VECTOR_ELT(out, 6, pos);
    for (i = 0; i < l.nfields; i++) {
        const zb_field *f = &l.fields[i];
        int wide = f->type == ZB_BYTES || f->type == ZB_STR || f->type == ZB_PAD;
        INTEGER(type)[i] = (int)f->type;
        INTEGER(count)[i] = wide ? 1 : (int)f->count;
        INTEGER(size)[i] = (int)f->size;
        INTEGER(offset)[i] = (int)f->offset;
        LOGICAL(bigs)[i] = zb_type_width(f->type) > 1 ? f->big_endian : NA_LOGICAL;
        if (f->name) {
            SET_STRING_ELT(name, i, Rf_mkCharLenCE(f->name, (int)f->name_len, CE_UTF8));
            INTEGER(pos)[i] = (int)(f->name - base);
        } else {
            SET_STRING_ELT(name, i, NA_STRING);
            INTEGER(pos)[i] = NA_INTEGER;
        }
    }
    SET_VECTOR_ELT(out, 7, Rf_ScalarInteger((int)l.size));
    SET_VECTOR_ELT(out, 8, Rf_ScalarInteger((int)l.align));
    UNPROTECT(1);
    return out;
}

/* ---- unpack (design 11.5, 13.3) ------------------------------------------ */

/* A column for one value field: n rows, and a matrix of `count` columns for
   an array field. */
static SEXP alloc_column(SEXPTYPE type, size_t n, uint32_t count)
{
    if (count > 1) return Rf_allocMatrix(type, (int)n, (int)count);
    return Rf_allocVector(type, (R_xlen_t)n);
}

/* s<n> into a character vector: bytes up to the first NUL or the width,
   validated as UTF-8 (encoding 0), or marked latin1 (1) or bytes (2). On an
   invalid string returns its record index, else -1. */
static R_xlen_t strings_column(const uint8_t *base, size_t n, size_t stride, const zb_field *f,
                               int encoding, SEXP col)
{
    size_t i;
    cetype_t ce = encoding == 1 ? CE_LATIN1 : encoding == 2 ? CE_BYTES : CE_UTF8;
    for (i = 0; i < n; i++) {
        const char *p = (const char *)(base + i * stride + f->offset);
        const char *nul = (const char *)memchr(p, 0, f->size);
        size_t len = nul ? (size_t)(nul - p) : f->size;
        if (encoding == 0 && !zuf_utf8_valid(p, len)) return (R_xlen_t)i;
        SET_STRING_ELT(col, (R_xlen_t)i, Rf_mkCharLenCE(p, (int)len, ce));
    }
    return -1;
}

/* Unpacks n records of the layout `spec` (normalised, aligned when align is
   TRUE) from x, the first at offset and the rest every stride bytes. n < 0
   reads every whole record that fits; stride < 0 is the record size. int64
   is 0 for double columns, 1 for integer64 bits; encoding 0, 1, 2 is UTF-8,
   latin1, bytes. Returns the value columns in a named list, or a status:
   ZB_ERR_EOF when the records do not fit, ZB_ERR_INVALID for a stride
   below the record size, ZB_ERR_RANGE with the field and record index of a
   value that does not fit, ZUBIN_ERR_ENCODING likewise for a string. */
SEXP zubin_unpack(SEXP x, SEXP spec, SEXP align, SEXP offset, SEXP n, SEXP stride,
                  SEXP int64, SEXP allow_na, SEXP encoding)
{
    zb_layout l;
    size_t err = 0, off, len, rs, nrec, last, i;
    uint32_t j, nv = 0;
    int as64 = Rf_asInteger(int64) == 1, na_ok = Rf_asLogical(allow_na) == TRUE;
    int enc = Rf_asInteger(encoding);
    zb_status st;
    const uint8_t *base;
    SEXP out, names;

    if (TYPEOF(x) != RAWSXP) return zubin_int_status(ZB_ERR_INVALID, -1);
    st = zubin_int_parse(spec, 0, Rf_asLogical(align) == TRUE, 0, &l, &err);
    if (st) return zubin_int_status(st, -1);
    len = (size_t)XLENGTH(x);
    if (zubin_int_size(offset, &off)) return zubin_int_status(ZB_ERR_INVALID, -1);
    if (Rf_asReal(stride) < 0) rs = l.size;
    else if (zubin_int_size(stride, &rs)) return zubin_int_status(ZB_ERR_INVALID, -1);
    if (rs < l.size) return zubin_int_status(ZB_ERR_INVALID, -1);
    if (off > len) return zubin_int_status(ZB_ERR_EOF, -1);
    if (Rf_asReal(n) < 0) {
        size_t avail = len - off;
        nrec = avail < l.size ? 0 : (avail - l.size) / rs + 1;
    } else {
        if (zubin_int_size(n, &nrec)) return zubin_int_status(ZB_ERR_INVALID, -1);
        /* the last record must end inside x: off + (nrec - 1) * rs + size <= len */
        if (nrec > 0) {
            if (zb_int_mul(nrec - 1, rs, &last) || zb_int_add(last, l.size, &last) ||
                zb_int_add(last, off, &last) || last > len)
                return zubin_int_status(ZB_ERR_EOF, -1);
        }
    }
    if (nrec > (size_t)R_XLEN_T_MAX) return zubin_int_status(ZB_ERR_MEMORY, -1);
    base = len ? RAW(x) + off : NULL;

    for (j = 0; j < l.nfields; j++) nv += l.fields[j].type != ZB_PAD;
    out = PROTECT(Rf_allocVector(VECSXP, nv));
    names = Rf_allocVector(STRSXP, nv);
    Rf_setAttrib(out, R_NamesSymbol, names);

    for (j = 0, nv = 0; j < l.nfields; j++) {
        const zb_field *f = &l.fields[j];
        size_t bad = 0;
        SEXP col = R_NilValue;
        if (f->type == ZB_PAD) continue;
        if (f->count > 1 && nrec > (size_t)INT_MAX) {
            UNPROTECT(1);
            return zubin_int_status(ZB_ERR_MEMORY, -1);
        }
        if (j) R_CheckUserInterrupt();
        SET_STRING_ELT(names, nv, f->name ? Rf_mkCharLenCE(f->name, (int)f->name_len, CE_UTF8)
                                          : NA_STRING);
        switch (f->type) {
        case ZB_U8: case ZB_I8: case ZB_U16: case ZB_I16: case ZB_I32: case ZB_BOOL:
            col = alloc_column(f->type == ZB_BOOL ? LGLSXP : INTSXP, nrec, f->count);
            SET_VECTOR_ELT(out, nv, col);
            st = zb_unpack_i32(base, nrec, rs, f,
                               f->type == ZB_BOOL ? LOGICAL(col) : INTEGER(col), na_ok, &bad);
            break;
        case ZB_U32: case ZB_F16: case ZB_BF16: case ZB_F32: case ZB_F64:
            col = alloc_column(REALSXP, nrec, f->count);
            SET_VECTOR_ELT(out, nv, col);
            st = zb_unpack_f64(base, nrec, rs, f, REAL(col));
            break;
        case ZB_I64: case ZB_U64:
            col = alloc_column(REALSXP, nrec, f->count);
            SET_VECTOR_ELT(out, nv, col);
            if (as64) {
                /* integer64 is the int64 bit pattern in a double */
                st = zb_unpack_i64(base, nrec, rs, f, (int64_t *)(void *)REAL(col), &bad);
                if (!st && !na_ok) {
                    /* -2^63 is bit64's NA_integer64_: as for i32, an error by
                       default rather than a silent missing value */
                    const int64_t *d = (const int64_t *)(const void *)REAL(col);
                    size_t m = nrec * f->count;
                    for (i = 0; i < m; i++) {
                        if (d[i] == INT64_MIN && (st == ZB_OK || i % nrec < bad)) {
                            bad = i % nrec;
                            st = ZB_ERR_RANGE;
                        }
                    }
                }
            } else {
                st = zb_unpack_f64x(base, nrec, rs, f, REAL(col), &bad);
            }
            break;
        case ZB_BYTES: {
            col = Rf_allocVector(VECSXP, (R_xlen_t)nrec);
            SET_VECTOR_ELT(out, nv, col);
            for (i = 0; i < nrec; i++) {
                SEXP r = Rf_allocVector(RAWSXP, f->size);
                SET_VECTOR_ELT(col, (R_xlen_t)i, r);
                memcpy(RAW(r), base + i * rs + f->offset, f->size);
            }
            st = ZB_OK;
            break;
        }
        case ZB_STR: {
            R_xlen_t at;
            col = Rf_allocVector(STRSXP, (R_xlen_t)nrec);
            SET_VECTOR_ELT(out, nv, col);
            at = strings_column(base, nrec, rs, f, enc, col);
            if (at >= 0) {
                UNPROTECT(1);
                return zubin_int_failure("ZUBIN_ERR_ENCODING", at, (int)j);
            }
            st = ZB_OK;
            break;
        }
        default:
            st = ZB_ERR_INVALID;
        }
        if (st) {
            UNPROTECT(1);
            return zubin_int_failure(zb_status_string(st), (R_xlen_t)bad, (int)j);
        }
        nv++;
    }
    UNPROTECT(1);
    return out;
}

/* ---- pack (design 11.5, 13.3-13.5) -------------------------------------- */

/* Writes one field of n records from an R column: integer or logical
   through zb_pack_i32, double through zb_pack_f64 (or zb_pack_i64 when it
   carries integer64 bits), character into s<n> (UTF-8, NUL-padded), and a
   list of raw vectors into b<n>. The column has n * count elements, which
   R arranged. The one pack path: bin_pack(), bin_encode() and typed
   bin_put() all come here. */
static zb_status pack_field(uint8_t *base, size_t n, size_t stride, const zb_field *f,
                            SEXP col, int is64, int allow_na, size_t *bad)
{
    size_t i, elems = f->type == ZB_BYTES || f->type == ZB_STR ? 1 : f->count;
    if ((size_t)XLENGTH(col) != n * elems) return ZB_ERR_INVALID;
    switch (TYPEOF(col)) {
    case INTSXP:
        return zb_pack_i32(base, n, stride, f, INTEGER(col), allow_na, bad);
    case LGLSXP:
        return zb_pack_i32(base, n, stride, f, LOGICAL(col), allow_na, bad);
    case REALSXP:
        if (is64) return zb_pack_i64(base, n, stride, f, (const int64_t *)(const void *)REAL(col), allow_na, bad);
        return zb_pack_f64(base, n, stride, f, REAL(col), allow_na, bad);
    case STRSXP: {
        const void *vmax = vmaxget();
        if (f->type != ZB_STR) return ZB_ERR_INVALID;
        for (i = 0; i < n; i++) {
            SEXP s = STRING_ELT(col, (R_xlen_t)i);
            const char *c;
            size_t len;
            uint8_t *p = base + i * stride + f->offset;
            if (s == NA_STRING) { *bad = i; return ZB_ERR_NA; }
            c = Rf_translateCharUTF8(s);
            len = strlen(c);
            if (len > f->size) { vmaxset(vmax); *bad = i; return ZB_ERR_RANGE; }
            memcpy(p, c, len);
            memset(p + len, 0, f->size - len);
            vmaxset(vmax);
        }
        return ZB_OK;
    }
    case VECSXP:
        if (f->type != ZB_BYTES) return ZB_ERR_INVALID;
        for (i = 0; i < n; i++) {
            SEXP r = VECTOR_ELT(col, (R_xlen_t)i);
            if (TYPEOF(r) != RAWSXP) { *bad = i; return ZB_ERR_INVALID; }
            if ((size_t)XLENGTH(r) != f->size) { *bad = i; return ZB_ERR_RANGE; }
            memcpy(base + i * stride + f->offset, RAW(r), f->size);
        }
        return ZB_OK;
    default:
        return ZB_ERR_INVALID;
    }
}

/* n records of the layout `spec` from `cols`, one column per value field in
   layout order (is64 flags the integer64 ones). The result is one
   exact-size raw vector, zeroed first so padding and alignment gaps are
   zeros and no byte is uninitialised; then each field is written. On
   failure, a status with the field's position and the record index. */
SEXP zubin_pack(SEXP spec, SEXP align, SEXP cols, SEXP is64, SEXP n, SEXP allow_na)
{
    zb_layout l;
    size_t err = 0, nrec, total;
    uint32_t j, c = 0;
    int na_ok = Rf_asLogical(allow_na) == TRUE;
    zb_status st;
    SEXP out;
    st = zubin_int_parse(spec, 0, Rf_asLogical(align) == TRUE, 0, &l, &err);
    if (st) return zubin_int_status(st, -1);
    if (TYPEOF(cols) != VECSXP || TYPEOF(is64) != LGLSXP || XLENGTH(is64) != XLENGTH(cols) ||
        zubin_int_size(n, &nrec)) return zubin_int_status(ZB_ERR_INVALID, -1);
    if (zb_int_mul(nrec, l.size, &total) || total > (size_t)R_XLEN_T_MAX)
        return zubin_int_status(ZB_ERR_MEMORY, -1);
    out = PROTECT(Rf_allocVector(RAWSXP, (R_xlen_t)total));
    {
        /* Zeroed first when the record has padding or alignment gaps, so
           they are zeros and no byte is uninitialised; when the value
           fields cover every byte, each is written below and the extra
           pass over the output is skipped. */
        size_t covered = 0;
        for (j = 0; j < l.nfields; j++) {
            if (l.fields[j].type != ZB_PAD) covered += l.fields[j].size;
        }
        if (total && covered != l.size) memset(RAW(out), 0, total);
    }
    for (j = 0; j < l.nfields; j++) {
        const zb_field *f = &l.fields[j];
        size_t bad = 0;
        if (f->type == ZB_PAD) continue;
        if (c >= (uint32_t)XLENGTH(cols)) {
            UNPROTECT(1);
            return zubin_int_status(ZB_ERR_INVALID, -1);
        }
        if (c) R_CheckUserInterrupt();
        st = pack_field(total ? RAW(out) : NULL, nrec, l.size, f, VECTOR_ELT(cols, c),
                        LOGICAL(is64)[c] == TRUE, na_ok, &bad);
        if (st) {
            UNPROTECT(1);
            return zubin_int_failure(zb_status_string(st), (R_xlen_t)bad, (int)j);
        }
        c++;
    }
    UNPROTECT(1);
    return out;
}

/* Appends a column encoded as the one-field layout `spec` to a builder.
   The space is reserved first and len advances only after the last chunk
   is written, with an interrupt check every 64 MiB in between, so neither
   an error nor an interrupt changes the builder. */
SEXP zubin_builder_put_typed(SEXP ptr, SEXP spec, SEXP col, SEXP is64)
{
    zb_buf *b = zb_r_buf_get(ptr);
    zb_layout l;
    size_t err = 0, n, total, done = 0, chunk;
    zb_status st;
    if (!b) return zubin_int_status(ZB_ERR_INVALID, -1);
    st = zubin_int_parse(spec, 0, 0, 0, &l, &err);
    if (st || l.nfields != 1 || l.fields[0].type == ZB_PAD ||
        (l.fields[0].type != ZB_BYTES && l.fields[0].type != ZB_STR && l.fields[0].count != 1))
        return zubin_int_status(ZB_ERR_INVALID, -1);
    n = (size_t)XLENGTH(col);
    if (zb_int_mul(n, l.size, &total)) return zubin_int_status(ZB_ERR_MEMORY, -1);
    st = zb_buf_reserve(b, total);
    if (st) return zubin_int_status(st, -1);
    chunk = ZUBIN_INTERRUPT_BYTES / l.size;
    if (!chunk) chunk = 1;
    while (done < n) {
        size_t m = n - done < chunk ? n - done : chunk, bad = 0;
        uint8_t *slot = b->data + b->len + done * l.size;
        SEXP part = col;
        if (done) R_CheckUserInterrupt();
        /* a chunk of the column: offset the kernel's source by `done` */
        switch (TYPEOF(col)) {
        case INTSXP: st = zb_pack_i32(slot, m, l.size, &l.fields[0], INTEGER(col) + done, 0, &bad); break;
        case LGLSXP: st = zb_pack_i32(slot, m, l.size, &l.fields[0], LOGICAL(col) + done, 0, &bad); break;
        case REALSXP:
            if (Rf_asLogical(is64) == TRUE)
                st = zb_pack_i64(slot, m, l.size, &l.fields[0], (const int64_t *)(const void *)REAL(col) + done, 0, &bad);
            else
                st = zb_pack_f64(slot, m, l.size, &l.fields[0], REAL(col) + done, 0, &bad);
            break;
        default:
            /* strings and byte lists go through pack_field whole */
            if (done) { st = ZB_ERR_INVALID; break; }
            st = pack_field(slot, n, l.size, &l.fields[0], part, 0, 0, &bad);
            m = n;
            break;
        }
        if (st) return zubin_int_failure(zb_status_string(st), (R_xlen_t)(done + bad), 0);
        done += m;
    }
    b->len += total;
    return R_NilValue;
}

/* ---- hexdump and diff (design 13.6) --------------------------------------- */

/* xxd-style lines for n bytes of x from offset, `width` bytes a line:
   the absolute offset in hex (8 digits, 16 past 4 GiB), the bytes in
   two-byte groups, and the printable ASCII, '.' for the rest. */
SEXP zubin_hexdump(SEXP x, SEXP offset, SEXP n, SEXP width)
{
    static const char digits[] = "0123456789abcdef";
    size_t off, cnt, w, len = (size_t)XLENGTH(x), lines, i, end;
    int odigits;
    char *line;
    SEXP out;
    if (TYPEOF(x) != RAWSXP || zubin_int_size(offset, &off) || zubin_int_size(width, &w) ||
        w == 0 || w > 256) return zubin_int_status(ZB_ERR_INVALID, -1);
    if (off > len) return zubin_int_status(ZB_ERR_EOF, -1);
    if (Rf_asReal(n) < 0) cnt = len - off;
    else if (zubin_int_size(n, &cnt)) return zubin_int_status(ZB_ERR_INVALID, -1);
    if (cnt > len - off) cnt = len - off;
    end = off + cnt;
    odigits = end > 0xFFFFFFFFu ? 16 : 8;
    lines = (cnt + w - 1) / w;
    /* offset, ": ", hex (2 per byte, a space per pair), two spaces, ASCII, NUL */
    line = R_alloc((size_t)odigits + 2 + 3 * w + 2 + w + 1, 1);
    out = PROTECT(Rf_allocVector(STRSXP, (R_xlen_t)lines));
    for (i = 0; i < lines; i++) {
        size_t start = off + i * w, k, m = end - start < w ? end - start : w;
        char *p = line;
        int d;
        for (d = odigits - 1; d >= 0; d--) *p++ = digits[(start >> (4 * d)) & 15];
        *p++ = ':';
        *p++ = ' ';
        for (k = 0; k < w; k++) {
            if (k < m) {
                uint8_t v = RAW(x)[start + k];
                *p++ = digits[v >> 4];
                *p++ = digits[v & 15];
            } else {
                *p++ = ' ';
                *p++ = ' ';
            }
            if (k % 2 == 1 && k + 1 < w) *p++ = ' ';
        }
        *p++ = ' ';
        *p++ = ' ';
        for (k = 0; k < m; k++) {
            uint8_t v = RAW(x)[start + k];
            *p++ = v >= 0x20 && v < 0x7f ? (char)v : '.';
        }
        *p = 0;
        SET_STRING_ELT(out, (R_xlen_t)i, Rf_mkCharCE(line, CE_UTF8));
    }
    UNPROTECT(1);
    return out;
}

/* The first n offsets at which a and b differ, over their common length,
   with both bytes there. */
SEXP zubin_diff(SEXP a, SEXP b, SEXP n)
{
    const char *names[] = {"offset", "a", "b", ""};
    size_t la, lb, common, i, max, found = 0;
    size_t *at;
    SEXP out, off, va, vb;
    if (TYPEOF(a) != RAWSXP || TYPEOF(b) != RAWSXP || zubin_int_size(n, &max))
        return zubin_int_status(ZB_ERR_INVALID, -1);
    la = (size_t)XLENGTH(a);
    lb = (size_t)XLENGTH(b);
    common = la < lb ? la : lb;
    at = (size_t *)R_alloc(max ? max : 1, sizeof(size_t));
    for (i = 0; i < common && found < max; i++) {
        if (RAW(a)[i] != RAW(b)[i]) at[found++] = i;
    }
    out = PROTECT(Rf_mkNamed(VECSXP, names));
    off = Rf_allocVector(REALSXP, (R_xlen_t)found); SET_VECTOR_ELT(out, 0, off);
    va = Rf_allocVector(RAWSXP, (R_xlen_t)found);   SET_VECTOR_ELT(out, 1, va);
    vb = Rf_allocVector(RAWSXP, (R_xlen_t)found);   SET_VECTOR_ELT(out, 2, vb);
    for (i = 0; i < found; i++) {
        REAL(off)[i] = (double)at[i];
        RAW(va)[i] = RAW(a)[at[i]];
        RAW(vb)[i] = RAW(b)[at[i]];
    }
    UNPROTECT(1);
    return out;
}

/* ---- serialization streams (Stage 10) --------------------------------------- */

static void count_sink(void *state, const void *p, size_t n)
{
    (void)p;
    *(size_t *)state += n;
}

/* Appends x's serialization to the builder. On ZB_ERR_LIMIT or
   ZB_ERR_MEMORY the status carries `size`, what the builder would have
   held, measured by serializing again into a counter: the failure path pays
   for it, the success path does not. */
SEXP zubin_serialize(SEXP ptr, SEXP x, SEXP version, SEXP xdr, SEXP refhook)
{
    zb_buf *b = zb_r_buf_get(ptr);
    int v = Rf_asInteger(version), x_dr = Rf_asLogical(xdr) == TRUE;
    zb_status st;
    if (!b) return zubin_int_status(ZB_ERR_INVALID, -1);
    st = zb_serialize(x, b, v, x_dr, refhook);
    if (st == ZB_ERR_LIMIT || st == ZB_ERR_MEMORY) {
        size_t need = 0;
        SEXP out, size;
        zb_r_int_serialize(x, count_sink, &need, v, x_dr, 0, refhook);
        out = PROTECT(zubin_int_status(st, -1));
        size = PROTECT(Rf_ScalarReal((double)b->len + (double)need));
        Rf_setAttrib(out, Rf_install("size"), size);
        UNPROTECT(2);
        return out;
    }
    return st ? zubin_int_status(st, -1) : R_NilValue;
}

/* One object from the stream that starts at `offset` in x, or ZB_ERR_EOF
   when the stream runs out. */
SEXP zubin_unserialize(SEXP x, SEXP offset, SEXP refhook)
{
    size_t off, len;
    zb_cur c;
    zb_status st;
    SEXP out;
    if (TYPEOF(x) != RAWSXP || zubin_int_size(offset, &off)) return zubin_int_status(ZB_ERR_INVALID, -1);
    len = (size_t)XLENGTH(x);
    if (off > len) return zubin_int_status(ZB_ERR_EOF, -1);
    zb_cur_init(&c, len ? RAW(x) : NULL, len);
    zb_cur_seek(&c, off);
    out = PROTECT(zb_unserialize(&c, refhook, &st));
    if (st) {
        UNPROTECT(1);
        return zubin_int_status(st, -1);
    }
    UNPROTECT(1);
    return out;
}

static void hash_sink(void *state, const void *p, size_t n)
{
    zuf_hasher_update((zuf_hasher *)state, p, n);
}

static void hex64_be(char *dst, uint64_t v)
{
    static const char digits[] = "0123456789abcdef";
    int i;
    for (i = 15; i >= 0; i--) {
        dst[i] = digits[v & 15];
        v >>= 4;
    }
}

/* XXH3 of x's serialization with the header skipped, streamed through the
   hasher: nothing is allocated, and the digest does not depend on the R
   version. The hasher lives on the stack, so a longjmp out of R_Serialize
   leaves nothing behind. Hex as zufast's fast_hash() writes it. */
SEXP zubin_hash_object(SEXP x, SEXP bits, SEXP version, SEXP seed)
{
    zuf_hasher h;
    char buf[32];
    int b = Rf_asInteger(bits);
    zuf_hasher_init(&h, (uint64_t)Rf_asReal(seed));
    zb_serialize_to_sink(x, hash_sink, &h, Rf_asInteger(version), 1, 1);
    if (b == 64) {
        hex64_be(buf, zuf_hasher_digest64(&h));
        return Rf_ScalarString(Rf_mkCharLen(buf, 16));
    } else {
        zuf_digest128 d = zuf_hasher_digest128(&h);
        hex64_be(buf, d.high);
        hex64_be(buf + 16, d.low);
        return Rf_ScalarString(Rf_mkCharLen(buf, 32));
    }
}
