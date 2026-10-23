# The R builder (design 13.5).

test_that("raw appends accumulate and bin_take() empties the builder", {
  b <- bin_builder()
  expect_identical(bin_size(b), 0)
  expect_invisible(bin_put(b, bytes("01 02")))
  bin_put(b, raw(0))
  bin_put(b, bytes("03"))
  expect_identical(bin_size(b), 3)
  expect_identical(as.raw(b), bytes("01 02 03"))
  expect_identical(bin_size(b), 3)
  expect_identical(bin_take(b), bytes("01 02 03"))
  expect_identical(bin_size(b), 0)
  expect_identical(bin_take(b), raw(0))
})

test_that("a taken builder keeps its capacity and accepts new appends", {
  b <- bin_builder(reserve = 1000)
  bin_put(b, as.raw(1:200))
  bin_take(b)
  expect_match(format(b), "0 bytes, capacity 1,000")
  bin_put(b, bytes("ff"))
  expect_identical(bin_take(b), bytes("ff"))
})

test_that("strings append as z (NUL-terminated) and s<n> (NUL-padded)", {
  b <- bin_builder()
  bin_put(b, c("ab", "", "c"), type = "z")
  expect_identical(bin_take(b), bytes("61 62 00 00 63 00"))
  bin_put(b, c("ab", "", "abcd"), type = "s4")
  expect_identical(bin_take(b), bytes("61 62 00 00 00 00 00 00 61 62 63 64"))
  bin_put(b, character(), type = "s4")
  expect_identical(bin_size(b), 0)
})

test_that("strings are written as UTF-8 whatever their declared encoding", {
  b <- bin_builder()
  latin1 <- iconv(rawToChar(bytes("e9")), "latin1", "latin1")
  Encoding(latin1) <- "latin1"
  bin_put(b, latin1, type = "z")
  expect_identical(bin_take(b), bytes("c3 a9 00"))
  euro <- rawToChar(bytes("e2 82 ac"))
  Encoding(euro) <- "UTF-8"   # rawToChar() gives native, which a C locale cannot translate
  bin_put(b, euro, type = "s3")
  expect_identical(bin_take(b), bytes("e2 82 ac"))
})

test_that("an NA or an overlong string appends nothing and names its index", {
  b <- bin_builder()
  bin_put(b, bytes("aa"))
  e <- expect_error(bin_put(b, c("x", NA), type = "z"), class = "zubin_na_error")
  expect_identical(e$index, 1)
  e <- expect_error(bin_put(b, c("abc", "abcde", "abcdef"), type = "s4"),
                    class = "zubin_range_error")
  expect_identical(e$index, 1)
  expect_identical(as.raw(b), bytes("aa"))
})

test_that("bin_put() refuses what it cannot append", {
  b <- bin_builder()
  expect_error(bin_put(b, 1:3), class = "zubin_invalid_argument")
  expect_error(bin_put(b, "a"), class = "zubin_invalid_argument")
  expect_error(bin_put(b, "a", type = "s0"), class = "zubin_invalid_argument")
  expect_error(bin_put(b, "a", type = "q"), class = "zubin_invalid_argument")
  expect_error(bin_put(b, 1, type = "z"), class = "zubin_invalid_argument")
  expect_error(bin_put(b, "a", type = c("z", "z")), class = "zubin_invalid_argument")
  expect_error(bin_put(raw(1), raw(1)), class = "zubin_invalid_argument")
  expect_error(bin_size(bytes("00")), class = "zubin_invalid_argument")
})

test_that("max is a hard cap: exactly max fits, one byte more is a limit error", {
  b <- bin_builder(max = 10)
  bin_put(b, as.raw(1:6))
  e <- expect_error(bin_put(b, as.raw(1:5)), class = "zubin_limit_error")
  expect_s3_class(e, "zubin_error")
  expect_identical(e$size, 11)
  expect_identical(e$max, 10)
  expect_identical(as.raw(b), as.raw(1:6))
  bin_put(b, as.raw(7:10))
  expect_identical(bin_size(b), 10)
  expect_error(bin_put(b, raw(1)), class = "zubin_limit_error")
  expect_error(bin_put(b, "x", type = "z"), class = "zubin_limit_error")
  expect_error(bin_reserve(b, 1), class = "zubin_limit_error")
  bin_put(b, raw(0))
  expect_identical(bin_take(b), as.raw(1:10))
})

test_that("a string append past max reports its size and appends nothing", {
  b <- bin_builder(max = 6)
  bin_put(b, bytes("01 02"))
  e <- expect_error(bin_put(b, c("ab", "cé"), type = "z"), class = "zubin_limit_error")
  expect_identical(e$size, 2 + 3 + 4)
  e <- expect_error(bin_put(b, c("ab", "c"), type = "s3"), class = "zubin_limit_error")
  expect_identical(e$size, 2 + 6)
  expect_identical(as.raw(b), bytes("01 02"))
  bin_put(b, c("ab", "c"), type = "s2")
  expect_identical(as.raw(b), bytes("01 02 61 62 63 00"))
})

