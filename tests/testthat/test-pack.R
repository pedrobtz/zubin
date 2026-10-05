# bin_pack() and bin_encode(): design 13.1 in the writing direction, 13.3, 14.

test_that("integer types take integer, whole double and (small ones) logical", {
  l <- "<a:u8 b:i8 c:u16 d:i16 e:u32 f:i32"
  expect_bytes(bin_pack(l, a = 255L, b = -128, c = TRUE, d = -1L, e = 4294967295, f = -2L),
               "ff 80 01 00 ff ff ff ff ff ff fe ff ff ff")
  expect_bytes(bin_encode(c(TRUE, FALSE), "u8"), "01 00")
  expect_error(bin_encode(TRUE, "i32"), class = "zubin_invalid_argument")
  expect_error(bin_encode(TRUE, "u32"), class = "zubin_invalid_argument")
})

test_that("a value out of range, a fraction or an infinity is a range error", {
  cases <- list(list(256, "u8"), list(-1, "u8"), list(128L, "i8"), list(65536, "u16"),
                list(-32769, "i16"), list(2^32, "u32"), list(-1, "u32"), list(2^31, "i32"),
                list(1.5, "i32"), list(Inf, "i16"), list(-Inf, "u64"), list(2^64, "u64"),
                list(2^63, "i64"), list(-1, "u64"), list(0.5, "u8"))
  for (c in cases) {
    e <- expect_error(bin_encode(c(0, c[[1]]), c[[2]]), class = "zubin_range_error",
                      label = paste(c[[1]], c[[2]]))
    expect_identical(e$index, 1)
  }
  expect_bytes(bin_encode(-2^63, "i64"), "00 00 00 00 00 00 00 80")
  expect_bytes(bin_encode(2^64 - 2048, "u64"), "00 f8 ff ff ff ff ff ff")
})

test_that("NA has bytes only in i32 (when allowed) and the floating types", {
  for (t in c("u8", "i8", "u16", "i16", "u32", "u64", "i64", "bool")) {
    x <- if (t == "bool") c(TRUE, NA) else c(1, NA)
    e <- expect_error(bin_encode(x, t), class = "zubin_na_error", label = t)
    expect_identical(e$index, 1)
  }
  expect_error(bin_encode(c(1L, NA), "i32"), class = "zubin_na_error")
  expect_error(bin_encode(NA_real_, "i32"), class = "zubin_na_error")
  expect_bytes(bin_encode(c(1L, NA), "i32", na = "allow"), "01 00 00 00 00 00 00 80")
  expect_bytes(bin_encode(NA_real_, "i32", na = "allow"), "00 00 00 80")
  # NA_real_ survives f64 bit for bit; elsewhere it is a NaN
  expect_identical(bin_encode(NA_real_, "f64"), writeBin(NA_real_, raw(), endian = "little"))
  for (t in c("f32", "f16", "bf16")) {
    expect_true(is.nan(bin_decode(bin_encode(NA_real_, t), t)), label = t)
  }
  expect_error(bin_pack("s:s4", s = NA_character_), class = "zubin_na_error")
})

test_that("integer64 writes its exact bits into i64 and u64", {
  v <- rw_read("i64", "le", bytes("ff ff ff ff ff ff ff 7f  01 00 00 00 00 00 20 00"))
  class(v) <- "integer64"
  expect_bytes(bin_encode(v, "i64"), "ff ff ff ff ff ff ff 7f 01 00 00 00 00 00 20 00")
  expect_bytes(bin_encode(v, "u64", endian = "big"), "7f ff ff ff ff ff ff ff 00 20 00 00 00 00 00 01")
  neg <- structure(rw_read("i64", "le", bytes("ff ff ff ff ff ff ff ff")), class = "integer64")
  expect_error(bin_encode(neg, "u64"), class = "zubin_range_error")
  na64 <- structure(rw_read("i64", "le", bytes("00 00 00 00 00 00 00 80")), class = "integer64")
  expect_error(bin_encode(na64, "i64"), class = "zubin_na_error")
  expect_bytes(bin_encode(na64, "i64", na = "allow"), "00 00 00 00 00 00 00 80")
  expect_error(bin_encode(na64, "u64", na = "allow"), class = "zubin_na_error")
  expect_error(bin_encode(v, "u32"), class = "zubin_invalid_argument")
})

test_that("integer64 round-trips through bit64", {
  skip_if_not_installed("bit64")
  v <- bit64::as.integer64(c("9223372036854775807", "-9223372036854775807", "123456789012345678"))
  expect_identical(bin_decode(bin_encode(v, "i64"), "i64", int64 = "integer64"), v)
})

