# Expectations shared by the pack and round-trip tests.

# bin_unpack(bin_pack(layout, cols), layout) gives cols back, in list mode
# and through a data frame, and packing the data frame gives the same bytes.
expect_roundtrip <- function(layout, cols, ...) {
  x <- bin_pack(layout, cols, ...)
  expect_identical(length(x), bin_size(layout) * pack_nrow(cols))
  back <- bin_unpack(x, layout, as = "list", ...)
  expect_identical(back, cols, label = layout$spec)
  df <- bin_unpack(x, layout, ...)
  expect_identical(bin_pack(layout, df, ...), x, label = layout$spec)
  invisible(x)
}

pack_nrow <- function(cols) {
  if (!length(cols)) return(0L)
  c1 <- cols[[1L]]
  if (is.matrix(c1)) nrow(c1) else length(c1)
}

expect_bytes <- function(x, hex) {
  expect_identical(x, bytes(hex))
}

# A random layout: 1 to 8 fields of every kind, arrays of the numeric
# types, padding, either byte order, packed or aligned.
random_layout <- function() {
  types <- c("u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f16", "bf16", "f32",
             "f64", "bool", "s", "b", "x")
  k <- sample(1:8, 1L)
  parts <- character()
  for (i in seq_len(k)) {
    t <- sample(types, 1L)
    if (t %in% c("s", "b", "x")) {
      tok <- paste0(t, sample(1:9, 1L))
    } else {
      tok <- t
      if (runif(1) < 0.25) tok <- paste0(tok, "[", sample(2:4, 1L), "]")
      if (!t %in% c("u8", "i8", "bool") && runif(1) < 0.25) tok <- paste0(tok, sample(c(".le", ".be"), 1L))
    }
    parts <- c(parts, if (t == "x") tok else paste0("f", i, ":", tok))
  }
  bin_layout(paste0(sample(c("<", ">"), 1L), paste(parts, collapse = " ")),
             align = runif(1) < 0.5)
}

# Exact-representable random values for one field, n records.
random_column <- function(type, count, n) {
  base <- sub("[0-9]+$", "", type)
  width <- as.integer(sub("^[a-z]+", "", type))
  m <- n * count
  v <- switch(type,
    u8 = sample(0:255, m, TRUE), i8 = sample(-128:127, m, TRUE),
    u16 = sample(0:65535, m, TRUE), i16 = sample(-32768:32767, m, TRUE),
    i32 = sample(c(-.Machine$integer.max, -1L, 0L, 1L, .Machine$integer.max,
                   sample.int(1e9, 20L)), m, TRUE),
    u32 = sample(c(0, 1, 2^32 - 1, round(runif(20, 0, 2^32 - 1))), m, TRUE),
    i64 = sample(c(-2^53, -1, 0, 2^53, round(runif(20, -2^53, 2^53))), m, TRUE),
    u64 = sample(c(0, 2^53, round(runif(20, 0, 2^53))), m, TRUE),
    f16 = rw_read("f16", "le", u16_bytes(sample(c(0:31743, 32768:64511), m, TRUE), "le")),
    bf16 = rw_read("bf16", "le", u16_bytes(sample(c(0:32639, 32768:65407), m, TRUE), "le")),
    f32 = { x <- rw_read("f32", "le", as.raw(sample(0:255, 4 * m, TRUE))); x[is.nan(x)] <- 0.5; x },
    f64 = c(stats::rnorm(m), -0, Inf, NA)[seq_len(m)],
    bool = sample(c(TRUE, FALSE), m, TRUE),
    NULL
  )
  if (base == "s") {
    pool <- c("", "a", "ab", "é", "xyz", "€", "hello", "abcdefgh")
    pool <- pool[nchar(pool, type = "bytes") <= width]
    return(sample(pool, n, TRUE))
  }
  if (base == "b") {
    return(lapply(seq_len(n), function(i) as.raw(sample(0:255, width, TRUE))))
  }
  if (count > 1L) v <- matrix(v, n, count)
  v
}

random_columns <- function(layout, n) {
  f <- layout$fields
  f <- f[!startsWith(f$type, "x"), ]
  cols <- lapply(seq_len(nrow(f)), function(i) random_column(f$type[i], f$count[i], n))
  names(cols) <- f$name
  cols
}
