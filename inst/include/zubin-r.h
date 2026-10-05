/*
 * zubin-r.h -- SEXP glue for consumers that are R packages (design 12).
 *
 * The only zubin header that includes R's, and not included by <zubin.h>:
 * include it by name, after defining R_NO_REMAP if you want R's API without
 * the short names.
 *
 *     zb_status st;
 *     SEXP ptr = PROTECT(zb_r_buf_new(0, 0, &st));     -- R_NilValue on failure
 *     zb_buf *b = zb_r_buf_get(ptr);
 *     zb_put_u32le(b, 1);                                -- may longjmp below: no leak
 *     SEXP out = zb_r_buf_to_raw(b);
 *     zb_r_buf_free(ptr);                                -- eager; the GC would too
 *
 * The rule it encodes is zukomp's: heap state that must survive a longjmp
 * from Rf_error() or R_CheckUserInterrupt() is owned by R before the first
 * call that can jump. zb_r_buf_new() creates the external pointer and
 * registers its finalizer (onexit = TRUE) before the buffer exists, and the
 * finalizer clears the pointer before freeing, so an eager zb_r_buf_free()
 * and a later collection are mutually safe. Nothing here raises an R error:
 * failures are statuses, and the caller decides how to report them.
 *
 * The serialization streams at the end (zb_serialize, zb_unserialize,
 * zb_serialize_to_sink) are the exception that proves the rule: R's own
 * errors during R_Serialize and R_Unserialize are R's, and reach the caller
 * unchanged; only the failures zubin owns come back as statuses.
 */
#ifndef ZUBIN_R_GLUE_H
#define ZUBIN_R_GLUE_H

#include "zubin/buf.h"
#include "zubin/cursor.h"

#include <Rinternals.h>

/* Test hooks: a package may define these before including this header to
   observe every buffer the glue creates and frees (zubin counts them, to
   prove that an interrupt leaks nothing). Internal, outside design 4.4. */
#ifndef ZB_INT_R_ON_NEW
#  define ZB_INT_R_ON_NEW() ((void)0)
#endif
#ifndef ZB_INT_R_ON_FREE
#  define ZB_INT_R_ON_FREE() ((void)0)
#endif

/* The tag that marks an external pointer as one of these buffers. */
ZB_INLINE SEXP zb_r_int_buf_tag(void)
{
    return Rf_install("zubin_buf");
}

ZB_INLINE void zb_r_int_buf_finalize(SEXP ptr)
{
    zb_buf *b = (zb_buf *)R_ExternalPtrAddr(ptr);
    if (!b) return;
    R_ClearExternalPtr(ptr);
    zb_buf_release(b);
    free(b);
    ZB_INT_R_ON_FREE();
}

/* A growable buffer with room for `reserve` bytes and the hard cap `max`
   (0: none), owned by a new external pointer. On failure returns R_NilValue
   and sets *st to ZB_ERR_LIMIT (reserve > max) or ZB_ERR_MEMORY; on success
   *st is ZB_OK. The pointer is unprotected, as from any allocator. */
ZB_INLINE SEXP zb_r_buf_new(size_t reserve, size_t max, zb_status *st)
{
    zb_buf *b;
    SEXP ptr;
    if (max && reserve > max) {
        *st = ZB_ERR_LIMIT;
        return R_NilValue;
    }
    /* Both of these can longjmp; nothing is allocated yet. */
    ptr = PROTECT(R_MakeExternalPtr(NULL, zb_r_int_buf_tag(), R_NilValue));
    R_RegisterCFinalizerEx(ptr, zb_r_int_buf_finalize, TRUE);
    b = (zb_buf *)malloc(sizeof *b);
    if (!b) {
        UNPROTECT(1);
        *st = ZB_ERR_MEMORY;
        return R_NilValue;
    }
    zb_buf_init(b);
    R_SetExternalPtrAddr(ptr, b);
    ZB_INT_R_ON_NEW();
    *st = zb_buf_alloc(b, reserve, max);
    if (*st) {
        zb_r_int_buf_finalize(ptr);
        UNPROTECT(1);
        return R_NilValue;
    }
    UNPROTECT(1);
    return ptr;
}

