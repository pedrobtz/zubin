# The golden vectors (design 16.4): bytes written from the formats'
# specifications, read identically on every platform, s390x included.

test_that("every golden vector decodes to its expected columns", {
  g <- golden()
  expect_gte(nrow(g), 30L)
  for (i in seq_len(nrow(g))) {
    got <- bin_unpack(bytes(g$hex[i]), g$spec[i], as = "list")
    expected <- eval(parse(text = g$expected[i]), baseenv())
    expect_identical(got, expected, label = g$name[i])
  }
})

test_that("the golden headers read the same as data frames, one row each", {
  g <- golden()
  for (name in c("wav", "png_ihdr", "java_class", "itch_add_order", "sbe_header")) {
    row <- g[g$name == name, ]
    df <- bin_unpack(bytes(row$hex), row$spec, n = 1)
    expect_s3_class(df, "data.frame")
    expect_identical(nrow(df), 1L, label = name)
  }
})
