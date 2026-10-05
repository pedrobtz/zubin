# The workload for alloc-failure.yaml (r-actions' allocation-failure sweep):
# the paths through zubin's one allocator, zb_buf_reserve(), from R. The
# sweep fails one allocation per run; a run whose failure landed in zubin
# must end in a zubin_memory_error, printed with its class so that the
# workflow's target-pattern can count it, and never in a crash.
suppressPackageStartupMessages(library(zubin))
tryCatch({
  b <- bin_builder()
  for (i in 1:40) bin_put(b, as.raw(seq_len(256) %% 256))      # growth from 0
  bin_put(b, c(1.5, 2.5), type = "f64", endian = "big")        # typed, in place
  bin_put(b, c("abc", "de"), type = "z")
  bin_reserve(b, 3e6)
  x <- bin_take(b)
  stopifnot(length(x) == 40 * 256 + 16 + 7)
  l <- bin_layout("<id:u32 v:f64 s:s4")
  y <- bin_pack(l, id = 1:1000, v = 0.5, s = "ab")
  stopifnot(identical(bin_unpack(y, l)$id, as.double(1:1000)))
}, error = function(e) {
  cat("caught:", class(e)[1], conditionMessage(e), "\n")
  quit(status = 1L)
})
cat("ok\n")