/* The buffer behind ptr; NULL when ptr is not one of these buffers or has
   been freed (eagerly, or because it was serialized and restored). Allocates
   nothing, so its caller need protect nothing around it: the tag is compared
   by name rather than through Rf_install(). */
ZB_INLINE zb_buf *zb_r_buf_get(SEXP ptr)
{
    SEXP tag;
    if (TYPEOF(ptr) != EXTPTRSXP) return NULL;
    tag = R_ExternalPtrTag(ptr);
    if (TYPEOF(tag) != SYMSXP || strcmp(CHAR(PRINTNAME(tag)), "zubin_buf") != 0) return NULL;
    return (zb_buf *)R_ExternalPtrAddr(ptr);
}

/* Frees the buffer now rather than at the next collection; the external
   pointer stays valid and zb_r_buf_get() returns NULL from then on. */
ZB_INLINE void zb_r_buf_free(SEXP ptr)
{
    if (zb_r_buf_get(ptr)) zb_r_int_buf_finalize(ptr);
}

/* Borrows a raw vector's bytes (zb_buf_borrow): the caller keeps raw
   protected for as long as it uses b, and b needs no release. */
ZB_INLINE void zb_r_buf_borrow(zb_buf *b, SEXP raw)
{
    R_xlen_t n = XLENGTH(raw);
    zb_buf_borrow(b, n ? RAW(raw) : NULL, (size_t)n);
}

/* A new raw vector holding a copy of the buffer's bytes; unprotected. */
ZB_INLINE SEXP zb_r_buf_to_raw(const zb_buf *b)
{
    SEXP out = Rf_allocVector(RAWSXP, (R_xlen_t)b->len);
    if (b->len) memcpy(RAW(out), b->data, b->len);
    return out;
}

/* ---- serialization streams (Stage 10, issue #25) -------------------------------

   R's own serialization pointed at zubin's byte containers instead of a raw
   vector or a connection, so a stream of any size is built, hashed or read
   without being allocated whole. The formats are R's: version 2 or 3, XDR
   (big-endian; xdr nonzero) or native binary. Unserialization reads any of
   them, ASCII too.

   Errors are R's. R_Serialize and R_Unserialize raise for an unserializable
   object, a malformed stream or a refhook that errors, and those conditions
   reach the caller unchanged, class and all. The failures reported as a
   status are the ones zubin owns: an in-stream that runs out (ZB_ERR_EOF),
   and a builder that cannot grow (ZB_ERR_LIMIT, ZB_ERR_MEMORY). */

/* A consumer's sink. It receives the stream in R's own pieces, in order, and
   must not call R_CheckUserInterrupt() or anything else that can jump. */
typedef void (*zb_sink_fn)(void *state, const void *p, size_t n);

/* The out-stream's state: the sink, and the header filter. The header is the
   format tag (2 bytes), three 4-byte integers (the stream version, R's
   version, the oldest R that reads it) and, in version 3, the native
   encoding as a 4-byte length and its bytes; integers are big-endian under
   XDR and native otherwise. Internal. */
typedef struct {
    zb_sink_fn fn;
    void *state;
    int skip;              /* still dropping the header */
    int xdr;
    int version;
    unsigned char head[18];
    size_t seen;           /* header bytes dropped so far */
    size_t need;           /* the header's length, once known */
} zb_r_int_sink;

ZB_INLINE void zb_r_int_outbytes(R_outpstream_t stream, void *buf, int n)
{
    zb_r_int_sink *s = (zb_r_int_sink *)stream->data;
    const unsigned char *p = (const unsigned char *)buf;
    size_t m = n > 0 ? (size_t)n : 0;
    while (m && s->skip) {
        if (s->seen < sizeof s->head) s->head[s->seen] = *p;
        s->seen++;
        p++;
        m--;
        if (s->version >= 3 && s->seen == 18) {
            uint32_t nelen;
            if (s->xdr) nelen = zuf_load_be32(s->head + 14);
            else memcpy(&nelen, s->head + 14, 4);
            s->need = 18 + (size_t)nelen;
        }
        if (s->seen == s->need) s->skip = 0;
    }
    if (m) s->fn(s->state, p, m);
}

