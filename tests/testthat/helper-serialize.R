# The serialization corpus (issue #25): one of each kind of object R
# serializes differently. Strings in other encodings are built from bytes.
serialize_corpus <- function() {
  latin1 <- rawToChar(bytes("66 61 e7 61 64 65"))   # "façade" in latin1
  Encoding(latin1) <- "latin1"
  raw_bytes <- rawToChar(bytes("41 ff 42"))
  Encoding(raw_bytes) <- "bytes"
  utf8 <- rawToChar(bytes("e2 82 ac 31"))
  Encoding(utf8) <- "UTF-8"
  # a closure over the global environment, which serializes as a reference;
  # one over a test's environment would drag that environment in with it
  closure <- function(x, y = 2) x + y
  environment(closure) <- globalenv()
  list(
    null = NULL,
    int = c(1L, NA, -.Machine$integer.max),
    dbl = c(0.5, NA, NaN, -0, Inf, -Inf, 2^-1074),
    lgl = c(TRUE, NA, FALSE),
    cplx = complex(real = 1, imaginary = -2),
    chr = c("a", NA, "", utf8, latin1, raw_bytes),
    raw = as.raw(0:255),
    nested = list(a = 1, b = list(c = "x", d = list())),
    attrs = structure(1:6, dim = 2:3, dimnames = list(c("a", "b"), NULL), extra = "yes"),
    factor = factor(c("lo", "hi", "lo", NA)),
    frame = data.frame(x = 1:3, y = c("a", "b", "c")),
    compact = 1:100000,                              # ALTREP compact sequence
    closure = closure,
    global = globalenv(),
    formula = stats::as.formula("y ~ x + z", env = globalenv()),
    sym = quote(a_symbol),
    call = quote(f(x, 1L))
  )
}

# The fixture for the committed digest: plain data only, nothing whose
# serialization depends on the R that writes it (no closures, environments
# or compact sequences), and every double exact: R's parser on macOS arm64
# reads 1e-300 four ulps away from Linux's correctly rounded value, which
# made the first version of this fixture a different object per platform.
# No double NA either: NA_real_ has a signalling NaN's bits, which the x87
# unit of 32-bit x86 quiets when it loads them, so i386 serializes NA_real_
# as different bytes (the second version's lesson). NA stays in the integer,
# logical and character elements, whose bytes no FPU touches.
hash_fixture <- function() {
  list(
    ints = c(1L, -2L, NA, 2147483647L),
    dbls = c(0.25, -1.5, 2^-996),
    chr = c("zubin", "", NA),
    lgl = c(TRUE, FALSE, NA),
    raw = as.raw(c(0, 127, 255)),
    attrs = structure(list(a = 1L), class = "fixture", note = "stable"),
    factor = factor(c("b", "a", "b"))
  )
}

# The header's length in a stream R wrote: 14 bytes, and in version 3 the
# native encoding's length field and bytes after it.
header_length <- function(stream, xdr = TRUE) {
  v <- if (xdr) readBin(stream[3:6], "integer", size = 4L, endian = "big")
       else readBin(stream[3:6], "integer", size = 4L)
  if (v == 2L) return(14L)
  nelen <- if (xdr) readBin(stream[15:18], "integer", size = 4L, endian = "big")
           else readBin(stream[15:18], "integer", size = 4L)
  18L + nelen
}

sink_bytes <- function(x, version, xdr, skip, chunk) {
  .Call(zubin_test_sink, x, as.integer(version), xdr, skip, as.double(chunk))
}
