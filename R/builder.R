#' A growable byte buffer
#'
#' `bin_builder()` creates a buffer that bytes are appended to with
#' [bin_put()] and taken out of with [bin_take()]. It grows by doubling (by
#' half once past 64 MiB), so a million appends cost a few dozen
#' reallocations, not a million copies; and it never grows past `max`.
#'
#' A builder is an external pointer: it is not copied on modification, every
#' function below changes it in place, and it does not survive
#' [saveRDS()] or a restored session; using a restored builder is a
#' `zubin_invalid_argument` error. Its memory is freed when it is garbage
#' collected.
#'
#' `bin_take(b)` returns the bytes and empties the builder, which keeps its
#' capacity and accepts new appends; `as.raw(b)` returns a copy and leaves the
#' builder as it is. `bin_reserve(b, n)` makes room for `n` more bytes now,
#' `bin_reset(b)` empties the builder without freeing its storage, and
#' `bin_size(b)` is the number of bytes it holds.
#'
#' @param reserve Bytes to allocate at once: a whole number, at least 0.
#' @param max The hard cap on the builder's size, in bytes: a positive whole
#'   number, or `Inf` for none. An append that would pass it is a
#'   `zubin_limit_error` and leaves the builder unchanged.
#' @param b,x A builder.
#' @param n Bytes to make room for.
#' @return `bin_builder()` returns a `zubin_builder`. `bin_take()` and
#'   `as.raw()` return a raw vector; `bin_size()` returns a double;
#'   `bin_reserve()` and `bin_reset()` return `b`, invisibly.
#' @seealso [bin_put()] to append.
#' @export
#' @examples
#' b <- bin_builder()
#' bin_put(b, as.raw(c(0x89, 0x50, 0x4e, 0x47)))
#' bin_put(b, "IHDR", type = "s4")
#' bin_size(b)
#' bin_take(b)
#' bin_size(b)
bin_builder <- function(reserve = 0, max = Inf) {
  check_size(reserve, "reserve")
  if (!identical(max, Inf)) {
    check_size(max, "max")
    if (max < 1) invalid_argument("`max` must be at least 1, or Inf.", arg = "max")
  }
  ptr <- .Call(zubin_builder_new, as.double(reserve), as.double(max))
  if (is_status(ptr)) {
    if (identical(as.character(ptr), "ZB_ERR_LIMIT")) {
      zb_fail(ptr, "`reserve` is larger than `max`.", size = reserve, max = max)
    }
    zb_fail(ptr, "Could not allocate the builder's storage.")
  }
  class(ptr) <- "zubin_builder"
  ptr
}

#' Append bytes or strings to a builder
#'
#' Appends `x` to the end of the builder `b`. A raw vector is appended as it
#' is (`type = NULL`). Any other vector needs a `type`, so that a double is
#' never silently eight bytes: a numeric, logical or `integer64` vector is
#' encoded as [bin_encode()] would encode it (`"u32"`, `"f64"`, `"bool"`, ...),
#' in `endian` order, written straight into the builder; a list of raw
#' vectors as `"b<n>"`. A character vector is appended with `type = "z"`,
#' each string's UTF-8 bytes followed by a NUL, or with `type = "s<n>"`, each
#' string's UTF-8 bytes padded with NULs to exactly `n` bytes, the
#' fixed-string field of [bin_layout()]. Nothing is appended unless every
#' element can be: an `NA` is a `zubin_na_error`, and a value that does not
#' fit its type is a `zubin_range_error`, each carrying the 0-based `index`
#' of the first one.
#'
#' Appending past the builder's `max` is a `zubin_limit_error` carrying
#' `size`, the size the builder would have reached, and `max`; the builder is
#' unchanged. Long appends check for an interrupt every 64 MiB, and an
#' interrupted append leaves the builder unchanged too.
#'
#' @param b A builder from [bin_builder()].
#' @param x A raw vector; or a vector to encode as `type`.
#' @param type `NULL` for raw input; one type token of [bin_layout()]
#'   otherwise; or `"z"` for NUL-terminated strings.
#' @param endian Byte order of a typed append: `"little"`, `"big"` or
#'   `"native"`.
#' @return `b`, invisibly, so appends can be chained.
#' @export
#' @examples
#' b <- bin_builder()
#' bin_put(b, c("RIFF", "WAVE"), type = "s4")
#' bin_put(b, c(1, 65535), type = "u16", endian = "big")
#' bin_put(b, "a C string", type = "z")
#' bin_take(b)
bin_put <- function(b, x, type = NULL, endian = c("little", "big", "native")) {
  # the common case, appending raw bytes, first and with the fewest checks:
  # C re-checks the builder and the type
  if (is.null(type) && is.raw(x)) {
    res <- .Call(zubin_builder_put_raw, b, x)
    if (!is.null(res)) builder_fail(res, check_builder(b), length(x), x)
    return(invisible(b))
  }
  ptr <- check_builder(b)
  endian <- match.arg(endian)
  if (is.null(type)) {
    invalid_argument("`x` must be a raw vector when `type` is NULL.", arg = "x")
  } else {
    if (!is.character(type) || length(type) != 1L || is.na(type)) {
      invalid_argument("`type` must be NULL or a single type token.", arg = "type")
    }
    width <- string_width(type)
    if (is.null(width)) {
      # a numeric, bool or b<n> token: the pack kernels write it in place
      layout <- type_layout(type, endian)
      col <- pack_value(x, layout$fields$type, 1L, "x", sys.call())
      is64 <- inherits(col, "integer64")
      res <- .Call(zubin_builder_put_typed, ptr, layout$spec, col, is64)
      n <- length(col) * bin_size(layout)
    } else {
      if (!is.character(x)) {
        invalid_argument(sprintf("`x` must be character for type \"%s\".", type), arg = "x")
      }
      res <- .Call(zubin_builder_put_str, ptr, x, width)
      n <- if (width < 0L) sum(nchar(enc2utf8(x[!is.na(x)]), type = "bytes") + 1) else length(x) * width
    }
  }
  if (is_status(res)) builder_fail(res, b, n, x)
  invisible(b)
}

