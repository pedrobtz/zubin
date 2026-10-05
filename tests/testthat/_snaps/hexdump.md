# a known 64-byte input dumps as xxd does, at widths 16 and 8

    Code
      bin_hexdump(x)
    Output
      00000000: 070c 1116 1b20 252a 2f34 393e 4348 4d52  ..... %*/49>CHMR
      00000010: 575c 6166 6b70 757a 7f84 898e 9398 9da2  W\afkpuz........
      00000020: a7ac b1b6 bbc0 c5ca cfd4 d9de e3e8 edf2  ................
      00000030: f7fc 0106 0b10 151a 1f24 292e 3338 3d42  .........$).38=B

---

    Code
      bin_hexdump(x, width = 8)
    Output
      00000000: 070c 1116 1b20 252a  ..... %*
      00000008: 2f34 393e 4348 4d52  /49>CHMR
      00000010: 575c 6166 6b70 757a  W\afkpuz
      00000018: 7f84 898e 9398 9da2  ........
      00000020: a7ac b1b6 bbc0 c5ca  ........
      00000028: cfd4 d9de e3e8 edf2  ........
      00000030: f7fc 0106 0b10 151a  ........
      00000038: 1f24 292e 3338 3d42  .$).38=B

---

    Code
      bin_hexdump(x, offset = 37, n = 20)
    Output
      00000025: c0c5 cacf d4d9 dee3 e8ed f2f7 fc01 060b  ................
      00000035: 1015 1a1f                                ....

