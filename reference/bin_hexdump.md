# Show bytes as a hex dump, and find where two raw vectors differ

`bin_hexdump()` formats `n` bytes of `x` from the 0-based byte `offset`
the way `xxd` does: each line is the offset of its first byte in hex,
the bytes in hex in groups of two, and the bytes as ASCII, with `.` for
anything not printable. Offsets are 0-based and absolute, so a dump from
`offset = 64` starts at `00000040`.

## Usage

``` r
bin_hexdump(x, offset = 0, n = NULL, width = 16L)

bin_diff(a, b, n = 10L)
```

## Arguments

- x, a, b:

  Raw vectors.

- offset:

  The 0-based position of the first byte to show; at most `length(x)`,
  or the call is a `zubin_bounds_error`.

- n:

  For `bin_hexdump()`, the number of bytes to show, or `NULL` for the
  rest; for `bin_diff()`, the most differences to list.

- width:

  Bytes per line, 1 to 256.

## Value

`bin_hexdump()` returns a character vector of class `zubin_hexdump`, one
line per element, which prints as the dump. `bin_diff()` returns a data
frame with columns `offset` (double), `a` and `b` (raw), and attributes
`length_a` and `length_b`.

## Details

`bin_diff()` lists the first `n` positions, 0-based, at which `a` and
`b` hold different bytes, comparing over the shorter length; the lengths
themselves are attributes, so a prefix shows as no rows and two
different lengths.

Both are presentation: they take any raw vector, and `n` past the end of
`x` shows what there is.

## Examples

``` r
x <- charToRaw("Structured binary data, one layout at a time.")
bin_hexdump(x)
#> 00000000: 5374 7275 6374 7572 6564 2062 696e 6172  Structured binar
#> 00000010: 7920 6461 7461 2c20 6f6e 6520 6c61 796f  y data, one layo
#> 00000020: 7574 2061 7420 6120 7469 6d65 2e         ut at a time.
bin_hexdump(x, offset = 16, n = 16, width = 8)
#> 00000010: 7920 6461 7461 2c20  y data, 
#> 00000018: 6f6e 6520 6c61 796f  one layo

y <- x
y[c(3, 20)] <- as.raw(0)
bin_diff(x, y)
#>   offset  a  b
#> 1      2 72 00
#> 2     19 61 00
```