ZB_INLINE void zb_r_int_outchar(R_outpstream_t stream, int c)
{
    unsigned char b = (unsigned char)c;
    zb_r_int_outbytes(stream, &b, 1);
}

/* A refhook, called as base R's serialize() and unserialize() call one. */
ZB_INLINE SEXP zb_r_int_call_hook(SEXP x, SEXP fun)
{
    SEXP call = PROTECT(Rf_lang2(fun, x));
    SEXP val = Rf_eval(call, R_GlobalEnv);
    UNPROTECT(1);
    return val;
}

ZB_INLINE void zb_r_int_serialize(SEXP x, zb_sink_fn fn, void *state, int version, int xdr,
                                  int skip_header, SEXP refhook)
{
    struct R_outpstream_st out;
    zb_r_int_sink s;
    int hook = !Rf_isNull(refhook);
    memset(&s, 0, sizeof s);
    s.fn = fn;
    s.state = state;
    s.skip = skip_header != 0;
    s.xdr = xdr != 0;
    s.version = version;
    s.need = version >= 3 ? 18 : 14;
    R_InitOutPStream(&out, (R_pstream_data_t)&s,
                     xdr ? R_pstream_xdr_format : R_pstream_binary_format, version,
                     zb_r_int_outchar, zb_r_int_outbytes,
                     hook ? zb_r_int_call_hook : NULL, hook ? refhook : R_NilValue);
    R_Serialize(x, &out);
}

/* Serializes x into fn, in R's pieces: what a block pipeline consumes. With
   skip_header the header is dropped, so that a digest of what the sink sees
   does not change with the R version that wrote it (nor, in version 3, with
   the native encoding). Raises as R_Serialize raises. */
ZB_INLINE void zb_serialize_to_sink(SEXP x, zb_sink_fn fn, void *state,
                                    int version, int xdr, int skip_header)
{
    zb_r_int_serialize(x, fn, state, version, xdr, skip_header, R_NilValue);
}

/* zb_serialize: the builder as the sink. A builder that refuses to grow
   keeps refusing; the stream runs to its end into nothing, and the status
   and the size it would have needed are reported afterwards. */
typedef struct {
    zb_buf *b;
    size_t len0;           /* the builder's length before, restored on failure */
    size_t total;          /* bytes R produced, whether they fitted or not */
    zb_status st;
    SEXP x, refhook;
    int version, xdr;
} zb_r_int_ser;

ZB_INLINE void zb_r_int_bufsink(void *state, const void *p, size_t n)
{
    zb_r_int_ser *s = (zb_r_int_ser *)state;
    s->total += n;
    if (!s->st) s->st = zb_put_bytes(s->b, p, n);
}

ZB_INLINE SEXP zb_r_int_ser_body(void *data)
{
    zb_r_int_ser *s = (zb_r_int_ser *)data;
    zb_r_int_serialize(s->x, zb_r_int_bufsink, s, s->version, s->xdr, 0, s->refhook);
    return R_NilValue;
}

ZB_INLINE void zb_r_int_ser_clean(void *data, Rboolean jump)
{
    zb_r_int_ser *s = (zb_r_int_ser *)data;
    if (jump) s->b->len = s->len0;   /* an error or an interrupt appends nothing */
}

/* Appends x's serialization to b. ZB_ERR_INVALID for a version other than 2
   or 3; ZB_ERR_LIMIT or ZB_ERR_MEMORY when b cannot hold it, with
   *size, when size is not NULL, set to the length b would have needed. On any
   failure, and when R_Serialize raises (b's memory must then be owned by R, as
   zb_r_buf_new() makes it), b holds exactly what it held. */
ZB_INLINE zb_status zb_serialize(SEXP x, zb_buf *b, int version, int xdr, SEXP refhook)
{
    zb_r_int_ser s;
    SEXP cont;
    if (version != 2 && version != 3) return ZB_ERR_INVALID;
    memset(&s, 0, sizeof s);
    s.b = b;
    s.len0 = b->len;
    s.x = x;
    s.refhook = refhook;
    s.version = version;
    s.xdr = xdr;
    cont = PROTECT(R_MakeUnwindCont());
    R_UnwindProtect(zb_r_int_ser_body, &s, zb_r_int_ser_clean, &s, cont);
    UNPROTECT(1);
    if (s.st) b->len = s.len0;
    return s.st;
}

