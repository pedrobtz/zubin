# The unpack kernels of design 11.5, driven directly by the harness.

test_that("kernels read at any stride, n = 0 included", {
  x <- as.raw(0:63)
  for (stride in c(3, 5, 7, 8, 13)) {
    k <- kernel(x, "a:u8 b:u16", stride = stride)
    n <- (64 - 3) %/% stride + 1
    expect_identical(k[[1]]$values, as.integer(seq(0, by = stride, length.out = n)), label = stride)
    expect_identical(k[[2]]$values, as.integer(seq(1, by = stride, length.out = n)) +
                       256L * as.integer(seq(2, by = stride, length.out = n)))
  }
  k <- kernel(x, "a:u8 b:f64 c:i64 d:s3", n = 0)
  expect_identical(k[[1]]$values, integer())
  expect_identical(k[[2]]$values, double())
  expect_identical(k[[3]]$values, double())
  expect_identical(k[[4]]$values, raw())
  expect_true(all(vapply(k, function(f) f$status == "ZB_OK", logical(1))))
})

test_that("array elements are placed column-major, element k of record i at k * n + i", {
  x <- as.raw(c(1, 2, 3, 0xaa, 4, 5, 6, 0xbb))
  k <- kernel(x, "v:u8[3] x1")
  expect_identical(k[[1]]$values, c(1L, 4L, 2L, 5L, 3L, 6L))
  k <- kernel(x, "v:u8[3] x1", stride = 4, n = 1)
  expect_identical(k[[1]]$values, 1:3)
})

test_that("each kernel reports the first record that does not fit", {
  x <- c(le_i32(c(5L, 6L)), bytes("00 00 00 80"), le_i32(7L), bytes("00 00 00 80"))
  k <- kernel(x, "v:i32")[[1]]
  expect_identical(k$status, "ZB_OK")              # allow_na: the bits come through
  expect_identical(k$values, c(5L, 6L, NA, 7L, NA))
  expect_identical(k$status2, "ZB_ERR_RANGE")
  expect_identical(k$bad2, 2)
  big <- bytes("00 00 00 00 00 00 00 80")
  k <- kernel(c(raw(8), big, big), "v:u64")[[1]]
  expect_identical(k$status, "ZB_ERR_RANGE")
  expect_identical(k$bad, 1)
  expect_identical(k$status2, "ZB_ERR_RANGE")   # 2^63 is past 2^53 as well
  expect_identical(k$bad2, 1)
})

test_that("the exact-double kernel refuses past 2^53 and the int64 kernel does not", {
  x <- bytes("01 00 00 00 00 00 20 00")
  k <- kernel(x, "v:i64")[[1]]
  expect_identical(k$status, "ZB_OK")
  expect_identical(k$status2, "ZB_ERR_RANGE")
  expect_identical(k$bad2, 0)
})

test_that("bytes and strings come out contiguous, record after record", {
  x <- as.raw(1:12)
  k <- kernel(x, "x1 a:b2 x1 b:s2", stride = 6)
  expect_identical(k[[2]]$values, as.raw(c(2, 3, 8, 9)))
  expect_identical(k[[4]]$values, as.raw(c(5, 6, 11, 12)))
})
