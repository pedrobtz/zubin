# Describe a binary record

`bin_layout()` parses a specification of a fixed-size binary record into
a `zubin_layout`: the type, element count, size, 0-based byte offset and
byte order of each field, the record's size, and its alignment. Offsets
are 0-based, as in every file format specification.

## Usage

``` r
bin_layout(spec, endian = c("little", "big", "native"), align = FALSE)
```

## Arguments

- spec:

  A specification string; or a named character vector of type tokens,
  whose names are the field names (`c(id = "u32", "x3")`); or a
  `zubin_layout`, which is returned unchanged.

- endian:

  The byte order of fields the specification does not order itself:
  `"little"`, `"big"`, or `"native"`, the host's (see
  [`bin_info()`](https://pedrobtz.github.io/zubin/reference/bin_info.md)).

- align:

  `FALSE` for packed fields, `TRUE` for C struct alignment.

## Value

A `zubin_layout`: a list with `spec`, the normalised specification;
`fields`, a data frame with columns `name`, `type`, `count` (array
elements, 1 for a scalar), `size`, `offset` and `endian` (`NA` for
one-byte types); `size`, the record size in bytes; and `align`, the
record's alignment (1 when packed).
[`length()`](https://rdrr.io/r/base/length.html) and
[`names()`](https://rdrr.io/r/base/names.html) describe the fields that
carry values, which excludes padding;
[`as.data.frame()`](https://rdrr.io/r/base/as.data.frame.html) returns
`fields`;
[`bin_size()`](https://pedrobtz.github.io/zubin/reference/bin_size.md)
returns `size`.

## The specification

One string of fields separated by whitespace or a comma:

    layout  := [ endian ] field { sep field }
    endian  := "<" | ">" | "="          little, big, host; default `endian`
    field   := [ name ":" ] type [ "[" count "]" ] [ "." order ]
             | "x" width                padding: never read, written as zeros
    type    := u8 i8 u16 i16 u32 i32 u64 i64 f16 bf16 f32 f64 bool
             | "b" width                fixed bytes
             | "s" width                fixed string
    order   := "le" | "be"              multi-byte numeric types only
    name    := [A-Za-z_] [A-Za-z0-9_.]*
    count, width := 1 .. 2^31 - 1

For example `"<magic:u32 version:u16 flags:u16 count:u64 name:s32"`, or
`"<ts:i64 price:f64 qty:i32 side:u8 x3 crc:u32.be"`. An endian prefix
overrides `endian`, and a field's `.le` or `.be` suffix overrides both.
Names are unique; an unnamed field is called `V1`, `V2`, ... by its
position among the fields that are not padding. `t[k]` is an array of
`k` elements, for the numeric types and `bool` only (write `b24`, not
`b8[3]`). Every malformed specification is a `zubin_spec_error` carrying
`position`, the 0-based byte position of the problem in the string.

## Types

|                 |           |                                       |
|-----------------|-----------|---------------------------------------|
| **Spec**        | **Bytes** | **R type**                            |
| `u8 i8 u16 i16` | 1–2       | integer                               |
| `i32`           | 4         | integer; -2^31 is `NA`                |
| `u32`           | 4         | double                                |
| `i64 u64`       | 8         | double, exact to 2^53, or `integer64` |
| `f16 bf16`      | 2         | double                                |
| `f32 f64`       | 4, 8      | double                                |
| `bool`          | 1         | logical                               |
| `b<n>`          | n         | a list of raw vectors                 |
| `s<n>`          | n         | character, up to the first NUL        |
| `x<n>`          | n         | not a value: padding                  |
| `t[k]`          | k x width | a matrix of k columns                 |

## Alignment

With `align = FALSE` fields are packed at consecutive offsets. With
`align = TRUE` each field starts at the next multiple of its element
width (1 for `bool`, `b`, `s` and `x`) and the record size is rounded up
to the largest of them: the rule C compilers apply to a `struct` on
64-bit targets, so a struct written with `fwrite()` reads back with
`align = TRUE`. (32-bit x86 aligns 8-byte members to 4 inside a struct;
such a struct needs explicit `x` padding.)

## Examples

``` r
wav <- bin_layout("<riff:s4 size:u32 wave:s4")
wav
#> <zubin_layout: 12 bytes, 3 fields>
#>  name type count size offset endian
#>  riff s4   1     4    0            
#>  size u32  1     4    4      little
#>  wave s4   1     4    8            
bin_size(wav)
#> [1] 12
wav$fields
#>   name type count size offset endian
#> 1 riff   s4     1    4      0   <NA>
#> 2 size  u32     1    4      4 little
#> 3 wave   s4     1    4      8   <NA>

# The same, built programmatically
bin_layout(c(riff = "s4", size = "u32", wave = "s4"))
#> <zubin_layout: 12 bytes, 3 fields>
#>  name type count size offset endian
#>  riff s4   1     4    0            
#>  size u32  1     4    4      little
#>  wave s4   1     4    8            

# A C struct { uint8_t a; uint32_t b; uint16_t c; }
bin_layout("a:u8 b:u32 c:u16", endian = "native", align = TRUE)$fields
#>   name type count size offset endian
#> 1    a   u8     1    1      0   <NA>
#> 2    b  u32     1    4      4 little
#> 3    c  u16     1    2      8 little
```
