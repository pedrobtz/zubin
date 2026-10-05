# a layout prints as a table

    Code
      print(bin_layout("<ts:i64 price:f64 qty:i32 side:u8 x3 crc:u32.be"))
    Output
      <zubin_layout: 28 bytes, 6 fields>
       name  type count size offset endian
       ts    i64  1     8     0     little
       price f64  1     8     8     little
       qty   i32  1     4    16     little
       side  u8   1     1    20           
             x3   1     3    21           
       crc   u32  1     4    24     big   

---

    Code
      print(bin_layout("a:u8 b:u32 c:u16", align = TRUE))
    Output
      <zubin_layout: 12 bytes, 3 fields, aligned to 4>
       name type count size offset endian
       a    u8   1     1    0            
       b    u32  1     4    4      little
       c    u16  1     2    8      little

