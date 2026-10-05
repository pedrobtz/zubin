# Drivers for the layout harness, and expectations on spec errors.

c_layout <- function(spec, big = FALSE, align = FALSE, max_fields = 0L) {
  .Call(zubin_test_layout, spec, big, align, as.integer(max_fields))
}

# The position a spec error reports, from C directly and through R.
spec_error_at <- function(spec, align = FALSE) {
  c_pos <- c_layout(spec, align = align)$position
  e <- tryCatch(bin_layout(spec, align = align), zubin_spec_error = identity)
  stopifnot(inherits(e, "zubin_spec_error"))
  stopifnot(identical(e$position, c_pos))
  e$position
}

# Layouts of real formats, from their specifications.
format_layouts <- list(
  wav = "<riff:s4 size:u32 wave:s4 fmt:s4 fmt_size:u32 format:u16 channels:u16
         rate:u32 byte_rate:u32 block_align:u16 bits:u16 data:s4 data_size:u32",
  bmp_file = "<sig:s2 file_size:u32 reserved:u32 data_offset:u32",
  bmp_info = "<hdr_size:u32 width:i32 height:i32 planes:u16 bpp:u16 compression:u32
              image_size:u32 xppm:i32 yppm:i32 colors:u32 important:u32",
  png_ihdr = ">length:u32 type:s4 width:u32 height:u32 depth:u8 color:u8 compression:u8
              filter:u8 interlace:u8 crc:u32",
  java_class = ">magic:u32 minor:u16 major:u16 cp_count:u16",
  cfb = "<signature:b8 clsid:b16 minor:u16 major:u16 byte_order:u16 sector_shift:u16
         mini_sector_shift:u16 reserved:b6 dir_sectors:u32 fat_sectors:u32 first_dir:u32
         transaction:u32 mini_cutoff:u32 first_minifat:u32 minifat_sectors:u32
         first_difat:u32 difat_sectors:u32 difat:u32[109]",
  itch_add_order = ">type:s1 locate:u16 tracking:u16 ts:b6 ref:u64 side:s1 shares:u32
                    stock:s8 price:u32"
)
