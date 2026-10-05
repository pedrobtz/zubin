# Conditions raised by zubin

Every error zubin raises inherits `zubin_error`, and its class says what
went wrong, so code catches it by kind and reads the details from its
fields; messages may be reworded. Every offset and index is 0-based. The
call is reduced to the function's name, so that a large raw vector is
never printed with an error.

## Details

- `zubin_invalid_argument`:

  An argument is unusable: the wrong type, a negative or fractional
  size, an unknown type token, a column with no field, or a builder that
  was saved and restored. Field: `arg`, the argument's name, when there
  is one.

- `zubin_spec_error`:

  A layout specification is malformed. Field: `position`, the byte in
  the specification where the problem is.

- `zubin_bounds_error`:

  The records asked for run past the end of the input. Fields: `offset`
  and `length`.

- `zubin_range_error`:

  A value does not fit: 300 into `u8`, 1.5 into an integer type, an
  `i64` above 2^53 read as a double, a string longer than its field.
  Fields: `field` (`NA` for a codec) and `index`, the record of the
  first such value.

- `zubin_na_error`:

  An `NA` has no bytes in its field. Fields: `field` and `index`.

- `zubin_encoding_error`:

  An `s<n>` field is not valid UTF-8 (read it with `encoding = "latin1"`
  or `"bytes"`). Fields: `field` and `index`.

- `zubin_limit_error`:

  An append would pass a builder's `max`, which leaves it unchanged.
  Fields: `size`, what the builder would have held, and `max`.

- `zubin_memory_error`:

  An allocation failed, or a size would not fit in memory.

## Examples

``` r
tryCatch(
  bin_encode(c(1, 300), "u8"),
  zubin_range_error = function(e) e$index
)
#> [1] 1
tryCatch(
  bin_layout("id:u32 name:s0"),
  zubin_spec_error = function(e) e$position
)
#> [1] 13
```
