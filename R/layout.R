#' Describe a binary record
#'
#' `bin_layout()` parses a specification of a fixed-size binary record into a
#' `zubin_layout`: the type, element count, size, 0-based byte offset and
#' byte order of each field, the record's size, and its alignment. Offsets are
#' 0-based, as in every file format specification.
#'
#' @section The specification:
#' One string of fields separated by whitespace or a comma:
#'
#' ```
#' layout  := [ endian ] field { sep field }
#' endian  := "<" | ">" | "="          little, big, host; default `endian`
#' field   := [ name ":" ] type [ "[" count "]" ] [ "." order ]
#'          | "x" width                padding: never read, written as zeros
#' type    := u8 i8 u16 i16 u32 i32 u64 i64 f16 bf16 f32 f64 bool
#'          | "b" width                fixed bytes
#'          | "s" width                fixed string
#' order   := "le" | "be"              multi-byte numeric types only
#' name    := [A-Za-z_] [A-Za-z0-9_.]*
#' count, width := 1 .. 2^31 - 1
#' ```
#'
#' For example `"<magic:u32 version:u16 flags:u16 count:u64 name:s32"`, or
#' `"<ts:i64 price:f64 qty:i32 side:u8 x3 crc:u32.be"`. An endian prefix
#' overrides `endian`, and a field's `.le` or `.be` suffix overrides both.
#' Names are unique; an unnamed field is called `V1`, `V2`, ... by its
#' position among the fields that are not padding. `t[k]` is an array of `k`
#' elements, for the numeric types and `bool` only (write `b24`, not
#' `b8[3]`). Every malformed specification is a `zubin_spec_error` carrying
#' `position`, the 0-based byte position of the problem in the string.
#'
#' @section Types:
#' The type model, in both directions ([bin_unpack()] and [bin_decode()]
#' read; [bin_pack()], [bin_encode()] and [bin_put()] write):
#'
#' \tabular{llll}{
#'   **Spec** \tab **Bytes** \tab **Read as** \tab **Written from** \cr
#'   `u8 i8 u16 i16` \tab 1--2 \tab integer \tab integer, whole double, logical \cr
#'   `i32` \tab 4 \tab integer; -2^31 is an error unless `na = "allow"` \tab
#'     integer, whole double; `NA` only with `na = "allow"` \cr
#'   `u32` \tab 4 \tab double \tab integer, whole double \cr
#'   `i64 u64` \tab 8 \tab double, an error above 2^53 in magnitude; or
#'     `integer64` (`int64 = "integer64"`), an error for `u64` from 2^63 \tab
#'     integer, whole double, `integer64` \cr
#'   `f16 bf16` \tab 2 \tab double \tab numeric, rounded to nearest even \cr
#'   `f32 f64` \tab 4, 8 \tab double \tab numeric; `NA` survives `f64` and is
#'     a NaN in the others \cr
#'   `bool` \tab 1 \tab logical; any non-zero byte is `TRUE` \tab logical, not
#'     `NA` \cr
#'   `b<n>` \tab n \tab a list of raw vectors \tab a list of raw vectors of
#'     exactly n bytes \cr
#'   `s<n>` \tab n \tab character: the bytes before the first NUL, as UTF-8
#'     \tab character of at most n UTF-8 bytes, NUL-padded \cr
#'   `x<n>` \tab n \tab not returned \tab not given; written as zeros \cr
#'   `t[k]` \tab k x width \tab a matrix of k columns (`name.1` ... `name.k` in
#'     a data frame) \tab a matrix of k columns, or one record's k values \cr
#' }
#'
#' A value that does not fit is a `zubin_range_error`, and an `NA` with no
#' bytes a `zubin_na_error`; nothing is truncated, wrapped or rounded into an
#' integer. See [zubin-conditions].
#'
#' @section Alignment:
#' With `align = FALSE` fields are packed at consecutive offsets. With
#' `align = TRUE` each field starts at the next multiple of its element width
#' (1 for `bool`, `b`, `s` and `x`) and the record size is rounded up to the
#' largest of them: the rule C compilers apply to a `struct` on 64-bit
#' targets, so a struct written with `fwrite()` reads back with
#' `align = TRUE`. (32-bit x86 aligns 8-byte members to 4 inside a struct;
#' such a struct needs explicit `x` padding.)
#'
#' @param spec A specification string; or a named character vector of type
#'   tokens, whose names are the field names (`c(id = "u32", "x3")`); or a
#'   `zubin_layout`, which is returned unchanged.
#' @param endian The byte order of fields the specification does not order
#'   itself: `"little"`, `"big"`, or `"native"`, the host's (see
#'   [bin_info()]).
#' @param align `FALSE` for packed fields, `TRUE` for C struct alignment.
#' @return A `zubin_layout`: a list with `spec`, the normalised specification;
#'   `fields`, a data frame with columns `name`, `type`, `count` (array
#'   elements, 1 for a scalar), `size`, `offset` and `endian` (`NA` for
#'   one-byte types); `size`, the record size in bytes; and `align`, the
#'   record's alignment (1 when packed). `length()` and `names()` describe the
#'   fields that carry values, which excludes padding; `as.data.frame()`
#'   returns `fields`; `bin_size()` returns `size`.
#' @export
#' @examples
#' wav <- bin_layout("<riff:s4 size:u32 wave:s4")
#' wav
#' bin_size(wav)
#' wav$fields
#'
#' # The same, built programmatically
#' bin_layout(c(riff = "s4", size = "u32", wave = "s4"))
#'
#' # A C struct { uint8_t a; uint32_t b; uint16_t c; }
#' bin_layout("a:u8 b:u32 c:u16", endian = "native", align = TRUE)$fields
bin_layout <- function(spec, endian = c("little", "big", "native"), align = FALSE) {
  if (inherits(spec, "zubin_layout")) {
    return(spec)
  }
  endian <- match.arg(endian)
  if (!is.logical(align) || length(align) != 1L || is.na(align)) {
    invalid_argument("`align` must be TRUE or FALSE.", arg = "align")
  }
  text <- spec_text(spec)
  big <- endian == "big" || (endian == "native" && bin_info()$endian == "big")
  parse_layout(text, big, align)
}

