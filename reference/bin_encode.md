# Write a vector of one type to bytes

`bin_encode()` writes every element of `x` as `type`, in `endian` order:
[`writeBin()`](https://rdrr.io/r/base/readBin.html) generalised to every
width and both byte orders, rounding to f16, bf16 and f32 to nearest
even, and refusing, never truncating, an integer that does not fit.

## Usage

``` r
bin_encode(
  x,
  type,
  endian = c("little", "big", "native"),
  na = c("error", "allow")
)
```

## Arguments

- x:

  A vector: numeric, logical, `integer64`, complex for `c64` and `c128`,
  character for `s<n>`, or a list of raw vectors for `b<n>`.

- type:

  One type token of
  [`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md):
  `"u8"` to `"f64"`, `"bool"`, `"b<n>"` or `"s<n>"`, optionally with a
  `.le` or `.be` suffix.

- endian:

  The byte order: `"little"`, `"big"`, or `"native"`.

- na:

  `"error"`, or `"allow"` to write `NA` as -2^31 into `i32`.

## Value

A raw vector of `length(x)` times the type's width.

## Details

`bin_encode(bin_decode(b, t), t)` is `b` when `b` is the canonical
encoding of its values, which every encoding of a number is, with three
exceptions. A `bool` byte other than 00 or 01 reads as `TRUE` and writes
back as 01. An `s<n>` reads up to its first NUL, so the bytes after it
write back as NULs. And an `i64`, `u64`, `i128` or `u128` beyond 2^53
does not decode as a double at all: ask for `int64 = "integer64"` (which
holds 64 bits). A NaN keeps its payload through `f64` and `c128`, but
the narrower floats may not.

## Examples

``` r
bin_encode(c(1, 2, 65535), "u16")
#> [1] 01 00 02 00 ff ff
bin_encode(1:3, "i32", endian = "big")
#>  [1] 00 00 00 01 00 00 00 02 00 00 00 03
bin_encode(c(0.1, 1e5), "f16")
#> [1] 66 2e 00 7c
try(bin_encode(300, "u8"))
#> Error in bin_encode() : Value 0 does not fit its type.

# bytes that are not the canonical encoding do not survive a round trip
bin_encode(bin_decode(as.raw(2), "bool"), "bool")
#> [1] 01
```
