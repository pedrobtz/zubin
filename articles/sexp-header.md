# Reading R's own object header

Every R object lives in memory behind a fixed-size C header, the
`SEXPREC`: a word of bitfields, three pointers and, for a vector, its
length. A fixed-size record is exactly what
[`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md)
describes, so the header makes a small worked example of a layout over
bytes that zubin did not write: native byte order, a bitfield word,
pointers, and a body at a known offset.

This is a way to look, not a way to program. The structure is private to
R (it is in R’s `Defn.h`, not in its API), it changes between R
versions, and nothing here belongs in a package. To inspect an object in
earnest use `lobstr::sxp()`, or `.Internal(inspect(x))` at the console;
the last section says how each reads the header. The page assumes a
64-bit build of R 4.x, where the header is 48 bytes.

## Getting the bytes

zubin reads raw vectors, and no R function hands over an object’s own
memory, so the page compiles a three-line C function that copies the
first `n` bytes of the header into one:

``` r

stopifnot(.Machine$sizeof.pointer == 8)
library(zubin)

src <- tempfile(fileext = ".c")
writeLines(c(
  "#include <string.h>",
  "#include <Rinternals.h>",
  "SEXP peek(SEXP x, SEXP n) {",
  "  SEXP out = PROTECT(Rf_allocVector(RAWSXP, Rf_asInteger(n)));",
  "  memcpy(RAW(out), (const void *) x, (size_t) XLENGTH(out));",
  "  UNPROTECT(1);",
  "  return out;",
  "}"
), src)
system2(file.path(R.home("bin"), "R"), c("CMD", "SHLIB", shQuote(src)), stdout = FALSE)
dll <- dyn.load(sub("\\.c$", .Platform$dynlib.ext, src))
peek <- function(x, n = 48L) .Call(dll$peek, x, n)
```

`peek()` copies as many bytes as it is asked for, and nothing checks
them against the object: the caller asks for no more than the object
holds. An integer vector of three elements holds 48 bytes of header and
12 of data.

``` r

x <- c(10L, 20L, 30L)
b <- peek(x, 60L)
bin_hexdump(b)
#> 00000000: 0d00 0040 0300 0000 208f cb23 b355 0000  ...@.... ..#.U..
#> 00000010: c841 1b28 b355 0000 4842 1b28 b355 0000  .A.(.U..HB.(.U..
#> 00000020: 0300 0000 0000 0000 0000 0000 0000 0000  ................
#> 00000030: 0a00 0000 1400 0000 1e00 0000            ............
```

## The layout

On a 64-bit build the header is:

| Offset | Field | C type |
|----|----|----|
| 0 | `sxpinfo` | 64 bits of bitfields |
| 8 | `attrib` | `SEXP`, the attribute pairlist |
| 16 | `gengc_next_node` | `SEXP`, the garbage collector’s list |
| 24 | `gengc_prev_node` | `SEXP` |
| 32 | `length` | `R_xlen_t`, vectors only |
| 40 | `truelength` | `R_xlen_t`, vectors only |
| 48 | the data | what `INTEGER(x)`, `REAL(x)` and the rest point to |

This is memory of the running process, so the byte order is the host’s:
`=`, never a guess at little-endian. `sxpinfo` is two `u32` because a
`u64` that holds bitfields can pass 2^53, past what a double holds
exactly; the pointers are user-space addresses, far below 2^53, and read
as `b8` to be printed in hexadecimal.
[`next`](https://rdrr.io/r/base/Control.html) is a reserved word in R,
so the pointer fields take other names.

``` r

header <- bin_layout(
  "= info:u32[2] attrib:b8 gc_next:b8 gc_prev:b8 length:i64 truelength:i64"
)
header
#> <zubin_layout: 48 bytes, 6 fields>
#>  name       type count size offset endian
#>  info       u32  2     8     0     little
#>  attrib     b8   1     8     8           
#>  gc_next    b8   1     8    16           
#>  gc_prev    b8   1     8    24           
#>  length     i64  1     8    32     little
#>  truelength i64  1     8    40     little
```

``` r

h <- bin_unpack(b, header)
h[c("length", "truelength")]
#>   length truelength
#> 1      3          0
bin_decode(b, "i32", offset = 48, n = h$length, endian = "native")
#> [1] 10 20 30
```

## The bitfields

zubin has no bitfield type: a field is whole bytes. The bits come out
with arithmetic on the two words. GCC and Clang allocate bitfields from
the least significant bit on the targets R runs on, which gives, in R
4.x:

| Word | Bits | Field |
|----|----|----|
| 1 | 0–4 | `type`, the `SEXPTYPE` (13 is `INTSXP`) |
| 1 | 5, 6, 7 | `scalar`, `obj` (has a class), `alt` (ALTREP) |
| 1 | 8–23 | `gp`, general-purpose bits |
| 1 | 24–27 | `mark`, `debug`, `trace`, `spare` |
| 1 | 28, 29–31 | `gcgen`, `gccls` (the collector’s generation and size class) |
| 2 | 0–15 | `named`, the reference count |
| 2 | 16–31 | `extra` |

``` r

bits <- function(v, from, width) (v %/% 2^from) %% 2^width

sxpinfo <- function(h) {
  w1 <- h$info.1
  w2 <- h$info.2
  data.frame(
    type = bits(w1, 0, 5), scalar = bits(w1, 5, 1), obj = bits(w1, 6, 1),
    alt = bits(w1, 7, 1), gp = bits(w1, 8, 16), gcgen = bits(w1, 28, 1),
    gccls = bits(w1, 29, 3), named = bits(w2, 0, 16)
  )
}

address <- function(p) paste0("0x", sub("^0+", "", paste(rev(as.character(p)), collapse = "")))

sxpinfo(h)
#>   type scalar obj alt gp gcgen gccls named
#> 1   13      0   0   0  0     0     2     3
```

## What the header shows

A second binding to the same vector raises its reference count, and
copies nothing:

``` r

refs <- function(x) sxpinfo(bin_unpack(peek(x), header))$named
before <- refs(x)
y <- x
c(before = before, after = refs(x))
#> before  after 
#>      4      6
```

Without attributes, `attrib` points to `NULL`, which is one object of
its own, so every bare object points to the same address. An attribute
makes it a pairlist of its own:

``` r

attrib <- function(x) address(bin_unpack(peek(x), header)$attrib[[1]])
z <- structure(c(1, 2), units = "cm")
c(double = attrib(c(1, 2)), string = attrib("a"), with_attr = attrib(z))
#>           double           string        with_attr 
#> "0x55b323cb8f20" "0x55b323cb8f20" "0x55b329b81b60"
```

`1:3` is an ALTREP object, a compact sequence that stores its start and
end, not its elements: `alt` is set, and the 16 bytes at offset 32 hold
ALTREP’s own pointers, not a length.

``` r

sxpinfo(bin_unpack(peek(1:3), header))[c("type", "alt")]
#>   type alt
#> 1   13   1
```

A class sets `obj`:

``` r

sxpinfo(bin_unpack(peek(factor("a")), header))[c("type", "obj")]
#>   type obj
#> 1   13   1
```

The same fields, as R prints them; the `ATTRIB` address is the one
`attrib()` read above:

``` r

.Internal(inspect(z))
#> @55b329b43308 14 REALSXP g0c2 [REF(2),ATT] (len=2, tl=0) 1,2
#> ATTRIB:
#>   @55b329b81b60 02 LISTSXP g0c0 [REF(1)] 
#>     TAG: @55b323dc6f48 01 SYMSXP g1c0 [MARK,REF(155),LCK,gp=0x4000] "units" (has value)
#>     @55b329b18c88 16 STRSXP g0c1 [REF(6)] (len=1, tl=0)
#>       @55b32447b848 09 CHARSXP g1c1 [MARK,REF(97),gp=0x61] [ASCII] [cached] "cm"
```

## How the real tools read the header

`peek()` is one of three ways to see these fields, and the only one that
can break without warning.

- **`.Internal(inspect(x))` is part of R.** It is compiled from R’s own
  sources against the private `Defn.h`, in the same build as the
  structure it prints, so it can read every bit and never falls out of
  step with the layout. Its output format is not promised, and
  [`.Internal()`](https://rdrr.io/r/base/Internal.html) is for R’s own
  code: use it at the console, never in a package.
- **`lobstr::sxp()` reads nothing directly.** It asks R through the API:
  `TYPEOF()`, `ALTREP()`, `Rf_isObject()`, `XLENGTH()`, `ANY_ATTRIB()`
  and `R_mapAttrib()`, and for the reference count `NO_REFERENCES()` and
  `MAYBE_SHARED()`. The address it prints is the pointer’s value, never
  dereferenced. It shows less as a result: references as 0, 1 or 2+, and
  no `gp` bits, collector state or `truelength`. Where R 4.5 replaced an
  accessor, lobstr follows behind `#if R_VERSION` guards.
- **`peek()` and a layout** copy the memory and decode it against a
  description of a private structure. That is how it shows the exact
  reference count and the collector’s size class, which the API does not
  expose; it is also why a change to the structure in a later R makes it
  print wrong numbers rather than fail.

Code that runs anywhere but your own console belongs with the second.
