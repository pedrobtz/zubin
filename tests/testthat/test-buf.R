# buf.h (design 9) through the harness: growth, the cap, borrowing, typed appends.

test_that("growth doubles to 64 MiB and grows by half after, from 256 bytes", {
  mib <- 2^20
  expect_identical(buf_growth(mib, 128 * mib), c(1, 2, 4, 8, 16, 32, 64, 96, 144) * mib)
  expect_identical(buf_growth(1, 1000), c(256, 512, 1024))
  expect_identical(buf_growth(300, 300), 300)
})

test_that("byte-sized appends from 0 past 64 MiB reallocate the expected number of times", {
  skip_if_no_slow_tests()
  caps <- buf_growth(1, 2^27)
  expect_identical(caps, c(2^(8:26), 1.5 * 2^26, 2.25 * 2^26))
  expect_length(caps, 21L)
})

test_that("a buffer fills to exactly max; one byte more is refused and changes nothing", {
  r <- buf_cap(0, 100, c(60, 40, 1, 0))
  expect_identical(r$alloc, "ZB_OK")
  expect_identical(r$status, c(0L, 0L, 5L, 0L))
  expect_identical(r$len, c(60, 100, 100, 100))
  expect_identical(r$cap, c(256 - 156, 100, 100, 100))
  expect_identical(r$hit_limit, c(FALSE, FALSE, TRUE, TRUE))
  expect_identical(r$bytes, as.raw(c(0:59, 0:39) %% 251))
})

test_that("a refused append in the middle of a buffer leaves its bytes as they were", {
  r <- buf_cap(8, 50, c(10, 41, 40))
  expect_identical(r$status, c(0L, 5L, 0L))
  expect_identical(r$len, c(10, 10, 50))
  expect_identical(r$bytes, as.raw(c(0:9, 0:39)))
})

test_that("a reserve above max is refused when the buffer is created", {
  expect_identical(buf_cap(101, 100, numeric())$alloc, "ZB_ERR_LIMIT")
  expect_identical(buf_cap(100, 100, 100)$status, 0L)
})

test_that("a borrowed buffer is writable up to its length and never grows", {
  r <- buf_borrow(bytes("01 02 03 04 05"), c(2L, 3L, 1L, 0L))
  expect_identical(r$status, c(0L, 0L, 1L, 0L))
  expect_identical(r$len, c(2, 5, 5, 5))
  expect_identical(r$bytes, bytes("ee ee ee ee ee"))
  expect_true(r$released)
  # an empty borrow refuses any write but a zero-length one
  r <- buf_borrow(raw(0), c(0L, 1L))
  expect_identical(r$status, c(0L, 1L))
})

test_that("typed appends equal the writers of rw.h, one at a time or vectorised", {
  for (type in rw_types) {
    v <- rw_values(type)
    for (e in c("le", "be")) {
      expect_identical(buf_put(type, e, v, FALSE), rw_write(type, e, v), label = paste(type, e))
      expect_identical(buf_put(type, e, v, TRUE), rw_write(type, e, v), label = paste(type, e, "_n"))
    }
    expect_identical(buf_put(type, "le", v[0], TRUE), raw(0))
  }
})

test_that("the corners of buf.h hold", {
  checks <- .Call(zubin_test_buf_misc)
  expect_true(all(checks), label = paste(names(checks)[!checks], collapse = ", "))
  expect_length(checks, 17L)
})
