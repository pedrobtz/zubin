# Layouts and the spec grammar (design 11.2-11.4, 13.2).

test_that("every type token parses with its width", {
  l <- bin_layout("a:u8 b:i8 c:u16 d:i16 e:u32 f:i32 g:u64 h:i64 i:f16 j:bf16 k:f32 l:f64
                   m:bool n:b3 o:s5 x7")
  f <- l$fields
  expect_identical(f$type, c("u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64", "f16",
                             "bf16", "f32", "f64", "bool", "b3", "s5", "x7"))
  expect_identical(f$size, c(1L, 1L, 2L, 2L, 4L, 4L, 8L, 8L, 2L, 2L, 4L, 8L, 1L, 3L, 5L, 7L))
  expect_identical(f$offset, c(0L, cumsum(f$size)[-16L]))
  expect_identical(bin_size(l), 62L)
  expect_identical(l$align, 1L)
  expect_identical(f$endian[c(1, 2, 13:16)], rep(NA_character_, 6L))
  expect_true(all(f$endian[3:12] == "little"))
})

test_that("arrays multiply the element width; count is the element count", {
  f <- bin_layout("xyz:f32[3] rgb:u8[3] flags:bool[2] x1")$fields
  expect_identical(f$count, c(3L, 3L, 2L, 1L))
  expect_identical(f$size, c(12L, 3L, 2L, 1L))
  expect_identical(f$offset, c(0L, 12L, 15L, 17L))
})

test_that("byte order: the argument, then a prefix, then a field suffix", {
  expect_identical(bin_layout("a:u16")$fields$endian, "little")
  expect_identical(bin_layout("a:u16", endian = "big")$fields$endian, "big")
  expect_identical(bin_layout("<a:u16", endian = "big")$fields$endian, "little")
  expect_identical(bin_layout(">a:u16 b:u16.le c:u32.be")$fields$endian, c("big", "little", "big"))
  expect_identical(bin_layout("=a:u16")$fields$endian, bin_info()$endian)
  expect_identical(bin_layout("a:u16", endian = "native")$fields$endian, bin_info()$endian)
  expect_identical(bin_layout("  >a:f64")$fields$endian, "big")
})

test_that("separators are whitespace or one comma", {
  ref <- bin_layout("a:u8 b:u16 c:u32")
  for (s in c("a:u8,b:u16,c:u32", "a:u8 , b:u16\t,\nc:u32", "  a:u8\n\nb:u16  c:u32  ",
              "a:u8, b:u16 c:u32")) {
    expect_identical(bin_layout(s)$fields, ref$fields, label = s)
  }
})

test_that("names may hold digits, underscores and dots; unnamed fields are V1, V2, ...", {
  f <- bin_layout("_a:u8 b.c:u8 d_1:u8")$fields
  expect_identical(f$name, c("_a", "b.c", "d_1"))
  f <- bin_layout("u8 x2 id:u32 f32")$fields
  expect_identical(f$name, c("V1", NA, "id", "V3"))
  expect_identical(names(bin_layout("u8 x2 id:u32 f32")), c("V1", "id", "V3"))
})

test_that("each grammar error is a spec error at the offending byte", {
  # unknown types
  expect_identical(spec_error_at("a:u7"), 2)
  expect_identical(spec_error_at("q8"), 0)
  expect_identical(spec_error_at("U32"), 0)
  expect_identical(spec_error_at("u08"), 0)
  expect_identical(spec_error_at("a:u8 b:f8"), 7)
  expect_identical(spec_error_at("boolean"), 0)
  expect_identical(spec_error_at("a:"), 2)
  # an array suffix on b, s or x
  expect_identical(spec_error_at("b8[3]"), 2)
  expect_identical(spec_error_at("v:s4[2]"), 4)
  expect_identical(spec_error_at("x2[2]"), 2)
  # an order suffix on a one-byte type, on b, s or x, or a bad one
  expect_identical(spec_error_at("u8.le"), 2)
  expect_identical(spec_error_at("i8.be"), 2)
  expect_identical(spec_error_at("bool.be"), 4)
  expect_identical(spec_error_at("b4.le"), 2)
  expect_identical(spec_error_at("u16.xx"), 3)
  expect_identical(spec_error_at("u16."), 3)
  expect_identical(spec_error_at("u16.l"), 3)
  # a zero or overlarge width or count
  expect_identical(spec_error_at("b0"), 1)
  expect_identical(spec_error_at("s0"), 1)
  expect_identical(spec_error_at("x0"), 1)
  expect_identical(spec_error_at("u8[0]"), 3)
  expect_identical(spec_error_at("u8[2147483648]"), 3)
  expect_identical(spec_error_at("b2147483648"), 1)
  # past 2^32 - 1: the parser must not wrap these back into range
  for (d in c("4294967295", "4294967296", "4294967297", "8589934593",
              "99999999999999999999999999999999")) {
    expect_identical(spec_error_at(paste0("x", d)), 1)
    expect_identical(spec_error_at(paste0("s", d)), 1)
    expect_identical(spec_error_at(paste0("b", d)), 1)
    expect_identical(spec_error_at(paste0("v:u8[", d, "]")), 5)
  }
  expect_identical(bin_layout("x2147483647")$fields$size, 2147483647L)
  expect_identical(bin_layout("v:u8[2147483647]")$fields$count, 2147483647L)
  expect_identical(spec_error_at("u8[]"), 3)
  expect_identical(spec_error_at("u8[3"), 4)
  # duplicate names, a name on padding, malformed names
  expect_identical(spec_error_at("a:u8 a:u16"), 5)
  expect_identical(spec_error_at("p:x3"), 0)
  expect_identical(spec_error_at("1a:u8"), 0)
  expect_identical(spec_error_at(".a:u8"), 0)
  expect_identical(spec_error_at(":u8"), 0)
  # a total size over 2^31 - 1
  expect_identical(spec_error_at("b2147483647 u8"), 12)
  expect_identical(spec_error_at("u64[268435456]"), 0)
  expect_identical(spec_error_at("u8 b2147483647", align = TRUE), 3)
  # fields that fit, and a final round-up to the record's alignment that
  # does not: the error is at the end of the spec
  expect_identical(bin_size(bin_layout("u32 b2147483643")), 2147483647L)
  expect_identical(spec_error_at("u32 b2147483643", align = TRUE), 15)
  # empty specs and stray separators
  expect_identical(spec_error_at(""), 0)
  expect_identical(spec_error_at("   "), 3)
  expect_identical(spec_error_at("<"), 1)
  expect_identical(spec_error_at("u8,"), 2)
  expect_identical(spec_error_at(",u8"), 0)
  expect_identical(spec_error_at("u8,,u8"), 3)
  expect_identical(spec_error_at("u8 , , u8"), 5)
  # anything else after a field
  expect_identical(spec_error_at("u8;"), 2)
  expect_identical(spec_error_at("u8u8"), 2)
  expect_identical(spec_error_at("u16]"), 3)
})

test_that("the largest record parses, and one more byte does not", {
  expect_identical(bin_size(bin_layout("b2147483646 u8")), 2147483647L)
  expect_identical(spec_error_at("b2147483646 u16"), 12)
})

test_that("a field array that is too small is a limit, not a spec error", {
  r <- c_layout("a:u8 b:u8 c:u8", max_fields = 2L)
  expect_identical(r$status, "ZB_ERR_LIMIT")
  expect_identical(r$position, 10)
  expect_identical(c_layout("a:u8 b:u8", max_fields = 2L)$status, "ZB_OK")
  # the bound never undercounts
  for (s in c("u8", "u8 u8", "a:u8,b:u8", "u8\n\n u8 , u8")) {
    r <- c_layout(s)
    expect_true(r$count_bound >= r$nfields, label = s)
  }
  # the bound counts runs of separators, so padding does not inflate it
  expect_identical(c_layout("  u8     u8 ,\t u8  ")$count_bound, 5)
  expect_identical(c_layout("u8 , u8")$count_bound, 2)
})

test_that("a duplicate name is found at its position in a wide spec", {
  spec <- paste(c(paste0("f", 1:5000, ":u8"), "f17:u16"), collapse = " ")
  e <- expect_error(bin_layout(spec), class = "zubin_spec_error")
  expect_identical(e$position, as.double(nchar(spec) - 7L))
  # names are checked once in R: the C parser is linear without the check
  r <- c_layout("a:u8 a:u16")
  expect_identical(r$status, "ZB_ERR_SPEC")
})

test_that("a layout of tens of thousands of fields parses and unpacks in linear time", {
  skip_if_no_slow_tests()
  spec <- paste0("f", 1:40000, ":u8", collapse = " ")
  # quadratic, this took seconds to parse and again on every unpack. Four
  # times the fields costs about four times as long when linear, sixteen
  # when quadratic: a ratio, so that a slow runner does not fail it.
  run <- function(k) {
    spec <- paste0("f", seq_len(k), ":u8", collapse = " ")
    system.time({
      l <- bin_layout(spec)
      cols <- bin_unpack(raw(k), l, as = "list")
    })[["elapsed"]]
  }
  small <- run(10000)
  expect_lt(run(40000), 10 * max(small, 0.05))
  expect_length(bin_unpack(raw(40000), spec, as = "list"), 40000L)
})

test_that("a field may not take the name of an array field's column", {
  # a:u8[2] is the columns a.1 and a.2 of a data frame
  for (spec in c("a:u8[2] a.1:u8", "a.2:u8 a:u8[2]", "a:f64[3] b:u8 a.3:u16")) {
    e <- expect_error(bin_layout(spec), class = "zubin_spec_error", label = spec)
    expect_identical(e$position, as.double(regexpr("a[.][0-9]", spec) - 1L), label = spec)
  }
  # past the count, or not a column number, is a different name
  for (spec in c("a:u8[2] a.3:u8", "a:u8[2] a.01:u8", "a:u8 a.1:u8", "a:s4 a.1:u8")) {
    expect_s3_class(bin_layout(spec), "zubin_layout")
  }
})

test_that("spec positions count bytes when names or values are not ASCII", {
  # e-acute is two bytes, so the space of "u8 x" after its name is byte 5
  spec <- "u8 x"
  names(spec) <- utf8("c3 a9")
  e <- expect_error(bin_layout(spec), class = "zubin_spec_error")
  expect_identical(e$position, 5)
  spec <- c(a = "u8", b = "u16 y")
  names(spec)[1] <- utf8("e2 82 ac")
  e <- expect_error(bin_layout(spec), class = "zubin_spec_error")
  expect_identical(e$position, 12)
})

test_that("an unnamed field's default name may not shadow an explicit one", {
  e <- expect_error(bin_layout("u8 V1:u16"), class = "zubin_spec_error")
  expect_identical(e$position, 3)
  expect_s3_class(bin_layout("V1:u8 u16"), "zubin_layout")   # V2, no clash
})

test_that("the R field table is the C one", {
  for (spec in c(unlist(format_layouts), "a:u8 x3 b:f32[2].be c:s4 bool")) {
    for (align in c(FALSE, TRUE)) {
      l <- bin_layout(spec, align = align)
      r <- c_layout(spec, align = align)
      expect_identical(l$fields$offset, r$offset)
      expect_identical(l$fields$size, r$size)
      expect_identical(bin_size(l), r$record_size)
      expect_identical(l$align, r$align)
      wide <- r$type_name %in% c("b", "s", "x")
      expect_identical(l$fields$type, ifelse(wide, paste0(r$type_name, r$size), r$type_name))
      named <- !is.na(r$name)
      expect_identical(l$fields$name[named], r$name[named])
    }
  }
})

test_that("real formats have the sizes and offsets their specifications give", {
  l <- lapply(format_layouts, bin_layout)
  expect_identical(bin_size(l$wav), 44L)
  expect_identical(l$wav$fields$offset, c(0L, 4L, 8L, 12L, 16L, 20L, 22L, 24L, 28L, 32L, 34L, 36L, 40L))
  expect_identical(bin_size(l$bmp_file), 14L)
  expect_identical(bin_size(l$bmp_info), 40L)
  expect_identical(l$bmp_info$fields$offset[c(4, 5, 11)], c(12L, 14L, 36L))
  expect_identical(bin_size(l$png_ihdr), 25L)
  expect_identical(l$png_ihdr$fields$offset, c(0L, 4L, 8L, 12L, 16L, 17L, 18L, 19L, 20L, 21L))
  expect_true(all(l$png_ihdr$fields$endian %in% c("big", NA)))
  expect_identical(bin_size(l$java_class), 10L)
  expect_identical(bin_size(l$cfb), 512L)
  expect_identical(l$cfb$fields$offset[c(3, 9, 18)], c(24L, 40L, 76L))
  expect_identical(l$cfb$fields$count[18], 109L)
  expect_identical(bin_size(l$itch_add_order), 36L)
  expect_identical(l$itch_add_order$fields$offset, c(0L, 1L, 3L, 5L, 11L, 19L, 20L, 24L, 32L))
})

test_that("align = TRUE places fields as the C compiler does", {
  cases <- .Call(zubin_test_struct_offsets)
  expect_length(cases, 12L)
  for (s in cases) {
    l <- bin_layout(s$spec, endian = "native", align = TRUE)
    if (s$has8 && .Machine$sizeof.pointer == 4L) {
      # 32-bit x86 aligns 8-byte struct members to 4 (design 11.4); the
      # layout keeps the 64-bit rule, which pads at least as much
      expect_gte(bin_size(l), s$size, label = s$spec)
      expect_identical(bin_layout(s$spec, align = TRUE)$align, 8L, label = s$spec)
      next
    }
    expect_identical(l$fields$offset, s$offsets, label = s$spec)
    expect_identical(bin_size(l), s$size, label = s$spec)
  }
})

test_that("explicit padding and alignment compose", {
  l <- bin_layout("a:u8 x1 b:u32 c:u8", align = TRUE)
  expect_identical(l$fields$offset, c(0L, 1L, 4L, 8L))
  expect_identical(bin_size(l), 12L)
  expect_identical(l$align, 4L)
  expect_identical(bin_layout("a:u8 b:u8", align = TRUE)$align, 1L)
})

test_that("the named-vector form is the string form", {
  expect_identical(bin_layout(c(a = "u8", "x3", b = "u32.be", c = "f32[2]")),
                   bin_layout("a:u8 x3 b:u32.be c:f32[2]"))
  expect_identical(bin_layout(c(a = "u8")), bin_layout("a:u8"))
  expect_identical(bin_layout(c("u8", "u16")), bin_layout("u8 u16"))
  e <- expect_error(bin_layout(c(a = "b:u8")), class = "zubin_spec_error")
  expect_identical(e$position, 3)
  expect_error(bin_layout(c(a = "u8", b = "u8 u8")), class = "zubin_spec_error")
  e <- expect_error(bin_layout(c(a = "u8", b = "u77")), class = "zubin_spec_error")
  expect_identical(e$position, 7)
})

test_that("a layout passes through unchanged, and its spec re-parses to itself", {
  for (spec in c(unlist(format_layouts), "u8 x2 id:u32 f32[3] b:u16.be c:s3")) {
    for (align in c(FALSE, TRUE)) {
      for (endian in c("little", "big")) {
        l <- bin_layout(spec, endian = endian, align = align)
        expect_identical(bin_layout(l), l)
        expect_identical(bin_layout(l$spec, align = align), l, label = l$spec)
      }
    }
  }
})

test_that("bin_layout() validates its other arguments", {
  expect_error(bin_layout(1), class = "zubin_invalid_argument")
  expect_error(bin_layout(NA_character_), class = "zubin_invalid_argument")
  expect_error(bin_layout(character()), class = "zubin_invalid_argument")
  expect_error(bin_layout("u8", align = NA), class = "zubin_invalid_argument")
  expect_error(bin_layout("u8", endian = "middle"), class = "zubin_invalid_argument")
})

test_that("length, names, as.data.frame and bin_size describe the layout", {
  l <- bin_layout("<id:u32 x4 xyz:f32[3] name:s8")
  expect_identical(length(l), 3L)
  expect_identical(names(l), c("id", "xyz", "name"))
  expect_identical(as.data.frame(l), l$fields)
  expect_identical(bin_size(l), 28L)
  expect_identical(l$spec, "<id:u32 x4 xyz:f32[3] name:s8")
})

test_that("a layout prints as a table", {
  expect_snapshot(print(bin_layout("<ts:i64 price:f64 qty:i32 side:u8 x3 crc:u32.be")))
  expect_snapshot(print(bin_layout("a:u8 b:u32 c:u16", align = TRUE)))
})

test_that("the type helpers name every type, and answer 0 and \"?\" outside the enumeration", {
  codes <- 0:40
  info <- .Call(zubin_test_type_info, codes)
  # type_tokens[i] is the token of value i; value 0 is not a type
  known <- c(FALSE, !is.na(type_tokens[1:40]))
  # R's table of tokens by type value is the C one
  expect_identical(info$name[known], type_tokens[codes[known]])
  expect_true(all(info$width[known] %in% c(1L, 2L, 4L, 8L, 16L)))
  expect_identical(info$align[codes == 34L | codes == 35L], c(4L, 8L))   # c64, c128
  expect_identical(unique(info$name[!known]), "?")
  expect_identical(unique(info$width[!known]), 0L)
  expect_identical(unique(info$align[!known]), 0L)
  # and a negative one
  expect_identical(.Call(zubin_test_type_info, -1L)$name, "?")
})
