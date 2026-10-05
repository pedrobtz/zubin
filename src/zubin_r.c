/* The .Call wrappers behind R/ (design 15). They include <zubin.h> like any
   consumer, so CRAN's instrumented builds exercise the headers through the
   package's own tests. */
#include <limits.h>
#include <stdio.h>
#include <math.h>

#include "zubin_r.h"

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
