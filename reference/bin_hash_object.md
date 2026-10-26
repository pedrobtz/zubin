# A content fingerprint of an R object

`bin_hash_object()` is XXH3 of the body of R's XDR serialisation of `x`,
format version 2 or 3: everything
[`serialize()`](https://rdrr.io/r/base/serialize.html) writes after its
header. It is streamed through the hasher, so the serialised bytes are
never allocated. Skipping the header, which records the R version that
wrote it (and, for `version = 3`, the native encoding), keeps the digest
from changing with those; it stays the same for as long as R writes the
same body for `x`, which R does not promise across its versions (see
[`serialize()`](https://rdrr.io/r/base/serialize.html)), least of all
for closures, environments and ALTREP objects. Objects with the same
serialisation have the same digest; the converse holds only up to the
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

  The serialisation format version, 2 or 3.

- seed:

  A whole number from 0 to 2^53, the XXH3 seed.

## Value

A string of 16 or 32 lower-case hexadecimal digits.

## Details

It is a fingerprint, not a cryptographic digest and not a durable
content address: fast, and well distributed, but not a defence against
someone choosing inputs to collide.

The digest is of the bytes R writes, not of the values they stand for,
so `identical(x, y)` does not imply equal digests:

- A double is its 64 bits. `0` and `-0` differ, as do NaNs of different
  sign or payload, and `NA` once arithmetic has set its quiet bit
  (`c(NA, 1) + 0`), though
  [`identical()`](https://rdrr.io/r/base/identical.html) treats each
  pair as the same. How NaN and `NA` propagate through arithmetic
  depends on the platform (see [NA](https://rdrr.io/r/base/NA.html)), so
  the same computation may hash differently on two machines; and on
  32-bit x86 even a double `NA` written as a literal has different bits.

- A string is its bytes and its encoding mark, so the same non-ASCII
  text hashes differently when it is marked UTF-8, latin1 or not at all;
  and unmarked text is in the session's native encoding, which depends
  on the locale. Apply
  [`enc2utf8()`](https://rdrr.io/r/base/Encoding.html) to the strings
  first for a digest that does not depend on either.

Apart from these, the digest of an object is the same on every platform
R runs on.

`version = 2`, the default, writes compact sequences such as `1:10` as
the vectors they are; `version = 3` writes their compact form, so the
same values may then hash differently.

## Examples

``` r
bin_hash_object(mtcars)
#> [1] "b5864d2491b162b9"
bin_hash_object(mtcars, "xxh3_128")
#> [1] "7999113ed3181597b5864d2491b162b9"
identical(bin_hash_object(1:3 + 0L), bin_hash_object(c(1L, 2L, 3L)))
#> [1] TRUE
```
