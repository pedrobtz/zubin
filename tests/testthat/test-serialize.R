# bin_serialize(), bin_unserialize(), bin_hash_object() and the zubin-r.h
# streams behind them (Stage 10, issue #25).

test_that("bin_serialize() writes exactly what serialize() writes", {
  corpus <- serialize_corpus()
  for (name in names(corpus)) {
    x <- corpus[[name]]
    for (version in 2:3) {
      for (xdr in c(TRUE, FALSE)) {
        b <- bin_builder()
        bin_serialize(x, b, version = version, xdr = xdr)
        got <- bin_take(b)
        expect_identical(got, serialize(x, NULL, version = version, xdr = xdr),
                         label = paste(name, version, xdr))
        expect_identical(unserialize(got), x, label = paste(name, version, xdr))
        expect_identical(bin_unserialize(got), x, label = paste(name, version, xdr))
      }
    }
  }
})

test_that("an environment round-trips by contents", {
  e <- new.env()
  assign("a", 1:3, envir = e)
  assign("self", e, envir = e)
  b <- bin_builder()
  bin_serialize(e, b)
  back <- bin_unserialize(bin_take(b))
  expect_identical(sort(ls(back)), c("a", "self"))
  expect_identical(back$a, 1:3)
  expect_identical(back$self, back)          # the reference survives
})

test_that("bin_unserialize() reads a stream at an offset and leaves what follows", {
  x <- serialize_corpus()$frame
  for (k in c(0, 1, 7, 64)) {
    b <- bin_builder()
    bin_put(b, as.raw(seq_len(k) %% 256))
    bin_serialize(x, b)
    bin_put(b, bytes("aa bb cc"))
    expect_identical(bin_unserialize(bin_take(b), offset = k), x, label = k)
  }
})

test_that("the cursor advances past the stream, and is unchanged at every truncation point", {
  stream <- serialize(list(1:3, "x"), NULL)
  r <- .Call(zubin_test_unserialize_cursor, c(stream, bytes("ff ff")), 0)
  expect_identical(r$status, "ZB_OK")
  expect_identical(r$pos, as.double(length(stream)))
  expect_identical(r$value, list(1:3, "x"))
  for (cut in 0:(length(stream) - 1L)) {
    r <- .Call(zubin_test_unserialize_cursor, c(raw(3), stream[seq_len(cut)]), 3)
    expect_identical(r$status, "ZB_ERR_EOF", label = cut)
    expect_identical(r$pos, 3)
    expect_null(r$value)
  }
})

test_that("a stream that ends early is a bounds error with the offset and length", {
  stream <- serialize(mtcars, NULL)
  e <- expect_error(bin_unserialize(c(raw(4), stream[1:200]), offset = 4), class = "zubin_bounds_error")
  expect_identical(e$offset, 4)
  expect_identical(e$length, 204)
  expect_error(bin_unserialize(raw(0)), class = "zubin_bounds_error")
  expect_error(bin_unserialize(raw(3), offset = 4), class = "zubin_bounds_error")
})

test_that("R's own errors reach the caller as R raised them", {
  # not a stream: R's error, not one of zubin's
  e <- expect_error(bin_unserialize(bytes("51 0a 00 00")))
  expect_false(inherits(e, "zubin_error"))
  # a refhook's classed condition arrives with its class, both ways
  cnd <- structure(class = c("my_hook_error", "error", "condition"),
                   list(message = "the hook says no", call = NULL))
  hooked <- serialize(new.env(), NULL, refhook = function(x) "an-id")
  expect_error(bin_unserialize(hooked, refhook = function(id) stop(cnd)), class = "my_hook_error")
  b <- bin_builder()
  bin_put(b, bytes("01 02"))
  expect_error(bin_serialize(new.env(), b, refhook = function(x) stop(cnd)), class = "my_hook_error")
  expect_identical(as.raw(b), bytes("01 02"))
})