# A spec as one string: the string itself, or a named vector joined with
# single spaces, each value prefixed by its name.
spec_text <- function(spec, call = sys.call(-1L)) {
  if (!is.character(spec) || length(spec) < 1L || anyNA(spec)) {
    invalid_argument("`spec` must be a string, a named character vector, or a layout.",
                     arg = "spec", call = call)
  }
  nms <- names(spec)
  if (length(spec) == 1L && is.null(nms)) {
    return(spec)
  }
  if (is.null(nms)) nms <- rep("", length(spec))
  nms[is.na(nms)] <- ""
  parts <- ifelse(nzchar(nms), paste0(nms, ":", spec), spec)
  starts <- cumsum(c(0L, nchar(parts, type = "bytes") + 1L))
  # a name inside a value, or anything that would split one value in two
  bad <- grepl("[:,[:space:]]", spec)
  if (any(bad)) {
    i <- which(bad)[1L]
    at <- regexpr("[:,[:space:]]", spec[i])
    zubin_abort(sprintf("The value of field %d, \"%s\", must be a type token alone.", i, spec[i]),
                "zubin_spec_error",
                position = as.double(starts[i] + nchar(parts[i]) - nchar(spec[i]) + at - 1L),
                call = call)
  }
  paste(parts, collapse = " ")
}

