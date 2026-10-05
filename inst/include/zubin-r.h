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
 */
#ifndef ZUBIN_R_GLUE_H
#define ZUBIN_R_GLUE_H

#include "zubin/buf.h"

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

#endif /* ZUBIN_R_GLUE_H */