test_that("bin_builder() validates reserve and max", {
  expect_error(bin_builder(reserve = -1), class = "zubin_invalid_argument")
  expect_error(bin_builder(reserve = 1.5), class = "zubin_invalid_argument")
  expect_error(bin_builder(reserve = NA), class = "zubin_invalid_argument")
  expect_error(bin_builder(max = 0), class = "zubin_invalid_argument")
  expect_error(bin_builder(max = c(1, 2)), class = "zubin_invalid_argument")
  e <- expect_error(bin_builder(reserve = 11, max = 10), class = "zubin_limit_error")
  expect_identical(e$max, 10)
  expect_s3_class(bin_builder(reserve = 10, max = 10), "zubin_builder")
})

test_that("bin_reserve() and bin_reset() keep the contents' contract", {
  b <- bin_builder()
  expect_invisible(bin_reserve(b, 5000))
  expect_match(format(b), "capacity 5,000")
  bin_put(b, bytes("01"))
  expect_invisible(bin_reset(b))
  expect_identical(bin_size(b), 0)
  expect_match(format(b), "capacity 5,000")
  expect_error(bin_reserve(b, -1), class = "zubin_invalid_argument")
})

test_that("a builder restored from serialization errors cleanly", {
  b <- bin_builder()
  bin_put(b, bytes("01"))
  b2 <- unserialize(serialize(b, NULL))
  expect_error(bin_put(b2, bytes("02")), class = "zubin_invalid_argument")
  expect_error(bin_size(b2), class = "zubin_invalid_argument")
  expect_error(bin_take(b2), class = "zubin_invalid_argument")
  expect_error(as.raw(b2), class = "zubin_invalid_argument")
  expect_error(bin_reset(b2), class = "zubin_invalid_argument")
  expect_error(bin_reserve(b2, 1), class = "zubin_invalid_argument")
  expect_error(bin_put(b2, "a", type = "z"), class = "zubin_invalid_argument")
  expect_identical(format(b2), "<zubin_builder: freed>")
  # the original is untouched
  expect_identical(as.raw(b), bytes("01"))
})

test_that("print shows size, capacity and max", {
  b <- bin_builder(reserve = 300, max = 1e6)
  bin_put(b, as.raw(1:3))
  expect_output(print(b), "<zubin_builder: 3 bytes, capacity 300, max 1,000,000>", fixed = TRUE)
  expect_output(print(bin_builder()), "max none")
})

test_that("builder operations survive gctorture", {
  skip_on_cran()
  skip_heavy()
  withr::defer(gctorture(FALSE))
  gctorture(TRUE)
  b <- bin_builder(reserve = 4)
  bin_put(b, bytes("01 02 03 04 05"))
  bin_put(b, c("a", "bc"), type = "z")
  bin_put(b, "x", type = "s2")
  x <- bin_take(b)
  y <- as.raw(b)
  gctorture(FALSE)
  expect_identical(x, bytes("01 02 03 04 05 61 00 62 63 00 78 00"))
  expect_identical(y, raw(0))
})

test_that("an allocation that fails is a memory error and changes nothing", {
  # CI runs it; CRAN's sanitizer machines may not set ASAN_OPTIONS
  skip_on_cran()
  skip_if(.Machine$sizeof.pointer < 8, "2^53 bytes are not addressable")
  # an allocation of 2^53 bytes is fatal under ASan rather than NULL, and
  # valgrind warns on it
  skip_if(nzchar(Sys.getenv("ASAN_OPTIONS")), "ASan aborts on an oversized allocation")
  skip_if(grepl("vgpreload", Sys.getenv("LD_PRELOAD")), "running under valgrind")
  b <- bin_builder()
  bin_put(b, bytes("01 02 03"))
  before <- format(b)
  # realloc() refuses 8 PiB on any machine, overcommit or not
  expect_error(bin_reserve(b, 2^53), class = "zubin_memory_error")
  expect_identical(as.raw(b), bytes("01 02 03"))
  expect_identical(format(b), before)
  bin_put(b, bytes("04"))
  expect_identical(as.raw(b), bytes("01 02 03 04"))
  expect_error(bin_builder(reserve = 2^53), class = "zubin_memory_error")
})

test_that("a buffer that cannot be allocated is freed before zb_r_buf_new() returns", {
  gc()
  before <- live_buffers()
  r <- .Call(zubin_test_buf_new_fail)
  expect_identical(r[[1L]], "ZB_ERR_MEMORY")
  expect_true(r[[2L]])
  expect_identical(live_buffers(), before)
  gc()
  expect_identical(live_buffers(), before)
})

test_that("a pack too large for a raw vector is a memory error before allocating", {
  skip_if(.Machine$sizeof.pointer < 8, "the size check is the 32-bit one there")
  # (2^22 + 1) records of 2^31 - 1 bytes: past R's largest vector, 2^52
  e <- expect_error(bin_pack("a:u8 x2147483646", a = integer(2^22 + 1)),
                    class = "zubin_memory_error")
  expect_identical(e$call, quote(bin_pack()))
})
