# The round-trip properties of design 16.4.

test_that("random layouts and frames round-trip through pack and unpack", {
  skip_heavy()
  withr::local_seed(20261005)
  for (i in 1:60) {
    l <- random_layout()
    if (length(l) == 0L) next
    cols <- random_columns(l, sample(0:6, 1L))
    expect_roundtrip(l, cols)
  }
})

test_that("bin_encode(bin_decode(b, t), t) is b for every type over random bytes", {
  withr::local_seed(20261006)
  rnd <- function(n) as.raw(sample(0:255, n, TRUE))
  for (e in c("little", "big")) {
    for (t in c("u8", "i8", "u16", "i16", "u32", "f64")) {
      b <- rnd(8 * 64)
      expect_identical(bin_encode(bin_decode(b, t, endian = e), t, endian = e), b, label = t)
    }
    b <- rnd(4 * 64)
    expect_identical(bin_encode(bin_decode(b, "i32", endian = e, na = "allow"), "i32",
                                endian = e, na = "allow"), b)
    for (t in c("i64", "u64")) {
      b <- matrix(rnd(8 * 64), 8)
      b[if (e == "little") 8 else 1, ] <- b[if (e == "little") 8 else 1, ] & as.raw(0x7f)
      b <- as.vector(b)
      v <- bin_decode(b, t, endian = e, int64 = "integer64", na = "allow")
      expect_identical(bin_encode(v, t, endian = e, na = "allow"), b, label = t)
    }
    for (t in c("f16", "bf16", "f32")) {
      w <- if (t == "f32") 4L else 2L
      b <- rnd(w * 64)
      v <- bin_decode(b, t, endian = e)
      keep <- rep(!is.nan(v), each = w)
      expect_identical(bin_encode(v, t, endian = e)[keep], b[keep], label = t)
    }
    b <- as.raw(sample(0:1, 64, TRUE))
    expect_identical(bin_encode(bin_decode(b, "bool"), "bool"), b)
  }
  b <- rnd(30)
  expect_identical(bin_encode(bin_decode(b, "b3"), "b3"), b)
  s <- bytes("61 62 00 00 c3 a9 00 00 61 62 63 64")
  expect_identical(bin_encode(bin_decode(s, "s4"), "s4"), s)
})

test_that("f16 and bf16 round-trip every pattern in both directions", {
  skip_heavy()
  u <- 0:65535
  for (t in c("f16", "bf16")) {
    for (e in c("little", "big")) {
      ee <- if (e == "little") "le" else "be"
      x <- u16_bytes(u, ee)
      v <- bin_decode(x, t, endian = e)
      back <- bin_encode(v, t, endian = e)
      nan <- rep(is.nan(v), each = 2L)
      expect_identical(back[!nan], x[!nan], label = paste(t, e))
      expect_true(all(is.nan(bin_decode(back, t, endian = e)[is.nan(v)])))
      # and every value survives encode then decode
      expect_identical(bits(bin_decode(back, t, endian = e)[!is.nan(v)]), bits(v[!is.nan(v)]))
    }
  }
})
