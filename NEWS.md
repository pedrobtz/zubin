# zubin 0.0.0.9000

The development version of the first release, 0.1.0 (this heading becomes `# zubin 0.1.0`
when it is submitted).

## R

* Fourteen functions, prefixed `bin_`: `bin_layout()` describes a fixed-size binary record
  in a one-line specification (integers of 8 to 64 bits, `f16`, `bf16`, `f32`, `f64`,
  `bool`, fixed bytes and strings, padding, arrays, either byte order, C struct alignment);
  `bin_unpack()` and `bin_pack()` read and write any number of records in one call;
  `bin_decode()` and `bin_encode()` convert whole vectors to and from bytes of one type;
  `bin_builder()`, `bin_put()`, `bin_reserve()`, `bin_reset()` and `bin_take()` append to a
  growable byte buffer with a hard cap; `bin_size()`; `bin_hexdump()` and `bin_diff()`; and
  `bin_info()`.
* Nothing is silently wrong: a value that does not fit its field, an `NA` with no bytes, a
  64-bit integer that a double cannot hold exactly, and a string that is not UTF-8 are each
  a classed error naming the field and the 0-based record (`?zubin-conditions`).

## C

* A header-only C99 library under `inst/include`, used through `LinkingTo: zubin, zufast`
  alone: `zubin/rw.h` (typed reads and writes at any alignment and byte order),
  `zubin/cursor.h` (checked sequential reads), `zubin/buf.h` (a buffer with ownership
  flags, a hard cap and checked growth), `zubin/layout.h` (the specification parser and
  the field-major unpack and pack kernels), and `zubin-r.h` (a buffer owned by an R
  external pointer). Source compatibility within major version 1; see the `c-api` article
  on the package website and "Using zubin from C" in the README.

## Not in this release

Variable-length fields, a cursor-style reader from R, byte search and splitting, zero-copy
views, memory-mapped files, connections, serialisation, bitfields and the nanoarrow bridge
are planned, each with the condition that admits it, in `.agents/next.md` in the source
repository.
