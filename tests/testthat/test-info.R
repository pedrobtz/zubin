test_that("bin_info() reports the header version from compiled code", {
  info <- bin_info()
  expect_type(info, "list")
  expect_named(info, c("version", "version_major", "version_minor", "version_patch",
                       "zufast", "endian", "compiler", "build"))
  expect_identical(
    info$version,
    paste(info$version_major, info$version_minor, info$version_patch, sep = ".")
  )
  expect_type(info$compiler, "character")
  expect_named(info$build, c("c_standard", "optimized", "ndebug", "fortify_source"))
  expect_true(info$build[["optimized"]] %in% c("true", "false"))
})

test_that("the header version matches DESCRIPTION", {
  desc <- unlist(utils::packageVersion("zubin"))
  info <- bin_info()
  expect_identical(
    c(info$version_major, info$version_minor, info$version_patch),
    as.integer(desc[1:3])
  )
})

test_that("the host byte order agrees with R's", {
  expect_identical(bin_info()$endian, .Platform$endian)
})

test_that("every status has a name, and every error status a condition class", {
  names <- .Call(zubin_test_status_string, 0:8)
  expect_identical(names, c("ZB_OK", "ZB_ERR_INVALID", "ZB_ERR_EOF", "ZB_ERR_RANGE",
                            "ZB_ERR_MEMORY", "ZB_ERR_LIMIT", "ZB_ERR_SPEC", "ZB_ERR_NA",
                            "ZB_ERR_UNKNOWN"))
  expect_setequal(names(zb_status_class), names[2:8])
  expect_true(all(startsWith(zb_status_class, "zubin_")))
})
