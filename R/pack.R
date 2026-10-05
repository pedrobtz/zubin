#' Write records to bytes
#'
#' `bin_pack()` writes one record per row of its columns, laid out as
#' `layout` describes, into a raw vector of `n * bin_size(layout)` bytes.
#' Padding and alignment gaps are zeros, so the output is deterministic and
#' never holds stray memory.
#'
#' The columns are named vectors in `...`, one per field that carries a
#' value (padding takes none), or a single data frame or list with those
#' names. A data frame from [bin_unpack()] works as it is: array fields may
#' be given as a matrix with one row per record, or as the columns `name.1`,
#' ..., `name.k`, and a single record's array as a vector of length `k`.
#' Columns are recycled to the longest; a length that does not divide it is a
#' `zubin_invalid_argument` error, never a warning.
#'
#' Each field accepts the R types of the table in [bin_layout()]: integer,
#' whole double and logical for the small integer types; integer and whole
#' double for `i32` and `u32`; integer, whole double and `integer64` for
#' `i64` and `u64`; numeric for the floating types (rounded to nearest even);
#' logical for `bool`; character for `s<n>` (UTF-8, at most `n` bytes); and a
#' list of raw vectors of exactly `n` bytes for `b<n>`. Nothing is truncated,
#' wrapped or rounded into an integer type: a value that does not fit is a
#' `zubin_range_error`, and an `NA` a `zubin_na_error`, each carrying `field`
#' and the 0-based record `index` of the first one. `NA` has bytes only in
#' `i32` (with `na = "allow"`), `f64`, and the other floating types, where it
#' becomes a NaN.
#'
#' @param layout A [bin_layout()], or a specification string for one.
#' @param ... Named columns, or one data frame or list of them.
#' @param na `"error"`, or `"allow"` to write `NA` into an `i32` field as
#'   -2^31 (and an `integer64` `NA` into `i64` as -2^63).
#' @return A raw vector.
#' @seealso [bin_unpack()], the inverse; [bin_put()] to append to a builder.
#' @export
#' @examples
#' hdr <- bin_layout(">magic:u32 version:u16 count:u32")
#' bin_pack(hdr, magic = 0xCAFEBABE, version = 1, count = 3)
#'
#' rec <- bin_layout("<id:u16 x:f32 flag:bool x1")
#' df <- data.frame(id = 1:3, x = c(0.5, 1.5, 2.5), flag = c(TRUE, FALSE, TRUE))
#' bytes <- bin_pack(rec, df)
#' bin_unpack(bytes, rec)
bin_pack <- function(layout, ..., na = c("error", "allow")) {
  layout <- bin_layout(layout)
  na <- match.arg(na)
  cols <- pack_inputs(list(...), layout)
  pack_columns(layout, cols, na)
}

#' Write a vector of one type to bytes
#'
#' `bin_encode()` writes every element of `x` as `type`, in `endian` order:
#' [writeBin()] generalised to every width and both byte orders, rounding to
#' f16, bf16 and f32 to nearest even, and refusing, never truncating, an
#' integer that does not fit. `bin_encode(bin_decode(b, t), t)` is `b`.
#'
#' @inheritParams bin_decode
#' @param x A vector: numeric, logical, `integer64`, character for `s<n>`, or
#'   a list of raw vectors for `b<n>`.
#' @param na `"error"`, or `"allow"` to write `NA` as -2^31 into `i32`.
#' @return A raw vector of `length(x)` times the type's width.
#' @export
#' @examples
#' bin_encode(c(1, 2, 65535), "u16")
#' bin_encode(1:3, "i32", endian = "big")
#' bin_encode(c(0.1, 1e5), "f16")
#' try(bin_encode(300, "u8"))
bin_encode <- function(x, type, endian = c("little", "big", "native"),
                       na = c("error", "allow")) {
  endian <- match.arg(endian)
  na <- match.arg(na)
  layout <- type_layout(type, endian)
  pack_columns(layout, list(v = x), na, field_names = NA_character_)
}

# The columns, from named arguments or one data frame or list, matched to
# the layout's value fields by name. Array fields may arrive as name.1 ...
# name.k columns.
pack_inputs <- function(args, layout, call = sys.call(-1L)) {
  nms <- names(args)
  if (length(args) == 1L && (is.null(nms) || !nzchar(nms)) && is.list(args[[1L]])) {
    cols <- as.list(args[[1L]])
  } else {
    if (length(args) && (is.null(nms) || any(!nzchar(nms)))) {
      invalid_argument("Every column in `...` must be named.", arg = "...", call = call)
    }
    cols <- args
  }
  f <- layout$fields
  value <- !startsWith(f$type, "x")
  fields <- f$name[value]
  counts <- f$count[value]
  out <- vector("list", length(fields))
  names(out) <- fields
  used <- character()
  for (i in seq_along(fields)) {
    nm <- fields[i]
    if (!is.null(cols[[nm]]) && nm %in% names(cols)) {
      out[[i]] <- cols[[nm]]
      used <- c(used, nm)
    } else if (counts[i] > 1L && all(paste0(nm, ".", seq_len(counts[i])) %in% names(cols))) {
      parts <- paste0(nm, ".", seq_len(counts[i]))
      out[[i]] <- array_from_columns(cols[parts])
      used <- c(used, parts)
    } else {
      invalid_argument(sprintf("No column for field `%s`.", nm), arg = nm, call = call)
    }
  }
  extra <- setdiff(names(cols), used)
  if (length(extra)) {
    invalid_argument(sprintf("No field for column `%s`.", extra[1L]), arg = extra[1L], call = call)
  }
  out
}

