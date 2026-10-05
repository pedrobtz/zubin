# zubin — After 0.1.0

Companion to [design.md](design.md) revision 2 and [roadmap.md](roadmap.md). Section
references (§) point to the design. Date: 2026-10-04.

This file holds what draft 1 of the design planned as releases 0.2 to 1.0, the ideas from
`ideas.md` and `domains.md` that touch zubin, and the packages that would be built on it.
Nothing here is promised. The rules:

1. **Additions only.** Within major version 1 the headers gain functions, types and struct
   fields and lose nothing (§4.4). Every item below is a new header area or a new `bin_`
   function, never a change to a shipped one.
2. **Every item has a trigger**, and the trigger is a consumer or a user that exists, not a
   use case that could. An item whose trigger has not fired is not scheduled.
3. **Candidates are decided when the stage before them has shipped** (zucrypt's rule), with
   the decision recorded here, so the plan says "waits" rather than slipping silently.
4. **One minor release per stage**, each with its own roadmap stage, gates and NEWS entry.
   Stages below are sketched to the level needed to see their dependencies, not to the
   level of roadmap.md.

## The ladder

| Release | Contents | Trigger | Needs |
|---|---|---|---|
| **0.2.0** | variable-length fields; the cursor-style reader from R; the Python `struct` translator | a format with length-prefixed or NUL-terminated fields that someone is reading in R (ISO 8583, SBE var data, copybook `OCCURS DEPENDING ON`); or rdz's C rewrite | 0.1.0 |
| **0.3.0** | zero-copy views, typed reinterpretation views, byte search and splitting over them, zero-copy `bin_take()` | a reader that holds a file larger than it wants to copy (zucsv; rdz's selective reads) | 0.1.0 |
| **0.4.0** | memory-mapped files; custom connections over views and builders | the same reader, past memory; connection-only consumers (`read.csv`, `readRDS`) over a view | 0.3.0 |
| **when asked** | nanoarrow bridge; bitfields; `blob` and `float` outputs; a `bigint` for `u64`; the POD-in-`RAWSXP` helper; explicit offsets and unions in the grammar | one named consumer each | varies |
| **1.0.0** | the header API frozen | a real C consumer, not the fixture, has shipped against the headers | any of the above |

## 0.2.0 — Variable-length fields and the cursor reader

**What.** `z` (NUL-terminated string) and `p8`, `p16`, `p32` (length-prefixed string or
bytes) as layout field types, with `zb_type` values from the reserved range 17–31. A
layout with a variable field has no fixed size, so unpack and pack run **record-major**:
one cursor walks each record, and the kernels become per-field reads into pre-grown
columns. `bin_size()` of such a layout is `NA`, `stride` is refused, `n = NULL` reads to
the end. The field-major path of 0.1.0 is untouched; the two paths share the type model and
the tests, and `bin_unpack()` chooses by `is_fixed`.

Alongside it, the imperative counterpart to vectorised unpack, for formats whose structure
depends on what was just read (TLV, chunked containers, type-tagged messages):

```r
cur <- bin_cursor(x, offset = 0)
bin_read(cur, "u32")            # advances; one value, or a vector with n =
bin_read(cur, layout)           # one record as a one-row frame
bin_tell(cur); bin_seek(cur, pos); bin_skip(cur, n)
```

The cursor is an external pointer over a borrowed raw vector (kept alive as an attribute,
zuxml's node-handle trick), and every read is bounds-checked with a `zubin_bounds_error`
carrying the position. It is `zb_cur` from R, nothing more.

Also here, because it is thirty lines and Python users ask for it first:
`bin_layout_struct("<IHd")`, a translator from `struct` format strings to a `zubin_layout`.

**Why not in 0.1.0.** A second kernel path and a second R object, each needing their own
sweeps; 0.1.0 ships one path completely (design decision 7).

**Grammar reservations to honour.** `:` after a width is reserved for bitfields; `@` after
a type is reserved for an explicit offset (`name:u32@12`), which COBOL `REDEFINES` and
C unions need; both are refused by the 0.1.0 parser with a message naming this file.

**Consumers.** ISO 8583 (`LLVAR`, `LLLVAR`), SBE var-data, copybooks, Postgres binary
`COPY`, and every chunked container.

## 0.3.0 — Views

**What.** Three ALTREP classes over the public ALTREP API only (`R_make_altraw_class`,
`R_make_altreal_class`, `R_make_altinteger_class`, `R_set_altrep_*`, `R_new_altrep`; never
`DATAPTR`, which is non-API; `DATAPTR_RO` and `DATAPTR_OR_NULL` where a pointer is read):

- `bin_view(x, offset, length)`: a `RAWSXP` whose `data1` is the parent raw and `data2` the
  offset; `Dataptr` returns `RAW(parent) + offset`; `Elt`, `Get_region` and
  `Extract_subset` work without materialising. Holding the parent raises its reference
  count, so a later modification of the parent copies and the view keeps the original.
- `bin_view_as(x, type, offset = 0, n = NULL, endian = "little")`: reinterpretation. An
  `f32` view is a `REALSXP` whose `Elt` loads four bytes and widens; `u32` likewise; `i32`
  an `INTSXP`. `Dataptr` must return a real pointer, so the first `DATAPTR` from a base
  function materialises into `data2` once; `Elt` and region iteration never do. Documented
  as the honest ALTREP trade-off: laziness is guaranteed for element access and region
  iteration, not for every base function.
- `bin_take(b, copy = FALSE)`: the builder's malloc block wrapped as a `RAWSXP` ALTREP
  with `release = free`, so a 1 GB build costs no second gigabyte. Uses `zb_buf_detach()`.

With views, the search functions draft 1 put in `search.h` earn their place: `bin_find(x,
pattern, all = TRUE, from = 0)` (0-based offsets, two-way `memmem`) and `bin_split(x,
delim)` returning a list of views, not copies; `grepRaw()` calls `RAW()` and materialises,
which is the whole difference. `zucsv`'s line splitting over a mapped file is the consumer.

**Serialisation policy.** Views serialise their bytes, never their state: a window onto
another R object has no identity outside the session. Only a mapping (0.4.0) may opt into
state.

**The one open design question, decided here and not before.** Draft 1 asked whether views
belong in a separate `zuview` package. Views add no dependency to zubin, and nothing in
0.1.0 or 0.2.0 depends on the answer. The decision is taken when this stage opens, on two
facts: whether CRAN's handling of ALTREP-heavy packages has changed, and whether any C
consumer links zubin by then (a consumer that does not want ALTREP code in its include
path is the argument for splitting).

## 0.4.0 — Memory mapping and connections

**What.**

- `bin_mmap(path, offset = 0, length = NULL, writable = FALSE, lazy_serialize = FALSE)`: a
  `RAWSXP` ALTREP over `mmap` (POSIX) or `MapViewOfFile` (Win32) with a finalizer that
  unmaps; `Dataptr(writeable = TRUE)` on a read-only mapping errors. Serialises its bytes
  by default; with `lazy_serialize = TRUE` serialises `(path, offset, length)` and re-maps
  on unserialise, erroring if the file is missing or shorter. `mmap` (Jeff Ryan) is the
  prior art to read for the Windows path.
- `bin_connection(view)`: a read-only, seekable custom connection over any raw vector or
  view through `R_new_custom_connection`, so `read.csv()`, `readRDS()`, `readLines()` and
  every connection-only reader consume a mapping or a slice without the copy
  `rawConnection()` makes on creation.
- `bin_builder_connection(b)`: a write-only connection appending to a builder, replacing
  the `rawConnection(raw(0), "w")` plus `rawConnectionValue()` pattern; `writeBin()`,
  `saveRDS()`, `write.csv()` and `cat()` all work against it.

R's connection API carries `R_CONNECTIONS_VERSION` and a declared instability; the
family's `zu_source.h` already guards on it, and these do the same. `R_new_custom_connection`
is not on the 4.6.1 non-API list.

**Consumers.** The 10 GB file use case in every reader; rdz's selective block reads;
shared-memory raw (domains.md §4.1) is this class with a `shm_open` backing and comes with
it if a parallel user asks.

## When asked

| Item | What | Trigger | Builds on |
|---|---|---|---|
| nanoarrow bridge | `as_nanoarrow_buffer(view)` wrapping a `zb_buf` as an `ArrowBuffer` with a release callback that drops the R reference, and `zb_buf_from_nanoarrow()`; the byte-level zero-copy bridge | a user moving bytes between Arrow and raw without a copy | 0.3.0, `Suggests: nanoarrow` |
| bitfields | `name:u32:12` fields; vectorised extraction with scale and offset | CAN/DBC decoding (domains.md §5.1), every signal is one | 0.1.0 grammar reservation |
| explicit offsets and unions | `name:u32@12`; overlapping fields | COBOL `REDEFINES`, C unions | 0.2.0 |
| `blob` and `float` outputs | `b<n>` columns as `blob`; `f32` as the `float` package's `float32` (bit-identical storage) | a user of either package asks | `Suggests` only |
| `bigint` for `u64` | an exact class for values at or above 2^63, following `zuyaml_bigint` / `cbor_bigint` | someone has such a file | 0.1.0 |
| POD in a `RAWSXP` | `zb_pod_alloc()`, `zb_pod_ptr()`, `zb_pod_check()`: plain-old-data state in a raw vector instead of an external pointer; copy-on-modify and serialisation for free; alignment and pointer caveats stated | a family package wants finaliser-free state | `zubin-r.h` |
| `bin_write()` in place | write one field into a copy of a raw vector at an offset | a patching use case | 0.1.0 |
| varints, zigzag, CRC32 | **zufast's**, on its §25 list; zubin's cursor and builder wrap them (`zb_cur_varint_u64()`, `zb_put_varint_u64()`) | a protobuf-shaped or Parquet-footer reader; rdz | zufast |

## 1.0.0 — The freeze

1.0.0 is a promise, not a feature count (zuyaml's framing). It asserts that the headers'
public surface is frozen under §4.4 and that the fourteen plus functions of the R API are
stable with a deprecation cycle. It is declared only after **a real C consumer, not the
fixture, has shipped against the headers**: rdz in C, zucbor's encoder, or zuhttp's buffer,
whichever comes first. zucrypt's review is the reason: an ABI frozen with no consumer is a
promise nobody tested. Until then every release stays 0.x and additive.

## Packages that would be built on zubin

From `ideas.md` and `domains.md`, each with what it needs from zubin and from where. None
is zubin's own scope; they are the demand this package exists to serve, and the list is
what decides the order of the ladder above.

| Package or idea | Source | Needs from zubin | Earliest | Notes |
|---|---|---|---|---|
| **rdz in C** | design §3.2 | layouts, builder, cursor, the serialisation sink (all 0.1.0, #26); views and mmap for selective reads | 0.1.0, then 0.3.0 and 0.4.0 for selective reads | The largest candidate consumer; what it needs is in 0.1.0 (#26) |
| COBOL copybooks | domains §1.1 | fixed layouts with `align`, `OCCURS` as arrays (0.1.0); `OCCURS DEPENDING ON` (0.2.0); `REDEFINES` (explicit offsets) | 0.2.0 | EBCDIC tables and packed decimal belong to the copybook package or zufast; `decimal` exists for exact values |
| ISO 8583 | domains §1.2 | cursor reader, `p*` fields | 0.2.0 | bitmap walk is `rawToBits()` |
| SBE / ITCH decoders | domains §2.1 | fixed layouts, type-byte dispatch (0.1.0); groups and var data (0.2.0); mmap (0.4.0) | 0.1.0 for ITCH | a schema compiler from SBE XML through `zuxml` |
| Postgres binary `COPY` | domains §3.3 | builder and cursor, big-endian | 0.2.0 | `decimal` for `numeric` |
| Parquet footer and bloom pruning | domains §3.2 | cursor; varints (zufast); remote ALTREP | 0.3.0 plus varints | `../carquet` (pure C Parquet) is the alternative route |
| Zarr | domains §3.4 | remote ALTREP; zukomp for codecs | 0.3.0 | |
| Remote byte-range ALTREP | domains §3.1 | the view infrastructure | 0.3.0 | its own package; `curl` and `quak`'s credentials |
| Shared-memory raw | domains §4.1 | the mapping class with a `shm_open` backing | 0.4.0 | |
| Mappable object format | ideas §1.1 | views, mmap, the serialisation policy | 0.4.0 | this *is* rdz's selective-read direction; one package, not two |
| Content-defined chunking and store | ideas §1.2 | cursor and builder; zufast hashing | 0.1.0 | `dastash`'s storage backend; nothing zubin must add |
| Bit-packed ALTREP integers; Gorilla/Chimp doubles | ideas §1.6, domains §2.2 | the ALTREP infrastructure and `zb_buf` | 0.3.0 | kernels from `simdcomp`; separate package |
| Protobuf wire decoder | ideas §1.5 | cursor; varints from zufast | 0.1.0 plus varints | the varint consumer zufast waits for |
| CAN/DBC | domains §5.1 | bitfields | bitfields item | |
| Shared-object inspector (ELF/Mach-O/PE imports) | ideas §1.7 | fixed layouts and `s<n>`, then `z` | 0.1.0, better at 0.2.0 | also a good vignette-sized demonstration of layouts |
| Kaitai-style declarative formats | ideas §1.4 | layouts, cursor, views; `zuyaml` for the spec | 0.2.0 | a subset of Kaitai's expression language, said so |
| Suffix arrays, StringZilla, binary fuse filters | ideas §1.7, §2 | raw in and out; views help | 0.1.0 | independent of zubin beyond the raw type |
| Byte-level BPE tokenizer | domains §6.1 | nothing from zubin; `zufast/utf8.h` | — | listed to say it is not zubin's |
| Byte-limited UTF-8 truncation | domains §6.2 | nothing; belongs in zufast | — | |
| Exact decimals | ideas §1.3 | nothing: `../decimal` exists (libmpdec) | — | zubin packs and unpacks its `i64` scales |
| Hash-chained tamper-evident log | domains §1.3 | builder and cursor; zucrypt for the hash | 0.1.0 | `dastash` |
| hexdump and diff | ideas §1.7 | — | **in 0.1.0** | |
| Fuzzing the headers | domains §7.1 | — | **in 0.1.0** | `zufuzz` is an empty skeleton; zubin's targets live in `tools/fuzz/` as zufast's do |

## Not planned, ever

From design §2: compression (zukomp), digests and encryption (zucrypt), arithmetic or
bitwise operators on raw (base R), `.rds` helpers (one line of base R), a serialisation
*format* (rdz), C++.

## Decisions this file leaves open, and when each is taken

| Decision | Taken when |
|---|---|
| Is rdz rewritten in C? | rdz's, before zubin 0.2.0 is planned, so the sink item can ride with it or wait |
| Views inside zubin or as `zuview`? | when 0.3.0 opens (above) |
| Does `bin_cursor()` return values or a value plus bytes consumed? | at 0.2.0 planning; the external-pointer design above makes "consumed" a `bin_tell()` difference and needs no second return |
| `u64` above 2^63: class, or stay an error? | when a file with one appears |
| Which views serialise lazily? | mappings only, decided above; revisit only if a consumer needs a lazy `bin_view()` |
