# Write records to bytes

`bin_pack()` writes one record per row of its columns, laid out as
`layout` describes, into a raw vector of `n * bin_size(layout)` bytes.
Padding and alignment gaps are zeros, so the output is deterministic and
never holds stray memory.

## Usage

``` r
bin_pack(layout, ..., na = c("error", "allow"))
```

## Arguments

- layout:

  A
  [`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md),
  or a specification string for one.

- ...:

  Named columns, or one data frame or list of them.

- na:

  `"error"`, or `"allow"` to write `NA` into an `i32` field as -2^31
  (and an `integer64` `NA` into `i64` as -2^63).

## Value

A raw vector.

## Details

The columns are named vectors in `...`, one per field that carries a
value (padding takes none), or a single data frame or list with those
names. A data frame from
[`bin_unpack()`](https://pedrobtz.github.io/zubin/reference/bin_unpack.md)
works as it is: array fields may be given as a matrix with one row per
record, or as the columns `name.1`, ..., `name.k`, and a single record's
array as a vector of length `k`. Columns are recycled to the longest; a
length that does not divide it is a `zubin_invalid_argument` error,
never a warning.

Each field accepts the R types of the table in
[`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md):
integer, whole double and logical for the small integer types; integer
and whole double for `i32` and `u32`; integer, whole double and
`integer64` for `i64` and `u64`; numeric for the floating types (rounded
to nearest even); logical for `bool`; character for `s<n>` (UTF-8, at
most `n` bytes); and a list of raw vectors of exactly `n` bytes for
`b<n>`. Nothing is truncated, wrapped or rounded into an integer type: a
value that does not fit is a `zubin_range_error`, and an `NA` a
`zubin_na_error`, each carrying `field` and the 0-based record `index`
of the first one. `NA` has bytes only in `i32` (with `na = "allow"`),
`f64`, and the other floating types, where it becomes a NaN.

## See also

[`bin_unpack()`](https://pedrobtz.github.io/zubin/reference/bin_unpack.md),
the inverse;
[`bin_put()`](https://pedrobtz.github.io/zubin/reference/bin_put.md) to
append to a builder.

## Examples

``` r
hdr <- bin_layout(">magic:u32 version:u16 count:u32")
bin_pack(hdr, magic = 0xCAFEBABE, version = 1, count = 3)
#>  [1] ca fe ba be 00 01 00 00 00 03

rec <- bin_layout("<id:u16 x:f32 flag:bool x1")
df <- data.frame(id = 1:3, x = c(0.5, 1.5, 2.5), flag = c(TRUE, FALSE, TRUE))
bytes <- bin_pack(rec, df)
bin_unpack(bytes, rec)
#>   id   x  flag
#> 1  1 0.5  TRUE
#> 2  2 1.5 FALSE
#> 3  3 2.5  TRUE
```