/* zb_unserialize: the cursor as the in-stream. */
typedef struct {
    const uint8_t *base;
    size_t len, pos;
    int eof;
    SEXP refhook;
} zb_r_int_in;

ZB_INLINE void zb_r_int_in_eof(zb_r_int_in *s)
{
    /* The one way out of R_Unserialize is a longjmp: signal a condition of
       a class private to this header, which zb_unserialize alone catches. */
    SEXP cond, cls, msg, call;
    s->eof = 1;
    msg = PROTECT(Rf_mkString("zubin: the serialization stream ends early"));
    cond = PROTECT(Rf_allocVector(VECSXP, 2));
    SET_VECTOR_ELT(cond, 0, msg);
    SET_VECTOR_ELT(cond, 1, R_NilValue);
    {
        SEXP nms = PROTECT(Rf_allocVector(STRSXP, 2));
        SET_STRING_ELT(nms, 0, Rf_mkChar("message"));
        SET_STRING_ELT(nms, 1, Rf_mkChar("call"));
        Rf_setAttrib(cond, R_NamesSymbol, nms);
        UNPROTECT(1);
    }
    cls = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(cls, 0, Rf_mkChar("zb_int_stream_eof"));
    SET_STRING_ELT(cls, 1, Rf_mkChar("error"));
    SET_STRING_ELT(cls, 2, Rf_mkChar("condition"));
    Rf_setAttrib(cond, R_ClassSymbol, cls);
    call = PROTECT(Rf_lang2(Rf_install("stop"), cond));
    Rf_eval(call, R_BaseEnv);
    UNPROTECT(4);   /* not reached */
}

ZB_INLINE void zb_r_int_inbytes(R_inpstream_t stream, void *buf, int n)
{
    zb_r_int_in *s = (zb_r_int_in *)stream->data;
    size_t m = n > 0 ? (size_t)n : 0;
    if (m > s->len - s->pos) zb_r_int_in_eof(s);
    if (m) memcpy(buf, s->base + s->pos, m);
    s->pos += m;
}

ZB_INLINE int zb_r_int_inchar(R_inpstream_t stream)
{
    unsigned char b;
    zb_r_int_inbytes(stream, &b, 1);
    return b;
}

ZB_INLINE SEXP zb_r_int_unser_body(void *data)
{
    zb_r_int_in *s = (zb_r_int_in *)data;
    struct R_inpstream_st in;
    int hook = !Rf_isNull(s->refhook);
    R_InitInPStream(&in, (R_pstream_data_t)s, R_pstream_any_format,
                    zb_r_int_inchar, zb_r_int_inbytes,
                    hook ? zb_r_int_call_hook : NULL, hook ? s->refhook : R_NilValue);
    return R_Unserialize(&in);
}

ZB_INLINE SEXP zb_r_int_unser_eof(SEXP cond, void *data)
{
    (void)cond;
    (void)data;
    return R_NilValue;
}

/* Unserializes one stream at the cursor and advances it past the stream,
   leaving any bytes after it for the next read. A stream that runs out is
   R_NilValue with *st = ZB_ERR_EOF and the cursor unchanged; every other
   error is R's (or the refhook's) and propagates unchanged. *st is ZB_OK
   otherwise. The result is unprotected. */
ZB_INLINE SEXP zb_unserialize(zb_cur *c, SEXP refhook, zb_status *st)
{
    zb_r_int_in s;
    SEXP cls, out;
    memset(&s, 0, sizeof s);
    s.base = c->base ? c->base + c->pos : c->base;
    s.len = c->len - c->pos;
    s.refhook = refhook;
    cls = PROTECT(Rf_mkString("zb_int_stream_eof"));
    out = R_tryCatch(zb_r_int_unser_body, &s, cls, zb_r_int_unser_eof, NULL, NULL, NULL);
    UNPROTECT(1);
    if (s.eof) {
        *st = ZB_ERR_EOF;
        return R_NilValue;
    }
    c->pos += s.pos;
    *st = ZB_OK;
    return out;
}

#endif /* ZUBIN_R_GLUE_H */
