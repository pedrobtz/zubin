# The C API

zubin’s R functions are built on a header-only C99 library, and any
package can use the same code: the buffer, the cursor, the typed reads
and writes, and the record layouts with their unpack and pack kernels.
This page is for the author of such a package.

## Linking

There is nothing to link. Every function is `static inline` in a header,
so a package that includes them carries its own copy of what it uses,
and zubin need not be installed when it runs. The whole recipe is three
files:

`DESCRIPTION` (an excerpt):

    LinkingTo: zubin, zufast

`NAMESPACE`:

    useDynLib(zubintest, .registration = TRUE)

`src/Makevars`:

``` make
PKG_CFLAGS = $(C_VISIBILITY)
```

`LinkingTo` names both zubin and zufast because it is not transitive: R
adds each named package’s `include/` directory to the compiler’s path,
and zubin’s headers include zufast’s `<zufast/bits.h>` and
`<zufast/utf8.h>`. `$(C_VISIBILITY)` hides your symbols; the headers
emit nothing that is not already `static`, so two packages that both use
zubin never bind to each other’s copies.

The contract is source compatibility, as for zufast: within major
version 1 a function, type, macro or enumerator that exists keeps
existing with the same meaning, enumerator values are permanent, and
struct fields are only ever appended. Initialise every struct through
its `zb_*` function, never by aggregate initialisation, so an appended
field has a defined value. `ZUBIN_VERSION_NUMBER` is
`10000 * major + 100 * minor + patch`.

## The headers

| Header | What it holds |
|----|----|
| `<zubin.h>` | everything below except `zubin-r.h` |
| `<zubin/status.h>` | `zb_status` and `zb_status_string()` |
| `<zubin/rw.h>` | `zb_rd_<type><order>()` and `zb_wr_...()` for every type at any alignment |
| `<zubin/cursor.h>` | `zb_cur`: checked sequential reads |
| `<zubin/buf.h>` | `zb_buf`: a buffer with ownership, a hard cap and checked growth |
| `<zubin/layout.h>` | record layouts, the spec parser, and the unpack and pack kernels |
| `<zubin-r.h>` | a `zb_buf` owned by an R external pointer; the only header that includes R’s |

Every function that can fail returns a `zb_status`, which is `ZB_OK` (0)
on success; results come back through out-parameters. **On failure the
inputs are unchanged**: a cursor that returns `ZB_ERR_EOF` has not
moved, a buffer that returns `ZB_ERR_LIMIT` holds what it held. Nothing
in the headers prints, aborts, exits or allocates, except `buf.h`, and
that only through `malloc`, `realloc` and `free`.

## Reading: the cursor

A cursor is a pointer, a length and a 0-based position, so every error a
reader reports is “at byte `c.pos`”. One read per type and byte order:
`zb_cur_u32le()`, `zb_cur_f64be()`, `zb_cur_bytes()` to borrow a run of
bytes, `zb_cur_seek()` and `zb_cur_skip()`.

``` c
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
```

## Records: layouts and kernels

A layout is parsed from the same specification string
[`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md)
takes, into a field array the caller supplies, so the parser allocates
nothing. Size the array with `zb_layout_count_fields()`; names in the
result are borrowed pointers into the specification, which must outlive
the layout.

The kernels then process one field of `n` records in one strided loop,
into one typed column: `zb_unpack_i32()` for the small integer types and
`bool`, `zb_unpack_f64()` for `u32` and the floating types,
`zb_unpack_i64()` and `zb_unpack_f64x()` (exact or an error) for the
64-bit integers, and `zb_unpack_bytes()` for `b` and `s`. Array fields
are written column-major, element `k` of record `i` at `dst[k * n + i]`.
The `zb_pack_*()` kernels are the inverses, and refuse a value that does
not fit rather than wrapping it. The kernels do not bounds-check: prove
`offset + (n - 1) * stride + size <= length` first.

``` c
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
```

## Writing: the buffer, and buffers owned by R

A `zb_buf` is a pointer, a length, a capacity, a hard cap `max` (0 for
none), flags, and a `release` function with the `owner` it frees: the
shape of the Arrow C data interface. `zb_buf_alloc()` makes a growable
buffer, `zb_buf_borrow()` a window onto your own memory (writable up to
its length, never grown), and `zb_buf_release()` frees whatever needs
freeing. Growth doubles to 64 MiB and grows by half after, never past
`max`: an append that would is `ZB_ERR_LIMIT`, sets `ZB_BUF_HIT_LIMIT`,
and changes nothing. There is no bare size arithmetic: sums and products
that would overflow are `ZB_ERR_MEMORY`.

Ownership is a flag, never the identity of `release`: every translation
unit has its own copy of a header-only function, so comparing function
pointers across units is meaningless.

In an R package, heap memory that must survive a longjmp from
`Rf_error()` or `R_CheckUserInterrupt()` must be owned by R before the
first call that can jump. `zb_r_buf_new()` creates the external pointer
and registers its finalizer before the buffer exists; `zb_r_buf_free()`
releases it early, and the finalizer then does nothing:

``` c
/* Translation unit 3: a buffer owned by R, through <zubin-r.h>. */
#include <zubin-r.h>
#include "zubintest.h"

/* Appends the doubles in `values` as f32be, then borrows the raw vector
   `tail` and appends its bytes, and returns the buffer's bytes. The buffer is
   owned by an external pointer from before it exists, so the error below
   cannot leak it. */
SEXP zt_build(SEXP values, SEXP tail)
{
    zb_status st;
    zb_buf view, *b;
    R_xlen_t i;
    SEXP out, ptr = PROTECT(zb_r_buf_new(16, 1 << 20, &st));
    if (st) Rf_error("zt_build: %s", zb_status_string(st));
    b = zb_r_buf_get(ptr);
    for (i = 0; i < XLENGTH(values); i++) {
        if (zb_put_f32be(b, (float)REAL(values)[i])) Rf_error("zt_build: put failed");
    }
    zb_r_buf_borrow(&view, tail);
    if (zb_put_bytes(b, view.data, view.len)) Rf_error("zt_build: the cap was reached");
    out = PROTECT(zb_r_buf_to_raw(b));
    zb_r_buf_free(ptr);
    UNPROTECT(2);
    return out;
}
```

`zb_r_buf_get()` allocates nothing, so nothing needs protecting around
it, and returns `NULL` for an external pointer that is not one of these
buffers or has been freed (one restored from a saved session, say).
