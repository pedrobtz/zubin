# Append bytes or strings to a builder

Appends `x` to the end of the builder `b`. A raw vector is appended as
it is. A character vector is appended with `type = "z"`, each string's
UTF-8 bytes followed by a NUL, or with `type = "s<n>"`, each string's
UTF-8 bytes padded with NULs to exactly `n` bytes, the fixed-string
field of a layout. Nothing is appended unless every element can be: an
`NA` is a `zubin_na_error`, and a string longer than `n` bytes is a
`zubin_range_error` carrying the 0-based `index` of the first one.

## Usage

``` r
bin_put(b, x, type = NULL, endian = c("little", "big", "native"))
```

## Arguments

- b:

  A builder from
  [`bin_builder()`](https://pedrobtz.github.io/zubin/reference/bin_builder.md).

- x:

  A raw vector, or a character vector with `type`.

- type:

  `NULL` for raw input; `"z"` or `"s<n>"` for character input.

- endian:

  Byte order, for typed appends.

## Value

`b`, invisibly, so appends can be chained.

## Details

Appending past the builder's `max` is a `zubin_limit_error` carrying
`size`, the size the builder would have reached, and `max`; the builder
is unchanged. Long appends check for an interrupt every 64 MiB, and an
interrupted append leaves the builder unchanged too.

## Examples

``` r
b <- bin_builder()
bin_put(b, c("RIFF", "WAVE"), type = "s4")
bin_put(b, "a C string", type = "z")
bin_take(b)
#>  [1] 52 49 46 46 57 41 56 45 61 20 43 20 73 74 72 69 6e 67 00
```
