# Read records from bytes

`bin_unpack()` reads `n` records described by `layout` from the raw
vector `x`, the first at the 0-based byte `offset` and each next one
`stride` bytes further on, into one column per field. Offsets are
0-based.

## Usage

``` r
bin_unpack(
  x,
  layout,
  offset = 0,
  n = NULL,
  stride = NULL,
  as = c("data.frame", "list"),
  int64 = c("double", "integer64"),
  na = c("error", "allow"),
  encoding = c("UTF-8", "latin1", "bytes")
)
```

## Arguments

- x:

  A raw vector.

- layout:

  A
  [`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md),
  or a specification string for one.

- offset:

  The 0-based byte position of the first record.

- n:

  The number of records, or `NULL` for every whole record that fits;
  trailing bytes that do not make a whole record are left alone. An
  explicit `n` that runs past the end of `x` is a `zubin_bounds_error`
  carrying `offset` and `length`.

- stride:

  Bytes from the start of one record to the next, at least the record
  size; `NULL` for the record size. A larger stride reads the head of a
  larger record.

- as:

  `"data.frame"` for a data frame, whose array fields expand to columns
  `name.1`, `name.2`, ... and whose `b<n>` fields are list columns
  wrapped in [`I()`](https://rdrr.io/r/base/AsIs.html); or `"list"` for
  the columns as they are, array fields as matrices.

- int64:

  How `i64` and `u64` fields are returned: `"double"`, exact or an
  error; or `"integer64"`, a double vector carrying the 64-bit pattern
  with class `integer64`, as the bit64 package defines it.

- na:

  `"error"`, or `"allow"` to read -2^31 in an `i32` field as
  `NA_integer_` (and -2^63 in `integer64` mode as `NA`).

- encoding:

  How `s<n>` bytes are marked: `"UTF-8"` (validated), `"latin1"` or
  `"bytes"` (any bytes).

## Value

A data frame with one row per record, or a named list of columns.

## Details

Fields are read field by field, each in one pass over the records, so a
million records cost one loop per field. Nothing is rounded or wrapped:
a value that has no exact R representation is a `zubin_range_error`
carrying `field` and the 0-based record `index` of the first one, namely
an `i32` of -2^31 (R's `NA_integer_`; allowed with `na = "allow"`), an
`i64` or `u64` above 2^53 in magnitude when `int64 = "double"`, and a
`u64` of 2^63 or more, or an `i64` of -2^63 (bit64's `NA`), when
`int64 = "integer64"`. An `s<n>` field holds the bytes up to its first
NUL; with `encoding = "UTF-8"` they must be valid UTF-8, or the call is
a `zubin_encoding_error` carrying `field` and `index`.

A single header is `bin_unpack(x, hdr, n = 1)`: a one-row result.

## See also

[`bin_decode()`](https://pedrobtz.github.io/zubin/reference/bin_decode.md)
for one field read as a vector; `bin_pack()` for the inverse.

## Examples

``` r
hdr <- bin_layout(">magic:u32 minor:u16 major:u16")
x <- as.raw(c(0xca, 0xfe, 0xba, 0xbe, 0x00, 0x00, 0x00, 0x41))
bin_unpack(x, hdr, n = 1)
#>        magic minor major
#> 1 3405691582     0    65

# records with an array field
pts <- bin_layout("<id:u16 xy:i8[2]")
y <- as.raw(c(1, 0, 5, 250, 2, 0, 7, 9))
bin_unpack(y, pts)
#>   id xy.1 xy.2
#> 1  1    5   -6
#> 2  2    7    9
bin_unpack(y, pts, as = "list")$xy
#>      [,1] [,2]
#> [1,]    5   -6
#> [2,]    7    9
```
