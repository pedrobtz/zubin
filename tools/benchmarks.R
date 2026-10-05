# Design 17: zubin against the base-R way of doing the same thing. Not run in
# CI as a gate; results go to .agents/benchmarks.md with the commit and the
# machine. Sizes scale with ZUBIN_BENCH_SCALE (1 = the design's sizes).
# Usage: tools/run-benchmarks, or Rscript tools/benchmarks.R from the root
# with zubin installed.
suppressPackageStartupMessages(library(zubin))
scale <- as.numeric(Sys.getenv("ZUBIN_BENCH_SCALE", "1"))
reps <- 3L

timed <- function(expr) {
  expr <- substitute(expr)
  env <- parent.frame()
  t <- vapply(seq_len(reps), function(i) {
    gc(FALSE)
    system.time(eval(expr, env))[["elapsed"]]
  }, double(1))
  stats::median(t)
}

results <- list()
add <- function(op, impl, n, secs) {
  results[[length(results) + 1L]] <<- data.frame(operation = op, implementation = impl,
                                                  n = n, seconds = secs)
}

## 1, 2. Records: 4 fields, 24 bytes (u32, f64, i32, f64), 10 M of them.
n <- as.integer(1e7 * scale)
rec <- bin_layout("<id:u32 price:f64 qty:i32 ts:f64")
df <- data.frame(id = as.double(seq_len(n)), price = seq_len(n) / 7, qty = seq_len(n) %% 1000L,
                 ts = 1.7e9 + seq_len(n))
x <- bin_pack(rec, df)

add("unpack 4 fields, 24-byte records", "bin_unpack()", n, timed(bin_unpack(x, rec)))
add("unpack 4 fields, 24-byte records", "readBin() per field", n, timed({
  m <- matrix(x, nrow = 24L)
  u <- readBin(as.vector(m[1:4, ]), "integer", n, size = 4L)
  data.frame(id = ifelse(u < 0, u + 2^32, u),
             price = readBin(as.vector(m[5:12, ]), "double", n, size = 8L),
             qty = readBin(as.vector(m[13:16, ]), "integer", n, size = 4L),
             ts = readBin(as.vector(m[17:24, ]), "double", n, size = 8L))
}))
add("pack 4 fields, 24-byte records", "bin_pack()", n, timed(bin_pack(rec, df)))
add("pack 4 fields, 24-byte records", "writeBin() per field", n, timed({
  id <- df$id
  id[id >= 2^31] <- id[id >= 2^31] - 2^32
  as.vector(rbind(matrix(writeBin(as.integer(id), raw()), 4L),
                  matrix(writeBin(df$price, raw()), 8L),
                  matrix(writeBin(df$qty, raw()), 4L),
                  matrix(writeBin(df$ts, raw()), 8L)))
}))
rm(x, df)

## 3. One column of f32.
n <- as.integer(1e8 * scale)
f <- writeBin(stats::runif(n), raw(), size = 4L)
add("decode f32", "bin_decode()", n, timed(bin_decode(f, "f32")))
add("decode f32", "readBin(size = 4)", n, timed(readBin(f, "double", n, size = 4L)))
rm(f)

## 4. Appends of 100 bytes.
n <- as.integer(1e6 * scale)
chunk <- as.raw(seq_len(100L) %% 256L)
add("append 100 bytes", "bin_put()", n, timed({
  b <- bin_builder()
  for (i in seq_len(n)) bin_put(b, chunk)
  bin_take(b)
}))
add("append 100 bytes", "rawConnection()", n, timed({
  con <- rawConnection(raw(0), "w")
  for (i in seq_len(n)) writeBin(chunk, con)
  r <- rawConnectionValue(con)
  close(con)
  r
}))
m <- as.integer(min(n, 2e4))
add("append 100 bytes", "c() accumulation", m, timed({
  r <- raw(0)
  for (i in seq_len(m)) r <- c(r, chunk)
  r
}))

## 5. A hex dump of 1 MiB.
n <- as.integer(2^20 * scale)
h <- as.raw(seq_len(n) %% 256L)
add("hexdump", "bin_hexdump()", n, timed(bin_hexdump(h)))
add("hexdump", "sprintf(\"%02x\")", n, timed(sprintf("%02x", as.integer(h))))

out <- do.call(rbind, results)
out$per_item_ns <- signif(out$seconds / out$n * 1e9, 3)
cat(sprintf("zubin %s on %s (%s), R %s, scale %s\n", bin_info()$version,
            Sys.info()[["sysname"]], Sys.info()[["machine"]],
            paste(R.version$major, R.version$minor, sep = "."), scale))
print(out, row.names = FALSE)
