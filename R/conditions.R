# Classed conditions (design 13.7). Every condition zubin raises inherits
# `zubin_error`; the class and the fields are the contract, the message may
# be reworded. C never raises below the outermost .Call: it returns a status,
# and R turns it into one of these.
#
# The call is reduced to the bare function name, so that a large raw vector
# passed as an argument is never printed with the error (zucrypt's rule):
# `bin_unpack(x, l)` becomes `bin_unpack()`.

#' Conditions raised by zubin
#'
#' Every error zubin raises inherits `zubin_error`, and its class says what
#' went wrong, so code catches it by kind and reads the details from its
#' fields; messages may be reworded. Every offset and index is 0-based. The
#' call is reduced to the function's name, so that a large raw vector is
#' never printed with an error.
#'
#' \describe{
#'   \item{`zubin_invalid_argument`}{An argument is unusable: the wrong type,
#'     a negative or fractional size, an unknown type token, a column with no
#'     field, or a builder that was saved and restored. Field: `arg`, the
#'     argument's name, when there is one.}
#'   \item{`zubin_spec_error`}{A layout specification is malformed. Field:
#'     `position`, the byte in the specification where the problem is.}
#'   \item{`zubin_bounds_error`}{The records asked for run past the end of
#'     the input. Fields: `offset` and `length`.}
#'   \item{`zubin_range_error`}{A value does not fit: 300 into `u8`, 1.5 into
#'     an integer type, an `i64` above 2^53 read as a double, a string longer
#'     than its field. Fields: `field` (`NA` for a codec) and `index`, the
#'     record of the first such value.}
#'   \item{`zubin_na_error`}{An `NA` has no bytes in its field. Fields:
#'     `field` and `index`.}
#'   \item{`zubin_encoding_error`}{An `s<n>` field is not valid UTF-8 (read
#'     it with `encoding = "latin1"` or `"bytes"`). Fields: `field` and
#'     `index`.}
#'   \item{`zubin_limit_error`}{An append would pass a builder's `max`, which
#'     leaves it unchanged. Fields: `size`, what the builder would have held,
#'     and `max`.}
#'   \item{`zubin_memory_error`}{An allocation failed, or a size would not
#'     fit in memory.}
#' }
#'
#' @name zubin-conditions
#' @examples
#' tryCatch(
#'   bin_encode(c(1, 300), "u8"),
#'   zubin_range_error = function(e) e$index
#' )
#' tryCatch(
#'   bin_layout("id:u32 name:s0"),
#'   zubin_spec_error = function(e) e$position
#' )
NULL

redact_call <- function(call) {
  if (!is.call(call)) {
    return(NULL)
  }
  fn <- call[[1L]]
  named <- is.symbol(fn) ||
    (is.call(fn) && length(fn) == 3L &&
       (identical(fn[[1L]], as.name("::")) ||
          identical(fn[[1L]], as.name(":::"))) &&
       is.symbol(fn[[2L]]) && is.symbol(fn[[3L]]))
  if (named) as.call(list(fn)) else NULL
}

zubin_abort <- function(message, class = character(), ..., call = sys.call(-1L)) {
  cnd <- structure(
    list(message = message, call = redact_call(call), ...),
    class = c(class, "zubin_error", "error", "condition")
  )
  stop(cnd)
}

invalid_argument <- function(message, ..., call = sys.call(-1L)) {
  zubin_abort(message, "zubin_invalid_argument", ..., call = call)
}

# zb_status enumerator names (zb_status_string() in C) to condition classes.
# Keyed by name, never by number, so a renumbered enum cannot silently remap
# a class. ZB_OK is absent: success never reaches zubin_abort().
zb_status_class <- c(
  ZB_ERR_INVALID = "zubin_invalid_argument",
  ZB_ERR_EOF     = "zubin_bounds_error",
  ZB_ERR_RANGE   = "zubin_range_error",
  ZB_ERR_MEMORY  = "zubin_memory_error",
  ZB_ERR_LIMIT   = "zubin_limit_error",
  ZB_ERR_SPEC    = "zubin_spec_error",
  ZB_ERR_NA      = "zubin_na_error"
)

# Raise the condition for a status returned by a .Call entry point. `res` is
# the classed zubin_status string; `...` carries the condition's fields.
zb_fail <- function(res, message, ..., call = sys.call(-1L)) {
  class <- zb_status_class[[as.character(res)]]
  zubin_abort(message, class, ..., call = call)
}

is_status <- function(x) inherits(x, "zubin_status")
