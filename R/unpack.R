#' Read records from bytes
#'
#' `bin_unpack()` reads `n` records described by `layout` from the raw vector
#' `x`, the first at the 0-based byte `offset` and each next one `stride`
#' bytes further on, into one column per field. Offsets are 0-based.
#'
#' Fields are read field by field, each in one pass over the records, so a
#' million records cost one loop per field. Nothing is rounded or wrapped: a
#' value that has no exact R representation is a `zubin_range_error`
#' carrying `field` and the 0-based record `index` of the first one, namely
#' an `i32` of -2^31 (R's `NA_integer_`; allowed with `na = "allow"`), an
#' `i64` or `u64` above 2^53 in magnitude when `int64 = "double"`, and a
#' `u64` of 2^63 or more, or an `i64` of -2^63 (bit64's `NA`), when
#' `int64 = "integer64"`. An `s<n>` field holds the bytes up to its first
#' NUL; with `encoding = "UTF-8"` they must be valid UTF-8, or the call is a
#' `zubin_encoding_error` carrying `field` and `index`.
#'
#' A single header is `bin_unpack(x, hdr, n = 1)`: a one-row result.
#'
#' @param x A raw vector.
#' @param layout A [bin_layout()], or a specification string for one.
#' @param offset The 0-based byte position of the first record.
#' @param n The number of records, or `NULL` for every whole record that
#'   fits; trailing bytes that do not make a whole record are left alone. An
#'   explicit `n` that runs past the end of `x` is a `zubin_bounds_error`
#'   carrying `offset` and `length`.
#' @param stride Bytes from the start of one record to the next, at least
#'   the record size; `NULL` for the record size. A larger stride reads the
#'   head of a larger record.
#' @param as `"data.frame"` for a data frame, whose array fields expand to
#'   columns `name.1`, `name.2`, ... and whose `b<n>` fields are list columns
#'   wrapped in [I()]; or `"list"` for the columns as they are, array fields
#'   as matrices.
#' @param int64 How `i64` and `u64` fields are returned: `"double"`, exact or
#'   an error; or `"integer64"`, a double vector carrying the 64-bit pattern
#'   with class `integer64`, as the bit64 package defines it.
#' @param na `"error"`, or `"allow"` to read -2^31 in an `i32` field as
#'   `NA_integer_` (and -2^63 in `integer64` mode as `NA`).
#' @param encoding How `s<n>` bytes are marked: `"UTF-8"` (validated),
#'   `"latin1"` or `"bytes"` (any bytes).
#' @return A data frame with one row per record, or a named list of columns.
#' @seealso [bin_decode()] for one field read as a vector; [bin_pack()] for
#'   the inverse.
#' @export
#' @examples
#' hdr <- bin_layout(">magic:u32 minor:u16 major:u16")
#' x <- as.raw(c(0xca, 0xfe, 0xba, 0xbe, 0x00, 0x00, 0x00, 0x41))
#' bin_unpack(x, hdr, n = 1)
#'
#' # records with an array field
#' pts <- bin_layout("<id:u16 xy:i8[2]")
#' y <- as.raw(c(1, 0, 5, 250, 2, 0, 7, 9))
#' bin_unpack(y, pts)
#' bin_unpack(y, pts, as = "list")$xy
bin_unpack <- function(x, layout, offset = 0, n = NULL, stride = NULL,
                       as = c("data.frame", "list"),
                       int64 = c("double", "integer64"), na = c("error", "allow"),
                       encoding = c("UTF-8", "latin1", "bytes")) {
  layout <- bin_layout(layout)
  as <- match.arg(as)
  cols <- unpack_columns(x, layout, offset, n, stride, match.arg(int64), match.arg(na),
                         match.arg(encoding), field_names = names(layout))
  if (as == "list") {
    return(cols)
  }
  columns_to_frame(cols)
}

#' Read a vector of one type from bytes
#'
#' `bin_decode()` reads `n` consecutive values of one type from the raw
#' vector `x`, starting at the 0-based byte `offset`: `bin_decode(x, "u32",
#' offset = 4)` is a header field, `bin_decode(x, "f32", offset = 64, n =
#' 1e6)` is a column. It is [readBin()] generalised to every width and both
#' byte orders, exact or an error; [bin_encode()] is its inverse. Offsets are
#' 0-based.
#'
#' @inheritParams bin_unpack
#' @param type One type token of [bin_layout()]: `"u8"` to `"f64"`, `"bool"`,
#'   `"b<n>"` or `"s<n>"`, optionally with a `.le` or `.be` suffix.
#' @param n The number of values, or `NULL` for every whole value that fits.
#' @param endian The byte order: `"little"`, `"big"`, or `"native"`.
#' @return A vector of the type's R type (see [bin_layout()]): integer,
#'   double, logical, character, or a list of raw vectors for `b<n>`.
#' @export
#' @examples
#' x <- as.raw(c(0x01, 0x00, 0x02, 0x00, 0xff, 0xff))
#' bin_decode(x, "u16")
#' bin_decode(x, "i16")
#' bin_decode(x, "u16", endian = "big", offset = 2, n = 1)
bin_decode <- function(x, type, offset = 0, n = NULL,
                       endian = c("little", "big", "native"),
                       int64 = c("double", "integer64"), na = c("error", "allow")) {
  endian <- match.arg(endian)
  layout <- type_layout(type, endian)
  cols <- unpack_columns(x, layout, offset, n, NULL, match.arg(int64), match.arg(na),
                         "UTF-8", field_names = NA_character_)
  cols[[1L]]
}

