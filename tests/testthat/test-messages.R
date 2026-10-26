# Every condition class's message, and every message that computes a number
# or an excerpt (design 13.7). Tests assert on classes and fields; the
# wording lives here, where a change to it is a reviewed diff: the record
# numbers, 0-based in the fields, and sizes printed in full. One test, so
# that a shuffled run cannot reorder the snapshot file.

test_that("every condition class's message reads well", {
  # 2e9, not 2^31: a 32-bit size_t holds no size past 2^31 - 1
  b <- bin_builder(max = 2e9)
  bin_put(b, raw(4))
  small <- bin_builder(max = 10)
  bin_put(small, raw(4))
  stream <- serialize(1:3, NULL)
  expect_snapshot(error = TRUE, {
    bin_unpack(raw(10), "a:u32", n = 3)
    bin_unpack(as.raw(rep(0xff, 9)), "a:u8 b:u64")
    bin_decode(as.raw(rep(0xff, 8)), "u64")
    bin_pack("a:u8 b:u8", a = 1:2, b = c(1, 300))
    bin_encode(c(1, 2, -1), "u32")
    bin_pack("a:i32", a = c(1L, NA))
    bin_unpack(as.raw(c(0x61, 0xff)), "s:s1")
    bin_reserve(b, 2e9)
    bin_put(b, c(1, 2, NA), type = "u8")
    bin_put(b, c("ab", "abc"), type = "s2")
    bin_layout("id:u32 name:s0")
    bin_layout("a:u8 a:u16")
    bin_layout("a:u8 b:")
    bin_layout("")
    bin_layout("a:u8[2] a.1:u8")
    bin_unpack(1:3, "a:u8")
    bin_hexdump(raw(3), offset = 4)
    bin_unserialize(stream[1:20])
    bin_serialize(1:3, small)
    bin_pack("a:u8 b:u8", a = 1:2, b = 1:3)
    bin_pack("a:u8", a = "x")
    bin_pack("a:u8[2]", a = 1:3)
    bin_pack("a:u8[2]", a = matrix(1:3, 1))
    bin_pack("a:u8 x2147483646", a = integer(2^22 + 1))
    bin_put(b, list(as.raw(1:2), 1:2), type = "b2")
    bin_serialize(list(new.env()), b, refhook = function(x) {
      bin_put(b, raw(1))
      "ref"
    })
  })
})
