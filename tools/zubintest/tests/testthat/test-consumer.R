# The fixture's three translation units, each through the headers alone.

test_that("layout.c parses a layout and unpacks records with the kernels", {
  x <- as.raw(c(0x01, 0x02, 0x00, 0x00, 0x80, 0x3f, 0xff,
                0x03, 0x04, 0x00, 0x00, 0x00, 0x40, 0x00))
  r <- .Call(zt_unpack, "<id:u16 v:f32 flag:bool", x)
  expect_identical(r, list(c(513, 1027), c(1, 2), c(1, 0)))
  expect_identical(.Call(zt_unpack, "<id:u16 v:f99", x), "ZB_ERR_SPEC")
  expect_identical(.Call(zt_unpack, "<a:u8 x1", as.raw(c(5, 0, 6, 0)))[[1]], c(5, 6))
})

test_that("cursor.c reads a header and stops at a truncation", {
  x <- as.raw(c(0xca, 0xfe, 0xba, 0xbe, 0x02, 0x00, 0x2a, 0x00, 0x00, 0x00,
                0x7a, 0x75, 0x62, 0x69, 0x6e, 0x00))
  expect_identical(.Call(zt_header, x), c(3405691582, 2, 42, 16))
  expect_identical(.Call(zt_header, x[1:9]), c("ZB_ERR_EOF", "6"))
  expect_identical(.Call(zt_header, raw(0)), c("ZB_ERR_EOF", "0"))
})

test_that("buffer.c builds bytes in a buffer owned by R", {
  r <- .Call(zt_build, c(1, -2), as.raw(c(0xde, 0xad)))
  expect_identical(r, as.raw(c(0x3f, 0x80, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xde, 0xad)))
  expect_identical(.Call(zt_build, numeric(), raw(0)), raw(0))
  expect_error(.Call(zt_build, numeric(), raw(2^20 + 1)), "cap")
})

test_that("serial.c round-trips an object through a buffer, a cursor and a sink", {
  x <- list(a = 1:3, b = "zubintest", c = c(1.5, NA))
  r <- .Call(zt_serial, x, 0L)
  expect_identical(r[[1]], x)
  expect_identical(r[[2]], as.double(length(serialize(x, NULL))))
  expect_identical(r[[3]], as.double(length(serialize(x, NULL, version = 2)) - 14))
  expect_identical(.Call(zt_serial, x, 5L), "ZB_ERR_EOF")
})

test_that("rdz.c: an object through 4 KiB blocks behind 40-byte headers, and back", {
  small <- list(a = 1:3, b = "zubin")
  r <- .Call(zt_rdz, small, -1L)
  expect_identical(r[[1]], small)
  expect_identical(r[[2]], 1)
  big <- list(x = as.double(seq_len(5000)), y = rep(c("p", "q"), 3000), m = matrix(1:400, 20))
  r <- .Call(zt_rdz, big, -1L)
  expect_identical(r[[1]], big)
  n <- length(serialize(big, NULL))
  expect_identical(r[[2]], as.double(ceiling(n / 4096)))
  expect_identical(r[[3]], as.double(n + 40 * ceiling(n / 4096)))
  # a byte flipped in the second block's payload is caught by its hash
  expect_identical(.Call(zt_rdz, big, 4096L + 80L + 10L), "hash mismatch")
})
