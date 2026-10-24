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
  # Not a skip: an append that finished inside every limit would mean the
  # interrupt check is gone, or proves nothing.
  expect_true(cut_short)
  expect_identical(bin_size(b), 1)
  expect_identical(as.raw(b), bytes("01"))
})

test_that("an interrupted string bin_put() leaves the builder unchanged", {
  skip_on_cran()
  skip_if_no_slow_tests()
  b <- bin_builder()
  bin_put(b, bytes("01"))
  big <- rep("abc", 2^24)   # 1 GiB as s64
  cut_short <- FALSE
  for (limit in c(0.02, 0.05, 0.1)) {
    if (interrupted_by_time_limit(function() bin_put(b, big, type = "s64"), limit)) {
      cut_short <- TRUE
      break
    }
    bin_reset(b)
    bin_put(b, bytes("01"))
  }
  # Not a skip: an append that finished inside every limit would mean the
  # interrupt check is gone, or proves nothing.
  expect_true(cut_short)
  expect_identical(bin_size(b), 1)
  expect_identical(as.raw(b), bytes("01"))
})

test_that("an interrupted typed bin_put() leaves the builder unchanged", {
  skip_on_cran()
  skip_if_no_slow_tests()
  b <- bin_builder()
  bin_put(b, bytes("01"))
  big <- integer(2^28)   # 1 GiB as i32, 16 chunks of 64 MiB
  cut_short <- FALSE
  for (limit in c(0.02, 0.05, 0.1)) {
    if (interrupted_by_time_limit(function() bin_put(b, big, type = "i32"), limit)) {
      cut_short <- TRUE
      break
    }
    bin_reset(b)
    bin_put(b, bytes("01"))
  }
  expect_true(cut_short)
  expect_identical(bin_size(b), 1)
  expect_identical(as.raw(b), bytes("01"))
})

test_that("an interrupted bin_pack() leaves nothing behind", {
  skip_on_cran()
  skip_if_no_slow_tests()
  # many wide fields: the interrupt is checked between them
  spec <- paste0("f", 1:64, ":f64", collapse = " ")
  cols <- rep(list(double(2^21)), 64)   # 1 GiB of records in all
  names(cols) <- paste0("f", 1:64)
  copy <- cols
  cut_short <- FALSE
  for (limit in c(0.02, 0.05, 0.1)) {
    if (interrupted_by_time_limit(function() bin_pack(spec, cols), limit)) {
      cut_short <- TRUE
      break
    }
  }
  expect_true(cut_short)
  expect_identical(cols, copy)
  gc()
  # the half-written result was R's, and is collected like any vector
  expect_length(bin_pack("a:u8", a = 1:3), 3L)
})

test_that("a buffer stranded by an erroring refhook during serialization is freed", {
  gc()
  before <- live_buffers()
  cnd <- structure(class = c("my_hook_error", "error", "condition"),
                   list(message = "the hook says no", call = NULL))
  expect_error(.Call(zubin_test_serialize_owned, list(new.env()), function(x) stop(cnd)),
               class = "my_hook_error")
  gc()
  expect_identical(live_buffers(), before)
})

test_that("an interrupt during a large serialization leaves nothing behind", {
  skip_on_cran()   # timing-dependent by construction
  skip_heavy()
  envs <- lapply(seq_len(20000), function(i) new.env())
  slow_hook <- function(x) {
    for (i in 1:200) NULL   # evaluation is where R checks for interrupts
    NULL
  }
  gc()
  before <- live_buffers()
  cut_short <- FALSE
  for (limit in c(0.05, 0.2, 1)) {
    if (interrupted_by_time_limit(function() .Call(zubin_test_serialize_owned, envs, slow_hook), limit)) {
      cut_short <- TRUE
      break
    }
  }
  expect_true(cut_short)
  gc()
  expect_identical(live_buffers(), before)
  # and a builder interrupted mid-serialization holds what it held
  b <- bin_builder()
  bin_put(b, bytes("01"))
  cut_short <- FALSE
  for (limit in c(0.05, 0.2, 1)) {
    if (interrupted_by_time_limit(function() bin_serialize(envs, b, refhook = slow_hook), limit)) {
      cut_short <- TRUE
      break
    }
  }
  expect_true(cut_short)
  expect_identical(as.raw(b), bytes("01"))
})

# Seen to fail: with R_ClearExternalPtr() removed from the extbuf finalizer,
# the eager release and the collector both release the block, and this test
# crashes on the double free (#63).
test_that("a foreign buffer is released exactly once, eagerly or by the collector", {
  gc()
  before <- live_buffers()
  released <- .Call(zubin_test_extbuf_released)
  h <- .Call(zubin_test_extbuf_new, 4)
  expect_identical(.Call(zubin_test_extbuf_bytes, h), bytes("ab ab ab ab"))
  .Call(zubin_test_extbuf_release, h)
  expect_null(.Call(zubin_test_extbuf_bytes, h))
  expect_identical(.Call(zubin_test_extbuf_released), released + 1L)
  rm(h)
  gc()
  expect_identical(.Call(zubin_test_extbuf_released), released + 1L)
  h <- .Call(zubin_test_extbuf_new, 0)
  rm(h)
  gc()
  expect_identical(.Call(zubin_test_extbuf_released), released + 2L)
  expect_identical(live_buffers(), before)
})

test_that("adopting twice, or into something that is not a handle, takes nothing", {
  m <- .Call(zubin_test_extbuf_misc)
  expect_identical(m$twice, "ZB_ERR_INVALID")
  expect_identical(m$not_handle, "ZB_ERR_INVALID")
  expect_true(m$ptr_of_raw)
  expect_true(m$empty_ptr)
})
