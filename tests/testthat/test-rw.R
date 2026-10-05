# rw.h (design 8): one reader and one writer per type and byte order.

test_that("readers agree with readBin() for every width and order base R has", {
  withr::local_seed(20261005)
  x <- as.raw(sample.int(256L, 4096L, replace = TRUE) - 1L)
  n <- length(x)
  for (e in names(base_endian)) {
    en <- base_endian[[e]]
    expect_identical(rw_read("u8", e, x), readBin(x, "integer", n, size = 1L, signed = FALSE))
    expect_identical(rw_read("i8", e, x), readBin(x, "integer", n, size = 1L, signed = TRUE))
    expect_identical(rw_read("u16", e, x),
                     readBin(x, "integer", n, size = 2L, signed = FALSE, endian = en))
    expect_identical(rw_read("i16", e, x), readBin(x, "integer", n, size = 2L, endian = en))
    # INT_MIN is NA_integer_ in both
    expect_identical(rw_read("i32", e, x), readBin(x, "integer", n, size = 4L, endian = en))
    expect_identical(bits(rw_read("f32", e, x)),
                     bits(readBin(x, "double", n, size = 4L, endian = en)))
    expect_identical(bits(rw_read("f64", e, x)),
                     bits(readBin(x, "double", n, size = 8L, endian = en)))
    # u32 from its two halves, which readBin can read
    halves <- matrix(readBin(x, "integer", n, size = 2L, signed = FALSE, endian = en), 2L)
    u32 <- if (e == "le") halves[1, ] + 65536 * halves[2, ] else halves[2, ] + 65536 * halves[1, ]
    expect_identical(rw_read("u32", e, x), u32)
    # 64-bit integers come back as their bit pattern, which is what reading
    # the same eight bytes as a double in the same order gives
    expect_identical(bits(rw_read("i64", e, x)),
                     bits(readBin(x, "double", n, size = 8L, endian = en)))
    expect_identical(bits(rw_read("u64", e, x)),
                     bits(readBin(x, "double", n, size = 8L, endian = en)))
  }
})

test_that("writers agree with writeBin() for every width and order base R has", {
  withr::local_seed(20261006)
  i32 <- c(NA, -.Machine$integer.max, -1L, 0L, 1L, .Machine$integer.max,
           sample(-.Machine$integer.max:.Machine$integer.max, 200L))
  dbl <- c(0, -0, 1, -1, 0.1, 1 / 3, pi, -2.5e-310, 1e300, -1e-300, Inf, -Inf, NaN, NA,
           3.4028234663852886e38, -3.4028234663852886e38, 1e39,
           stats::rnorm(200L, sd = 1e6))
  for (e in names(base_endian)) {
    en <- base_endian[[e]]
    expect_identical(rw_write("u8", e, 0:255), writeBin(0:255, raw(), size = 1L))
    expect_identical(rw_write("i8", e, -128:127), writeBin(-128:127, raw(), size = 1L))
    expect_identical(rw_write("u16", e, 0:65535), writeBin(0:65535, raw(), size = 2L, endian = en))
    expect_identical(rw_write("i16", e, -32768:32767),
                     writeBin(-32768:32767, raw(), size = 2L, endian = en))
    expect_identical(rw_write("i32", e, i32), writeBin(i32, raw(), size = 4L, endian = en))
    expect_identical(rw_write("f32", e, dbl), writeBin(dbl, raw(), size = 4L, endian = en))
    expect_identical(rw_write("f64", e, dbl), writeBin(dbl, raw(), size = 8L, endian = en))
    u32 <- c(0, 1, 2^31 - 1, 2^31, 2^32 - 1, stats::runif(100L, 0, 2^32 - 1) %/% 1)
    # 2^31 is the bit pattern of INT_MIN, which is NA_integer_
    as_i32 <- rep(NA_integer_, length(u32))
    ok <- u32 != 2^31
    as_i32[ok] <- as.integer(ifelse(u32[ok] > 2^31, u32[ok] - 2^32, u32[ok]))
    expect_identical(rw_write("u32", e, u32), writeBin(as_i32, raw(), size = 4L, endian = en))
  }
})

test_that("every type round-trips its bytes in both orders", {
  withr::local_seed(20261007)
  x <- as.raw(sample.int(256L, 4096L, replace = TRUE) - 1L)
  for (type in rw_types) {
    for (e in c("le", "be")) {
      v <- rw_read(type, e, x)
      back <- rw_write(type, e, v)
      if (type %in% c("f16", "bf16", "f32")) {
        # a NaN is quieted on its way through float; compare the rest
        w <- rw_width[[type]]
        keep <- rep(!is.nan(v), each = w)
        expect_identical(back[keep], x[keep], label = paste(type, e))
        expect_true(all(is.nan(rw_read(type, e, back)[is.nan(v)])))
      } else {
        expect_identical(back, x, label = paste(type, e))
      }
    }
  }
})

