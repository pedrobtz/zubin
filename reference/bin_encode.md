# Write a vector of one type to bytes

`bin_encode()` writes every element of `x` as `type`, in `endian` order:
[`writeBin()`](https://rdrr.io/r/base/readBin.html) generalised to every
width and both byte orders, rounding to f16, bf16 and f32 to nearest
even, and refusing, never truncating, an integer that does not fit.
`bin_encode(bin_decode(b, t), t)` is `b`.

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

  A vector: numeric, logical, `integer64`, character for `s<n>`, or a
  list of raw vectors for `b<n>`.

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
```
