# limit, spec and bin_put() index errors read well

    Code
      bin_reserve(b, 2147483648)
    Condition
      Error in `bin_reserve()`:
      ! Appending 2147483648 bytes would pass the builder's max of 2147483648 bytes.
    Code
      bin_put(b, c(1, 2, NA), type = "u8")
    Condition
      Error in `bin_put()`:
      ! `x[3]` is NA, which has no bytes.
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

# bounds, range, NA and encoding errors read well

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
      ! Value 1 of field `a` is NA, which has no bytes in its type.
    Code
      bin_unpack(as.raw(c(97, 255)), "s:s1")
    Condition
      Error in `bin_unpack()`:
      ! The string of record 1 in field `s` is not valid UTF-8.

