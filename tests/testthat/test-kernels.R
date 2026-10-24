# The unpack kernels of design 11.5, driven directly by the harness.

test_that("kernels read at any stride, n = 0 included", {
  x <- as.raw(0:63)
  for (stride in c(3, 5, 7, 8, 13)) {
    k <- kernel(x, "a:u8 b:u16", stride = stride)
    n <- (64 - 3) %/% stride + 1
    expect_identical(k[[1]]$values, as.integer(seq(0, by = stride, length.out = n)), label = stride)
    expect_identical(k[[2]]$values, as.integer(seq(1, by = stride, length.out = n)) +
                       256L * as.integer(seq(2, by = stride, length.out = n)))
  }
  k <- kernel(x, "a:u8 b:f64 c:i64 d:s3", n = 0)
  expect_identical(k[[1]]$values, integer())
  expect_identical(k[[2]]$values, double())
  expect_identical(k[[3]]$values, double())
  expect_identical(k[[4]]$values, raw())
  expect_true(all(vapply(k, function(f) f$status == "ZB_OK", logical(1))))
})

test_that("array elements are placed column-major, element k of record i at k * n + i", {
  x <- as.raw(c(1, 2, 3, 0xaa, 4, 5, 6, 0xbb))
  k <- kernel(x, "v:u8[3] x1")
  expect_identical(k[[1]]$values, c(1L, 4L, 2L, 5L, 3L, 6L))
  k <- kernel(x, "v:u8[3] x1", stride = 4, n = 1)
  expect_identical(k[[1]]$values, 1:3)
})

test_that("each kernel reports the first record that does not fit", {
  x <- c(le_i32(c(5L, 6L)), bytes("00 00 00 80"), le_i32(7L), bytes("00 00 00 80"))
  k <- kernel(x, "v:i32")[[1]]
  expect_identical(k$status, "ZB_OK")              # allow_na: the bits come through
  expect_identical(k$values, c(5L, 6L, NA, 7L, NA))
  expect_identical(k$status2, "ZB_ERR_RANGE")
  expect_identical(k$bad2, 2)
  big <- bytes("00 00 00 00 00 00 00 80")
  k <- kernel(c(raw(8), big, big), "v:u64")[[1]]
  expect_identical(k$status, "ZB_ERR_RANGE")
  expect_identical(k$bad, 1)
  expect_identical(k$status2, "ZB_ERR_RANGE")   # 2^63 is past 2^53 as well
  expect_identical(k$bad2, 1)
})

test_that("the exact-double kernel refuses past 2^53 and the int64 kernel does not", {
  x <- bytes("01 00 00 00 00 00 20 00")
  k <- kernel(x, "v:i64")[[1]]
  expect_identical(k$status, "ZB_OK")
  expect_identical(k$status2, "ZB_ERR_RANGE")
  expect_identical(k$bad2, 0)
})

test_that("bytes and strings come out contiguous, record after record", {
  x <- as.raw(1:12)
  k <- kernel(x, "x1 a:b2 x1 b:s2", stride = 6)
  expect_identical(k[[2]]$values, as.raw(c(2, 3, 8, 9)))
  expect_identical(k[[4]]$values, as.raw(c(5, 6, 11, 12)))
})

test_that("i128 and u128 read into int64 or an exact double while they fit", {
  ones <- rep(as.raw(0xff), 16)
  five <- c(as.raw(5), raw(15))
  two64 <- c(raw(8), as.raw(1), raw(7))
  k <- kernel(c(five, ones, two64), "v:i128")[[1]]
  expect_identical(k$status, "ZB_ERR_RANGE")   # 2^64 is past int64
  expect_identical(k$bad, 2)
  expect_identical(k$status2, "ZB_ERR_RANGE")
  k <- kernel(c(five, ones), "v:i128")[[1]]
  expect_identical(bits(k$values), c(as.raw(5), raw(7), rep(as.raw(0xff), 8)))
  expect_identical(k$values2, c(5, -1))
  k <- kernel(c(five, ones), "v:u128")[[1]]
  expect_identical(k$status, "ZB_ERR_RANGE")   # all ones is 2^128 - 1
  expect_identical(k$bad, 1)
  k <- kernel(c(raw(15), as.raw(5)), "v:u128.be")[[1]]
  expect_identical(k$values2, 5)
})

test_that("c64 and c128 read as real and imaginary pairs, exactly", {
  x <- c(bytes("00 00 c0 3f"), bytes("00 00 80 be"))   # 1.5, -0.25
  k <- kernel(x, "v:c64")[[1]]
  expect_identical(k$values, c(1.5, -0.25))
  x <- c(writeBin(c(2.5, -3), raw(), size = 8L, endian = "big"))
  expect_identical(kernel(x, "v:c128.be")[[1]]$values, c(2.5, -3))
})

test_that("native unpack keeps each value's width in host order, and packs back", {
  rec <- c(bytes("01 02"), c(raw(14), bytes("01 02")), bytes("3f 80 00 00 40 00 00 00"),
           bytes("aa"), bytes("61 62"))
  k <- kernel(c(rec, rec), "a:u16.be b:u128.be c:c64.be d:bool e:s2")
  expect_identical(k[[1]]$native, rep(host_order(bytes("02 01"), 2L), 2))
  lo <- host_order(c(bytes("02 01"), raw(6)), 8L)
  expect_identical(k[[2]]$native, rep(c(lo, raw(8)), 2))
  expect_identical(k[[3]]$native, rep(host_order(bytes("00 00 80 3f 00 00 00 40"), 4L), 2))
  expect_identical(k[[4]]$native, bytes("aa aa"))
  expect_identical(k[[5]]$native, bytes("61 62 61 62"))
  for (f in k) expect_true(f$roundtrip, label = f$type)
  # an array field is column-major, as every kernel
  k <- kernel(bytes("01 00 02 00 03 00 04 00"), "v:u16[2]")
  expect_identical(k[[1]]$native, host_order(bytes("01 00 03 00 02 00 04 00"), 2L))
  expect_true(k[[1]]$roundtrip)
})

test_that("each public kernel accepts its types and refuses the rest untouched", {
  r <- .Call(zubin_test_kernel_types)
  expect_true(r$untouched)
  st <- r$status
  expect_true(all(st %in% c("ZB_OK", "ZB_ERR_INVALID")))
  small <- c("u8", "i8", "u16", "i16", "i32", "bool")
  floats <- c("u32", "f16", "bf16", "f32", "f64")
  wide <- c("u64", "i64", "u128", "i128")
  every <- setdiff(colnames(st), "x")
  accepts <- list(
    unpack_i32 = small, unpack_f64 = floats, unpack_i64 = wide, unpack_f64x = wide,
    unpack_bytes = c("b", "s"), unpack_cplx = c("c64", "c128"), unpack_native = every,
    pack_i32 = small,
    pack_f64 = c("u8", "i8", "u16", "i16", "i32", floats, wide),
    pack_i64 = wide, pack_bytes = c("b", "s"), pack_cplx = c("c64", "c128"),
    pack_native = every
  )
  expect_setequal(rownames(st), names(accepts))
  for (k in names(accepts)) {
    expect_setequal(colnames(st)[st[k, ] == "ZB_OK"], accepts[[k]])
  }
})
