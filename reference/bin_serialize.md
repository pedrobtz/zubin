# Serialise R objects into a builder, and back from bytes at an offset

`bin_serialize()` appends the serialisation of `x` to the builder `b`,
exactly as [`serialize()`](https://rdrr.io/r/base/serialize.html) would
write it, without first allocating the whole stream as a raw vector.
`bin_unserialize()` reads one serialised object from the raw vector `x`,
starting at the 0-based byte `offset`, so a stream stored inside a
larger file or record is read where it lies. Offsets are 0-based.

## Usage

``` r
bin_serialize(x, b, version = 3L, xdr = TRUE, refhook = NULL)

bin_unserialize(x, offset = 0, refhook = NULL)
```

## Arguments

- x:

  For `bin_serialize()`, any R object; for `bin_unserialize()`, a raw
  vector holding a serialisation stream at `offset`.

- b:

  A builder from
  [`bin_builder()`](https://pedrobtz.github.io/zubin/reference/bin_builder.md).

- version:

  The serialisation format version, 2 or 3, as in
  [`serialize()`](https://rdrr.io/r/base/serialize.html).

- xdr:

  `TRUE` for the portable big-endian (XDR) format, `FALSE` for the
  host's native binary format, as in
  [`serialize()`](https://rdrr.io/r/base/serialize.html).

- refhook:

  `NULL`, or a function handling reference objects, as in
  [`serialize()`](https://rdrr.io/r/base/serialize.html) and
  [`unserialize()`](https://rdrr.io/r/base/serialize.html).

- offset:

  The 0-based position of the stream in `x`.

## Value

`bin_serialize()` returns `b`, invisibly. `bin_unserialize()` returns
the object.

## Details

Both are R's own serialisation: `unserialize(bin_take(b))` after
`bin_serialize(x, b)` is identical to `x`, and `bin_unserialize()` reads
what [`serialize()`](https://rdrr.io/r/base/serialize.html) writes.
Errors that R raises while writing or reading the stream (an object it
cannot serialise, a malformed stream, an error in `refhook`) arrive as R
raised them. A stream that ends before the object does is a
`zubin_bounds_error` carrying `offset` and `length`. An append that
would pass the builder's `max` is a `zubin_limit_error`; in every
failure, an interrupt included, the builder is left as it was. `refhook`
must not use `b`: a refhook that changes it is a
`zubin_invalid_argument` error, and one that removed bytes from `b`
leaves it holding what the refhook left.

Only read serialised data from a source you trust: a serialisation
stream can hold any R object, functions and environments included.

## See also

[`bin_hash_object()`](https://pedrobtz.github.io/zubin/reference/bin_hash_object.md)
for a digest of the same stream.

## Examples

``` r
b <- bin_builder()
bin_put(b, as.raw(c(0xde, 0xad)))          # a 2-byte header of our own
bin_serialize(list(a = 1:3, b = "zubin"), b)
bytes <- bin_take(b)
bin_unserialize(bytes, offset = 2)
#> $a
#> [1] 1 2 3
#> 
#> $b
#> [1] "zubin"
#> 
```
