# Heap state owned by R must be freed on every way out of a call, including
# the ones that longjmp (design 12, 16.3). The glue counts the buffers it
# creates and frees, so a leak is a number rather than a guess. With the
# finalizer broken, the error and interrupt tests below fail; see the
# Stage 2 pull request for that run.

test_that("builders that are dropped are freed by the collector", {
  gc()
  before <- live_buffers()
  local({
    b <- bin_builder(reserve = 100)
    bin_put(b, as.raw(1:10))
    expect_identical(live_buffers(), before + 1)
  })
  gc()
  expect_identical(live_buffers(), before)
})

test_that("a buffer stranded by an error is freed once collected", {
  gc()
  before <- live_buffers()
  expect_error(.Call(zubin_test_put_then_error), "planned error")
  gc()
  expect_identical(live_buffers(), before)
})

test_that("a buffer stranded by an interrupt is freed once collected", {
  skip_on_cran()   # timing-dependent by construction
  skip_heavy()
  gc()
  before <- live_buffers()
  cut_short <- FALSE
  for (limit in c(0.05, 0.2, 1)) {
    # a million 64 KiB appends: far longer than any of these limits
    if (interrupted_by_time_limit(function() .Call(zubin_test_put_loop, 65536, 1e6), limit)) {
      cut_short <- TRUE
      break
    }
  }
  # Not a skip: a machine fast enough to finish would mean this proves nothing.
  expect_true(cut_short)
  gc()
  expect_identical(live_buffers(), before)
})

test_that("an interrupted bin_put() leaves the builder unchanged", {
  skip_on_cran()
  skip_if_no_slow_tests()
  b <- bin_builder()
  bin_put(b, bytes("01"))
  big <- raw(2^30)
  cut_short <- FALSE
  for (limit in c(0.02, 0.05, 0.1)) {
    if (interrupted_by_time_limit(function() bin_put(b, big), limit)) {
      cut_short <- TRUE
      break
    }
    bin_reset(b)
    bin_put(b, bytes("01"))
  }
  skip_if_not(cut_short, "the 1 GiB append finished inside every limit")
  expect_identical(bin_size(b), 1)
  expect_identical(as.raw(b), bytes("01"))
})
