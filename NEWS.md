# zubin 0.1.0

The first release.

## R

* Seventeen functions, prefixed `bin_`: `bin_layout()` describes a fixed-size binary record
  in a one-line specification (integers of 8 to 128 bits, `f16`, `bf16`, `f32`, `f64`,
  complex `c64` and `c128`, `bool`, fixed bytes and strings, padding, arrays, either byte
  order, C struct alignment);
  `bin_unpack()` and `bin_pack()` read and write any number of records in one call;
  `bin_decode()` and `bin_encode()` convert whole vectors to and from bytes of one type;
  `bin_builder()`, `bin_put()`, `bin_reserve()`, `bin_reset()` and `bin_take()` append to a
  growable byte buffer with a hard cap; `bin_size()`; `bin_hexdump()` and `bin_diff()`; and
  `bin_info()`.
* `bin_serialize()` appends R's serialization of an object to a builder, without first
  allocating it as a raw vector; `bin_unserialize()` reads one from a raw vector at a 0-based
  offset; `bin_hash_object()` gives an XXH3 fingerprint of an object's serialization, without
  the header that records the R version, streamed so that the serialization is never
  allocated (#25).
* Nothing is silently wrong: a value that does not fit its field, an `NA` with no bytes, a
  64-bit integer that a double cannot hold exactly, and a string that is not UTF-8 are each
  a classed error naming the field and the 0-based record (`?zubin-conditions`). That
  holds on write too: a string must be valid UTF-8, unless it is marked `"bytes"`, which
  is written as its bytes, so that what `bin_unpack(encoding = "bytes")` reads packs back
  (#44, #50). A layout may not name a field `a.1` beside an array field `a`, whose
  data-frame columns those are (#73), and a `refhook` that changes the builder
  `bin_serialize()` writes into is an error rather than a corrupt stream (#72).
* `bin_encode()` and a typed `bin_put()` write every element of a matrix or array, as
  `writeBin()` does (#79); a typed `bin_put()` of complex numbers or a byte list checks
  for an interrupt every 64 MiB, as one of numbers does (#77).

## C

* A header-only C99 library under `inst/include`, used through `LinkingTo: zubin, zufast`
  alone: `zubin/rw.h` (typed reads and writes at any alignment and byte order),
  `zubin/cursor.h` (checked sequential reads), `zubin/buf.h` (a buffer with ownership
  flags, a hard cap and checked growth, adopting and detaching `malloc` blocks, and the
  public checked size arithmetic `zb_size_add()` and `zb_size_mul()`), `zubin/layout.h` (the specification parser and
  the field-major unpack and pack kernels), and `zubin-r.h` (a buffer owned by an R
  external pointer, and foreign memory handed to R with the function that releases it).
  `zubin/column.h` holds what columnar formats need: bitmaps, bit-packed booleans,
  validity bitmaps to and from R's missing values, and byte swapping; the layout kernels
  also read and write values in their own width and the host's order, and `rw.h` adds
  128-bit integers and reads at a width chosen at run time. Every header but `zubin-r.h` may be used from any thread, as long as
  no two threads share a buffer or cursor (#26). Source compatibility within major version 1; see the `c-api` article
  on the package website.
* `zubin-r.h` also points R's serialization at these containers: `zb_serialize()` into a
  buffer, `zb_unserialize()` from a cursor, and `zb_serialize_to_sink()` through any sink,
  as a block pipeline consumes it (#25). R's own errors reach the caller unchanged; a
  stream that runs out and a builder that cannot grow are statuses.

## Not in this release

Variable-length fields, a cursor-style reader from R, byte search and splitting, zero-copy
views, memory-mapped files, connections, bitfields and the integers of zuint as an `int64` mode
are planned, each with the condition that admits it, in `.agents/next.md` in the source
repository.
