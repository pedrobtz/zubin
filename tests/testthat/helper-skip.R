# Sweeps and big buffers run when ZUBIN_SLOW_TESTS=true (CI does); the
# gctorture leg sets ZUBIN_SKIP_HEAVY to leave out tests that allocate
# millions of objects.
skip_if_no_slow_tests <- function() {
  skip_if_not(identical(Sys.getenv("ZUBIN_SLOW_TESTS"), "true"), "ZUBIN_SLOW_TESTS is not true")
}

skip_heavy <- function() {
  skip_if(nzchar(Sys.getenv("ZUBIN_SKIP_HEAVY")), "ZUBIN_SKIP_HEAVY is set")
}
