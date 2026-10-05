# Structured binary data with zubin

``` r

library(zubin)
```

Binary formats are described in their specifications as tables of
fields: a name, a type, a width, an offset. zubin lets you write that
table down once, as a *layout*, and then read or write any number of
records in one call. Offsets are 0-based throughout, as in every
specification.

## Layouts

A layout is a string of fields, `name:type`, with an optional byte order
prefix: `<` little, `>` big, `=` the host’s.

``` r

wav <- bin_layout("<riff:s4 size:u32 wave:s4 fmt:s4 fmt_size:u32 format:u16 channels:u16
                   rate:u32 byte_rate:u32 block_align:u16 bits:u16 data:s4 data_size:u32")
wav
#> <zubin_layout: 44 bytes, 13 fields>
#>  name        type count size offset endian
#>  riff        s4   1     4     0           
#>  size        u32  1     4     4     little
#>  wave        s4   1     4     8           
#>  fmt         s4   1     4    12           
#>  fmt_size    u32  1     4    16     little
#>  format      u16  1     2    20     little
#>  channels    u16  1     2    22     little
#>  rate        u32  1     4    24     little
#>  byte_rate   u32  1     4    28     little
#>  block_align u16  1     2    32     little
#>  bits        u16  1     2    34     little
#>  data        s4   1     4    36           
#>  data_size   u32  1     4    40     little
bin_size(wav)
#> [1] 44
```

The types are the integers `u8` to `i64`, the floats `f16`, `bf16`,
`f32` and `f64`, `bool`, fixed bytes `b<n>` and fixed strings `s<n>`,
padding `x<n>`, and arrays of the numeric types, `f32[3]`. A field can
override the prefix with `.le` or `.be`.

| Spec | Bytes | R type on unpack | On pack, accepts |
|----|----|----|----|
| `u8 i8 u16 i16` | 1–2 | integer | integer, whole double, logical |
| `i32` | 4 | integer; −2^31 is an error unless `na = "allow"` | integer, whole double |
| `u32` | 4 | double | integer, whole double |
| `i64 u64` | 8 | double, exact to 2^53, or `integer64` | integer, whole double, `integer64` |
| `f16 bf16 f32 f64` | 2–8 | double | numeric, rounded to nearest even |
| `bool` | 1 | logical | logical |
| `b<n>` | n | list of raw | list of raw, each n bytes |
| `s<n>` | n | character, up to the first NUL | character of at most n UTF-8 bytes |
| `x<n>` | n | not returned | not given; written as zeros |
| `t[k]` | k × width | matrix of k columns | matrix of k columns |

Nothing is ever rounded into an integer, wrapped or truncated: a value
that does not fit its field is an error naming the field and the record.

## Writing and reading a WAV file

