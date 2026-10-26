# Byte-explicit inputs (roadmap, testing strategy): every test spells its
# bytes as hex, never as a source-file string literal.

# bytes("1a 2b", "3c") is as.raw(c(0x1a, 0x2b, 0x3c)); spaces are ignored.
bytes <- function(...) {
  h <- gsub("[[:space:]]", "", paste0(c(...), collapse = ""))
  if (!nzchar(h)) {
    return(raw(0))
  }
  stopifnot(nchar(h) %% 2L == 0L)
  as.raw(strtoi(substring(h, seq(1L, nchar(h), 2L), seq(2L, nchar(h), 2L)), 16L))
}

# utf8("c3 a9") is the one-character string of those bytes, marked UTF-8:
# non-ASCII text without a non-ASCII source literal, which would read
# differently in a session whose native encoding is not UTF-8.
utf8 <- function(...) {
  s <- rawToChar(bytes(...))
  Encoding(s) <- "UTF-8"
  s
}

# hex(as.raw(c(0x1a, 0x2b))) is "1a 2b".
hex <- function(x) {
  paste(format(as.hexmode(as.integer(x)), width = 2L), collapse = " ")
}

# The bit pattern of a double, so NaN payloads and -0 compare exactly.
bits <- function(x) {
  writeBin(as.double(x), raw(), size = 8L, endian = "little")
}
