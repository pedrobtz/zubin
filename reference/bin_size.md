# The size of a builder or a layout, in bytes

For a builder, the number of bytes it holds; for a layout, the size of
one record.

## Usage

``` r
bin_size(x, ...)

# S3 method for class 'zubin_layout'
bin_size(x, ...)
```

## Arguments

- x:

  A `zubin_builder` or a `zubin_layout`.

- ...:

  Unused.

## Value

A double for a builder; an integer for a layout.

## Examples

``` r
b <- bin_builder()
bin_put(b, as.raw(1:3))
bin_size(b)
#> [1] 3
```
