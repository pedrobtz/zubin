# zubin

<!-- badges: start -->
[![R-CMD-check](https://github.com/pedrobtz/zubin/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/pedrobtz/zubin/actions/workflows/R-CMD-check.yaml)
[![coverage](https://raw.githubusercontent.com/pedrobtz/zubin/main/.github/badges/coverage.svg)](https://github.com/pedrobtz/zubin/actions/workflows/coverage.yaml)
<!-- badges: end -->

zubin reads and writes structured binary data in R: describe a fixed binary record once, as a
layout, and read or write millions of them in one call; convert typed vectors to and from
bytes at any width and byte order; append to a growable byte buffer that is not quadratic.
The same machinery is a header-only C library that other packages use through `LinkingTo`.

zubin is under development towards its first release; nothing is exported yet.

## Installation

You can install the development version of zubin from [GitHub](https://github.com/) with:

``` r
# install.packages("pak")
pak::pak("pedrobtz/zubin")
```
