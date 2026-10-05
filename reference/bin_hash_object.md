# A content fingerprint of an R object

`bin_hash_object()` hashes the serialization of `x` with XXH3, streaming
it through the hasher so that the serialized bytes are never allocated.
The serialization header is skipped, so the digest does not change with
the version of R that computes it (nor, for `version = 3`, with the
native encoding). Two objects with the same digest have the same
serialization.

## Usage

``` r
bin_hash_object(x, algo = c("xxh3_64", "xxh3_128"), version = 2L, seed = 0)
```

## Arguments

- x:

  Any R object.

- algo:

  `"xxh3_64"` for a 64-bit digest, `"xxh3_128"` for 128 bits.

- version:

  The serialization format version, 2 or 3.

- seed:

  A whole number from 0 to 2^53, the XXH3 seed.

## Value

A string of 16 or 32 lower-case hexadecimal digits.

## Details

It is a fingerprint, not a cryptographic digest: fast, and well
distributed, but not a defence against someone choosing inputs to
collide. `version = 2`, the default, writes compact sequences such as
`1:10` as the vectors they are; `version = 3` writes their compact form,
so the same values may then hash differently.

## Examples

``` r
bin_hash_object(mtcars)
#> [1] "b5864d2491b162b9"
bin_hash_object(mtcars, "xxh3_128")
#> [1] "7999113ed3181597b5864d2491b162b9"
identical(bin_hash_object(1:3 + 0L), bin_hash_object(c(1L, 2L, 3L)))
#> [1] TRUE
```