parse_layout <- function(text, big, align, call = sys.call(-1L)) {
  res <- .Call(zubin_layout_parse, text, big, align)
  if (is_status(res)) {
    pos <- attr(res, "position")
    zb_fail(res, sprintf("Malformed layout specification at byte %s: %s", pos,
                         spec_context(text, pos)),
            position = pos, call = call)
  }
  types <- c("u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f16", "bf16",
             "f32", "f64", "bool", "b", "s", "x")[res$type]
  wide <- res$type >= 14L
  token <- ifelse(wide, paste0(types, res$size), types)
  pad <- res$type == 16L
  name <- res$name
  # unnamed value fields are V1, V2, ... by their position among value fields
  k <- cumsum(!pad)
  generated <- is.na(name) & !pad
  name[generated] <- paste0("V", k[generated])
  clash <- generated & name %in% res$name
  if (any(clash)) {
    taken <- name[clash][1L]
    pos <- as.double(res$name_pos[match(taken, res$name)])
    zubin_abort(sprintf("The name \"%s\" is taken by an unnamed field's default name.", taken),
                "zubin_spec_error", position = pos, call = call)
  }
  endian <- ifelse(is.na(res$big), NA_character_, ifelse(res$big, "big", "little"))
  fields <- data.frame(
    name = name, type = token, count = res$count, size = res$size,
    offset = res$offset, endian = endian, stringsAsFactors = FALSE
  )
  # the order the spec itself declares, when it declares one
  prefix <- substr(sub("^[[:space:]]+", "", text), 1L, 1L)
  if (prefix == "<") big <- FALSE
  if (prefix == ">") big <- TRUE
  if (prefix == "=") big <- bin_info()$endian == "big"
  structure(
    list(
      spec = normalise_spec(fields, pad, big),
      fields = fields,
      size = res$record_size,
      align = res$align
    ),
    class = "zubin_layout"
  )
}

# The canonical spec: the default order as a prefix, every value field
# named, an array count when there is one, and an order suffix only where a
# field differs from the prefix. Parsing it gives the same layout.
normalise_spec <- function(fields, pad, big) {
  prefix <- if (big) ">" else "<"
  default <- if (big) "big" else "little"
  tok <- fields$type
  arr <- !pad & fields$count > 1L
  tok[arr] <- paste0(tok[arr], "[", fields$count[arr], "]")
  differs <- !is.na(fields$endian) & fields$endian != default
  tok[differs] <- paste0(tok[differs], ifelse(fields$endian[differs] == "big", ".be", ".le"))
  tok[!pad] <- paste0(fields$name[!pad], ":", tok[!pad])
  paste0(prefix, paste(tok, collapse = " "))
}

# A few characters either side of a position, for the message.
spec_context <- function(text, pos) {
  bytes <- charToRaw(text)
  lo <- max(1L, pos - 9L)
  hi <- min(length(bytes), pos + 11L)
  if (hi < lo) {
    return("at the end")
  }
  sprintf("near \"%s\"", rawToChar(bytes[lo:hi]))
}

layout_value_fields <- function(x) {
  f <- unclass(x)$fields
  f[!startsWith(f$type, "x"), , drop = FALSE]
}

#' @export
length.zubin_layout <- function(x) {
  nrow(layout_value_fields(x))
}

#' @export
names.zubin_layout <- function(x) {
  layout_value_fields(x)$name
}

#' @export
as.data.frame.zubin_layout <- function(x, ...) {
  unclass(x)$fields
}

#' @rdname bin_size
#' @export
bin_size.zubin_layout <- function(x, ...) {
  unclass(x)$size
}

#' @export
format.zubin_layout <- function(x, ...) {
  x <- unclass(x)
  f <- x$fields
  f$name[is.na(f$name)] <- ""
  f$endian[is.na(f$endian)] <- ""
  head <- sprintf("<zubin_layout: %d bytes, %d field%s%s>", x$size, nrow(f),
                  if (nrow(f) == 1L) "" else "s",
                  if (x$align > 1L) sprintf(", aligned to %d", x$align) else "")
  tab <- utils::capture.output(print(f, row.names = FALSE, right = FALSE))
  c(head, tab)
}

#' @export
print.zubin_layout <- function(x, ...) {
  cat(format(x), sep = "\n")
  invisible(x)
}
