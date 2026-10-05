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
