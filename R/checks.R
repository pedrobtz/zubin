# Argument checks shared across the exported functions. Each raises
# zubin_invalid_argument naming the argument, with the caller's call.

is_string <- function(x) {
  is.character(x) && length(x) == 1L && !is.na(x)
}

# A non-negative whole number of bytes, at most 2^53; `message` replaces the
# default for a whole number that is not a size.
check_size <- function(x, arg, message = NULL, call = sys.call(-1L)) {
  if (
    !is.numeric(x) ||
      length(x) != 1L ||
      is.na(x) ||
      x < 0 ||
      x != floor(x) ||
      x > 2^53
  ) {
    if (is.null(message)) {
      message <- sprintf(
        "`%s` must be a whole number of bytes, at least 0.",
        arg
      )
    }
    invalid_argument(
      message,
      arg = arg,
      call = call
    )
  }
}

check_version <- function(version, call = sys.call(-1L)) {
  if (!is.numeric(version) || length(version) != 1L || !version %in% c(2, 3)) {
    invalid_argument("`version` must be 2 or 3.", arg = "version", call = call)
  }
  as.integer(version)
}

check_flag <- function(x, arg, call = sys.call(-1L)) {
  if (!is.logical(x) || length(x) != 1L || is.na(x)) {
    invalid_argument(
      sprintf("`%s` must be TRUE or FALSE.", arg),
      arg = arg,
      call = call
    )
  }
}

check_refhook <- function(refhook, call = sys.call(-1L)) {
  if (!is.null(refhook) && !is.function(refhook)) {
    invalid_argument(
      "`refhook` must be NULL or a function.",
      arg = "refhook",
      call = call
    )
  }
}