# "z" is -1; "s<n>" is n; anything else is NULL. No regular expression: this
# runs on every typed bin_put(), and the allocation-failure sweep found R's
# regex engine segfaulting when an allocation inside grepl() fails.
string_width <- function(type) {
  if (identical(type, "z")) {
    return(-1L)
  }
  if (startsWith(type, "s")) {
    digits <- substring(type, 2L)
    w <- suppressWarnings(as.numeric(digits))
    if (!is.na(w) && w >= 1 && w <= .Machine$integer.max && w == floor(w) &&
          identical(digits, format(w, scientific = FALSE))) {
      return(as.integer(w))
    }
  }
  NULL
}

#' @rdname bin_builder
#' @export
bin_reserve <- function(b, n) {
  ptr <- check_builder(b)
  check_size(n, "n")
  res <- .Call(zubin_builder_reserve, ptr, as.double(n))
  if (is_status(res)) builder_fail(res, b, n)
  invisible(b)
}

#' @rdname bin_builder
#' @export
bin_reset <- function(b) {
  ptr <- check_builder(b)
  if (is_status(.Call(zubin_builder_reset, ptr))) builder_state(b)
  invisible(b)
}

#' @rdname bin_builder
#' @export
bin_take <- function(b) {
  ptr <- check_builder(b)
  res <- .Call(zubin_builder_take, ptr, TRUE)
  if (is_status(res)) builder_state(b)
  res
}

#' The size of a builder or a layout, in bytes
#'
#' For a builder, the number of bytes it holds; for a layout, the size of
#' one record.
#'
#' @param x A `zubin_builder` or a `zubin_layout`.
#' @param ... Unused.
#' @return A double for a builder; an integer for a layout.
#' @export
#' @examples
#' b <- bin_builder()
#' bin_put(b, as.raw(1:3))
#' bin_size(b)
bin_size <- function(x, ...) {
  UseMethod("bin_size")
}

#' @export
bin_size.zubin_builder <- function(x, ...) {
  builder_state(x)[[1L]]
}

#' @rdname bin_builder
#' @export
as.raw.zubin_builder <- function(x) {
  ptr <- check_builder(x)
  res <- .Call(zubin_builder_take, ptr, FALSE)
  if (is_status(res)) builder_state(x)
  res
}

#' @export
format.zubin_builder <- function(x, ...) {
  s <- tryCatch(builder_state(x), zubin_error = function(e) NULL)
  if (is.null(s)) {
    return("<zubin_builder: freed>")
  }
  sprintf("<zubin_builder: %s bytes, capacity %s, max %s>",
          format(s[[1L]], scientific = FALSE, big.mark = ","),
          format(s[[2L]], scientific = FALSE, big.mark = ","),
          if (is.infinite(s[[3L]])) "none" else format(s[[3L]], scientific = FALSE, big.mark = ","))
}

#' @export
print.zubin_builder <- function(x, ...) {
  cat(format(x), "\n", sep = "")
  invisible(x)
}

# The builder's external pointer, unclassed so that .Call sees a plain
# EXTPTRSXP; or an error for anything else.
check_builder <- function(b, call = sys.call(-1L)) {
  if (!inherits(b, "zubin_builder") || typeof(b) != "externalptr") {
    invalid_argument("`b` must be a builder from bin_builder().", arg = "b", call = call)
  }
  b
}

builder_state <- function(b, call = sys.call(-1L)) {
  s <- .Call(zubin_builder_state, check_builder(b, call))
  if (is_status(s)) {
    invalid_argument("The builder has been freed (was it saved and restored?).",
                     arg = "b", call = call)
  }
  s
}

builder_fail <- function(res, b, n, x = NULL, call = sys.call(-1L)) {
  name <- as.character(res)
  index <- attr(res, "index")
  if (is.null(index)) index <- NA_real_
  switch(name,
    ZB_ERR_LIMIT = {
      s <- builder_state(b, call)
      zb_fail(res, sprintf("Appending %s bytes would pass the builder's max of %s bytes.",
                           format(n, scientific = FALSE), format(s[[3L]], scientific = FALSE)),
              size = s[[1L]] + n, max = s[[3L]], call = call)
    },
    ZB_ERR_NA = zb_fail(res, sprintf("`x[%s]` is NA, which has no bytes.", index + 1),
                        field = NA_character_, index = index, call = call),
    ZB_ERR_RANGE = zb_fail(res, sprintf("`x[%s]` does not fit the type.", index + 1),
                           field = NA_character_, index = index, call = call),
    ZB_ERR_INVALID = {
      builder_state(b, call)
      zb_fail(res, sprintf("`x[%s]` cannot be written as this type.", index + 1),
              field = NA_character_, index = index, call = call)
    },
    zb_fail(res, "Could not allocate the builder's storage.", call = call)
  )
}

# A non-negative whole number of bytes, below 2^53.
check_size <- function(x, arg, call = sys.call(-1L)) {
  if (!is.numeric(x) || length(x) != 1L || is.na(x) || x < 0 || x != floor(x) ||
        x > 2^53) {
    invalid_argument(sprintf("`%s` must be a whole number of bytes, at least 0.", arg),
                     arg = arg, call = call)
  }
}
