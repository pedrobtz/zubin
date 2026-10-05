# bin_hexdump() and bin_diff() (design 13.6).

test_that("a known 64-byte input dumps as xxd does, at widths 16 and 8", {
  x <- as.raw((0:63 * 5 + 7) %% 256)
  expect_snapshot(bin_hexdump(x))
  expect_snapshot(bin_hexdump(x, width = 8))
  expect_snapshot(bin_hexdump(x, offset = 37, n = 20))
})

test_that("each line holds its absolute offset, its bytes, and their ASCII", {
  h <- bin_hexdump(bytes("41 42 0a 09 7a 7e 7f"), width = 4)
  expect_s3_class(h, "zubin_hexdump")
  expect_identical(unclass(h), c("00000000: 4142 0a09  AB..", "00000004: 7a7e 7f    z~."))
  expect_identical(format(h), unclass(h))
  h <- bin_hexdump(as.raw(1:5), width = 3)
  expect_identical(unclass(h), c("00000000: 0102 03  ...", "00000003: 0405     .."))
})

test_that("n past the end shows what there is; offset past the end is an error", {
  x <- as.raw(1:10)
  expect_identical(bin_hexdump(x, offset = 8, n = 100), bin_hexdump(x, offset = 8))
  expect_length(bin_hexdump(x, offset = 10), 0L)
  e <- expect_error(bin_hexdump(x, offset = 11), class = "zubin_bounds_error")
  expect_identical(e$length, 10)
  expect_length(bin_hexdump(x, n = 0), 0L)
})

test_that("empty input dumps to nothing and says so", {
  h <- bin_hexdump(raw(0))
  expect_length(h, 0L)
  expect_output(print(h), "no bytes")
})

test_that("bin_hexdump() validates its arguments", {
  expect_error(bin_hexdump(1:3), class = "zubin_invalid_argument")
  expect_error(bin_hexdump(raw(3), width = 0), class = "zubin_invalid_argument")
  expect_error(bin_hexdump(raw(3), width = 257), class = "zubin_invalid_argument")
  expect_error(bin_hexdump(raw(3), offset = -1), class = "zubin_invalid_argument")
})

test_that("bin_diff() lists the first differing offsets, with both bytes", {
  a <- as.raw(0:20)
  b <- a
  b[c(4, 8, 21)] <- as.raw(255)
  d <- bin_diff(a, b)
  expect_s3_class(d, "data.frame")
  expect_identical(d$offset, c(3, 7, 20))
  expect_identical(d$a, as.raw(c(3, 7, 20)))
  expect_identical(d$b, as.raw(c(255, 255, 255)))
  expect_identical(nrow(bin_diff(a, b, n = 2)), 2L)
  expect_identical(attr(d, "length_a"), 21)
})

test_that("equal inputs and a prefix have no differing bytes, only lengths", {
  a <- as.raw(1:10)
  d <- bin_diff(a, a)
  expect_identical(nrow(d), 0L)
  d <- bin_diff(a, a[1:4])
  expect_identical(nrow(d), 0L)
  expect_identical(c(attr(d, "length_a"), attr(d, "length_b")), c(10, 4))
  expect_identical(nrow(bin_diff(raw(0), raw(0))), 0L)
  expect_error(bin_diff(a, 1:3), class = "zubin_invalid_argument")
  expect_error(bin_diff(a, a, n = -1), class = "zubin_invalid_argument")
})
