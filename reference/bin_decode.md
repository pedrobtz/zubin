# Read a vector of one type from bytes

`bin_decode()` reads `n` consecutive values of one type from the raw
vector `x`, starting at the 0-based byte `offset`:
`bin_decode(x, "u32", offset = 4)` is a header field,
`bin_decode(x, "f32", offset = 64, n = 1e6)` is a column. It is
[`readBin()`](https://rdrr.io/r/base/readBin.html) generalised to every
width and both byte orders, exact or an error;
[`bin_encode()`](https://pedrobtz.github.io/zubin/reference/bin_encode.md)
is its inverse. Offsets are 0-based.

## Usage

``` r
bin_decode(
  x,
  type,
  offset = 0,
  n = NULL,
  endian = c("little", "big", "native"),
  int64 = c("double", "integer64"),
  na = c("error", "allow")
)
```

## Arguments

- x:

  A raw vector.

- type:

  One type token of
  [`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md):
  `"u8"` to `"f64"`, `"bool"`, `"b<n>"` or `"s<n>"`, optionally with a
  `.le` or `.be` suffix.

- offset:

  The 0-based byte position of the first record.

- n:

  The number of values, or `NULL` for every whole value that fits.

- endian:

  The byte order: `"little"`, `"big"`, or `"native"`.

- int64:

  How `i64` and `u64` fields are returned: `"double"`, exact or an
  error; or `"integer64"`, a double vector carrying the 64-bit pattern
  with class `integer64`, as the bit64 package defines it.

- na:

  `"error"`, or `"allow"` to read -2^31 in an `i32` field as
  `NA_integer_` (and -2^63 in `integer64` mode as `NA`).

## Value

A vector of the type's R type (see
[`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md)):
integer, double, logical, character, or a list of raw vectors for
`b<n>`.

## Examples

``` r
x <- as.raw(c(0x01, 0x00, 0x02, 0x00, 0xff, 0xff))
bin_decode(x, "u16")
#> [1]     1     2 65535
bin_decode(x, "i16")
#> [1]  1  2 -1
bin_decode(x, "u16", endian = "big", offset = 2, n = 1)
#> [1] 512
```
