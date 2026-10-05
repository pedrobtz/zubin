# cursor.h (design 10): checked sequential reads that never move on failure.

test_that("a mixed record reads back field by field", {
  r <- cursor_run(mixed_bytes(), mixed_steps)
  expect_identical(r$status, rep(0L, 12L))
  expect_identical(r$pos, cumsum(mixed_widths))
  expect_identical(r$value, c(200, 65000, 4e9, -2, 1.5, -0.1, 0.5, -2, -7, -300, 2^40, -123456))
})

test_that("every truncation point fails at the right field and leaves the cursor unchanged", {
  x <- mixed_bytes()
  ends <- cumsum(mixed_widths)
  for (k in 0:(length(x) - 1L)) {
    r <- cursor_run(x[seq_len(k)], mixed_steps)
    j <- which(ends > k)[1L]
    before <- if (j == 1L) 0 else ends[j - 1L]
    expect_identical(r$status[seq_len(j - 1L)], rep(0L, j - 1L))
    expect_identical(r$status[j], 2L)
    expect_identical(r$pos[j], before)
    expect_true(r$untouched[j])
    failed <- r$status != 0L
    expect_true(all(r$untouched[failed]))
    expect_identical(r$status[failed], rep(2L, sum(failed)))
    # a failed step never moves the cursor
    prev <- c(0, r$pos)[seq_along(r$pos)]
    expect_identical(r$pos[failed], prev[failed])
  }
})

test_that("every type at every position reads or refuses without moving", {
  for (len in 0:9) {
    x <- as.raw(seq_len(len))
    for (type in rw_types) {
      w <- rw_width[[type]]
      for (e in c("le", "be")) {
        step <- cursor_step(type, e)
        for (p in 0:len) {
          r <- cursor_run(x, c(sprintf("seek:%d", p), step))
          expect_identical(r$status[1L], 0L)
          if (len - p >= w) {
            expect_identical(r$status[2L], 0L)
            expect_identical(r$pos[2L], as.double(p + w))
          } else {
            expect_identical(r$status[2L], 2L, label = paste(type, e, len, p))
            expect_identical(r$pos[2L], as.double(p))
            expect_true(r$untouched[2L])
          }
        }
      }
    }
  }
})

test_that("seek, skip and bytes refuse to pass the end", {
  x <- bytes("01 02 03 04")
  r <- cursor_run(x, c("seek:4", "seek:5", "seek:1", "skip:3", "skip:1", "seek:2",
                       "bytes:2", "bytes:1", "seek:1", "bytes:4", "bytes:0"))
  expect_identical(r$status, c(0L, 2L, 0L, 0L, 2L, 0L, 0L, 2L, 0L, 2L, 0L))
  expect_identical(r$pos, c(4, 4, 1, 4, 4, 2, 4, 4, 1, 1, 1))
  # bytes reports the borrowed offset; a refused borrow leaves *p untouched
  expect_identical(r$value[c(7L, 11L)], c(2, 1))
  expect_true(all(r$untouched[c(8L, 10L)]))
})

test_that("an empty cursor reads nothing and borrows zero bytes", {
  r <- cursor_run(raw(0), c("bytes:0", "u8", "seek:0", "skip:0", "skip:1", "f64be"))
  expect_identical(r$status, c(0L, 2L, 0L, 0L, 2L, 2L))
  expect_identical(r$pos, rep(0, 6L))
})
