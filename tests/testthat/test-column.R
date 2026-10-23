# zubin/column.h (design 11.6): bitmaps, validity and byte order, through
# the harness.

test_that("bits are least significant first; the msb variants the other way", {
  x <- bytes("01 80")
  expect_identical(bitmap_run(x)$bits, c(TRUE, rep(FALSE, 14), TRUE))
  expect_identical(bitmap_run(x, msb = TRUE)$bits, c(rep(FALSE, 7), TRUE, TRUE, rep(FALSE, 7)))
  expect_identical(bitmap_run(x, offset = 7, n = 2)$bits, c(FALSE, FALSE))
})

test_that("setting a bit changes that bit alone", {
  expect_bytes(bitmap_run(bytes("00 ff"), idx = c(3, 9), val = c(TRUE, FALSE))$set, "08 fd")
  expect_bytes(bitmap_run(bytes("00 ff"), msb = TRUE, idx = c(0, 15), val = c(TRUE, FALSE))$set, "80 fe")
})

test_that("counts agree with rawToBits() at every offset and length", {
  withr::local_seed(21)
  x <- as.raw(sample(0:255, 6L, replace = TRUE))
  ref <- as.integer(rawToBits(x))
  for (off in 0:15) {
    for (n in 0:(48 - off)) {
      expect_identical(bitmap_run(x, off, n)$count, as.double(sum(ref[off + seq_len(n)])),
                       label = paste(off, n))
    }
  }
})

test_that("a bitmap of n bits takes ceiling(n / 8) bytes", {
  sizes <- vapply(c(0, 1, 7, 8, 9, 16, 17), function(n) bitmap_run(raw(3), 0, n)$nbytes, double(1))
  expect_identical(sizes, c(0, 1, 1, 1, 2, 2, 3))
})

test_that("bit-packed booleans unpack to 0 and 1 and pack back", {
  r <- .Call(zubin_test_bits, bytes("b5 00"), 0, 8, c(1L, 0L, 5L, -1L))
  expect_identical(r$unpacked, c(TRUE, FALSE, TRUE, FALSE, TRUE, TRUE, FALSE, TRUE))
  expect_identical(r$status, "ZB_OK")
  expect_bytes(r$packed, "bd 00")   # 1 0 1 1 over b5's low bits
  r <- .Call(zubin_test_bits, bytes("00 00"), 3, 0, c(1L, 0L, 5L, -1L))
  expect_bytes(r$packed, "68 00")   # bits 3 to 6: 1 0 1 1
  r <- .Call(zubin_test_bits, bytes("00"), 0, 0, c(1L, 1L, NA, 1L))
  expect_identical(r$status, "ZB_ERR_NA")
  expect_identical(r$bad, 2)
  expect_bytes(r$packed, "03")      # the bits before the NA are written
})

test_that("a cleared validity bit becomes R's missing value", {
  r <- .Call(zubin_test_validity_apply, bytes("05"), 0, 1:4, NULL)
  expect_identical(r$values, c(1L, NA, 3L, NA))
  expect_identical(r$nulls, 2)
  r <- .Call(zubin_test_validity_apply, bytes("05"), 1, 1:4, NULL)
  expect_identical(r$values, c(NA, 2L, NA, NA))
  r <- .Call(zubin_test_validity_apply, NULL, 0, 1:4, NULL)
  expect_identical(r$values, 1:4)
  expect_identical(r$nulls, 0)
  r <- .Call(zubin_test_validity_apply, bytes("02"), 0, c(1.5, 2.5), NULL)
  expect_identical(bits(r$values), bits(c(NA_real_, 2.5)))
  r <- .Call(zubin_test_validity_apply, bytes("01"), 0, raw(32), rep(as.raw(0xff), 16))
  expect_identical(r$values, c(raw(16), rep(as.raw(0xff), 16)))
  expect_identical(r$nulls, 1)
})

test_that("a missing value clears its validity bit and touches no other", {
  r <- .Call(zubin_test_validity_from, c(1L, NA, 3L), 0, NULL)
  expect_bytes(r$valid, "fd")
  expect_identical(r$nulls, 1)
  r <- .Call(zubin_test_validity_from, c(1L, NA, 3L), 6, NULL)
  expect_bytes(r$valid, "7f ff")
  # NA_real_ is missing; NaN is a value
  r <- .Call(zubin_test_validity_from, c(NA, NaN, 1), 0, NULL)
  expect_bytes(r$valid, "fe")
  expect_identical(r$nulls, 1)
  r <- .Call(zubin_test_validity_from, bytes("00 00 ff ff 00 01"), 0, bytes("ff ff"))
  expect_bytes(r$valid, "fd")
})

test_that("a swapped copy reverses every element, in place or not", {
  x <- as.raw(1:16)
  for (in_place in c(FALSE, TRUE)) {
    expect_bytes(.Call(zubin_test_swap_copy, x[1:4], 2L, in_place), "02 01 04 03")
    expect_bytes(.Call(zubin_test_swap_copy, x[1:4], 4L, in_place), "04 03 02 01")
    expect_identical(.Call(zubin_test_swap_copy, x[1:8], 8L, in_place), rev(x[1:8]))
    expect_identical(.Call(zubin_test_swap_copy, x, 16L, in_place), rev(x))
    expect_bytes(.Call(zubin_test_swap_copy, x[1:3], 3L, in_place), "03 02 01")
    expect_identical(.Call(zubin_test_swap_copy, x[1:3], 1L, in_place), x[1:3])
  }
})
