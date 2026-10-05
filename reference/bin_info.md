# Information about the compiled zubin headers

Reports, from compiled code, the version of the zubin headers the
package's own shared object was built with, the version of the zufast
headers they were compiled against, the host's byte order, and the
compiler and build flags. Packages that use `LinkingTo: zubin, zufast`
carry their own copy of the code; this describes zubin's copy only.

## Usage

``` r
bin_info()
```

## Value

A named list with elements `version` (a string such as `"0.1.0"`),
`version_major`, `version_minor`, `version_patch` (integers), `zufast`
(the zufast header version, a string), `endian` (`"little"` or `"big"`,
what `endian = "native"` means on this host), `compiler` (a string) and
`build` (a named character vector: `c_standard`, the value of
`__STDC_VERSION__`; `optimized` and `ndebug`, each `"true"` or
`"false"`; and `fortify_source`, the `_FORTIFY_SOURCE` level or `"0"`).

## Examples

``` r
bin_info()$version
#> [1] "0.0.0"
bin_info()$endian
#> [1] "little"
```