[`bin_encode()`](https://pedrobtz.github.io/zubin/reference/bin_encode.md)
turns a vector into bytes of one type, here one second of a 440 Hz tone
as 16-bit samples, and
[`bin_pack()`](https://pedrobtz.github.io/zubin/reference/bin_pack.md)
writes one header record from named values.

``` r

samples <- bin_encode(round(32767 * sin(2 * pi * 440 * seq(0, 1, by = 1 / 8000))), "i16")
hdr <- bin_pack(wav, riff = "RIFF", size = 36 + length(samples), wave = "WAVE",
                fmt = "fmt ", fmt_size = 16, format = 1, channels = 1, rate = 8000,
                byte_rate = 16000, block_align = 2, bits = 16, data = "data",
                data_size = length(samples))
path <- tempfile(fileext = ".wav")
writeBin(c(hdr, samples), path)
```

Reading it back is the same two steps in reverse: the header as a
one-row data frame, then the samples from the byte after it.

``` r

x <- readBin(path, "raw", file.size(path))
h <- bin_unpack(x, wav, n = 1)
h[, c("channels", "rate", "bits", "data_size")]
#>   channels rate bits data_size
#> 1        1 8000   16     16002
pcm <- bin_decode(x, "i16", offset = bin_size(wav), n = h$data_size / 2)
head(pcm)
#> [1]     0 11099 20886 28204 32187 32364
bin_hexdump(x, n = 48)
#> 00000000: 5249 4646 a63e 0000 5741 5645 666d 7420  RIFF.>..WAVEfmt 
#> 00000010: 1000 0000 0100 0100 401f 0000 803e 0000  ........@....>..
#> 00000020: 0200 1000 6461 7461 823e 0000 0000 5b2b  ....data.>....[+
```

## Records, a builder, and big-endian data

A file of records behind a header, built with a byte builder, which
grows without copying everything on each append:

``` r

hdr <- bin_layout(">magic:u32 version:u16 count:u32")
rec <- bin_layout(">id:u32 ts:i64 price:f64 qty:i32 side:u8 x3")
trades <- data.frame(id = 1:4, ts = 1.7e12 + c(0, 15, 40, 41),
                     price = c(101.5, 101.25, 101.75, 101.5),
                     qty = c(100L, 50L, 75L, 10L), side = c(1L, 2L, 1L, 2L))

b <- bin_builder()
bin_put(b, bin_pack(hdr, magic = 0xCAFEBABE, version = 1, count = nrow(trades)))
bin_put(b, bin_pack(rec, trades))
b
#> <zubin_builder: 122 bytes, capacity 256, max none>
path <- tempfile()
writeBin(bin_take(b), path)
```

``` r

x <- readBin(path, "raw", file.size(path))
h <- bin_unpack(x, hdr, n = 1)
h$magic == 0xCAFEBABE
#> [1] TRUE
bin_unpack(x, rec, offset = bin_size(hdr), n = h$count)
#>   id      ts  price qty side
#> 1  1 1.7e+12 101.50 100    1
#> 2  2 1.7e+12 101.25  50    2
#> 3  3 1.7e+12 101.75  75    1
#> 4  4 1.7e+12 101.50  10    2
```

[`bin_put()`](https://pedrobtz.github.io/zubin/reference/bin_put.md)
also encodes vectors directly, so a writer that does not have whole
records at hand appends field by field:
`bin_put(b, x, type = "f32", endian = "big")`. Strings append as C
strings (`type = "z"`) or fixed fields (`type = "s16"`).

## C structs

With `align = TRUE` each field is placed at a multiple of its own width
and the record is padded to the widest, as a C compiler lays out a
struct on 64-bit targets. A
`struct { uint8_t a; uint32_t b; uint16_t c; }` written with `fwrite()`:

``` r

s <- bin_layout("=a:u8 b:u32 c:u16", align = TRUE)
s$fields
#>   name type count size offset endian
#> 1    a   u8     1    1      0   <NA>
#> 2    b  u32     1    4      4 little
#> 3    c  u16     1    2      8 little
bin_size(s)
#> [1] 12
```

## Arrays and 64-bit integers

Array fields are matrices in list mode and expand to `name.1`, `name.2`,
… in a data frame:

``` r

pts <- bin_layout("<id:u16 xyz:f32[3]")
x <- bin_pack(pts, id = 1:2, xyz = matrix(c(0, 1, 2, 3, 4, 5), 2))
bin_unpack(x, pts)
#>   id xyz.1 xyz.2 xyz.3
#> 1  1     0     2     4
#> 2  2     1     3     5
bin_unpack(x, pts, as = "list")$xyz
#>      [,1] [,2] [,3]
#> [1,]    0    2    4
#> [2,]    1    3    5
```

A 64-bit integer reads as a double when it is exactly representable, and
is an error when it is not; `int64 = "integer64"` returns the bits as
the bit64 package’s class instead:

``` r

big <- bin_encode(2^53, "u64")
bin_decode(big, "u64")
#> [1] 9.007199e+15
try(bin_decode(bin_encode(2^53 + 2, "u64"), "u64"))
#> Error in bin_decode() : The value of record 0 has no exact R value.
```

## Finding where two files differ

``` r

a <- bin_pack(rec, trades)
b <- a
b[c(9, 50)] <- as.raw(0)
bin_diff(a, b)
#>   offset  a  b
#> 1      8 cf 00
```

## Errors

Every error zubin raises inherits `zubin_error` and has a class saying
what went wrong, with the details as fields, so code can catch it by
kind:

``` r

e <- tryCatch(bin_encode(c(1, 300), "u8"), zubin_range_error = function(e) e)
c(e$index, class(e)[1])
#> [1] "1"                 "zubin_range_error"
```
