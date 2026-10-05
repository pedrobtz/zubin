# A growable byte buffer

`bin_builder()` creates a buffer that bytes are appended to with
[`bin_put()`](https://pedrobtz.github.io/zubin/reference/bin_put.md) and
taken out of with `bin_take()`. It grows by doubling (by half once past
64 MiB), so a million appends cost a few dozen reallocations, not a
million copies; and it never grows past `max`.

## Usage

``` r
bin_builder(reserve = 0, max = Inf)

bin_reserve(b, n)

bin_reset(b)

bin_take(b)

# S3 method for class 'zubin_builder'
as.raw(x)
```

## Arguments

- reserve:

  Bytes to allocate at once: a whole number, at least 0.

- max:

  The hard cap on the builder's size, in bytes: a positive whole number,
  or `Inf` for none. An append that would pass it is a
  `zubin_limit_error` and leaves the builder unchanged.

- b, x:

  A builder.

- n:

  Bytes to make room for.

## Value

`bin_builder()` returns a `zubin_builder`. `bin_take()` and
[`as.raw()`](https://rdrr.io/r/base/raw.html) return a raw vector;
[`bin_size()`](https://pedrobtz.github.io/zubin/reference/bin_size.md)
returns a double; `bin_reserve()` and `bin_reset()` return `b`,
invisibly.

## Details

A builder is an external pointer: it is not copied on modification,
every function below changes it in place, and it does not survive
[`saveRDS()`](https://rdrr.io/r/base/readRDS.html) or a restored
session; using a restored builder is a `zubin_invalid_argument` error.
Its memory is freed when it is garbage collected.

`bin_take(b)` returns the bytes and empties the builder, which keeps its
capacity and accepts new appends; `as.raw(b)` returns a copy and leaves
the builder as it is. `bin_reserve(b, n)` makes room for `n` more bytes
now, `bin_reset(b)` empties the builder without freeing its storage, and
`bin_size(b)` is the number of bytes it holds.

## See also

[`bin_put()`](https://pedrobtz.github.io/zubin/reference/bin_put.md) to
append.

## Examples

``` r
b <- bin_builder()
bin_put(b, as.raw(c(0x89, 0x50, 0x4e, 0x47)))
bin_put(b, "IHDR", type = "s4")
bin_size(b)
#> [1] 8
bin_take(b)
#> [1] 89 50 4e 47 49 48 44 52
bin_size(b)
#> [1] 0
```
