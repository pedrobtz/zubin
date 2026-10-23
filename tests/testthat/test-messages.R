# One message per condition class (design 13.7). Tests assert on classes and
# fields; the wording lives here, where a change to it is a reviewed diff:
# the record numbers, 0-based in the fields, and sizes printed in full.

test_that("bounds, range, NA and encoding errors read well", {
  expect_snapshot(error = TRUE, {
    bin_unpack(raw(10), "a:u32", n = 3)
    bin_unpack(as.raw(rep(0xff, 9)), "a:u8 b:u64")
    bin_decode(as.raw(rep(0xff, 8)), "u64")
    bin_pack("a:u8 b:u8", a = 1:2, b = c(1, 300))
    bin_encode(c(1, 2, -1), "u32")
    bin_pack("a:i32", a = c(1L, NA))
    bin_unpack(as.raw(c(0x61, 0xff)), "s:s1")
  })
})

test_that("limit, spec and bin_put() index errors read well", {
  b <- bin_builder(max = 2147483648)
  bin_put(b, raw(4))
  expect_snapshot(error = TRUE, {
    bin_reserve(b, 2147483648)
    bin_put(b, c(1, 2, NA), type = "u8")
    bin_put(b, c("ab", "abc"), type = "s2")
    bin_layout("id:u32 name:s0")
    bin_layout("a:u8 a:u16")
  })
})
