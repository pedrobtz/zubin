# bin_unpack() and bin_decode(): design 13.1 in the reading direction, 13.3, 14.

test_that("integer types read as integer, u32 as double, bool as logical", {
  x <- bytes("ff 80 ff ff 00 80 ff ff ff ff ff ff ff ff 01")
  r <- bin_unpack(x, "<a:u8 b:i8 c:u16 d:i16 e:u32 f:i32 g:bool", as = "list")
  expect_identical(r, list(a = 255L, b = -128L, c = 65535L, d = -32768L,
                           e = 4294967295, f = -1L, g = TRUE))
})

test_that("an i32 of -2^31 is a range error unless na = 'allow'", {
  x <- c(le_i32(1:2), bytes("00 00 00 80"), le_i32(4L))
  e <- expect_error(bin_unpack(x, "v:i32"), class = "zubin_range_error")
  expect_identical(e$field, "v")
  expect_identical(e$index, 2)
  expect_identical(bin_unpack(x, "v:i32", na = "allow")$v, c(1L, 2L, NA, 4L))
  expect_error(bin_decode(x, "i32"), class = "zubin_range_error")
  expect_identical(bin_decode(x, "i32", na = "allow"), c(1L, 2L, NA, 4L))
  # the other integer types have no NA to collide with
  expect_identical(bin_decode(bytes("00 80"), "i16"), -32768L)
})

test_that("64-bit integers read exactly as double, or are a range error", {
  ok <- bytes("00 00 00 00 00 00 20 00  00 00 00 00 00 00 e0 ff")   # 2^53, -2^53
  expect_identical(bin_decode(ok, "i64"), c(2^53, -2^53))
  over <- bytes("01 00 00 00 00 00 20 00")                            # 2^53 + 1
  e <- expect_error(bin_decode(c(ok, over), "i64"), class = "zubin_range_error")
  expect_identical(e$index, 2)
  expect_identical(bin_decode(ok[1:8], "u64"), 2^53)
  expect_error(bin_decode(over, "u64"), class = "zubin_range_error")
  under <- bytes("ff ff ff ff ff ff df ff")                           # -2^53 - 1
  expect_error(bin_decode(under, "i64"), class = "zubin_range_error")
})

test_that("64-bit integers read as integer64 carry the exact bits", {
  x <- bytes("ff ff ff ff ff ff ff 7f  01 00 00 00 00 00 20 00  ff ff ff ff ff ff ff ff")
  v <- bin_decode(x, "i64", int64 = "integer64")
  expect_s3_class(v, "integer64")
  expect_identical(unclass(v), rw_read("i64", "le", x))
  skip_if_not_installed("bit64")
  expect_identical(as.character(v), c("9223372036854775807", "9007199254740993", "-1"))
})

test_that("integer64 mode refuses u64 from 2^63 and i64 at -2^63 unless allowed", {
  big <- bytes("00 00 00 00 00 00 00 80")
  e <- expect_error(bin_decode(c(bytes("01 00 00 00 00 00 00 00"), big), "u64", int64 = "integer64"),
                    class = "zubin_range_error")
  expect_identical(e$index, 1)
  expect_error(bin_decode(big, "u64", int64 = "integer64", na = "allow"), class = "zubin_range_error")
  expect_error(bin_decode(big, "i64", int64 = "integer64"), class = "zubin_range_error")
  v <- bin_decode(big, "i64", int64 = "integer64", na = "allow")
  expect_identical(unclass(v), rw_read("i64", "le", big))
  # an array field reports the lowest record holding the value
  x <- c(bytes("01 00 00 00 00 00 00 00"), big, big, bytes("01 00 00 00 00 00 00 00"))
  e <- expect_error(bin_unpack(x, "a:i64[2]", int64 = "integer64"), class = "zubin_range_error")
  expect_identical(e$index, 0)
})

test_that("floating point reads exactly, NA_real_ and NaN payloads included", {
  x <- le_f64(c(NA, NaN, -0, 1e-310))
  v <- bin_decode(x, "f64")
  expect_identical(bits(v), bits(c(NA, NaN, -0, 1e-310)))
  expect_identical(bin_decode(bytes("00 3c 00 c0"), "f16"), c(1, -2))
  expect_identical(bin_decode(bytes("3f 80 c0 40"), "bf16", endian = "big"), c(1, -3))
  expect_identical(bin_decode(writeBin(c(1.5, -0.25), raw(), size = 4L, endian = "big"), "f32",
                              endian = "big"), c(1.5, -0.25))
})

