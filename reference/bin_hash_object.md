# A content fingerprint of an R object

`bin_hash_object()` is XXH3 of the body of R's XDR serialization of `x`,
format version 2 or 3: everything
[`serialize()`](https://rdrr.io/r/base/serialize.html) writes after its
header. It is streamed through the hasher, so the serialized bytes are
never allocated. Skipping the header, which records the R version that
wrote it (and, for `version = 3`, the native encoding), keeps the digest
from changing with those; it stays the same for as long as R writes the
same body for `x`, which R does not promise across its versions (see
[`serialize()`](https://rdrr.io/r/base/serialize.html)), least of all
for closures, environments and ALTREP objects. Objects with the same
serialization have the same digest; the converse holds only up to the
chance of a collision.

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

It is a fingerprint, not a cryptographic digest and not a durable
content address: fast, and well distributed, but not a defence against
someone choosing inputs to collide. The digest is the same on every
platform R runs on, with one exception: on 32-bit x86 a double `NA` is
stored with different bits, so an object holding one hashes differently
there. `version = 2`, the default, writes compact sequences such as
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
