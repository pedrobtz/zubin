## Submission

This is the first submission of zubin.

zubin reads and writes structured binary data: a fixed binary record is described once, as
a layout, and any number of records are read or written in one vectorised call. The same
code is a header-only C99 library that other packages use through `LinkingTo: zubin,
zufast` alone, exactly as they use zufast: every function is `static inline`, nothing is
registered with `R_RegisterCCallable`, and `zubin.so` exports `R_init_zubin` only. The
fixture package that proves that shape (built, checked with `--as-cran` and tested with
zubin uninstalled, on Linux, macOS and Windows) is in the repository under `tools/` and is
not part of the package. zubin vendors no code.

zubin `LinkingTo`s zufast, which is on CRAN (version 0.1.0).

## Test environments

The checks below ran on the release commit in the package's continuous integration.

* GitHub Actions: macOS (R release), Windows (R release and R-devel), Ubuntu (R release
  and oldrel-1).
* R-hub containers with CRAN's compilers: clang 23, Ubuntu clang, Ubuntu GCC 16.
* 32-bit i386 and musl (Alpine), and big-endian s390x under QEMU.
* UBSan and ASan (GCC and clang), valgrind, LTO, `gctorture`, rchk, and GCC's
  `-fanalyzer`.

## R CMD check results

0 errors | 0 warnings | 1 note

* This is a new release.
