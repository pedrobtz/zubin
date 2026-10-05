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
