# Exploration notes — domains that live on bytes

Second brainstorming round, companion to `design.md` (zubin) and `ideas.md`. This one starts from domains that work in raw bytes and have thin or no R coverage, and asks what a zubin-based package could do there. Ordered by overlap with banking, market data and cloud storage work.

Date: 2026-10-04

---

## 1. Banking and payments

### 1.1 COBOL copybooks → zubin layouts

**What.** Mainframe extracts are the original fixed-width binary format. A copybook parser that emits zubin layouts, plus the decoders the layouts cannot express on their own:

- EBCDIC code pages (CP037, CP500, CP1047 and the national variants) → UTF-8.
- `COMP-3` packed decimal (BCD nibbles, sign nibble) and zoned decimal (overpunched sign in the last byte) → exact decimals (`ideas.md` §1.3) or doubles with a scale.
- `COMP` / `COMP-5` big-endian binary ints, `COMP-1`/`COMP-2` floats.
- `PIC` clauses with implied decimal points (`PIC 9(7)V99`), `SIGN LEADING SEPARATE`.
- `REDEFINES` (union views over the same bytes), `OCCURS` (array fields), `OCCURS DEPENDING ON` (variable records).
- RDW (record descriptor word) prefixes for VB files, fixed blocks for FB.

**Why.** Every bank has these feeds; the tooling is Spark (Cobrix), Python (`copybook`, `cobol-parser`) or vendor ETL. R has nothing. It is also the best stress test for layouts (unions, dependent arrays, decimals).

**Builds on.** zubin layouts, `as_bytes`/`from_bytes`, decimals; zufast for the EBCDIC tables if they are added there.

**Scope control.** Copybook grammar is larger than it looks (levels, `FILLER`, `RENAMES`, `JUSTIFIED`, `BLANK WHEN ZERO`). Implement the data-description subset and refuse the rest loudly.

### 1.2 ISO 8583 card-transaction messages

**What.** Message type indicator, then a 64-bit (optionally 128-bit) presence bitmap, then the present fields in order. Fields are fixed-length, `LLVAR` or `LLLVAR` (2- or 3-digit length prefix), encoded as BCD, EBCDIC or ASCII depending on the network. A parser needs a per-network field table (which is just a layout with variable fields) and the bitmap walk.

**Why.** Payments analytics, chargeback investigation, authorisation-log replay. Small spec, very large datasets, no R package.

**Builds on.** zubin cursor and variable-length fields; bit operations on raw for the bitmap.

### 1.3 Tamper-evident binary logs

**What.** An append-only log of amber frames where each record carries the BLAKE3 of the previous record (hash chain) and the content hashes of its inputs and outputs (from the chunking idea, `ideas.md` §1.2). Verification is a single pass and needs no server. Optionally sign the chain head with an age/X25519 key.

**Why.** Data lineage and audit requirements in banking; a lineage ledger that auditors can verify offline without a sidecar lineage system. Event sourcing for data pipelines.

**Builds on.** zubin builder and streams, zufast/BLAKE3, amber frames, dastash as the natural home.

---

## 2. Market data

### 2.1 SBE / ITCH decoders

**What.** CME MDP 3.0 uses Simple Binary Encoding (FIX community); Nasdaq TotalView-ITCH, OUCH and similar feeds use fixed binary messages with a message-type byte. SBE schemas are XML and map one-to-one onto zubin layouts (fixed fields, groups, variable data), so a schema compiler gives vectorised decoding; ITCH needs a hand-written table of ~20 message layouts.

**Gives.** Research-grade order-book reconstruction from archived feeds or PCAPs in R, without a C++ feed handler. Message-type dispatch is `switch` over a byte; group repetition is the `repeat` case from the Kaitai idea.

**Builds on.** zubin layouts and views; `find_bytes`/`split_at` for framing; mmap for multi-GB daily files.

**Prior art.** `simple-binary-encoding` reference codecs (Java/C++), Python `sbe` libraries, `kaitai` ITCH specs. None in R.

### 2.2 Gorilla / Chimp / ALP compression for doubles