test_that("floating point rounds to nearest even; integers into floats are exact", {
  expect_bytes(bin_encode(c(1, 1 + 2^-11, 1 + 3 * 2^-11), "f16"), "00 3c 00 3c 02 3c")
  expect_bytes(bin_encode(1L, "f32", endian = "big"), "3f 80 00 00")
  expect_bytes(bin_encode(c(65520, 1e10), "f16"), "00 7c 00 7c")
  expect_identical(bin_encode(c(0.1, -0, 1e300), "f64"), writeBin(c(0.1, -0, 1e300), raw(), endian = "little"))
  expect_identical(bin_encode(c(0.1, 1e300), "f32"), writeBin(c(0.1, 1e300), raw(), size = 4L, endian = "little"))
})

test_that("bool takes logical only", {
  expect_bytes(bin_encode(c(TRUE, FALSE, TRUE), "bool"), "01 00 01")
  expect_error(bin_encode(1, "bool"), class = "zubin_invalid_argument")
  expect_error(bin_encode(1L, "bool"), class = "zubin_invalid_argument")
})

test_that("s<n> takes character of at most n UTF-8 bytes, NUL-padded", {
  expect_bytes(bin_encode(c("ab", "", "abcd"), "s4"), "61 62 00 00 00 00 00 00 61 62 63 64")
  expect_bytes(bin_encode("é", "s2"), "c3 a9")
  e <- expect_error(bin_encode(c("ok", "éé"), "s3"), class = "zubin_range_error")
  expect_identical(e$index, 1)
  expect_error(bin_encode(1, "s4"), class = "zubin_invalid_argument")
})

test_that("b<n> takes a list of raw vectors of exactly n bytes", {
  expect_bytes(bin_encode(list(as.raw(1:2), as.raw(3:4)), "b2"), "01 02 03 04")
  e <- expect_error(bin_encode(list(as.raw(1:2), as.raw(1:3)), "b2"), class = "zubin_range_error")
  expect_identical(e$index, 1)
  expect_error(bin_encode(list(as.raw(1:2), 1:2), "b2"), class = "zubin_invalid_argument")
  expect_error(bin_encode(as.raw(1:2), "b2"), class = "zubin_invalid_argument")
})

test_that("characters, lists and the wrong classes are refused for numeric fields", {
  expect_error(bin_encode("1", "u8"), class = "zubin_invalid_argument")
  expect_error(bin_encode(list(1), "u8"), class = "zubin_invalid_argument")
  expect_error(bin_encode(as.raw(1), "u8"), class = "zubin_invalid_argument")
})

test_that("array fields take a matrix, name.k columns, or one record's vector", {
  l <- bin_layout("<id:u8 v:u8[3]")
  expect_bytes(bin_pack(l, id = 1:2, v = matrix(1:6, 2)), "01 01 03 05 02 02 04 06")
  expect_bytes(bin_pack(l, id = 9, v = c(7, 8, 9)), "09 07 08 09")
  expect_bytes(bin_pack(l, data.frame(id = 1:2, v.1 = 1:2, v.2 = 3:4, v.3 = 5:6)),
               "01 01 03 05 02 02 04 06")
  expect_error(bin_pack(l, id = 1, v = 1:2), class = "zubin_invalid_argument")
  expect_error(bin_pack(l, id = 1, v = matrix(1:4, 2)), class = "zubin_invalid_argument")
})

test_that("columns recycle to the longest when their length divides it", {
  l <- bin_layout("<a:u8 b:u8")
  expect_bytes(bin_pack(l, a = 1:4, b = 9), "01 09 02 09 03 09 04 09")
  expect_bytes(bin_pack(l, a = 1:4, b = 8:9), "01 08 02 09 03 08 04 09")
  e <- expect_error(bin_pack(l, a = 1:3, b = 1:2), class = "zubin_invalid_argument")
  expect_identical(e$arg, "b")
  expect_error(bin_pack(l, a = 1:3, b = integer()), class = "zubin_invalid_argument")
  expect_bytes(bin_pack("<id:u8 v:u8[2]", id = 1:2, v = c(5, 6)), "01 05 06 02 05 06")
})

test_that("zero-length columns pack to raw(0)", {
  expect_identical(bin_pack("<a:u8 b:f64 s:s3", a = integer(), b = double(), s = character()), raw(0))
  expect_identical(bin_encode(double(), "f32"), raw(0))
})

