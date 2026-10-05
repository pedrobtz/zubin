#' Show bytes as a hex dump, and find where two raw vectors differ
#'
#' `bin_hexdump()` formats `n` bytes of `x` from the 0-based byte `offset`
#' the way `xxd` does: each line is the offset of its first byte in hex, the
#' bytes in hex in groups of two, and the bytes as ASCII, with `.` for
#' anything not printable. Offsets are 0-based and absolute, so a dump from
#' `offset = 64` starts at `00000040`.
#'
#' `bin_diff()` lists the first `n` positions, 0-based, at which `a` and `b`
#' hold different bytes, comparing over the shorter length; the lengths
#' themselves are attributes, so a prefix shows as no rows and two
#' different lengths.
#'
#' Both are presentation: they take any raw vector, and `n` past the end of
#' `x` shows what there is.
#'
#' @param x,a,b Raw vectors.
#' @param offset The 0-based position of the first byte to show; at most
#'   `length(x)`, or the call is a `zubin_bounds_error`.
#' @param n For `bin_hexdump()`, the number of bytes to show, or `NULL` for
#'   the rest; for `bin_diff()`, the most differences to list.
#' @param width Bytes per line, 1 to 256.
#' @return `bin_hexdump()` returns a character vector of class
#'   `zubin_hexdump`, one line per element, which prints as the dump.
#'   `bin_diff()` returns a data frame with columns `offset` (double), `a` and
#'   `b` (raw), and attributes `length_a` and `length_b`.
#' @export
#' @examples
#' x <- charToRaw("Structured binary data, one layout at a time.")
#' bin_hexdump(x)
#' bin_hexdump(x, offset = 16, n = 16, width = 8)
#'
#' y <- x
#' y[c(3, 20)] <- as.raw(0)
#' bin_diff(x, y)
bin_hexdump <- function(x, offset = 0, n = NULL, width = 16L) {
  if (!is.raw(x)) invalid_argument("`x` must be a raw vector.", arg = "x")
  check_size(offset, "offset")
  if (!is.null(n)) check_size(n, "n")
  check_size(width, "width")
  if (width < 1 || width > 256) invalid_argument("`width` must be between 1 and 256.", arg = "width")
  res <- .Call(zubin_hexdump, x, as.double(offset), if (is.null(n)) -1 else as.double(n),
               as.double(width))
  if (is_status(res)) {
    zubin_abort(sprintf("`offset` %s is past the end of `x`, which has %s bytes.",
                        format(offset, scientific = FALSE), length(x)),
                "zubin_bounds_error", offset = as.double(offset), length = as.double(length(x)))
  }
  class(res) <- "zubin_hexdump"
  res
}

#' @export
print.zubin_hexdump <- function(x, ...) {
  if (length(x)) cat(unclass(x), sep = "\n") else cat("<zubin_hexdump: no bytes>\n")
  invisible(x)
}

#' @export
format.zubin_hexdump <- function(x, ...) {
  unclass(x)
}

#' @rdname bin_hexdump
#' @export
bin_diff <- function(a, b, n = 10L) {
  if (!is.raw(a)) invalid_argument("`a` must be a raw vector.", arg = "a")
  if (!is.raw(b)) invalid_argument("`b` must be a raw vector.", arg = "b")
  check_size(n, "n")
  res <- .Call(zubin_diff, a, b, as.double(n))
  structure(res, row.names = .set_row_names(length(res$offset)), class = "data.frame",
            length_a = as.double(length(a)), length_b = as.double(length(b)))
}