# A one-field layout for a single type token, or an error naming `type`.
type_layout <- function(type, endian, call = sys.call(-1L)) {
  if (!is.character(type) || length(type) != 1L || is.na(type) ||
        !grepl("^[a-z]+[0-9]*(\\.(le|be))?$", type) || grepl("^x", type)) {
    invalid_argument("`type` must be one type token, such as \"u32\", \"f64.be\" or \"s16\".",
                     arg = "type", call = call)
  }
  tryCatch(
    bin_layout(paste0("v:", type), endian = endian),
    zubin_spec_error = function(e) {
      invalid_argument(sprintf("Unknown `type` \"%s\".", type), arg = "type", call = call)
    }
  )
}

unpack_columns <- function(x, layout, offset, n, stride, int64, na, encoding,
                           field_names, call = sys.call(-1L)) {
  if (!is.raw(x)) {
    invalid_argument("`x` must be a raw vector.", arg = "x", call = call)
  }
  check_size(offset, "offset", call = call)
  if (!is.null(n)) check_size(n, "n", call = call)
  size <- bin_size(layout)
  if (!is.null(stride)) {
    check_size(stride, "stride", call = call)
    if (stride < size) {
      invalid_argument(sprintf("`stride` must be at least the record size, %d.", size),
                       arg = "stride", call = call)
    }
  }
  res <- .Call(zubin_unpack, x, layout$spec, layout$align > 1L, as.double(offset),
               if (is.null(n)) -1 else as.double(n), if (is.null(stride)) -1 else as.double(stride),
               if (int64 == "integer64") 1L else 0L, na == "allow",
               match(encoding, c("UTF-8", "latin1", "bytes")) - 1L)
  if (is_status(res)) {
    unpack_fail(res, x, layout, offset, n, stride, field_names, call)
  }
  value <- !startsWith(layout$fields$type, "x")
  names(res) <- layout$fields$name[value]
  if (int64 == "integer64") {
    wide <- layout$fields$type[value] %in% c("i64", "u64")
    res[wide] <- lapply(res[wide], function(col) {
      class(col) <- "integer64"
      col
    })
  }
  res
}

unpack_fail <- function(res, x, layout, offset, n, stride, field_names, call) {
  name <- as.character(res)
  index <- attr(res, "index")
  pos <- attr(res, "field")
  field <- if (is.null(pos)) NA_character_ else layout$fields$name[pos + 1L]
  if (length(field_names) == 1L && is.na(field_names)) field <- NA_character_
  what <- if (is.na(field)) "" else sprintf(" in field `%s`", field)
  switch(name,
    ZB_ERR_EOF = {
      size <- bin_size(layout)
      stride <- if (is.null(stride)) size else stride
      need <- if (is.null(n) || n == 0) offset else offset + (n - 1) * stride + size
      zubin_abort(sprintf("Reading to byte %s needs more than the %s bytes of `x`.",
                          format(need, scientific = FALSE), format(length(x), scientific = FALSE)),
                  "zubin_bounds_error", offset = offset, length = as.double(length(x)),
                  call = call)
    },
    ZB_ERR_RANGE = zb_fail(res, sprintf("The value of record %s%s has no exact R value.",
                                        format(index, scientific = FALSE), what),
                           field = field, index = index, call = call),
    ZUBIN_ERR_ENCODING = zubin_abort(
      sprintf("The string of record %s%s is not valid UTF-8.", format(index, scientific = FALSE), what),
      "zubin_encoding_error", field = field, index = index, call = call
    ),
    ZB_ERR_MEMORY = zb_fail(res, "The result is too large for R.", call = call),
    zb_fail(res, "Invalid arguments to the unpack kernel.", call = call)
  )
}

# The columns as a data frame: matrices expand to name.1 ... name.k, and
# byte lists stay list columns, wrapped in I().
columns_to_frame <- function(cols) {
  out <- list()
  nms <- character()
  nrow <- 0L
  for (i in seq_along(cols)) {
    col <- cols[[i]]
    nm <- names(cols)[i]
    if (is.matrix(col)) {
      cls <- oldClass(col)
      m <- unclass(col)
      nrow <- nrow(m)
      for (j in seq_len(ncol(m))) {
        v <- m[, j]
        if (!is.null(cls)) class(v) <- cls
        out[[length(out) + 1L]] <- v
      }
      nms <- c(nms, paste0(nm, ".", seq_len(ncol(m))))
    } else {
      if (is.list(col)) col <- I(col)
      nrow <- length(col)
      out[[length(out) + 1L]] <- col
      nms <- c(nms, nm)
    }
  }
  names(out) <- nms
  structure(out, row.names = .set_row_names(nrow), class = "data.frame")
}
