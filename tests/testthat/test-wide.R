# The 128-bit and complex types (design 11.1, 13.1): u128, i128, c64, c128.

test_that("the new types parse, with their widths and natural alignment", {
  l <- bin_layout("a:u128 b:i128.be c:c64 d:c128[2]")
  expect_identical(l$fields$type, c("u128", "i128", "c64", "c128"))
  expect_identical(l$fields$size, c(16L, 16L, 8L, 32L))
  expect_identical(bin_size(l), 72L)
  expect_identical(bin_layout("a:u8 b:c64", align = TRUE)$fields$offset, c(0L, 4L))
  expect_identical(bin_size(bin_layout("a:u8 b:c64", align = TRUE)), 12L)
  expect_identical(bin_layout("a:u8 b:c128", align = TRUE)$fields$offset, c(0L, 8L))
  expect_identical(bin_layout("a:u8 b:u128", align = TRUE)$fields$offset, c(0L, 16L))
  expect_identical(bin_size(bin_layout("a:u8 b:u128", align = TRUE)), 32L)
  l <- bin_layout("a:i128.be b:c64[2]", endian = "little")
  expect_identical(bin_layout(l$spec), l)
  for (bad in c("u256", "c32", "c16", "u0128", "i1280", "f128")) {
    expect_error(bin_layout(bad), class = "zubin_spec_error", label = bad)
  }
})

test_that("i128 and u128 write whole doubles exactly, and refuse the rest", {
  expect_bytes(bin_encode(c(1, -3), "i128"), paste0("01", strrep("00", 15), "fd", strrep("ff", 15)))
  expect_bytes(bin_encode(2^100, "i128"), paste0(strrep("00", 12), "10 00 00 00"))
  expect_bytes(bin_encode(-2^100, "i128"), paste0(strrep("00", 12), "f0 ff ff ff"))
  expect_bytes(bin_encode(-2^127, "i128"), paste0(strrep("00", 15), "80"))
  expect_bytes(bin_encode(2^127, "u128"), paste0(strrep("00", 15), "80"))
  expect_bytes(bin_encode(5, "u128", endian = "big"), paste0(strrep("00", 15), "05"))
  for (v in list(2^127, 0.5, Inf, -Inf)) {
    expect_error(bin_encode(v, "i128"), class = "zubin_range_error")
  }
  expect_error(bin_encode(2^128, "u128"), class = "zubin_range_error")
  expect_error(bin_encode(-1, "u128"), class = "zubin_range_error")
  expect_error(bin_encode(NA_real_, "i128"), class = "zubin_na_error")
})

test_that("i128 and u128 read as doubles while exact, as integer64 while they fit", {
  x <- c(bin_encode(c(-1, 2^53), "i128"))
  expect_identical(bin_decode(x, "i128"), c(-1, 2^53))
  e <- expect_error(bin_decode(c(x, bin_encode(2^53 + 2, "i128")), "i128"),
                    class = "zubin_range_error")
  expect_identical(e$index, 2)
  y <- bin_decode(bin_encode(c(-1, 2^62), "i128"), "i128", int64 = "integer64")
  expect_s3_class(y, "integer64")
  expect_identical(bits(unclass(y)), c(rep(as.raw(0xff), 8), raw(7), as.raw(0x40)))
  expect_error(bin_decode(bin_encode(2^64, "u128"), "u128", int64 = "integer64"),
               class = "zubin_range_error")
  expect_error(bin_decode(bin_encode(2^100, "u128"), "u128"), class = "zubin_range_error")
})

test_that("integer64 writes into i128 and u128 sign-extended", {
  minus1 <- structure(readBin(rep(as.raw(0xff), 8), "double", size = 8L, endian = "little"),
                      class = "integer64")
  expect_bytes(bin_encode(minus1, "i128"), strrep("ff", 16))
  expect_error(bin_encode(minus1, "u128"), class = "zubin_range_error")
})

test_that("c64 and c128 write complex values, and real ones as complex", {
  z <- complex(real = c(1, 1.5), imaginary = c(2, -0.25))
  expect_bytes(bin_encode(z[1], "c64"), "00 00 80 3f 00 00 00 40")
  expect_bytes(bin_encode(z[1], "c64", endian = "big"), "3f 80 00 00 40 00 00 00")
  expect_identical(bin_decode(bin_encode(z, "c64"), "c64"), z)
  expect_identical(bin_decode(bin_encode(z, "c128", endian = "big"), "c128", endian = "big"), z)
  expect_bytes(bin_encode(1.5, "c64"), "00 00 c0 3f 00 00 00 00")
  expect_bytes(bin_encode(2L, "c64"), "00 00 00 40 00 00 00 00")
  expect_error(bin_encode("1", "c64"), class = "zubin_invalid_argument")
  # NA's payload survives c128, as it survives f64
  back <- bin_decode(bin_encode(NA_complex_, "c128"), "c128")
  expect_identical(bits(c(Re(back), Im(back))), bits(c(Re(NA_complex_), Im(NA_complex_))))
  # c64 rounds each part to nearest even
  expect_identical(bin_decode(bin_encode(complex(real = 0.1), "c64"), "c64"),
                   complex(real = bin_decode(bin_encode(0.1, "f32"), "f32")))
})

test_that("records with the new types unpack and pack, arrays included", {
  rec <- "id:u128 z:c64[2] w:i128.be"
  z <- matrix(complex(real = 1:4, imaginary = -(1:4)), nrow = 2)
  x <- bin_pack(rec, id = c(1, 2^50), z = z, w = c(-5, 7))
  expect_identical(length(x), 2L * 48L)
  df <- bin_unpack(x, rec)
  expect_identical(names(df), c("id", "z.1", "z.2", "w"))
  expect_identical(df$id, c(1, 2^50))
  expect_identical(df$z.2, z[, 2])
  expect_identical(df$w, c(-5, 7))
  l <- bin_unpack(x, rec, as = "list")
  expect_identical(l$z, z)
})

test_that("typed bin_put() appends what bin_encode() returns for the new types", {
  b <- bin_builder()
  bin_put(b, -2, type = "i128", endian = "big")
  bin_put(b, 3+4i, type = "c128")
  expect_identical(bin_take(b), c(bin_encode(-2, "i128", endian = "big"), bin_encode(3+4i, "c128")))
})
