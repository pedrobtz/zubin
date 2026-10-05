# zubin: Read and Write Structured Binary Data

Describe a fixed binary record once, as a layout, and read or write
millions of them in one vectorised call: integers from 8 to 64 bits,
IEEE half, 'bfloat16' (brain floating point), single and double
precision floats, booleans, fixed bytes and fixed strings, padding,
arrays and C struct alignment, in either byte order. Typed codecs
convert whole vectors to and from bytes at every width and byte order,
refusing rather than rounding a value that does not fit, and a growable
byte builder with a hard cap appends without quadratic copying. The same
buffer, cursor, typed reads and writes and layout kernels are a
header-only C library that other packages use through 'LinkingTo' alone.

## See also

[`bin_layout()`](https://pedrobtz.github.io/zubin/reference/bin_layout.md)
for the specification of a record and the type table; the vignette,
[`vignette("zubin")`](https://pedrobtz.github.io/zubin/articles/zubin.md),
for a tour.

## Author

**Maintainer**: Pedro Baltazar <pedrobtz@gmail.com> \[copyright holder\]

Authors:

- Pedro Baltazar <pedrobtz@gmail.com> \[copyright holder\]
