# Exploration notes — raw bytes in R

Companion to `design.md` (zubin). Ideas to explore and small C libraries worth vendoring in `zu*`-style packages (header-only or single-file, permissive licence, no C++ runtime). Nothing here is committed; it is a shortlist to pick from.

Date: 2026-10-04

---

## 1. Original ideas

### 1.1 Memory-mappable object format ("rkyv for R")

**What.** A serialisation layout where every R vector sits at an 8-byte-aligned offset behind a table of contents, so that `unserialize_from(mmap_raw(path))` returns ALTREP vectors pointing *into* the mapping instead of copying. A 20 GB list costs nothing until an element is touched; the OS pages data in on demand.

**How.**
- Header + TOC (type, length, offset, attributes offset) per node; recursive for lists.
- Numeric/integer/logical/raw vectors: ALTREP views over the mapping (`view_as()` from zubin).
- Character vectors: a byte arena of UTF-8 strings plus an offset table; an ALTREP `STRSXP` whose `Elt` creates the `CHARSXP` on first access. This sidesteps R's global string cache for huge string columns (100M strings no longer cost 56+ bytes each up front).
- Attributes materialised eagerly (they are small).
- `Dataptr` on a view materialises once; document which base functions trigger it.

**Builds on.** zubin views, mmap, serialization policy; zufast for hashing the TOC.

**Prior art.** arrow's ALTREP wrappers over Arrow arrays (strings included, since arrow 6.0); `fst` (columnar, not general objects); rkyv (Rust), FlatBuffers, Cap'n Proto.

**Risks.** Lifetime of views when the mapping is closed; writes to views; CRAN's view of ALTREP-heavy packages.

**Where it would land.** amber v2 (or `amber_lazy`).

### 1.2 Content-defined chunking + content-addressed store

**What.** Split byte streams into variable-size chunks at content-determined boundaries (FastCDC, gear rolling hash — about 60 lines of C), hash each chunk (BLAKE3 or XXH3-128), store chunks by hash. Two versions of a serialised data frame that differ in a few rows share almost all chunks.

**Gives.**
- Chunk-level dedup across artifact versions.
- Delta uploads to object storage (only new chunks).
- "What changed between these two objects" at byte level, cheaply.
- A Merkle-tree style manifest per artifact (list of chunk hashes), which is also a strong content identity.

**Builds on.** zubin builder/cursor, zufast hash.h, `qak` for the Azure side.

**Prior art.** borg, casync, restic, rsync's rolling checksum; Thiago Macieira's borg fork is a reminder that this pattern is common.

**Where it would land.** dastash storage backend; possibly a standalone `zudedup`.

### 1.3 Exact decimal vectors

