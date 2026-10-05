# Classed conditions (design 13.7). Every condition zubin raises inherits
# `zubin_error`; the class and the fields are the contract, the message may
# be reworded. C never raises below the outermost .Call: it returns a status,
# and R turns it into one of these.
#
# The call is reduced to the bare function name, so that a large raw vector
# passed as an argument is never printed with the error (zucrypt's rule):
# `bin_unpack(x, l)` becomes `bin_unpack()`.

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