test_that("64-bit integers are exact at their extremes", {
  skip_if_not_installed("bit64")
  as64 <- function(x) structure(x, class = "integer64")
  lo <- bytes("01 00 00 00 00 00 00 80")
  hi <- bytes("ff ff ff ff ff ff ff 7f")
  m1 <- bytes("ff ff ff ff ff ff ff ff")
  expect_identical(as.character(as64(rw_read("i64", "le", lo))), "-9223372036854775807")
  # -2^63 is bit64's NA_integer64_: the reader returns the bits faithfully,
  # and what they mean in R is the R glue's decision (design 14.3)
  expect_true(is.na(as64(rw_read("i64", "le", bytes("00 00 00 00 00 00 00 80")))))
  expect_identical(as.character(as64(rw_read("i64", "le", hi))), "9223372036854775807")
  expect_identical(as.character(as64(rw_read("i64", "le", m1))), "-1")
  expect_identical(as.character(as64(rw_read("i64", "be", rev(hi)))), "9223372036854775807")
  # u64 has the same bits: the all-ones pattern is 2^64 - 1, which bit64
  # prints signed
  expect_identical(bits(rw_read("u64", "le", m1)), bits(rw_read("i64", "le", m1)))
  expect_identical(rw_write("i64", "be", rw_read("i64", "le", lo)), rev(lo))
})

test_that("f16 and bf16 decode every one of the 65 536 patterns exactly", {
  skip_heavy()
  u <- 0:65535
  for (e in c("le", "be")) {
    x <- u16_bytes(u, e)
    for (type in c("f16", "bf16")) {
      got <- rw_read(type, e, x)
      ref <- if (type == "f16") ref_f16(u) else ref_bf16(u)
      nan <- is.nan(ref)
      expect_identical(is.nan(got), nan, label = paste(type, e))
      expect_identical(bits(got[!nan]), bits(ref[!nan]), label = paste(type, e))
      # and every non-NaN pattern is written back to itself; a NaN stays NaN
      back <- rw_write(type, e, got)
      keep <- rep(!nan, each = 2L)
      expect_identical(back[keep], x[keep], label = paste(type, e))
      expect_true(all(is.nan(rw_read(type, e, back)[nan])))
    }
  }
})

test_that("f16 and bf16 writers round twice, through float, as documented", {
  # 1 + 2^-11 + 2^-40 is above the midpoint between the halves 1 and
  # 1 + 2^-10, so rounding once gives 1 + 2^-10 (3c01). Through float it is
  # first rounded to the midpoint itself, and the tie goes to even: 1 (3c00).
  expect_identical(rw_write("f16", "le", 1 + 2^-11 + 2^-40), bytes("00 3c"))
  expect_identical(rw_write("f16", "le", 1 + 2^-11 + 2^-20), bytes("01 3c"))
  # The same for bfloat16, whose midpoint above 1 is 1 + 2^-8.
  expect_identical(rw_write("bf16", "be", 1 + 2^-8 + 2^-40), bytes("3f 80"))
  expect_identical(rw_write("bf16", "be", 1 + 2^-8 + 2^-20), bytes("3f 81"))
  # ties to even at the half's own precision
  expect_identical(rw_write("f16", "be", c(1 + 2^-11, 1 + 3 * 2^-11)), bytes("3c 00 3c 02"))
  # overflow and underflow
  expect_identical(rw_write("f16", "le", c(65504, 65519.99, 65520, 2^-25, 2^-25 * 1.5)),
                   bytes("ff 7b ff 7b 00 7c 00 00 01 00"))
})

test_that("doubles beyond the float range narrow by rounding, not by undefined behaviour", {
  flt_max <- 3.4028234663852886e38
  mid <- 2^128 - 2^103   # halfway between FLT_MAX and 2^128
  x <- c(flt_max, flt_max + 2^100, mid - 2^75, mid, 2^128, 1e300)
  expect_identical(rw_write("f32", "le", x),
                   bytes(rep(c("ff ff 7f 7f", "00 00 80 7f"), each = 3L)))
  expect_identical(rw_write("f32", "le", -x),
                   bytes(rep(c("ff ff 7f ff", "00 00 80 ff"), each = 3L)))
  expect_identical(rw_write("f32", "le", x), writeBin(x, raw(), size = 4L))
})