test_that("b<n> reads a list of raw vectors; x<n> is not returned", {
  r <- bin_unpack(bytes("01 02 aa aa 03 04 bb bb"), "v:b2 x2", as = "list")
  expect_identical(r, list(v = list(bytes("01 02"), bytes("03 04"))))
  df <- bin_unpack(bytes("01 02 aa aa 03 04 bb bb"), "v:b2 x2")
  expect_s3_class(df$v, "AsIs")
  expect_identical(unclass(df$v), list(bytes("01 02"), bytes("03 04")))
  expect_identical(bin_decode(bytes("01 02 03 04"), "b2"), list(bytes("01 02"), bytes("03 04")))
})

test_that("s<n> stops at the first NUL, at every position of the field", {
  for (p in 0:8) {
    raw8 <- bytes("41 42 43 44 45 46 47 48")
    if (p < 8) raw8[p + 1L] <- as.raw(0)
    expect_identical(bin_decode(raw8, "s8"), substr("ABCDEFGH", 1L, p), label = p)
  }
  expect_identical(bin_decode(bytes("41 00 42 00"), "s4"), "A")
})

test_that("s<n> validates UTF-8, or marks latin1 or bytes", {
  utf8 <- bytes("ce ba e1 bd b9 cf 83 ce bc ce b5 00")   # Markus Kuhn's "kosme", NUL-padded
  v <- bin_decode(utf8, "s12")
  expect_identical(Encoding(v), "UTF-8")
  expect_identical(charToRaw(v), utf8[1:11])
  bad <- bytes("41 c3 28 00")
  e <- expect_error(bin_decode(c(bytes("41 42 43 00"), bad), "s4"), class = "zubin_encoding_error")
  expect_identical(e$index, 1)
  expect_true(is.na(e$field))
  l1 <- bin_unpack(bad, "s:s4", encoding = "latin1")$s
  expect_identical(Encoding(l1), "latin1")
  expect_identical(charToRaw(l1), bad[1:3])
  b <- bin_unpack(bad, "s:s4", encoding = "bytes")$s
  expect_identical(Encoding(b), "bytes")
  expect_identical(charToRaw(b), bad[1:3])
})

test_that("Markus Kuhn's UTF-8 stress cases are accepted or refused as UTF-8 requires", {
  valid <- c("00", "7f", "c2 80", "df bf", "e0 a0 80", "ef bf bd", "ef bf bf",
             "f0 90 80 80", "f4 8f bf bf", "ee 80 80", "ef bf bd")
  invalid <- c("80", "bf", "80 bf", "c0 20", "e0 20", "f0 20", "fe", "ff", "fe fe ff ff",
               "c0 af", "e0 80 af", "f0 80 80 af", "c1 bf", "e0 9f bf",
               "ed a0 80", "ed bf bf", "ed a0 80 ed b0 80", "f4 90 80 80",
               "c2", "e0 a0", "f0 90 80", "df", "ef bf")
  for (h in valid) {
    raw <- c(bytes(h), raw(6 - length(bytes(h))))
    v <- bin_decode(raw, "s6")
    expect_identical(charToRaw(v), bytes(h)[bytes(h) != as.raw(0)], label = h)
  }
  for (h in invalid) {
    raw <- c(bytes(h), raw(10 - length(bytes(h))))
    expect_error(bin_decode(raw, "s10"), class = "zubin_encoding_error", label = h)
    expect_type(bin_unpack(raw, "s:s10", encoding = "latin1")$s, "character")
  }
})

test_that("array fields are matrices in list mode and name.k columns in a data frame", {
  x <- bytes("01 0a 0b 0c 02 14 15 16")
  r <- bin_unpack(x, "id:u8 v:u8[3]", as = "list")
  expect_identical(r$v, matrix(c(10L, 20L, 11L, 21L, 12L, 22L), 2L))
  df <- bin_unpack(x, "id:u8 v:u8[3]")
  expect_named(df, c("id", "v.1", "v.2", "v.3"))
  expect_identical(df$v.3, c(12L, 22L))
  r <- bin_unpack(bytes("01 00 ff"), "f:bool[3]", as = "list")
  expect_identical(r$f, matrix(c(TRUE, FALSE, TRUE), 1L))
})

test_that("offset, n and stride select records; trailing bytes are left alone", {
  x <- as.raw(0:20)
  expect_identical(bin_decode(x, "u8", offset = 18), 18:20)
  expect_identical(bin_unpack(x, "v:u16", offset = 1)$v,
                   bin_decode(x[-1], "u16"))
  expect_length(bin_decode(x, "u32"), 5L)                       # 21 bytes: 5 whole records
  expect_identical(bin_unpack(x, "v:u8", stride = 4)$v, c(0L, 4L, 8L, 12L, 16L, 20L))
  expect_identical(bin_unpack(x, "a:u8 b:u8", stride = 10)$b, c(1L, 11L))
  expect_identical(bin_unpack(x, "v:u8", offset = 3, n = 2, stride = 5)$v, c(3L, 8L))
  expect_identical(bin_unpack(x, "v:u8", offset = 21)$v, integer())
  expect_identical(bin_unpack(x, "v:u8", n = 0)$v, integer())
})

