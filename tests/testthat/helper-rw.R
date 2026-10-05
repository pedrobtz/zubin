# Drivers for the rw.h and cursor.h harness (src/zubin_test.c).

rw_read <- function(type, endian, x) {
  .Call(zubin_test_rw, type, endian, x)
}

rw_write <- function(type, endian, values) {
  .Call(zubin_test_rw_write, type, endian, values)
}

cursor_run <- function(x, plan) {
  .Call(zubin_test_cursor, x, plan)
}

rw_types <- c("u8", "i8", "u16", "i16", "u32", "i32", "u64", "i64",
              "f16", "bf16", "f32", "f64")
rw_width <- c(u8 = 1L, i8 = 1L, u16 = 2L, i16 = 2L, u32 = 4L, i32 = 4L,
              u64 = 8L, i64 = 8L, f16 = 2L, bf16 = 2L, f32 = 4L, f64 = 8L)

# A cursor step for a type and order: "u8" has none, the rest end in le/be.
cursor_step <- function(type, endian) {
  if (rw_width[[type]] == 1L) type else paste0(type, endian)
}

# Reference decoders written from the IEEE 754 definitions, independent of
# zufast: binary16 by its fields, bfloat16 as the top half of a binary32.
ref_f16 <- function(u) {
  s <- ifelse(bitwAnd(u, 0x8000L) != 0L, -1, 1)
  e <- bitwAnd(bitwShiftR(u, 10L), 0x1FL)
  m <- bitwAnd(u, 0x3FFL)
  ifelse(e == 0L, s * m * 2^-24,
         ifelse(e == 31L, ifelse(m == 0L, s * Inf, NaN),
                s * (1 + m / 1024) * 2^(e - 15L)))
}

ref_bf16 <- function(u) {
  raw4 <- rbind(as.raw(0L), as.raw(0L), as.raw(bitwAnd(u, 0xFFL)), as.raw(bitwShiftR(u, 8L)))
  readBin(as.vector(raw4), "double", n = length(u), size = 4L, endian = "little")
}

u16_bytes <- function(u, endian) {
  writeBin(as.integer(u), raw(), size = 2L, endian = if (endian == "le") "little" else "big")
}

base_endian <- c(le = "little", be = "big")

# A record of one field of each kind, for the cursor tests.
mixed_steps <- c("u8", "u16le", "u32be", "i64le", "f32be", "f64le", "f16le",
                 "bf16be", "i8", "i16be", "u64be", "i32le")
mixed_widths <- c(1, 2, 4, 8, 4, 8, 2, 2, 1, 2, 8, 4)
mixed_bytes <- function() {
  c(rw_write("u8", "le", 200L), rw_write("u16", "le", 65000L),
    rw_write("u32", "be", 4e9), rw_write("i64", "le", rw_read("i64", "le", bytes("fe ff ff ff ff ff ff ff"))),
    rw_write("f32", "be", 1.5), rw_write("f64", "le", -0.1), rw_write("f16", "le", 0.5),
    rw_write("bf16", "be", -2), rw_write("i8", "le", -7L), rw_write("i16", "be", -300L),
    bytes("00 00 01 00 00 00 00 00"), rw_write("i32", "le", -123456L))
}
