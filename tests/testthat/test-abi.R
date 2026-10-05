# Design 16.2: zubin.so exports R_init_zubin and nothing else.

test_that("the shared object exports exactly R_init_zubin", {
  skip_on_cran()
  skip_on_os("windows")
  # covr links the gcov runtime into the shared object, which exports its own
  # symbols.
  skip_if(nzchar(Sys.getenv("R_COVR")), "coverage build")
  skip_if(!nzchar(Sys.which("nm")), "nm not available")
  path <- getLoadedDLLs()[["zubin"]][["path"]]
  syms <- exported_symbols(path)
  skip_if(is.null(syms))
  expect_identical(syms, "R_init_zubin")
})

test_that("bin_info() reports the zufast version the headers were compiled against", {
  info <- bin_info()
  expect_match(info$zufast, "^[0-9]+\\.[0-9]+\\.[0-9]+$")
  # LinkingTo: zufast (>= 0.1.0); zubin.h #errors below it.
  expect_true(package_version(info$zufast) >= "0.1.0")
  # zufast need not be installed at run time; when it is, it is the copy
  # that was built against.
  installed <- tryCatch(utils::packageVersion("zufast"), error = function(e) NULL)
  skip_if(is.null(installed), "zufast is not installed")
  expect_identical(info$zufast, paste(unlist(installed)[1:3], collapse = "."))
})