**What.** R has no exact money type. Two levels:
- `decimal(scale = n)`: scaled int64 stored in a raw-backed or integer64-style vector with `scale` attribute; `+`, `-`, comparison exact; `*`, `/` with explicit rounding mode. `bit64` is the precedent for the representation and the S3 plumbing.
- IEEE 754-2008 decimal128 via `libmpdec` (Python's `_decimal`, BSD) or `decNumber` (ICU licence) for arbitrary-precision decimal arithmetic.

**Why.** XVA, FRTB and ledger work; every other finance stack (Python `decimal`, Java `BigDecimal`, Rust `rust_decimal`, DuckDB `DECIMAL`) has one. DuckDB ↔ R round-trips currently lose exactness through doubles.

**Builds on.** zubin `as_bytes`/`from_bytes` for i64; ALTREP for a lazy double view.

**Risks.** Operator dispatch and `data.frame` printing are more work than the arithmetic. Check what `qak`/DuckDB expose for `DECIMAL` columns first.

### 1.4 Kaitai-style declarative binary formats

**What.** A YAML format description (`seq` of typed fields, `repeat`, `switch-on` an enum, `instances` at absolute positions, nested `types`) compiled to zubin layouts plus a small interpreter for the parts layouts cannot express (conditionals, repeat-until, pointers). Kaitai Struct already has hundreds of specs: ELF, PNG, ZIP, SQLite3, Parquet footers, PCAP, EXIF.

**Gives.** `parse_format("sqlite3.ksy", mmap_raw(path))` returning nested lists/data frames, zero-copy for fixed parts.

**Builds on.** zuyaml for the spec, zubin layouts for the fixed parts, views for zero-copy.

**Risks.** Kaitai's expression language is larger than it looks; implement a subset and say so. Python's `construct` is a closer model for a library-style API.

### 1.5 Schema-less protobuf wire decoder

**What.** `protoc --decode_raw` in ~300 lines of C99: varints, zigzag, tags, length-delimited fields, nested messages guessed heuristically (valid sub-message vs. bytes vs. UTF-8 string). Returns a tree of field number → value(s).

**Why.** Inspecting unknown blobs (gRPC payloads, Delta Lake checkpoints carry protobuf-ish structures, telemetry) without a `.proto`. RProtoBuf links the full libprotobuf C++ runtime; this would be `LinkingTo`-light.

**Builds on.** zubin cursor.

**Extension.** Given a `.proto`, nanopb-style descriptors could turn the same decoder into a typed one.

### 1.6 Bit-packed / frame-of-reference ALTREP integer vectors

**What.** In-memory columnar compression for integer vectors, Arrow/Parquet style: delta encoding, frame-of-reference, bit packing (3-bit values packed 10 per 32-bit word), optional RLE. Stored in a raw vector; exposed as an ALTREP `INTSXP` with pay-per-`Elt` decode and block-wise `Get_region`.

**Gives.** A 100M-row small-int column at ~40 MB instead of 400 MB; `sum`, `mean`, subsetting and `head` without materialising; `Dataptr` materialises once when something insists.

**Builds on.** `simdcomp` for packing kernels; zubin views; the same ALTREP infrastructure as 1.1.

**Prior art.** `bit` (1-bit only), arrow (on Arrow arrays only), DuckDB's in-memory encodings.

### 1.7 Smaller ideas

- **`hexdump(x)` / `diff_bytes(a, b)`** — xxd-style output with offsets and an ASCII column, and the first N differing offsets between two raw vectors. Trivial, but R's raw printing is poor and this is used constantly when developing binary formats.
- **Suffix arrays over bytes** (via libsais): substring counting, longest repeated substring, LCP intervals, BWT. General-purpose text/bytes indexing that Biostrings does only for sequences.
- **Binary fuse filters** as an R package: probabilistic membership for anti-joins and dedup on large key sets, ~9 bits per key, no false negatives.
- **Shared-object inspector**: parse ELF/Mach-O/PE import tables from a raw vector (a Kaitai spec exists for each) to list which `R_*` symbols a compiled package uses — a cheap non-API audit, and a demo of 1.4. Adjacent to `fax`.
- **Delta-encoded artifact versions** with bsdiff or zstd `--patch-from` (`ZSTD_CCtx_refPrefix`) as the brute-force alternative to 1.2.

---

## 2. Small C libraries over bytes

Licence and repo as remembered; verify before vendoring. "Fit" says where it would plug in.

### Search, sort, strings

| Library | Shape | What it does | Fit |
|---|---|---|---|
| StringZilla (`ashvardanian/StringZilla`) | C99 header, Apache-2 | SIMD substring search, sorting, edit distance, hashing over byte strings | No R binding exists; also implements `find_bytes()` |
| libsais (`IlyaGrebnov/libsais`) | C, MIT | Suffix array, LCP, BWT construction; state of the art | Idea 1.7 (suffix arrays); BWT-based compression |
| utf8proc (JuliaLang) | C, MIT | Unicode normalisation, case folding, grapheme breaking | If zufast's `utf8.h` ever needs normalisation |
| yxml (`yorhel/yxml`) | C, MIT, ~1000 lines | Zero-allocation streaming XML parser | zuxlsx — the C99 answer to the pugixml/RapidXML evaluation |
| jsmn / yyjson | C, MIT | Tiny JSON tokenizer / fast full JSON parser | If a `zujson` is ever wanted |

### Compression and encoding

| Library | Shape | What it does | Fit |
|---|---|---|---|
| LZAV (`avaneev/lzav`) | Header-only, MIT | Very fast LZ77 compressor | Builder `take(compress = )`, if that non-goal is reversed |
| lz4 (`lz4/lz4`, `lz4.c`) | Single file, BSD-2 | LZ4 block/frame | Same |
| libdeflate (`ebiggers/libdeflate`) | C, MIT | Whole-buffer gzip/zlib/deflate, much faster than zlib, no streaming | Exactly the raw-vector shape; `memCompress` replacement |
| streamvbyte (`lemire/streamvbyte`) | C, Apache-2 | SIMD varint for integer streams | Index arrays, idea 1.6 |
| simdcomp (`lemire/simdcomp`) | C, BSD | SIMD bit packing / frame-of-reference | Idea 1.6 kernels |
| bitshuffle (`kiyo-masui/bitshuffle`) | C, MIT | Byte/bit transpose before compression; 2–3× better ratios on numeric columns | Pre-filter before zstd in amber |
| aklomp/base64 | C, BSD-2 | SIMD base64 | Only if zufast's base64 becomes a bottleneck |

### Hashing, integrity, crypto

| Library | Shape | What it does | Fit |
|---|---|---|---|
| BLAKE3 (`BLAKE3-team/BLAKE3`, `c/`) | C with SIMD dispatch, CC0/Apache-2 | Fast cryptographic hash, tree-hashable | Content addressing (idea 1.2) |
| rapidhash (`Nicoshev/rapidhash`) | Single header, BSD-2 | Very fast 64-bit non-crypto hash | Covered by XXH3 in zufast; alternative |
| Monocypher (`LoupVaillant/Monocypher`) | Single file, BSD-2/CC0, ~2000 lines | ChaCha20/XChaCha20, Poly1305, X25519, BLAKE2b, Argon2, EdDSA | The age package; lacks SHA-256/HKDF and scrypt, which age needs — pair with a small SHA-256 |
| crcany (Mark Adler) | Generator | Emits C code for any CRC polynomial | Checksums for amber/zubin containers |

### Serialisation formats

| Library | Shape | What it does | Fit |
|---|---|---|---|
| tinycbor (`intel/tinycbor`) | C, MIT | CBOR encoder/decoder (Thiago Macieira) | zucbor |
| flatcc (`dvidelabs/flatcc`) | C, Apache-2 | FlatBuffers compiler and runtime for C; zero-copy access | Same ALTREP opportunity as idea 1.1 |
| nanopb (`nanopb/nanopb`) | C, zlib | Protobuf for embedded; small runtime | Typed extension of idea 1.5 |
| bsdiff (`mendsley/bsdiff`, from Colin Percival) | C, BSD | Binary diff/patch | Idea 1.7 (delta versions) |

### Data structures and containers

| Library | Shape | What it does | Fit |
|---|---|---|---|
| xor_singleheader (`FastFilter/xor_singleheader`) | Header-only, Apache-2 | Xor and binary fuse filters | Idea 1.7 (filters) |
| stb_ds (`nothings/stb`) | Header-only, public domain | Dynamic arrays and hash maps in C | Prior art for the builder's growth policy |
| klib (`attractivechaos/klib`) | Header-only, MIT | `kvec`, `kstring`, `khash`, `ksort`, `kseq` | Same; `kstring` is a growable byte string |
| sds (`antirez/sds`) | C, BSD-2 | Simple Dynamic Strings with length header | Same |
| libart (`armon/libart`) | C, BSD | Adaptive radix tree over byte keys | Ordered byte-key maps, prefix search |
| CRoaring | C amalgamation, Apache-2 | Compressed bitmaps | Already bound; pairs with idea 1.6 |

### Numeric and misc

| Library | Shape | What it does | Fit |
|---|---|---|---|
| libmpdec (`bytereef.org/mpdecimal`) | C, BSD-2 | Arbitrary-precision decimal (Python's `_decimal`) | Idea 1.3 |
| decNumber | C, ICU | IEEE 754-2008 decimal | Idea 1.3 alternative |
| mini-gmp | Single file, LGPL (note licence) | Arbitrary-precision integers | Only if bigint is needed; the `gmp` R package links full GMP |
| QOI (`phoboslab/qoi`) | Single header, MIT | Lossless image codec, ~300 lines | uint8-raw-as-image use case |
| stb_image / stb_image_write | Header-only, public domain | PNG/JPEG/BMP decode/encode | Same |
| SIMDe (`simd-everywhere/simde`) | Header-only, MIT | Portable intrinsics (SSE/AVX/NEON emulation) | Writing SIMD kernels once for zufast/zubin; large |
| libpopcnt | Header-only, BSD-2 | SIMD popcount | Bit-level counting on raw |

---

## 3. Existing R packages to check before building

- `arrow` — ALTREP over Arrow arrays (idea 1.1 prior art), `binary`/`large_binary` columns.
- `nanoarrow` — buffers with release callbacks; the closest thing to a buffer protocol.
- `bit64` — int64 representation and S3 dispatch (idea 1.3 template).
- `bit` — 1-bit vectors (idea 1.6 prior art).
- `thor` — LMDB bindings (byte-keyed KV store, mmap-based).
- `tdigest`, `stringdist`, `textreuse` — sketches, edit distance, minhash already exist.
- `RProtoBuf`, `msgpackR`, `RcppMsgPack`, `jsonlite` — format coverage to avoid duplicating.
- `zstdlite`, `qs2`, `fst` — compression and serialisation baselines for benchmarks.
- `mmap` — pre-ALTREP memory mapping; worth reading for the Windows path.

---

## 4. Suggested order

1. zubin v0.1–v0.3 (design.md) — everything else sits on it.
2. `hexdump()` / `diff_bytes()` inside zubin — cheap, used daily.
3. StringZilla or libsais binding — small package, immediately useful, exercises views.
4. Idea 1.2 (chunking + content addressing) — unblocks dastash and reuses zufast hashing.
5. Idea 1.1 (mappable object format) — the big one; only after views are stable.
6. Idea 1.3 (decimals) — independent of the above; valuable at work.
7. Ideas 1.4–1.6 — opportunistic.