test_that("refhooks persist and restore references as in base R", {
  e <- new.env()
  assign("tag", "kept", envir = e)
  store <- new.env()
  out_hook <- function(x) if (is.environment(x) && identical(x, e)) "env-1" else NULL
  in_hook <- function(id) e
  b <- bin_builder()
  bin_serialize(list(e, 1), b, refhook = out_hook)
  bytes_ <- bin_take(b)
  expect_identical(bytes_, serialize(list(e, 1), NULL, refhook = out_hook))
  back <- bin_unserialize(bytes_, refhook = in_hook)
  expect_identical(back[[1]], e)
})

test_that("a builder that cannot hold the stream is a limit error and keeps its bytes", {
  b <- bin_builder(max = 100)
  bin_put(b, bytes("ab cd"))
  e <- expect_error(bin_serialize(mtcars, b), class = "zubin_limit_error")
  expect_identical(e$max, 100)
  expect_identical(e$size, 2 + length(serialize(mtcars, NULL)))
  expect_identical(as.raw(b), bytes("ab cd"))
  bin_serialize(1L, b)
  expect_identical(bin_unserialize(bin_take(b), offset = 2), 1L)
})

test_that("the sink form equals the builder form at every chunk size", {
  x <- serialize_corpus()[c("frame", "chr", "compact", "attrs")]
  for (version in 2:3) {
    for (xdr in c(TRUE, FALSE)) {
      full <- serialize(x, NULL, version = version, xdr = xdr)
      hl <- header_length(full, xdr)
      for (chunk in c(1:64, 100, 1000, 4096, 1e6)) {
        expect_identical(sink_bytes(x, version, xdr, FALSE, chunk), full,
                         label = paste(version, xdr, chunk))
      }
      for (chunk in c(1, 3, 13, 4096)) {
        expect_identical(sink_bytes(x, version, xdr, TRUE, chunk), full[-seq_len(hl)],
                         label = paste("skip", version, xdr, chunk))
      }
    }
  }
})

test_that("bin_hash_object() is the committed digest, on every R version", {
  x <- hash_fixture()
  # Committed on 2026-10-05 (R 4.6.1, macOS arm64); every CI leg, oldrel and
  # devel and big-endian s390x included, must reproduce them.
  expect_identical(bin_hash_object(x), "a0b4f6c369f76312")
  expect_identical(bin_hash_object(x, "xxh3_128"), "4967ba1cceedd163a0b4f6c369f76312")
  expect_identical(bin_hash_object(x, seed = 42), "27f4747ea8ac90d5")
})

test_that("bin_hash_object() hashes the stream after its header", {
  x <- serialize_corpus()$frame
  expect_identical(bin_hash_object(x), bin_hash_object(x, version = 2L))
  expect_match(bin_hash_object(x), "^[0-9a-f]{16}$")
  expect_match(bin_hash_object(x, "xxh3_128"), "^[0-9a-f]{32}$")
  expect_false(identical(bin_hash_object(x), bin_hash_object(x, seed = 1)))
  expect_false(identical(bin_hash_object(x), bin_hash_object(rev(x))))
  # the same values as an ordinary vector and as a compact sequence: equal in
  # version 2, which expands them, and not in version 3, which does not
  expect_identical(bin_hash_object(1:10), bin_hash_object(c(1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L, 10L)))
  expect_false(identical(bin_hash_object(1:10, version = 3L),
                         bin_hash_object(c(1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L, 10L), version = 3L)))
})

test_that("the serialization functions validate their arguments", {
  b <- bin_builder()
  expect_error(bin_serialize(1, b, version = 1), class = "zubin_invalid_argument")
  expect_error(bin_serialize(1, b, xdr = NA), class = "zubin_invalid_argument")
  expect_error(bin_serialize(1, b, refhook = "f"), class = "zubin_invalid_argument")
  expect_error(bin_serialize(1, raw(1)), class = "zubin_invalid_argument")
  expect_error(bin_unserialize("x"), class = "zubin_invalid_argument")
  expect_error(bin_unserialize(raw(1), offset = -1), class = "zubin_invalid_argument")
  expect_error(bin_hash_object(1, "md5"))
  expect_error(bin_hash_object(1, version = 4), class = "zubin_invalid_argument")
  expect_error(bin_hash_object(1, seed = -1), class = "zubin_invalid_argument")
})