test_that("padding and alignment gaps are zeros, and packing is deterministic", {
  l <- bin_layout("a:u8 x3 b:u32 c:u8", align = TRUE)
  x <- bin_pack(l, a = 1:2, b = c(0x01020304, 5), c = 7)
  expect_bytes(x, "01 00 00 00 04 03 02 01 07 00 00 00 02 00 00 00 05 00 00 00 07 00 00 00")
  expect_identical(bin_pack(l, a = 1:2, b = c(0x01020304, 5), c = 7), x)
})

test_that("columns are matched by name; missing, extra and unnamed ones are errors", {
  l <- bin_layout("<a:u8 b:u8")
  expect_identical(bin_pack(l, b = 2, a = 1), bin_pack(l, a = 1, b = 2))
  expect_identical(bin_pack(l, list(b = 2, a = 1)), bin_pack(l, a = 1, b = 2))
  expect_error(bin_pack(l, a = 1), class = "zubin_invalid_argument")
  e <- expect_error(bin_pack(l, a = 1, b = 2, c = 3), class = "zubin_invalid_argument")
  expect_identical(e$arg, "c")
  expect_error(bin_pack(l, 1, 2), class = "zubin_invalid_argument")
  expect_bytes(bin_pack("u8 x1 u8", V1 = 1, V2 = 2), "01 00 02")
})

test_that("the range error names the field and the record", {
  l <- bin_layout("<a:u8 b:u16 c:u8[2]")
  e <- expect_error(bin_pack(l, a = 1:3, b = c(1, 2, 70000), c = matrix(1, 3, 2)),
                    class = "zubin_range_error")
  expect_identical(e$field, "b")
  expect_identical(e$index, 2)
  e <- expect_error(bin_pack(l, a = 1:3, b = 1, c = matrix(c(1, 1, 1, 1, 1, 300), 3, 2)),
                    class = "zubin_range_error")
  expect_identical(e$field, "c")
  expect_identical(e$index, 2)
})

test_that("bin_encode() agrees with writeBin() for every width base R writes", {
  withr::local_seed(11)
  ints <- sample(-1e9:1e9, 100L)
  dbl <- stats::rnorm(100L) * 1e6
  for (e in c("little", "big")) {
    expect_identical(bin_encode(ints %% 256L, "u8"), writeBin(ints %% 256L, raw(), size = 1L))
    expect_identical(bin_encode(ints %% 65536L - 32768L, "i16", endian = e),
                     writeBin(ints %% 65536L - 32768L, raw(), size = 2L, endian = e))
    expect_identical(bin_encode(ints, "i32", endian = e), writeBin(ints, raw(), size = 4L, endian = e))
    expect_identical(bin_encode(dbl, "f32", endian = e), writeBin(dbl, raw(), size = 4L, endian = e))
    expect_identical(bin_encode(dbl, "f64", endian = e), writeBin(dbl, raw(), size = 8L, endian = e))
  }
})

test_that("typed bin_put() appends what bin_encode() returns", {
  withr::local_seed(12)
  for (t in c("u8", "i16", "u32", "i32", "u64", "f16", "bf16", "f32", "f64", "bool")) {
    x <- if (t == "bool") sample(c(TRUE, FALSE), 20, TRUE) else sample(0:200, 20, TRUE)
    for (e in c("little", "big")) {
      b1 <- bin_builder()
      bin_put(b1, x, type = t, endian = e)
      b2 <- bin_builder()
      bin_put(b2, bin_encode(x, t, endian = e))
      expect_identical(bin_take(b1), bin_take(b2), label = paste(t, e))
    }
  }
  b <- bin_builder()
  bin_put(b, list(as.raw(1:2)), type = "b2")
  expect_bytes(bin_take(b), "01 02")
})

test_that("a typed bin_put() that fails appends nothing", {
  b <- bin_builder()
  bin_put(b, bytes("aa"))
  e <- expect_error(bin_put(b, c(1, 2, 300), type = "u8"), class = "zubin_range_error")
  expect_identical(e$index, 2)
  expect_error(bin_put(b, c(1, NA), type = "u16"), class = "zubin_na_error")
  expect_error(bin_put(b, 1, type = "x2"), class = "zubin_invalid_argument")
  expect_error(bin_put(b, 1.5), class = "zubin_invalid_argument")
  expect_identical(as.raw(b), bytes("aa"))
  b <- bin_builder(max = 5)
  expect_error(bin_put(b, 1:3, type = "u16"), class = "zubin_limit_error")
  expect_identical(bin_size(b), 0)
})
