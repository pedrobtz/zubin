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
    # 32-bit x86 aligns 8-byte struct members to 4; design 11.4 documents it
    if (s$has8 && .Machine$sizeof.pointer == 4L) next
    l <- bin_layout(s$spec, endian = "native", align = TRUE)
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
  expect_error(bin_layout("u8", endian = "middle"))
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
