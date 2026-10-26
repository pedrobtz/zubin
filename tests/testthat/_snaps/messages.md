# every condition class's message reads well

    Code
      bin_unpack(raw(10), "a:u32", n = 3)
    Condition
      Error in `bin_unpack()`:
      ! Reading to byte 12 needs more than the 10 bytes of `x`.
    Code
      bin_unpack(as.raw(rep(255, 9)), "a:u8 b:u64")
    Condition
      Error in `bin_unpack()`:
      ! The value of record 0 in field `b` has no exact R value.
    Code
      bin_decode(as.raw(rep(255, 8)), "u64")
    Condition
      Error in `bin_decode()`:
      ! The value of record 0 has no exact R value.
    Code
      bin_pack("a:u8 b:u8", a = 1:2, b = c(1, 300))
    Condition
      Error in `bin_pack()`:
      ! Value 1 of field `b` does not fit its type.
    Code
      bin_encode(c(1, 2, -1), "u32")
    Condition
      Error in `bin_encode()`:
      ! Value 2 does not fit its type.
    Code
      bin_pack("a:i32", a = c(1L, NA))
    Condition
      Error in `bin_pack()`:
      ! Value 1 of field `a` is NA or NaN, which has no bytes in its type.
    Code
      bin_unpack(as.raw(c(97, 255)), "s:s1")
    Condition
      Error in `bin_unpack()`:
      ! The string of record 1 in field `s` is not valid UTF-8.
    Code
      bin_reserve(b, 2e+09)
    Condition
      Error in `bin_reserve()`:
      ! Appending 2000000000 bytes would pass the builder's max of 2000000000 bytes.
    Code
      bin_put(b, c(1, 2, NA), type = "u8")
    Condition
      Error in `bin_put()`:
      ! `x[3]` is NA or NaN, which has no bytes.
    Code
      bin_put(b, c("ab", "abc"), type = "s2")
    Condition
      Error in `bin_put()`:
      ! `x[2]` does not fit the type.
    Code
      bin_layout("id:u32 name:s0")
    Condition
      Error in `bin_layout()`:
      ! Malformed layout specification at byte 13: near "u32 name:s0"
    Code
      bin_layout("a:u8 a:u16")
    Condition
      Error in `bin_layout()`:
      ! The name "a" is used by more than one field.
    Code
      bin_layout("a:u8 b:")
    Condition
      Error in `bin_layout()`:
      ! Malformed layout specification at byte 7: near "a:u8 b:"
    Code
      bin_layout("")
    Condition
      Error in `bin_layout()`:
      ! Malformed layout specification at byte 0: at the end
    Code
      bin_layout("a:u8[2] a.1:u8")
    Condition
      Error in `bin_layout()`:
      ! The name "a.1" is also a column of the array field "a".
    Code
      bin_unpack(1:3, "a:u8")
    Condition
      Error in `bin_unpack()`:
      ! `x` must be a raw vector.
    Code
      bin_hexdump(raw(3), offset = 4)
    Condition
      Error in `bin_hexdump()`:
      ! `offset` 4 is past the end of `x`, which has 3 bytes.
    Code
      bin_unserialize(stream[1:20])
    Condition
      Error in `bin_unserialize()`:
      ! The serialisation stream at offset 0 ends before its object does (`x` has 20 bytes).
    Code
      bin_serialize(1:3, small)
    Condition
      Error in `bin_serialize()`:
      ! The serialisation needs 133 bytes, past the builder's max of 10.
    Code
      bin_pack("a:u8 b:u8", a = 1:2, b = 1:3)
    Condition
      Error in `bin_pack()`:
      ! Column `a` has 2 rows, which does not divide 3.
    Code
      bin_pack("a:u8", a = "x")
    Condition
      Error in `bin_pack()`:
      ! Field `a` (u8) cannot be written from character.
    Code
      bin_pack("a:u8[2]", a = 1:3)
    Condition
      Error in `bin_pack()`:
      ! Field `a` is an array of 2: give a matrix of 2 columns, or one record's 2 values.
    Code
      bin_pack("a:u8[2]", a = matrix(1:3, 1))
    Condition
      Error in `bin_pack()`:
      ! Field `a` needs 2 columns, not 3.
    Code
      bin_pack("a:u8 x2147483646", a = integer(2^22 + 1))
    Condition
      Error in `bin_pack()`:
      ! The result is too large for a raw vector.
    Code
      bin_put(b, list(as.raw(1:2), 1:2), type = "b2")
    Condition
      Error in `bin_put()`:
      ! `x[2]` cannot be written as this type.
    Code
      bin_serialize(list(new.env()), b, refhook = function(x) {
        bin_put(b, raw(1))
        "ref"
      })
    Condition
      Error in `bin_serialize()`:
      ! `refhook` changed the builder `b` that `x` was being serialised into.