**What.** XOR-based delta encoding of consecutive doubles (Facebook's Gorilla, improved by Chimp and Chimp128) and delta-of-delta for timestamps; ALP (adaptive lossless floating-point) for decimals-stored-as-doubles. A few hundred lines of C each.

**Gives.** 5–10× compression on prices and timestamps; the natural sibling of the bit-packed integer vectors (`ideas.md` §1.6) for an in-memory tick store, with ALTREP pay-per-touch decode.

**Prior art.** Gorilla paper (VLDB 2015), Chimp (VLDB 2022), ALP (SIGMOD 2024); implementations in InfluxDB, Prometheus, DuckDB. `pco` (Rust) was already looked at for numeric compression.

---

## 3. Cloud storage and file formats

### 3.1 Remote byte-range ALTREP

**What.** `remote_raw(url, block = 4 MiB, cache = tempdir())`: a raw vector whose `Get_region` issues HTTP Range requests against ADLS Gen2 / S3 / HTTP through a local block cache (on disk, LRU). `Elt` reads from the cache; `Dataptr` fetches everything (documented as the slow path).

**Gives.** Everything built on views then works on blobs without downloading them: layouts, the mappable object format, Parquet footers, Zarr chunks, archived feeds.

**Builds on.** zubin ALTREP infrastructure; `qak`'s credential handling for Azure; `curl` for the requests.

**Prior art.** fsspec's block cache, Arrow's `RandomAccessFile` over object storage, DuckDB's httpfs.

**Risks.** Credential refresh mid-read (qak already solves this); consistency if the blob changes (pin the ETag at open).

### 3.2 Parquet footer, statistics and bloom-filter pruning

**What.** Read only the footer (one Range request for the last few KB, one more for the metadata), decode the Thrift-compact metadata, evaluate per-row-group min/max statistics and the split-block bloom filters (xxHash-based, specified in the Parquet format) against a predicate, and return which files and row groups to touch.

**Why.** Delta Lake tables on object storage with hundreds of files; pruning before reading is the difference between seconds and minutes. nanoparquet reads metadata but not bloom filters; arrow reads them internally but does not expose the decision.

**Builds on.** remote_raw, zubin cursor (Thrift compact is varints and zigzag, the same primitives as the protobuf decoder), zufast XXH64 for the bloom hash.

### 3.3 Postgres binary COPY

**What.** `COPY ... WITH (FORMAT binary)` has a documented layout: signature, flags, per-tuple field count, per-field length-prefixed values in Postgres's binary send format (big-endian ints, IEEE floats, `numeric` as base-10000 digits, timestamps as microseconds since 2000-01-01, text as UTF-8). An encoder from a data frame and a decoder to one.

**Why.** Bulk load and extract several times faster than the text format, no quoting or locale issues; RPostgres does not offer binary COPY.

**Builds on.** zubin builder (encode) and cursor (decode); decimals for `numeric`.

### 3.4 Zarr

**What.** Chunked n-dimensional arrays on object storage or disk: a JSON metadata file plus one object per chunk, each compressed with a codec pipeline (blosc with shuffle/bitshuffle, zstd, lz4, zlib) and optional filters (delta, fixed-scale-offset). Zarr v3 adds sharding (many chunks per object with an index).

**Gives.** Lazy n-dimensional arrays over remote_raw with ALTREP decode per chunk; the same shape as the lazy object format but for scientific arrays.

**Prior art.** `Rarr` (Bioconductor), `pizzarr`; both partial. Python `zarr` is the reference.

**Builds on.** remote_raw, bitshuffle, libdeflate/lz4/zstd, zubin views.

---

## 4. Parallel R

### 4.1 Shared-memory raw

**What.** An ALTREP raw over POSIX `shm_open` + `mmap` (Linux/macOS) or a named file mapping (Windows), created by one process and attached by name in others. Read-only in attachers; the creator writes once, then publishes.

**Gives.** mirai or `parallel` workers map one dataset instead of each receiving a serialised copy; with the mappable object format (`ideas.md` §1.1) they get whole R objects for free. Fork provides this on Unix only, and never for non-forked or remote-launched workers.

**Builds on.** zubin mmap code (same ALTREP class, different backing); `nanonext` for publishing the name.

**Prior art.** `bigmemory` (numeric matrices only), Python's `multiprocessing.shared_memory`, Arrow Plasma (retired).

**Risks.** Lifetime (who unlinks the segment), Windows semantics, and the usual view-after-unmap problem.

---

## 5. IoT and instruments

### 5.1 CAN bus via DBC files

**What.** CAN frames are 8 bytes (64 for CAN FD); a DBC file describes each message's signals as bit position, bit length, Intel or Motorola byte order, signed/unsigned, scale, offset, unit, and multiplexing. Decoding a drive log is millions of frames through a bitfield extractor with scale and offset applied.

**Why.** Automotive and industrial telemetry analysis in R; the best real motivation for adding bitfields to the zubin layout spec (`design.md` open question 8), since every signal is one.

**Builds on.** zubin layouts with bitfields, vectorised over frames; `split_at` for log framing.

**Prior art.** Python `cantools`, `can-utils`. No R package.

---

## 6. Text

### 6.1 Byte-level BPE tokenizer in C99

**What.** Load a Hugging Face `tokenizer.json` (vocabulary, merges, pre-tokenizer regex, special tokens, byte-level encoding tables) and tokenize UTF-8 bytes: pre-tokenize with a regex, apply BPE merges by rank per piece, map to ids; decode back. GPT-2, Llama and most modern vocabularies are byte-level BPE; SentencePiece unigram models are a separate, later case.

**Why.** The `tok` package needs a Rust toolchain at install time; a C99 implementation installs anywhere and makes token counting, chunking for embeddings, and prompt budgeting available without Rust. Also the missing piece identified in the LLM-in-R gap list.

**Builds on.** zufast `utf8.h`; a regex engine for pre-tokenization (vendored PCRE2 subset or a hand-written matcher for the handful of patterns the common tokenizers use); a hash map keyed by byte pairs (klib `khash`).

### 6.2 Byte-limited UTF-8 truncation

**What.** `truncate_utf8(x, bytes, grapheme = FALSE)`: cut strings to at most *n* bytes without splitting a code point (or, with `utf8proc`, a grapheme cluster), returning properly marked UTF-8.

**Why.** Databases and APIs measure `VARCHAR(255)` in bytes; `substr()` counts characters; `strtrim()` counts width. Breaks pipelines writing to Oracle, Postgres with byte semantics, and most REST APIs. Trivial to implement, used constantly.

**Builds on.** zufast `utf8.h`. Could live in zufast rather than zubin.

---

## 7. Dev workflow

### 7.1 Fuzz the headers

**What.** Because zufast's and zubin's core headers are R-free C99, libFuzzer / AFL++ harnesses for the layout parser, cursor, UTF-8 validation, number parsing and any format decoder (copybook, ISO 8583, SBE, COPY) are a few lines each and run in CI with sanitizers. Corpus seeds from real files; crashes become regression tests.

**Why.** These parsers consume untrusted bytes. Very few R packages fuzz; the ones that parse binary input should. It also catches the unaligned-access and integer-overflow classes of bugs before CRAN's UBSAN run does.

**Builds on.** The header-only split in design.md §5 is what makes this cheap; keep it.

---

## 8. Suggested order (this file only)

1. Byte-limited UTF-8 truncation (6.2) — an afternoon, immediately useful at work.
2. Fuzzing harness (7.1) — set up once, protects everything after.
3. COBOL copybooks (1.1) — highest work value; also the best test of layouts, decimals and variable records.
4. Remote byte-range ALTREP (3.1) — unlocks 3.2, 3.4 and the lazy object format on blob storage.
5. Parquet pruning (3.2) — small once 3.1 exists; direct Delta Lake payoff.
6. SBE/ITCH (2.1) and Gorilla/Chimp (2.2) — if tick data research comes back.
7. Postgres binary COPY (3.3), ISO 8583 (1.2), shared-memory raw (4.1) — standalone, pick when needed.
8. BPE tokenizer (6.1), CAN/DBC (5.1), Zarr (3.4), hash-chained log (1.3) — larger or more speculative.
