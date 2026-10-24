test_that("the shared object is loaded with registered routines only", {
  dll <- getLoadedDLLs()[["zubin"]]
  expect_s3_class(dll, "DLLInfo")
  expect_false(dll[["dynamicLookup"]])
  # DLLInfo has carried forceSymbols since R 4.3
  if (!is.null(dll[["forceSymbols"]])) expect_true(dll[["forceSymbols"]])
})

test_that("conditions inherit zubin_error and carry only the function name", {
  f <- function(x) invalid_argument("bad `x`.", arg = "x")
  cnd <- tryCatch(f(as.raw(1:255)), error = identity)
  expect_s3_class(cnd, c("zubin_invalid_argument", "zubin_error", "error"))
  expect_identical(cnd$arg, "x")
  expect_identical(cnd$call, quote(f()))
})

test_that("an anonymous caller leaves no call at all", {
  cnd <- tryCatch((function() invalid_argument("bad."))(), error = identity)
  expect_null(cnd$call)
})

test_that("errors returned from C as statuses carry only the exported function's name", {
  call_of <- function(expr) tryCatch(expr, zubin_error = function(e) e$call)
  big <- raw(1e6)
  expect_identical(call_of(bin_decode(bytes("ff ff ff ff ff ff ff ff"), "u64")), quote(bin_decode()))
  expect_identical(call_of(bin_unpack(big, "a:u8", n = 2e6)), quote(bin_unpack()))
  expect_identical(call_of(bin_unpack(bytes("ff"), "s:s1")), quote(bin_unpack()))
  expect_identical(call_of(bin_encode(c(1, 300), "u8")), quote(bin_encode()))
  expect_identical(call_of(bin_pack("a:u8", a = c(1, 300))), quote(bin_pack()))
  expect_identical(call_of(bin_layout("a:u8 a:u8")), quote(bin_layout()))
  expect_identical(call_of(bin_layout("a:u7")), quote(bin_layout()))
  b <- bin_builder(max = 2)
  expect_identical(call_of(bin_put(b, big)), quote(bin_put()))
  expect_identical(call_of(bin_put(b, 300, type = "u8")), quote(bin_put()))
  expect_identical(call_of(bin_put(b, "abc", type = "s2")), quote(bin_put()))
  expect_identical(call_of(bin_reserve(b, 10)), quote(bin_reserve()))
  expect_identical(call_of(bin_serialize(big, b)), quote(bin_serialize()))
  expect_identical(call_of(bin_unserialize(serialize(1, NULL)[1:20])), quote(bin_unserialize()))
  expect_identical(call_of(bin_hexdump(big, offset = 2e6)), quote(bin_hexdump()))
})
