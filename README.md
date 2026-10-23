
<!-- README.md is generated from README.Rmd. Please edit that file. -->

# zubin

<!-- badges: start -->

[![R-CMD-check](https://github.com/pedrobtz/zubin/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/pedrobtz/zubin/actions/workflows/R-CMD-check.yaml)
[![coverage](https://raw.githubusercontent.com/pedrobtz/zubin/main/.github/badges/coverage.svg)](https://github.com/pedrobtz/zubin/actions/workflows/coverage.yaml)
<!-- badges: end -->

Every binary format, from an image header to a market data feed to a C
struct dumped to disk, is specified the same way: a table of fields,
each with a name, a type, a width and an offset. Base R reads such data
one `readBin()` call at a time, loops over the records itself, has no
unsigned 32-bit integer and no half-precision float, and reads an 8-byte
integer silently wrong once it outgrows 32 bits.

zubin lets you write the table down once, as a *layout*, and then read
or write any number of records in one call.

- **Describe once, read millions.** A layout is one string,
  `"<id:u32 ts:i64 price:f64 side:u8"`, and `bin_unpack()` turns a raw
  vector into a data frame of every record in it, one tight loop per
  field.
- **Every type a format uses.** Integers from 8 to 128 bits, signed and
  unsigned; `f16`, `bf16`, `f32` and `f64`; complex numbers; booleans;
  fixed bytes and strings; arrays; padding; either byte order, per
  field; and C struct alignment.
- **Exact, or an error.** Nothing is silently rounded, wrapped or
  truncated. A 64-bit integer that a double cannot hold, or a value too
  large for its field, is an error that names the field and the record.
- **Writing too.** `bin_pack()` and `bin_encode()` are the inverses, and
  a byte builder appends without quadratic copying, up to a hard cap.
- **A C library underneath.** The same buffers, cursors and kernels are
  a header-only C99 library that other packages use through `LinkingTo`
  alone.

## Installation

``` r
install.packages("zubin")
```

or the development version from GitHub:

``` r
# install.packages("pak")
pak::pak("pedrobtz/zubin")
```

## Usage

``` r
library(zubin)
```

**Read a real header.** A PNG file opens with an 8-byte signature and
then an `IHDR` chunk, big-endian, as its specification lays it out. That
table, written down, reads the image’s size from the file R just wrote:

``` r
png_file <- tempfile(fileext = ".png")
png(png_file, width = 640, height = 480)
plot(pressure)
invisible(dev.off())

ihdr <- bin_layout(">x8 length:u32 type:s4 width:u32 height:u32 depth:u8 colour:u8")
bin_unpack(readBin(png_file, "raw", 64), ihdr, n = 1)
#>   length type width height depth colour
#> 1     13 IHDR   640    480     8      6
```

**The types base R lacks.** `readBin()` has no unsigned 32-bit integers
and no half floats; zubin decodes both, in either byte order:

``` r
x <- as.raw(c(0xff, 0xff, 0xff, 0xff))
readBin(x, "integer", size = 4, signed = FALSE)
#> Warning in readBin(x, "integer", size = 4, signed = FALSE): 'signed = FALSE' is
#> only valid for integers of sizes 1 and 2
#> [1] -1
bin_decode(x, "u32")
#> [1] 4294967295

# half-precision floats, as machine-learning weights are stored
bin_decode(as.raw(c(0x00, 0x3c, 0x00, 0xc0, 0x00, 0x7c)), "f16")
#> [1]   1  -2 Inf
```

**Exact, or an error.** `readBin()` reads the 8-byte integer 2^32 as 0,
without a word. zubin reads it exactly, and refuses a value above 2^53,
which has no exact double, naming the record rather than handing back a
neighbouring number (`int64 = "integer64"` returns the exact bits
instead):

``` r
counts <- bin_encode(c(2^32, 2^53 + 2), "u64")
readBin(counts, "integer", size = 8, n = 2)
#> [1] 0 2
bin_decode(counts[1:8], "u64")
#> [1] 4294967296
bin_decode(counts, "u64")
#> Error in `bin_decode()`:
#> ! The value of record 1 has no exact R value.
```

**A million records in one call.** Packing a data frame and unpacking it
again is one call each way, and the round trip is exact:

``` r
set.seed(1)
rec <- bin_layout("<id:i32 price:f64 qty:i16 x2")
trades <- data.frame(id = 1:1e6, price = round(runif(1e6, 90, 110), 2),
                     qty = sample(1:500, 1e6, replace = TRUE))
bytes <- bin_pack(rec, trades)
length(bytes)
#> [1] 16000000
identical(bin_unpack(bytes, rec), trades)
#> [1] TRUE
```

**Mixed byte orders: a shapefile.** The `.shp` part of a shapefile mixes
byte orders inside one record: its file code and length are big-endian,
everything after them little-endian. A `.be` suffix overrides the
layout’s order for one field, so each record is still a single layout.
Here the `quakes` data become a point shapefile, written with a builder,
as the ESRI specification lays it out:

``` r
shp_header <- bin_layout("<code:i32.be x20 words:i32.be version:i32 type:i32
                          xmin:f64 ymin:f64 xmax:f64 ymax:f64 x32")
point <- bin_layout("<id:i32.be words:i32.be type:i32 x:f64 y:f64")

n <- nrow(quakes)
b <- bin_builder()
bin_put(b, bin_pack(shp_header, code = 9994, words = (100 + 28 * n) / 2,
                    version = 1000, type = 1,
                    xmin = min(quakes$long), ymin = min(quakes$lat),
                    xmax = max(quakes$long), ymax = max(quakes$lat)))
bin_put(b, bin_pack(point, id = seq_len(n), words = 10, type = 1,
                    x = quakes$long, y = quakes$lat))
shp <- bin_take(b)
bin_hexdump(shp, n = 48)
#> 00000000: 0000 270a 0000 0000 0000 0000 0000 0000  ..'.............
#> 00000010: 0000 0000 0000 0000 0000 36e2 e803 0000  ..........6.....
#> 00000020: 0100 0000 3d0a d7a3 70b5 6440 ec51 b81e  ....=...p.d@.Q..
```

The file code 9994 opens the file big-endian (`0000 270a`), and the
version 1000 at offset 28 is little-endian (`e803 0000`). Reading it
back is one call for the header and one for every point after it:

``` r
bin_unpack(shp, shp_header, n = 1)
#>   code words version type   xmin   ymin   xmax   ymax
#> 1 9994 14050    1000    1 165.67 -38.59 188.13 -10.72
pts <- bin_unpack(shp, point, offset = 100)
head(pts, 3)
#>   id words type      x      y
#> 1  1    10    1 181.62 -20.42
#> 2  2    10    1 181.03 -20.62
#> 3  3    10    1 184.10 -26.00
identical(pts$x, quakes$long)
#> [1] TRUE
```

Every function is prefixed `bin_`, and offsets are 0-based everywhere,
as in every format specification.

## Learn more

- [Structured binary data with
  zubin](https://pedrobtz.github.io/zubin/articles/zubin.html), the
  getting-started guide: layouts, a WAV file written and read back, C
  structs, arrays, 64-bit integers and errors.
- [The C API](https://pedrobtz.github.io/zubin/articles/c-api.html), for
  package authors: the `LinkingTo` recipe, the cursor, the record
  kernels, and buffers owned by R.
- The [reference index](https://pedrobtz.github.io/zubin/reference/),
  with the full type table under `?bin_layout`.

## Related work

zubin’s integer loads and stores, half-float conversions and UTF-8
validation come from [zufast](https://github.com/pedrobtz/zufast).
Python’s [`struct`](https://docs.python.org/3/library/struct.html)
module and the structured dtypes of NumPy are the nearest equivalents in
other languages.
