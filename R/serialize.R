#' Serialise R objects into a builder, and back from bytes at an offset
#'
#' `bin_serialize()` appends the serialisation of `x` to the builder `b`,
#' exactly as [serialize()] would write it, without first allocating the
#' whole stream as a raw vector. `bin_unserialize()` reads one serialised
#' object from the raw vector `x`, starting at the 0-based byte `offset`, so
#' a stream stored inside a larger file or record is read where it lies.
#' Offsets are 0-based.
#'
#' Both are R's own serialisation: `unserialize(bin_take(b))` after
#' `bin_serialize(x, b)` is identical to `x`, and `bin_unserialize()` reads
#' what [serialize()] writes. Errors that R raises while writing or reading
#' the stream (an object it cannot serialise, a malformed stream, an error in
#' `refhook`) arrive as R raised them. A stream that ends before the
#' object does is a `zubin_bounds_error` carrying `offset` and `length`. An
#' append that would pass the builder's `max` is a `zubin_limit_error`; in
#' every failure, an interrupt included, the builder is left as it was.
#'
#' Only read serialised data from a source you trust: a serialisation
#' stream can hold any R object, functions and environments included.
#'
#' @param x For `bin_serialize()`, any R object; for `bin_unserialize()`, a
#'   raw vector holding a serialisation stream at `offset`.
#' @param b A builder from [bin_builder()].
#' @param version The serialisation format version, 2 or 3, as in
#'   [serialize()].
#' @param xdr `TRUE` for the portable big-endian (XDR) format, `FALSE` for
#'   the host's native binary format, as in [serialize()].
#' @param refhook `NULL`, or a function handling reference objects, as in
#'   [serialize()] and [unserialize()].
#' @param offset The 0-based position of the stream in `x`.
#' @return `bin_serialize()` returns `b`, invisibly. `bin_unserialize()`
#'   returns the object.
#' @seealso [bin_hash_object()] for a digest of the same stream.
#' @export
#' @examples
#' b <- bin_builder()
#' bin_put(b, as.raw(c(0xde, 0xad)))          # a 2-byte header of our own
#' bin_serialize(list(a = 1:3, b = "zubin"), b)
#' bytes <- bin_take(b)
#' bin_unserialize(bytes, offset = 2)
bin_serialize <- function(x, b, version = 3L, xdr = TRUE, refhook = NULL) {
  ptr <- check_builder(b)
  version <- check_version(version)
  check_flag(xdr, "xdr")
  check_refhook(refhook)
  res <- .Call(zubin_serialize, ptr, x, version, xdr, refhook)
  if (is_status(res)) {
    # a freed builder is the one invalid argument C can see; whatever else
    # comes back is raised, never dropped
    s <- builder_state(b)
    name <- as.character(res)
    if (name == "ZB_ERR_LIMIT") {
      size <- attr(res, "size")
      zb_fail(res, sprintf("The serialisation needs %s bytes, past the builder's max of %s.",
                           format(size - s[[1L]], scientific = FALSE),
                           format(s[[3L]], scientific = FALSE)),
              size = size, max = s[[3L]])
    }
    if (name == "ZB_ERR_MEMORY") {
      zb_fail(res, "Could not grow the builder to hold the serialisation.")
    }
    zb_fail(res, "Could not serialise `x` into the builder.")
  }
  invisible(b)
}

#' @rdname bin_serialize
#' @export
bin_unserialize <- function(x, offset = 0, refhook = NULL) {
  if (!is.raw(x)) invalid_argument("`x` must be a raw vector.", arg = "x")
  check_size(offset, "offset")
  check_refhook(refhook)
  # the object comes back in a one-element list, so that no object can pass
  # for a status
  res <- .Call(zubin_unserialize, x, as.double(offset), refhook)
  if (is_status(res)) {
    zubin_abort(sprintf("The serialisation stream at offset %s ends before its object does (`x` has %s bytes).",
                        format(offset, scientific = FALSE), length(x)),
                "zubin_bounds_error", offset = as.double(offset),
                length = as.double(length(x)))
  }
  res[[1L]]
}

#' A content fingerprint of an R object
#'
#' `bin_hash_object()` is XXH3 of the body of R's XDR serialisation of `x`,
#' format version 2 or 3: everything [serialize()] writes after its header.
#' It is streamed through the hasher, so the serialised bytes are never
#' allocated. Skipping the header, which records the R version that wrote it
#' (and, for `version = 3`, the native encoding), keeps the digest from
#' changing with those; it stays the same for as long as R writes the same
#' body for `x`, which R does not promise across its versions (see
#' [serialize()]), least of all for closures, environments and ALTREP
#' objects. Objects with the same serialisation have the same digest; the
#' converse holds only up to the chance of a collision.
#'
#' It is a fingerprint, not a cryptographic digest and not a durable content
#' address: fast, and well distributed, but not a defence against someone
#' choosing inputs to collide. The digest is the same on every platform R
#' runs on, with two exceptions:
#'
#' * On 32-bit x86 a double `NA` is stored with different bits, so an object
#'   holding one hashes differently there.
#' * R serialises a string as its bytes and its encoding mark, so the same
#'   non-ASCII text hashes differently when it is marked UTF-8, latin1 or
#'   not at all; and unmarked text is in the session's native encoding,
#'   which depends on the locale. `identical()` compares the text, not the
#'   mark, so `identical(x, y)` does not imply equal digests. Apply
#'   [enc2utf8()] to the strings first for a digest that does not depend on
#'   either.
#'
#' `version = 2`, the default, writes compact sequences such as `1:10` as the
#' vectors they are; `version = 3` writes their compact form, so the same
#' values may then hash differently.
#'
#' @param x Any R object.
#' @param algo `"xxh3_64"` for a 64-bit digest, `"xxh3_128"` for 128 bits.
#' @param version The serialisation format version, 2 or 3.
#' @param seed A whole number from 0 to 2^53, the XXH3 seed.
#' @return A string of 16 or 32 lower-case hexadecimal digits.
#' @export
#' @examples
#' bin_hash_object(mtcars)
#' bin_hash_object(mtcars, "xxh3_128")
#' identical(bin_hash_object(1:3 + 0L), bin_hash_object(c(1L, 2L, 3L)))
bin_hash_object <- function(x, algo = c("xxh3_64", "xxh3_128"), version = 2L, seed = 0) {
  algo <- arg_match(algo)
  version <- check_version(version)
  if (!is.numeric(seed) || length(seed) != 1L || is.na(seed) || seed < 0 || seed > 2^53 ||
        seed != floor(seed)) {
    invalid_argument("`seed` must be a whole number from 0 to 2^53.", arg = "seed")
  }
  .Call(zubin_hash_object, x, if (algo == "xxh3_64") 64L else 128L, version, as.double(seed))
}

check_version <- function(version, call = sys.call(-1L)) {
  if (!is.numeric(version) || length(version) != 1L || !version %in% c(2, 3)) {
    invalid_argument("`version` must be 2 or 3.", arg = "version", call = call)
  }
  as.integer(version)
}

check_flag <- function(x, arg, call = sys.call(-1L)) {
  if (!is.logical(x) || length(x) != 1L || is.na(x)) {
    invalid_argument(sprintf("`%s` must be TRUE or FALSE.", arg), arg = arg, call = call)
  }
}

check_refhook <- function(refhook, call = sys.call(-1L)) {
  if (!is.null(refhook) && !is.function(refhook)) {
    invalid_argument("`refhook` must be NULL or a function.", arg = "refhook", call = call)
  }
}
