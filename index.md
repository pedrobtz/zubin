# zubin

zubin reads and writes structured binary data in R. Describe a fixed
binary record once, as a layout, and read or write millions of them in
one call; convert typed vectors to and from bytes at any width and byte
order, exactly or with an error, never silently rounded; append to a
growable byte buffer that is not quadratic. The same machinery is a
header-only C99 library that other packages use through `LinkingTo`.

## Installation

zubin is not on CRAN yet. It links to
[zufast](https://github.com/pedrobtz/zufast), which is not either, so
install both from GitHub:

``` r

# install.packages("pak")
pak::pak("pedrobtz/zubin")
```

## Example

``` r

library(zubin)

hdr <- bin_layout(">magic:u32 version:u16 count:u32")
rec <- bin_layout("<id:u32 ts:i64 price:f64 qty:i32 side:u8 x3")

trades <- data.frame(id = 1:3, ts = c(1e12, 1e12 + 5, 1e12 + 9),
                     price = c(101.5, 101.25, 101.75), qty = c(100L, 50L, 75L),
                     side = c(1L, 2L, 1L))

b <- bin_builder()
bin_put(b, bin_pack(hdr, magic = 0xCAFEBABE, version = 1, count = nrow(trades)))
bin_put(b, bin_pack(rec, trades))
x <- bin_take(b)

h <- bin_unpack(x, hdr, n = 1)
bin_unpack(x, rec, offset = bin_size(hdr), n = h$count)
bin_hexdump(x, n = 32)
```

Offsets are 0-based everywhere. Every field type, its R type, and the
specification grammar are in
[`?bin_layout`](https://pedrobtz.github.io/zubin/reference/bin_layout.md).

## Using zubin from C

This is the whole recipe. The fixture package `tools/zubintest` follows
it verbatim, and it is built, checked and tested on Linux, macOS and
Windows on every change, then tested again with zubin and zufast
uninstalled.

**1. `DESCRIPTION`**: list zubin *and* zufast in `LinkingTo`, and
nothing else: no `Imports:`, no `importFrom()`, no `configure`, no
`PKG_LIBS`.

    LinkingTo: zubin, zufast

Both, because `LinkingTo` is not transitive and zubin’s headers include
zufast’s. Neither is needed when your package *runs*: your shared object
carries its own copy of every zubin function it uses, and keeps working
if both are removed.

**2. `NAMESPACE`**: only your own `useDynLib`.

    useDynLib(zubintest, .registration = TRUE)

**3. `src/Makevars`**: hide your symbols, as the zu family does. Every
function zubin emits is already `static inline`, so two packages that
both use zubin never bind to each other’s copies; this is defence in
depth.

``` make
PKG_CFLAGS = $(C_VISIBILITY)
```

**4. Include and call.** `<zubin.h>` for everything, or one area at a
time (`<zubin/cursor.h>`, `<zubin/buf.h>`, `<zubin/layout.h>`, …). Any
number of translation units may include them; there is nothing to link
and no implementation macro. They compile as C99 and as C++11. A checked
sequential read of a header:

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

Every function that can fail returns a `zb_status` (`ZB_OK` is 0), and
on failure its inputs are unchanged: a cursor that returns `ZB_ERR_EOF`
has not moved.

**5. Buffers owned by R.** `<zubin-r.h>` is the one header that includes
R’s, and is not part of `<zubin.h>`. `zb_r_buf_new()` hands you a buffer
owned by an external pointer whose finalizer is registered before the
buffer exists, so an `Rf_error()` or an interrupt cannot leak it:

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

The C API, buffer ownership and the layout kernels are described in the
`c-api` article on the package website.