array_from_columns <- function(parts) {
  cls <- oldClass(parts[[1L]])
  m <- do.call(cbind, lapply(parts, unclass))
  if (identical(cls, "integer64")) class(m) <- cls
  m
}

# Checks each column against its field, recycles to the longest, converts it
# to what the kernel reads, and packs.
pack_columns <- function(layout, cols, na, field_names = NULL, call = sys.call(-1L)) {
  f <- layout$fields
  value <- !startsWith(f$type, "x")
  types <- f$type[value]
  counts <- f$count[value]
  names <- f$name[value]
  rows <- integer(length(cols))
  for (i in seq_along(cols)) {
    cols[[i]] <- pack_value(cols[[i]], types[i], counts[i], names[i], call)
    rows[i] <- pack_rows(cols[[i]], counts[i])
  }
  n <- if (length(rows)) max(rows) else 0
  for (i in seq_along(cols)) {
    if (rows[i] != n) {
      if (rows[i] == 0L || n %% rows[i] != 0) {
        invalid_argument(sprintf("Column `%s` has %s rows, which does not divide %s.",
                                 names[i], rows[i], n), arg = names[i], call = call)
      }
      cols[[i]] <- recycle_rows(cols[[i]], counts[i], n)
    }
  }
  # C reads each column's type and length only, so attributes (dim, the
  # integer64 class) stay: stripping them would copy every column.
  is64 <- vapply(cols, inherits, logical(1), "integer64")
  res <- .Call(zubin_pack, layout$spec, layout$align > 1L, unname(cols), unname(is64),
               as.double(n), na == "allow")
  if (is_status(res)) pack_fail(res, layout, field_names, call)
  res
}

# What the kernel for `type` reads: integer for the small integer types,
# logical for bool, double (or integer64 bits) for the rest of the numbers,
# character for s<n>, a list for b<n>.
pack_value <- function(x, type, count, name, call) {
  base <- sub("[0-9]+$", "", type)
  wrong <- function() {
    invalid_argument(sprintf("Field `%s` (%s) cannot be written from %s.", name, type,
                             class(x)[1L]), arg = name, call = call)
  }
  if (count > 1L && !is.matrix(x)) {
    if (length(x) != count) {
      invalid_argument(sprintf("Field `%s` is an array of %d: give a matrix of %d columns, or one record's %d values.",
                               name, count, count, count), arg = name, call = call)
    }
    cls <- oldClass(x)
    x <- matrix(unclass(x), nrow = 1L)
    if (identical(cls, "integer64")) class(x) <- cls
  }
  if (is.matrix(x) && ncol(x) != count) {
    invalid_argument(sprintf("Field `%s` needs %d columns, not %d.", name, count, ncol(x)),
                     arg = name, call = call)
  }
  if (inherits(x, "integer64")) {
    if (!type %in% c("i64", "u64")) wrong()
    return(x)
  }
  if (base %in% c("u", "i", "f", "bf") && !is.numeric(x) && !is.logical(x)) wrong()
  switch(base,
    s = if (is.character(x)) x else wrong(),
    b = if (is.list(x)) unclass(x) else wrong(),
    bool = if (is.logical(x)) x else wrong(),
    {
      small <- type %in% c("u8", "i8", "u16", "i16", "i32")
      if (is.logical(x) && !type %in% c("u8", "i8", "u16", "i16")) wrong()
      # converted only when needed: even a no-op storage.mode<- copies
      if (small && (is.integer(x) || is.logical(x))) {
        if (!is.integer(x)) storage.mode(x) <- "integer"
        x
      } else {
        if (!is.double(x)) storage.mode(x) <- "double"
        x
      }
    }
  )
}

pack_rows <- function(x, count) {
  if (is.matrix(x)) nrow(x) else length(x)
}

recycle_rows <- function(x, count, n) {
  if (is.matrix(x)) {
    cls <- oldClass(x)
    m <- unclass(x)[rep_len(seq_len(nrow(x)), n), , drop = FALSE]
    if (!is.null(cls)) class(m) <- cls
    return(m)
  }
  cls <- oldClass(x)
  v <- rep_len(unclass(x), n)
  if (identical(cls, "integer64")) class(v) <- cls
  v
}

pack_fail <- function(res, layout, field_names, call) {
  name <- as.character(res)
  index <- attr(res, "index")
  pos <- attr(res, "field")
  field <- if (is.null(pos)) NA_character_ else layout$fields$name[pos + 1L]
  if (!is.null(field_names) && is.na(field_names[1L])) field <- NA_character_
  what <- if (is.na(field)) "" else sprintf(" of field `%s`", field)
  rec <- format(index, scientific = FALSE)
  switch(name,
    ZB_ERR_RANGE = zb_fail(res, sprintf("Value %s%s does not fit its type.", rec, what),
                           field = field, index = index, call = call),
    ZB_ERR_NA = zb_fail(res, sprintf("Value %s%s is NA, which has no bytes in its type.", rec, what),
                        field = field, index = index, call = call),
    ZB_ERR_MEMORY = zb_fail(res, "The result is too large for a raw vector.", call = call),
    zb_fail(res, sprintf("Value %s%s cannot be written.", rec, what),
            field = field, index = index, call = call)
  )
}