test_that("an explicit n past the end is a bounds error with offset and length", {
  x <- as.raw(0:9)
  e <- expect_error(bin_decode(x, "u32", n = 3), class = "zubin_bounds_error")
  expect_identical(e$offset, 0)
  expect_identical(e$length, 10)
  e <- expect_error(bin_decode(x, "u8", offset = 11), class = "zubin_bounds_error")
  expect_identical(e$offset, 11)
  # the last record must end inside x, wherever the stride puts it
  expect_identical(bin_unpack(x, "v:u8", offset = 2, n = 2, stride = 7)$v, c(2L, 9L))
  expect_error(bin_unpack(x, "v:u8", offset = 2, n = 2, stride = 8), class = "zubin_bounds_error")
  expect_identical(bin_unpack(x, "v:u16", offset = 0, n = 5)$v, bin_decode(x, "u16"))
  expect_error(bin_unpack(x, "v:u16", offset = 1, n = 5), class = "zubin_bounds_error")
})

test_that("empty input gives zero rows of the right types", {
  df <- bin_unpack(raw(0), "<a:u8 b:f64 c:s4 d:b2 e:u16[2] f:bool")
  expect_identical(nrow(df), 0L)
  expect_named(df, c("a", "b", "c", "d", "e.1", "e.2", "f"))
  expect_identical(df$a, integer())
  expect_identical(df$b, double())
  expect_identical(df$c, character())
  expect_identical(unclass(df$d), list())
  expect_identical(df$f, logical())
  expect_identical(bin_decode(raw(0), "u32"), double())
  r <- bin_unpack(raw(0), "e:u16[2]", as = "list")
  expect_identical(dim(r$e), c(0L, 2L))
})

test_that("unpack and decode validate their arguments", {
  x <- as.raw(1:8)
  expect_error(bin_unpack(1:8, "u8"), class = "zubin_invalid_argument")
  expect_error(bin_unpack(x, "u8", offset = -1), class = "zubin_invalid_argument")
  expect_error(bin_unpack(x, "u8", n = 1.5), class = "zubin_invalid_argument")
  expect_error(bin_unpack(x, "u16", stride = 1), class = "zubin_invalid_argument")
  expect_error(bin_unpack(x, "u8", as = "matrix"))
  expect_error(bin_unpack(x, "u8 u7"), class = "zubin_spec_error")
  expect_error(bin_decode(x, "x2"), class = "zubin_invalid_argument")
  expect_error(bin_decode(x, "u8[2]"), class = "zubin_invalid_argument")
  expect_error(bin_decode(x, "a:u8"), class = "zubin_invalid_argument")
  expect_error(bin_decode(x, "u7"), class = "zubin_invalid_argument")
  expect_error(bin_decode(x, c("u8", "u8")), class = "zubin_invalid_argument")
})

test_that("a layout object, a spec string and bin_decode() agree", {
  withr::local_seed(42)
  x <- as.raw(sample.int(256L, 64L, replace = TRUE) - 1L)
  for (t in c("u8", "i8", "u16", "i16", "u32", "f32", "f64", "bool", "f16", "bf16")) {
    for (e in c("little", "big")) {
      l <- bin_layout(paste0("v:", t), endian = e)
      expect_identical(bin_unpack(x, l, as = "list")$v, bin_decode(x, t, endian = e), label = t)
    }
  }
})

test_that("a raw vector above 2^31 bytes unpacks", {
  skip_on_cran()
  skip_if_no_slow_tests()
  x <- raw(2^31 + 16)
  x[2^31 + 1:16] <- as.raw(1:16)
  v <- bin_decode(x, "u8", offset = 2^31)
  expect_identical(v, 1:16)
  expect_identical(bin_decode(x, "u32", offset = 2^31 + 12), 0x100f0e0d)
  expect_identical(bin_unpack(x, "a:u32", offset = 2^31 - 4, as = "list")$a,
                   c(0, 0x04030201, 0x08070605, 0x0c0b0a09, 0x100f0e0d))
  rm(x)
  gc()
})

test_that("an interrupted unpack leaves nothing behind, and the input reads in full after", {
  skip_on_cran()
  skip_heavy()
  # 64 string fields over 200 000 records: one interrupt check per field
  spec <- paste(sprintf("f%d:s4", 1:64), collapse = " ")
  x <- rep_len(bytes("41 42 43 44"), 64 * 4 * 2e5)
  cut_short <- FALSE
  for (limit in c(0.02, 0.05, 0.2)) {
    if (interrupted_by_time_limit(function() bin_unpack(x, spec, as = "list"), limit)) {
      cut_short <- TRUE
      break
    }
  }
  expect_true(cut_short)
  r <- bin_unpack(x, spec, as = "list")
  expect_length(r, 64L)
  expect_identical(r$f64[2e5], "ABCD")
})
