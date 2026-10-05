# zubin — Design

**Revision 2, 2026-10-04.** Replaces draft 1 (same date), which described an unprefixed R
API (`layout()`, `unpack()`, `builder()`), a C layer that re-implemented zufast's byte
helpers, a serialisation and hashing layer built for a package called `amber`, and a
five-release roadmap ending in a 1.0 CRAN submission. Revision 2 adopts the `zu*` family
conventions, builds the C layer on `zufast`, names the consumers that exist, and cuts the
first release to what can ship complete and gated: **v0.1.0 is the first CRAN release.**
Everything draft 1 planned beyond that is in [next.md](next.md), each item with the trigger
that admits it. The reasoning behind each change is in the decision log (§19).

Sibling design documents this one is written against: `../zufast/.agents/design.md`
(§3 family rows, §4 header-only consumption, §21 gates), `../zukomp/.agents/design-zukomp.md`
(§13 memory and longjmp rules, §14 namespacing, §26 family table),
`../zucbor/.agents/design.md` (document shape, decision discipline),
`../zuhttp/src/zu_buffer.h` (the capped growable buffer this package generalises).
Where this document departs from them it says so.

Every statement here is a decision. Things not yet decided live in §19 and nowhere else.
[roadmap.md](roadmap.md) sequences the work; [next.md](next.md) holds what comes after.
Section references (§) in both point here.

---

## 1. What zubin is

zubin gives R what every systems-adjacent language has for bytes and R does not: a way to
describe a fixed binary record once and read or write millions of them in one call, a way to
convert typed vectors to and from bytes at any width and byte order, and a growable byte
buffer that is not quadratic. It is two products in one package:

1. **An R API over raw vectors**, prefixed `bin_`: typed record layouts
   (`bin_layout()`), vectorised unpack and pack (`bin_unpack()`, `bin_pack()`),
   homogeneous codecs (`bin_decode()`, `bin_encode()`), a byte builder
   (`bin_builder()`, `bin_put()`, `bin_take()`), and the two things anyone developing a
   binary format uses every day (`bin_hexdump()`, `bin_diff()`).
2. **A header-only C99 library** under `inst/include/zubin/`, consumed through
   `LinkingTo` alone: a byte buffer with ownership and limits, a checked cursor, typed
   reads and writes at any alignment and byte order, and a record-layout descriptor with
   vectorised unpack and pack kernels. It is the buffer and cursor every binary reader and
   writer in the family carries privately today (§3.2).

The one-line statement that governs every decision below:

> **Describe bytes once, in a layout; move them in bulk, with no copy that is not asked
> for and no value that is silently wrong.**

What zubin is not: a serialisation format, a compression library, a cryptography library, or
an arithmetic type over bytes. Each has its owner in the family (§2).

## 2. Scope

| | v0.1.0 | Later (next.md) | Never |
|---|---|---|---|
| Fixed-size record layouts: integers 8–64 bits, floats 16/32/64, bool, fixed bytes, fixed strings, padding, arrays, C alignment | yes | | |
| Variable-length fields (`z`, `p8/16/32`) and a cursor-style reader from R | | 0.2.0 | |
| Vectorised `bin_unpack()` / `bin_pack()`, `offset`/`n`/`stride` | yes | | |
| Homogeneous `bin_decode()` / `bin_encode()` with rounding to f16/bf16/f32 | yes | | |
| 64-bit integers as `double` (exact or error) or `integer64` | yes | `bigint`-style class for `u64` above 2^63 | |
| Byte builder with amortised growth, hard cap, typed vectorised appends | yes | zero-copy `bin_take()` | |
| `bin_hexdump()`, `bin_diff()` | yes | | |
| Header-only C API: buffer, cursor, typed rw, layout, unpack/pack kernels | yes | varints (through zufast), bitfields | |
| Byte search and delimiter splitting | | 0.3.0, with views | |
| Zero-copy views, typed reinterpretation views (ALTREP) | | 0.3.0 | |
| Memory-mapped files | | 0.4.0 | |
| Custom connections over views and builders | | 0.4.0 | |
| R serialisation into a builder, unserialise from an offset, version-stable object hash | | when rdz (§3.2) is rewritten in C | |
| nanoarrow buffer bridge | | when a consumer asks | |
| Compression, hashing as a digest, encryption | | | yes: zukomp, zucrypt |
| Arithmetic or bitwise operators on raw | | | yes: base R has them |
| `.rds` helpers | | | yes: one line of base R |
| C++, Rcpp, cpp11 | | | yes |

The admission test for anything new:

> Is it a byte-level primitive that a reader or writer of a binary format needs, that base R
> lacks or does badly, and that at least one real consumer (R user or family package) has
> asked for?

## 3. Position in the `zu*` family

### 3.1 The family rows

The five-repository table in the siblings' design documents (zukomp §26) is changed in all
repositories together or not at all, so zubin is not added to it here. These are zubin's
cells, to be merged at the next change:

| | zubin |
|---|---|
| Role | **provider, header mode** (as zufast), *and* an R API that is a product in its own right |
| R prefix | `bin_` |
| Info function | `bin_info()` |
| Root condition class | `zubin_error` |
| Public C prefix | `zb_` / `ZB_` |
| Registered table | none, by design (§4) |
| Static archive | none, by design (§4) |
| How a consumer links | `LinkingTo: zubin, zufast`; `#include <zubin.h>` |
| Consumers today | none on release; adoption issues filed (§3.2) |
| Vendored code | none |
| Symbols hidden (`$(C_VISIBILITY)`) | yes; `zubin.so` exports `R_init_zubin` and nothing else, audited |
| r-actions pin | commit, with the tag in a trailing comment |
| `Depends: R` | 4.1 |
| `LinkingTo` | `zufast (>= 0.1.0)` |

**Why `bin_`.** The family's R prefix is the package name minus `zu` (`fast_`, `komp_`,
`crypt_`, `json_`, `xml_`, `yaml_`, `cbor_`). Draft 1's unprefixed names also collide with
base R: `layout()` is `graphics::layout()`, attached in every session. `bin_` reads as the
domain ("binary"); `bytes_` and `raw_` were considered and rejected, the second because
`raw()` is base.

**Why `zb_`.** `zu_`/`ZU_` is zukomp's public prefix family-wide (zukomp §14); `zuf_`,
`zuc_`, `zux_` are taken. `zb_`/`ZB_` appears in no sibling's `src/` or `inst/include/`
(checked 2026-10-04).

### 3.2 Consumers, as decided rather than as hoped

zuxml's review names skipping this inventory as its most expensive mistake. The inventory
below is what exists on disk on 2026-10-04. No sibling can depend on zubin until zubin is on
CRAN, so each adoption is an issue in the consumer's repository, gated on the release; the
C API ships with a fixture consumer (§16.5) and no real one, and says so.

| Package | What it carries today | What zubin would replace | Mode | Status |
|---|---|---|---|---|
| **rdz** (`../rdz`, Rust, savvy) | A versioned seekable block container: 32-byte file header, 40-byte block headers, 36-byte directory header, 48/32/56-byte entries, all little-endian with CRC32; native typed block codecs; an R serialisation v3 XDR fallback stream that today allocates one complete raw vector | If rewritten in C (under consideration): every header and entry is a `bin_layout`; the forward writing pass is a `zb_buf`; bounded reading is a `zb_cur`; the fallback stream is the serialisation sink of next.md; selective reads are the views and mmap of next.md | C, header-only | **Candidate.** The decision to rewrite is rdz's; it is the trigger for next.md's serialisation items. CRC32 is not zubin's: it would be a zufast addition on rdz's request (zufast §25 lists CRC32C) |
| **zuhttp** | `src/zu_buffer.h`: a malloc-backed growable buffer with a hard cap and a `hit_limit` flag, `append_u64`, `consume`, `cstr` | `zb_buf` (§9), which adopts the cap and the flag | C | Adoption issue to file on release. zuhttp is not on CRAN |
| **zucbor** | `src/zu_encode.c`: a growing malloc buffer under a finalized external pointer; decoder heads read by hand | `zb_buf` through `zubin-r.h` (§12); `zb_cur` for heads | C | Adoption issue. zucbor's design says no C dependency without a reason; the reason is one fewer private buffer to audit |
| **zukomp** | `zu_int_add/mul/grow` checked arithmetic; a realloc sink under an external pointer | the same functions in `zb_buf` | C | Low value: zukomp's own are audited and shipped. No issue unless zukomp asks |
| **zuxlsx** | hand-rolled `le16()`/`le32()` in the CFB reader and the Agile decryptor | `zuf_load_le*` (already tracked as zuxlsx#71) and `bin_layout` for the CFB header | C and R | The CFB header is the worked big-format example in zubin's documentation |
| **zucsv** | — | `bin_split()` over views, later | R | 0.3.0 at the earliest |
| **mdbx**, **quak**, **dastash** | byte-keyed stores, Parquet and Delta on object storage, a content-addressed store | `bin_pack()` for keys; `bin_unpack()` on footers and manifests | R | R-level users once on CRAN; nothing to shape in advance |
| **decimal** (`../decimal`, libmpdec) | exact decimals | nothing: ideas.md §1.3 is already this package | — | zubin only has to pack and unpack `i64` scales |

Draft 1 named `amber` as the first consumer and shaped a serialisation layer around it. There
is no `amber`; the idea exists as rdz, in Rust. Nothing in v0.1.0 is shaped around a
consumer that has not decided.

### 3.3 Relationships to the other providers

- **zufast** owns the scalar byte primitives: endian loads and stores, byte swaps, f16 and
  bf16 conversion, UTF-8 validation, XXH3. zubin's headers include `<zufast/bits.h>` and
  `<zufast/utf8.h>` and add nothing those already do (§8). Varints and CRC32 belong there
  too, when a consumer asks (zufast §25); zubin's cursor will wrap them.
- **zukomp** owns compression. A builder does not compress; `komp_compress(bin_take(b))`
  is the composition.
- **zucrypt** owns digests. `bin_hexdump()` prints bytes; it does not hash them.
- **zucbor, zujson, zuyaml, zuxml** are self-describing formats. zubin describes formats
  that are not self-describing: the layout *is* the schema.
- **rdz** is a container format. zubin is what a container format is built from.

## 4. Consumption: header-only, `LinkingTo` alone

### 4.1 The decision

zubin's C layer is consumed by including a header, exactly as zufast is (zufast §4):

```
LinkingTo: zubin, zufast
```

Nothing else: no `Imports:`, no `importFrom()`, no `configure`, no `PKG_LIBS`. zubin need
not be installed at run time.

**Both packages must be listed.** `LinkingTo` is not transitive: R adds `<pkg>/include` to
the include path for each package named, and zubin's headers include zufast's. This is the
same shape as `LinkingTo: Rcpp, RcppArmadillo`, which every RcppArmadillo consumer carries,
and the fixture package of §16.5 is built this way so the recipe is tested, not assumed.
The alternative, zubin re-implementing `zuf_load_le32()` and friends so that its headers
stand alone, was draft 1's `rw.h`; the family does not keep two copies of the same
primitive (decision 3, §19).

### 4.2 Why not the siblings' other modes

zufast §4.2 makes the argument and it transfers whole: a registered table forbids inlining,
and inlining is the entire cost model of a four-byte load inside a ten-million-iteration
loop; a static archive needs a `configure` in every consumer. zubin's primitives allocate
only inside the buffer, whose allocation is the consumer's to own, and parse bounded input;
a bug is a wrong value or a refused buffer, which the "fix reaches the consumer on rebuild"
model already accepted for zufast covers.

### 4.3 What a consumer sees

```c
#include <zubin.h>            /* everything, including zufast's bits.h and utf8.h */
#include <zubin/buf.h>        /* or one area at a time */

zb_buf b;
if (zb_buf_alloc(&b, 4096, 0)) return ZB_ERR_MEMORY;      /* 0: no cap */
zb_put_u32le(&b, 0x1A5A4452u);
zb_put_bytes(&b, payload, n);
/* b.data, b.len are yours to read; release when done */
zb_buf_release(&b);
```

Every function is `static inline`. There is no implementation macro and nothing to link
(zufast decision 3). The only header that includes R is `zubin-r.h`, which a consumer
writing an R package includes by name (§12); it is outside the umbrella header.

### 4.4 The contract: source compatibility, not ABI

As zufast §4.4, within major version 1:

- a function, type, macro or enumerator that exists in a release exists in every later
  release with the same meaning;
- enumerator values are permanent (`zb_type`, `zb_status`);
- struct fields are never removed or reordered; fields may be appended, and every struct
  is initialised through a `zb_*` function, never by aggregate initialisation in the
  consumer, so an appended field has a defined value;
- a consumer built against an older zubin keeps working unchanged, because it carries its
  own copy of the code.

`ZUBIN_VERSION_MAJOR/MINOR/PATCH`, `ZUBIN_VERSION` and `ZUBIN_VERSION_NUMBER` follow
zufast's `version.h`. `zubin.h` checks `ZUFAST_VERSION_NUMBER >= 100` and `#error`s below
it.

### 4.5 Linkage rules

The four rules of zufast §4.5, inherited verbatim: every emitted symbol is `static`;
`static inline`, never plain `static`, for anything defined in a header; consumers compile
with `PKG_CFLAGS = $(C_VISIBILITY)`; the headers reference nothing `R CMD check` forbids
in compiled code.

One rule is zubin's own, because it has a function pointer in a struct and zufast does not:

5. **Never compare function pointers taken from a header-only function.** Every translation
   unit has its own `static` copy of `zb_int_release_malloc()`, so `b->release ==
   zb_int_release_malloc` is false across units. Ownership and growability are flags in
   `zb_buf` (§9.1), never inferred from `release`.

## 5. Header layout

```
inst/include/
├── zubin.h                  umbrella: version, status, rw, buf, cursor, layout
├── zubin-r.h                SEXP glue for R-package consumers; the only header that
│                            includes <Rinternals.h>; not included by zubin.h
└── zubin/
    ├── version.h            ZUBIN_VERSION_* macros
    ├── status.h             zb_status, zb_status_string()
    ├── rw.h                 §8  typed reads and writes over zufast's loads and stores
    ├── buf.h                §9  zb_buf: ownership, limits, growth, typed appends
    ├── cursor.h             §10 zb_cur: checked sequential reads
    ├── layout.h             §11 zb_type, zb_field, zb_layout, parse, unpack/pack kernels
    └── detail/
        └── portability.h    ZB_INLINE, ZB_STATIC_ASSERT; includes zufast's portability.h
```

Rules:

- Every header under `zubin/` compiles standalone as C99 and as C++11 against
  `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`, `<string.h>`, `<stdlib.h>` (for `malloc`,
  `realloc`, `free` in `buf.h` only) and the zufast headers, under
  `-Wall -Wextra -Wpedantic -Werror`.
- No R header and no `SEXP` under `zubin/`; `zubin-r.h` is gated separately (§16.1).
- No allocation anywhere except `buf.h`, and there only through the three libc calls above,
  so a consumer can audit "where can this allocate" by reading one file.
- No `printf`, `abort`, `exit`, `assert`, `rand`, `stdout`, `stderr` (zufast §4.5 rule 4).

## 6. Naming

| Layer | Prefix | Examples |
|---|---|---|
| R exports | `bin_` | `bin_layout()`, `bin_unpack()`, `bin_info()` |
| R classes | `zubin_` | `zubin_layout`, `zubin_builder`, `zubin_hexdump` |
| R condition classes | `zubin_` | `zubin_error`, `zubin_range_error` |
| Public C (installed headers) | `zb_` / `ZB_` | `zb_buf`, `zb_put_u32le()`, `ZB_U32`, `ZB_OK` |
| Internal helpers inside the headers | `zb_int_` / `ZB_INT_` | `zb_int_grow()` |
| Entry points and registration | `zubin_` | `R_init_zubin`, `zubin_unpack` (`.Call`) |
| Test-only `.Call` symbols | `zubin_test_` | `zubin_test_cursor_sweep()` |
| Fixture consumer package | `zubintest` | `tools/zubintest` |

`zb_int_` symbols are visible to consumers and outside the §4.4 promise.

## 7. Result model

```c
typedef enum {
    ZB_OK          = 0,
    ZB_ERR_INVALID = 1,   /* a precondition on arguments failed */
    ZB_ERR_EOF     = 2,   /* fewer bytes than the read or the layout needs */
    ZB_ERR_RANGE   = 3,   /* a value does not fit the target type */
    ZB_ERR_MEMORY  = 4,   /* allocation failed, or size arithmetic would overflow */
    ZB_ERR_LIMIT   = 5,   /* the buffer's hard cap was reached */
    ZB_ERR_SPEC    = 6,   /* a layout specification is malformed */
    ZB_ERR_NA      = 7    /* a value has no representation in the field (NA into u8) */
} zb_status;

static inline const char *zb_status_string(zb_status s);   /* never NULL */
```

`ZB_OK` is 0 and nothing is negative, so `if (st)` means "not success", as in every
sibling. Functions that cannot fail return `void`; functions that can return `zb_status`
and write results through out-parameters. **On any failure the inputs are unchanged**: a
cursor that returns `ZB_ERR_EOF` has not advanced, a buffer that returns `ZB_ERR_LIMIT`
holds exactly what it held. That property is tested at every truncation point (§16.3) and
is what lets a caller retry or report without undoing anything.

## 8. `rw.h` — typed reads and writes

zufast's `bits.h` already does unaligned endian-explicit loads and stores of integers and
converts f16/bf16. `rw.h` adds only what a layout kernel needs on top: floating point and
signed integers at a given byte order, as one name per (type, order):

```c
/* u8 i8 u16 i16 u32 i32 u64 i64 f16 bf16 f32 f64, each le and be where width > 1 */
static inline float    zb_rd_f32le(const void *p);   /* zuf_load_le32 + bit cast */
static inline void     zb_wr_f32le(void *p, float v);
static inline double   zb_rd_f16le(const void *p);   /* zuf_f16_to_f32, widened */
static inline void     zb_wr_f16le(void *p, double v);   /* RNE through zuf_f32_to_f16 */
static inline int32_t  zb_rd_i32be(const void *p);   /* (int32_t) zuf_load_be32 */
/* ... */
```

Every access goes through `memcpy` inside zufast, so views at arbitrary offsets are safe on
strict-alignment targets and under UBSan. The f16 and bf16 writers narrow from `double`
through `float` with round-to-nearest-even at each step, which is what every other
language does and is documented as such (a double that is exactly halfway between two
halfs after the first narrowing is a known double-rounding case; the test suite pins the
behaviour rather than claiming single rounding).

Two helpers sit beside the readers and writers. `zb_host_big_endian()` returns 1 on a
big-endian host and is what `"native"` and the `=` prefix resolve to (§14.2). The double to
float narrowing used by every f32, f16 and bf16 writer clamps a finite double beyond
`FLT_MAX` itself, to `FLT_MAX` below the midpoint to 2^128 and to infinity from it, because
the C conversion of an out-of-range finite double is undefined behaviour; the result is the
IEEE rounding, and `writeBin(size = 4)` agrees with it on every tested value.

## 9. `buf.h` — the buffer

### 9.1 The struct

```c
typedef struct zb_buf {
    uint8_t *data;
    size_t   len;      /* bytes in use */
    size_t   cap;      /* bytes allocated or borrowed */
    size_t   max;      /* hard cap on cap; 0 = unlimited */
    uint32_t flags;    /* ZB_BUF_OWNED | ZB_BUF_GROWABLE | ZB_BUF_HIT_LIMIT */
    void   (*release)(struct zb_buf *);   /* NULL for a borrowed buffer */
    void    *owner;    /* opaque: the malloc block, a SEXP, later an mmap handle */
} zb_buf;
```

The shape is the Arrow C data interface's: whoever sets `release` frees `owner`. In v0.1.0
exactly two backings exist, malloc (the builder) and borrowed (a window onto an R raw
vector or any caller memory). The struct is designed now for the ones next.md adds (an
mmap handle, a nanoarrow `ArrowBuffer`), so that they are new `release` functions and no
new fields.

`flags` exists because of §4.5 rule 5: `ZB_BUF_OWNED` says `release` must be called,
`ZB_BUF_GROWABLE` says `realloc` may be used on `data`, `ZB_BUF_HIT_LIMIT` records that the
last failure was the cap and not the allocator (zuhttp's `hit_limit`, so a caller can raise
a limit error rather than a memory error).

### 9.2 Construction and release

```c
static inline void      zb_buf_init   (zb_buf *b);                             /* zeroed, unowned */
static inline zb_status zb_buf_alloc  (zb_buf *b, size_t cap, size_t max);     /* malloc; growable */
static inline void      zb_buf_borrow (zb_buf *b, const void *p, size_t n);    /* len = cap = n; not growable */
static inline void      zb_buf_release(zb_buf *b);                             /* calls release if OWNED; zeroes */
static inline void      zb_buf_reset  (zb_buf *b);                             /* len = 0; keeps cap */
static inline zb_status zb_buf_detach (zb_buf *b, uint8_t **data, size_t *len); /* hand the malloc block to the caller */
```

Writing into a borrowed buffer is allowed up to `cap` (it is the caller's memory); growing
one is `ZB_ERR_INVALID`. `zb_buf_borrow` takes `const void *` and casts, because the same
view type serves reads and writes; the kernels that only read take `const uint8_t *, size_t`
and never a `zb_buf`, so const-correctness is lost only where the caller chose to borrow
for writing.

### 9.3 Growth and limits

```c
static inline zb_status zb_buf_reserve(zb_buf *b, size_t extra);   /* room for len + extra */
```

`reserve` is the only place that reallocates. Growth doubles until 64 MiB and grows by half
after that, never below `len + extra`, never below 256 bytes, never above `max` when set
(the last growth is clamped to `max`, so a buffer can fill to exactly its cap). Every size
computation goes through `zb_int_add()` and `zb_int_mul()`, which return `ZB_ERR_MEMORY`
instead of wrapping (zukomp's `zu_buf.c` rule: there is no bare size arithmetic anywhere).
A request that would exceed `max` returns `ZB_ERR_LIMIT`, sets `ZB_BUF_HIT_LIMIT`, and
changes nothing.

### 9.4 Typed appends

```c
static inline zb_status zb_put_bytes(zb_buf *b, const void *p, size_t n);
static inline zb_status zb_put_u8   (zb_buf *b, uint8_t v);
static inline zb_status zb_put_u16le(zb_buf *b, uint16_t v);   /* every type of §8, le and be */
static inline zb_status zb_put_f64be(zb_buf *b, double v);
static inline zb_status zb_put_u32le_n(zb_buf *b, const uint32_t *v, size_t n);   /* vectorised, one reserve */
static inline zb_status zb_put_zeros(zb_buf *b, size_t n);
static inline uint8_t  *zb_put_raw(zb_buf *b, size_t n);        /* reserve n, advance len, return the slot; NULL on failure */
```

`zb_put_raw` is the shape the R glue and a format writer want: reserve once, then write a
whole record with `zb_wr_*` into the returned slot. For `n = 0` it never returns `NULL` (so
`NULL` always means failure), but the slot must not be written through. Every `_n` function reserves once and
then loops, so appending a million values costs one allocation check, not a million.

## 10. `cursor.h` — checked sequential reads

```c
typedef struct {
    const uint8_t *base;
    size_t         len;
    size_t         pos;
} zb_cur;

static inline void      zb_cur_init     (zb_cur *c, const void *p, size_t n);
static inline size_t    zb_cur_remaining(const zb_cur *c);
static inline zb_status zb_cur_seek     (zb_cur *c, size_t pos);          /* ZB_ERR_EOF past len */
static inline zb_status zb_cur_skip     (zb_cur *c, size_t n);
static inline zb_status zb_cur_bytes    (zb_cur *c, const uint8_t **p, size_t n);   /* borrow n bytes */
static inline zb_status zb_cur_u8       (zb_cur *c, uint8_t *out);
static inline zb_status zb_cur_u32le    (zb_cur *c, uint32_t *out);     /* every type of §8 */
static inline zb_status zb_cur_f64be    (zb_cur *c, double *out);
```

Offsets rather than two pointers, because every error a format reader reports is "at byte
N", and `c->pos` is N. On `ZB_ERR_EOF` the cursor is unchanged (§7). Draft 1's `zb_memmem`
search functions are not here: `grepRaw(fixed = TRUE)` serves a raw vector, and the case
zubin improves on, searching a view or a mapping without materialising it, needs the views
of next.md first.

## 11. `layout.h` — record layouts

### 11.1 Types

```c
typedef enum {            /* values are permanent; 17..31 reserved for variable-length and bitfields */
    ZB_U8 = 1, ZB_I8 = 2, ZB_U16 = 3, ZB_I16 = 4, ZB_U32 = 5, ZB_I32 = 6,
    ZB_U64 = 7, ZB_I64 = 8, ZB_F16 = 9, ZB_BF16 = 10, ZB_F32 = 11, ZB_F64 = 12,
    ZB_BOOL = 13, ZB_BYTES = 14, ZB_STR = 15, ZB_PAD = 16
} zb_type;

typedef struct {
    zb_type     type;
    uint32_t    count;      /* array length; 1 for a scalar; the byte width for b, s and x */
    uint32_t    size;       /* bytes the field occupies in a record: elements * width */
    uint32_t    offset;     /* from the start of the record */
    uint8_t     big_endian;
    const char *name;       /* borrowed from the spec; NULL when unnamed */
    uint32_t    name_len;
} zb_field;

typedef struct {
    zb_field *fields;       /* caller-supplied storage (§11.3) */
    uint32_t  nfields;
    uint32_t  size;         /* record size, including trailing alignment padding */
    uint32_t  align;        /* 1 when packed; the largest field alignment when aligned */
} zb_layout;
```

Record size and every offset are `uint32_t`: a record is at most 2^31 − 1 bytes, which is
also what fits an R integer, so `bin_size()` never needs a double. Record *counts* are
`size_t` in every kernel; a raw vector of 10 GB is a long vector and is supported.

### 11.2 The specification grammar

One string, whitespace- or comma-separated fields:

```
layout  := [ endian ] field { sep field }
endian  := "<" | ">" | "="                     little, big, host; default from the caller
field   := [ name ":" ] type [ "[" count "]" ] [ "." order ]
         | "x" width                            padding: never read, written as zeros
type    := "u8" | "i8" | "u16" | "i16" | "u32" | "i32" | "u64" | "i64"
         | "f16" | "bf16" | "f32" | "f64" | "bool"
         | "b" width                            fixed bytes
         | "s" width                            fixed string, C semantics (§14.6)
order   := "le" | "be"                          per-field override; multi-byte types only
name    := [A-Za-z_] [A-Za-z0-9_.]*             unique within a layout; unnamed fields are V1, V2, ...
count, width := decimal, 1 .. 2^31-1
sep     := whitespace | ","
```

Examples:

```
<magic:u32 version:u16 flags:u16 count:u64 name:s32
>magic:u32 minor:u16 major:u16
<xyz:f32[3] rgb:u8[3] x1
<ts:i64 price:f64 qty:i32 side:u8 x3 crc:u32.be
```

Rules the parser enforces, each with a `ZB_ERR_SPEC` and a byte position: unknown type;
array suffix on `b`, `s` or `x` (write `b24`, not `b8[3]`); order suffix on a one-byte type
or on `b`/`s`/`x`; a zero width or count; a duplicate name; a name on a padding field; a
total size over 2^31 − 1; more fields than the caller's array holds (`ZB_ERR_LIMIT`).

Mnemonic widths rather than Python `struct` letters (decision 12): `u32` is read by anyone,
`I` is not, and a translator for Python specs is a thirty-line function that can come when
someone porting Python code asks.

### 11.3 Parsing without allocating

```c
static inline zb_status zb_layout_parse(const char *spec, size_t n,
                                        int default_big_endian, int align,
                                        zb_field *fields, uint32_t max_fields,
                                        zb_layout *out, size_t *err_pos);
static inline uint32_t  zb_layout_count_fields(const char *spec, size_t n);   /* upper bound: separators + 1 */
```

The caller supplies the field array (on the stack, or `R_alloc` in the R glue), so the
header allocates nothing and a layout is a plain value. Names are borrowed pointers into
`spec`, which the caller keeps alive for as long as it uses the layout; the R object stores
its own copy of the spec and of every derived field (§13.2).

### 11.4 Alignment

With `align = 0` fields are packed at consecutive offsets. With `align = 1` each field is
placed at the next multiple of its natural alignment, which is the element width for
numeric types (1, 2, 4 or 8), 1 for `b`, `s`, `x` and `bool`, and the record size is
rounded up to the largest field alignment. This is the System V rule GCC and clang apply to
a C struct on every target R supports, so a `struct` dumped with `fwrite()` reads back with
`align = TRUE`. Explicit `x` padding and `align` compose: padding is a field of alignment 1.

### 11.5 Unpack and pack kernels

Fixed layouts are processed **field-major**: for each field, one strided loop over all
records fills one typed output column. Each loop is a load, a widen and a store that the
compiler vectorises, and the output is exactly an R column.

```c
/* Reads n records starting at base with the given stride, one field into one column. */
static inline zb_status zb_unpack_i32 (const uint8_t *base, size_t n, size_t stride, const zb_field *f, int32_t *dst, int allow_na);  /* u8 i8 u16 i16 i32 bool */
static inline void      zb_unpack_f64 (const uint8_t *base, size_t n, size_t stride, const zb_field *f, double *dst);                /* u32 f16 bf16 f32 f64 */
static inline zb_status zb_unpack_i64 (const uint8_t *base, size_t n, size_t stride, const zb_field *f, int64_t *dst);               /* i64; u64 while < 2^63 */
static inline zb_status zb_unpack_f64x(const uint8_t *base, size_t n, size_t stride, const zb_field *f, double *dst, size_t *bad);   /* i64 u64 -> double, exact or ZB_ERR_RANGE at *bad */
static inline void      zb_unpack_bytes(const uint8_t *base, size_t n, size_t stride, const zb_field *f, uint8_t *dst);              /* b s: n * size contiguous */

/* The inverses. *bad receives the index of the first value that does not fit. */
static inline zb_status zb_pack_i32  (uint8_t *base, size_t n, size_t stride, const zb_field *f, const int32_t *src, int allow_na, size_t *bad);
static inline zb_status zb_pack_f64  (uint8_t *base, size_t n, size_t stride, const zb_field *f, const double  *src, size_t *bad);
static inline zb_status zb_pack_i64  (uint8_t *base, size_t n, size_t stride, const zb_field *f, const int64_t *src, size_t *bad);
static inline void      zb_pack_bytes(uint8_t *base, size_t n, size_t stride, const zb_field *f, const uint8_t *src);
static inline void      zb_pack_zeros(uint8_t *base, size_t n, size_t stride, const zb_field *f);                                   /* x, and the alignment gaps */
```

Array fields (`count > 1`) are written **column-major**: element k of record i lands at
`dst[k * n + i]`, which is R's matrix layout, so the R glue hands the kernel a matrix's
data pointer and nothing is transposed afterwards. The kernels never see a `SEXP`; they are
what a C consumer calls to decode a block of records into its own arrays.

## 12. `zubin-r.h` — SEXP glue

The only header that includes `<Rinternals.h>`. For a consumer that is itself an R package:

```c
/* A zb_buf owned by an external pointer with a finalizer (onexit = TRUE), so that a
   longjmp from Rf_error() or R_CheckUserInterrupt() cannot leak it. */
SEXP    zb_r_buf_new   (size_t reserve, size_t max, zb_status *st);  /* R_NilValue on failure */
zb_buf *zb_r_buf_get   (SEXP ptr);                 /* NULL once finalized, or not a zubin buffer */
void    zb_r_buf_free  (SEXP ptr);                 /* eager release; the finalizer then does nothing */
void    zb_r_buf_borrow(zb_buf *b, SEXP raw);       /* borrow RAW(raw); caller keeps raw protected */
SEXP    zb_r_buf_to_raw(const zb_buf *b);          /* allocVector(RAWSXP) + memcpy */
```

Nothing in `zubin-r.h` raises: `zb_r_buf_new` reports `ZB_ERR_LIMIT` (reserve above max) or
`ZB_ERR_MEMORY` through `*st` and returns `R_NilValue`, and the caller decides how to
report it (Stage 2 added the status out-parameter, so the "C never raises" rule of §13.7
holds for consumers too). The external pointer carries the tag symbol `zubin_buf`, which
`zb_r_buf_get` checks, so a foreign external pointer is `NULL` rather than a wild cast. Two
internal hooks, `ZB_INT_R_ON_NEW()` and `ZB_INT_R_ON_FREE()`, may be defined before the
include to count buffers; zubin's own test harness does, for the lifetime tests of §16.3.

The rule it encodes is zukomp §13's: any heap state that must survive a longjmp is owned by
R before the first call that can jump. `zb_r_buf_new` registers the finalizer before the
buffer exists, and the finalizer clears the pointer before freeing, so an eager release on
the success path and a later GC pass are mutually safe. zubin's own `.Call` entry points
use these same functions; there is no second path.

Draft 1's `zb_pod_*` helpers (a plain-old-data struct in a `RAWSXP`) and the serialisation
streams are not here (next.md); nothing in v0.1.0 needs them.

## 13. R API

Fourteen exports, plus S3 methods. Every argument is validated in R first with a classed
condition, and again in C where native safety depends on it.

### 13.1 Type model

| Spec | Bytes | R type on unpack | On pack, accepts | Notes |
|---|---|---|---|---|
| `u8 i8 u16 i16` | 1–2 | `integer` | integer, double (whole), logical | out of range is `zubin_range_error` |
| `i32` | 4 | `integer` | integer, double (whole) | `-2^31` is `NA_integer_`: read is an error unless `na = "allow"`; write of `NA` likewise |
| `u32` | 4 | `double` | integer, double (whole) | exact |
| `i64 u64` | 8 | `double` (`int64 = "double"`, the default) or `integer64` (`int64 = "integer64"`) | integer, double (whole), `integer64` | double mode errors on a value above 2^53 in magnitude rather than rounding; `integer64` mode errors on `u64` ≥ 2^63 |
| `f16 bf16` | 2 | `double` | numeric | round to nearest even on write (§8) |
| `f32 f64` | 4, 8 | `double` | numeric | `NA_real_` survives `f64`; becomes `NaN` in `f32`/`f16` (payload lost), documented |
| `bool` | 1 | `logical` | logical | non-zero reads `TRUE`; `NA` on write is `zubin_na_error` |
| `b<n>` | n | `list` of `raw` | list of raw, each exactly n bytes | the `blob`-compatible shape |
| `s<n>` | n | `character`, UTF-8 | character, at most n bytes | §14.6 |
| `x<n>` | n | not returned | not supplied; written as zeros | |
| `t[k]` | k × width | matrix `n × k` (list mode) or columns `name.1 … name.k` (data frame mode) | matrix `n × k`, or a vector of length k when n = 1 | numeric and `bool` types only |

`integer64` output needs no dependency: it is a `double` vector carrying the class
`integer64`, exactly as `bit64` defines it, and `bit64` is in `Suggests` for the tests and
for users who want its arithmetic. `pack` recognises `integer64` input by class and
reinterprets the bits.

### 13.2 Layouts

```r
l <- bin_layout(spec, endian = c("little", "big", "native"), align = FALSE)
```

`spec` is the string of §11.2, or a named character vector (`c(id = "u32", ts = "i64",
"x3")`) for programmatic construction, where the names are the field names and a name
inside a value is an error. An endian prefix in the string overrides `endian`; a field
suffix overrides both. A `zubin_layout` passed as `spec` is returned unchanged.

The result is a classed list, not an external pointer: `spec` (the normalised string),
`fields` (a data frame of `name`, `type`, `count`, `size`, `offset`, `endian`), `size`,
`align`. It serialises, copies, prints as a table, and is rebuilt into `zb_field[]` by every
`.Call` in microseconds. Methods: `print`, `format`, `length` (fields), `names`,
`as.data.frame` (the field table), and `bin_size(l)`.

### 13.3 Unpack and pack

```r
bin_unpack(x, layout, offset = 0, n = NULL, stride = NULL,
           as = c("data.frame", "list"),
           int64 = c("double", "integer64"), na = c("error", "allow"),
           encoding = "UTF-8")
```

- `x` is a raw vector. (A view, in 0.3.0, will be a raw vector too.)
- `offset` is the 0-based byte position of the first record (§14.1); `n = NULL` reads as
  many whole records as fit, and trailing bytes that do not make a record are left alone,
  which is what a file with a footer needs; an explicit `n` past the end is
  `zubin_bounds_error` with the offset and length in the condition.
- `stride` defaults to `bin_size(layout)` and lets a layout describe the head of a larger
  record.
- `as = "data.frame"` is `data.frame()` over the columns with `b<n>` columns wrapped in
  `I()`, so matrices expand to `name.1 …` and byte lists stay list columns; `"list"` returns
  the columns as they are, matrices included.
- A single header is `bin_unpack(x, hdr, n = 1)`: a one-row frame, `h$count`. There is no
  scalar API.

```r
bin_pack(layout, ..., na = c("error", "allow"))
```

`...` is either named vectors, one per non-padding field, or a single data frame or list
with those names. Columns are recycled to the longest length, and a length that does not
divide it is `zubin_invalid_argument`, not a warning. The result is a raw vector of `n *
bin_size(layout)` bytes; padding and alignment gaps are zeros, so output is deterministic
and never leaks heap contents. `bin_pack` into a builder is `bin_put(b, bin_pack(l, df))`:
one extra copy in R, none in C, where `zb_put_raw()` plus the pack kernels write in place.

### 13.4 Homogeneous codecs

```r
bin_decode(x, type, offset = 0, n = NULL, endian = c("little", "big", "native"),
           int64 = c("double", "integer64"), na = c("error", "allow"))
bin_encode(x, type, endian = c("little", "big", "native"), na = c("error", "allow"))
```

`type` is one type token of §11.2 (`"f32"`, `"u16"`, also `"b8"` and `"s16"`). `bin_decode`
is the one-field read at an offset, vectorised: `bin_decode(x, "u32", offset = 4)` is a
header field, `bin_decode(x, "f32", offset = 64, n = 1e6)` is a column. `bin_encode` is
`writeBin(x, raw(), size = 4)` generalised to every width, with rounding to f16/bf16/f32
and an error, never a silent truncation, for an integer that does not fit.
`bin_encode(bin_decode(b, t), t)` is the identity for every type, pinned by a property test.

### 13.5 The builder

```r
b <- bin_builder(reserve = 0, max = Inf)
bin_put(b, x, type = NULL, endian = c("little", "big", "native"))
bin_reserve(b, n);  bin_reset(b);  bin_size(b)
bin_take(b)       # the bytes as raw; the builder is emptied and keeps its capacity
as.raw(b)         # the bytes as raw; the builder is unchanged
```

`bin_put` appends a raw vector as it is (`type` must be `NULL`), or a vector encoded as
`type` (required otherwise, so a double is never silently eight bytes), or a string as
`"z"` (bytes plus a NUL) or `"s<n>"` (fixed width, NUL-padded). `max` is the hard cap of
§9.3; exceeding it is `zubin_limit_error` and the builder is unchanged. The builder is an
external pointer created through `zb_r_buf_new()` (§12); a finalized or taken builder that
is used again errors cleanly. `print` shows size, capacity and cap.

### 13.6 Hexdump and diff

```r
bin_hexdump(x, offset = 0, n = NULL, width = 16L)   # xxd style; a zubin_hexdump character vector with a print method
bin_diff(a, b, n = 10L)                             # data frame: offset, a, b for the first n differing bytes; lengths as attributes
```

Both take any raw vector and are pure presentation. They are in v0.1.0 because R's printing
of raw is unusable past a few dozen bytes, because every test and example in this package
wants them, and because ideas.md ranks them first among the small things (decision 15).

### 13.7 Information and conditions

`bin_info()` returns, from compiled code, `version`, the `zufast` version compiled against,
`endian` of the host, the compiler, and build flags, in zufast's `fast_info()` shape.

Every condition zubin raises inherits `zubin_error`; the specific classes are
`zubin_invalid_argument`, `zubin_spec_error` (with `position`), `zubin_bounds_error`
(`offset`, `length`), `zubin_range_error` (`field`, `index`), `zubin_na_error` (`field`,
`index`), `zubin_encoding_error` (`field`, `index`), `zubin_limit_error` (`size`, `max`)
and `zubin_memory_error`. C never raises below the outermost `.Call`; statuses are mapped
to classes by enumerator name in `R/conditions.R`, and the call is reduced to the function
name so that a large raw argument is not printed with the error (zucrypt's rule).

### 13.8 Examples

A WAV file, written and read back, which is the vignette's running example:

```r
wav <- bin_layout("<riff:s4 size:u32 wave:s4 fmt:s4 fmt_size:u32 format:u16 channels:u16
                   rate:u32 byte_rate:u32 block_align:u16 bits:u16 data:s4 data_size:u32")
samples <- bin_encode(round(32767 * sin(2 * pi * 440 * seq(0, 1, by = 1 / 8000))), "i16")
hdr <- bin_pack(wav, riff = "RIFF", size = 36 + length(samples), wave = "WAVE", fmt = "fmt ",
                fmt_size = 16, format = 1, channels = 1, rate = 8000, byte_rate = 16000,
                block_align = 2, bits = 16, data = "data", data_size = length(samples))
writeBin(c(hdr, samples), path)

x <- readBin(path, "raw", file.size(path))
h <- bin_unpack(x, wav, n = 1)
pcm <- bin_decode(x, "i16", offset = bin_size(wav), n = h$data_size / 2)
```

A big-endian header and a record file, through a builder:

```r
hdr <- bin_layout(">magic:u32 version:u16 count:u32")
rec <- bin_layout(">id:u32 ts:i64 price:f64 qty:i32 side:u8 x3")
b <- bin_builder()
bin_put(b, bin_pack(hdr, magic = 0xCAFEBABE, version = 1, count = nrow(trades)))
bin_put(b, bin_pack(rec, trades))
writeBin(bin_take(b), path)

x <- readBin(path, "raw", file.size(path))
h <- bin_unpack(x, hdr, n = 1)
trades2 <- bin_unpack(x, rec, offset = bin_size(hdr), n = h$count)
bin_hexdump(x, n = 64)
```

A C struct dumped by another program, with alignment: `bin_layout("=a:u8 b:u32 c:u16",
align = TRUE)` has size 12 and offsets 0, 4, 8, as `struct { uint8_t a; uint32_t b;
uint16_t c; }` does on every target R supports.

## 14. Semantics decided once

### 14.1 Offsets are 0-based

Everywhere: `offset` arguments, `bin_hexdump()` and `bin_diff()` output, `position` in a
spec error, `offset` in a bounds error, and the `offset` column of a layout. This matches
`seek()`, every format specification, every other language, and zucbor's conditions. The
help pages say so in the first paragraph of every function that takes one.

### 14.2 Byte order

The default is little-endian everywhere, because that is what files written in the last
thirty years mostly are; network protocols say `>`. `"native"` resolves at run time to the
host order reported by `bin_info()`. Nothing is ever read in an order the caller did not
name or default to.

### 14.3 Missing values

`NA` has no byte representation except for `i32` (`INT_MIN`) and `f64` (R's NaN payload).
Writing an `NA` anywhere else is `zubin_na_error`; reading `INT_MIN` into `i32` is an error
by default because a file that happens to contain `-2147483648` would otherwise acquire a
missing value silently. `na = "allow"` turns both directions on for `i32` and nothing else.

### 14.4 Range and exactness

A value that does not fit its field is an error with the field and the 0-based index of the
first offending value: 300 into `u8`, 1.5 into any integer type, 2^53 + 1 into an `i64`
read as double, 2^63 into `u64` read as `integer64`. zubin never truncates, wraps or rounds
an integer; it rounds floating point only where the type is floating point.

### 14.5 Recycling

Columns passed to `bin_pack()` recycle to the longest length when their length divides it;
otherwise `zubin_invalid_argument`. Length-one columns are the common case (a constant
flag); the strict rule is vctrs', not base R's silent partial recycling.

### 14.6 Strings

`s<n>` is a C string in a fixed field: on read, bytes up to the first NUL or the field
width, whichever is first, validated as UTF-8 through `zuf_utf8_valid()` and marked
`CE_UTF8`; invalid bytes are `zubin_encoding_error` unless `encoding = "latin1"` (marked
`CE_LATIN1`, any bytes valid) or `encoding = "bytes"` (marked `CE_BYTES`). On write, the
string's bytes (through `enc2utf8()`) padded with NULs to the width; longer is a range
error; `NA_character_` is an NA error. A field that must preserve interior NULs or exact
bytes is `b<n>`. There is no `"z"` or `"p*"` in a layout before 0.2.0; `bin_put(b, s,
"z")` exists because appending a NUL-terminated string is the one variable-length case a
writer meets constantly.

### 14.7 Long vectors and big inputs

Record counts are `R_xlen_t` and `size_t`; a raw vector above 2^31 bytes unpacks. Record
size and field offsets are bounded by 2^31 − 1 (§11.1). `R_CheckUserInterrupt()` runs
between fields in unpack and pack and every 64 MiB in `bin_put()`, never inside a kernel
loop, so a 10 GB unpack can be interrupted and leaves nothing behind (the output columns
are R-allocated and collected; the builder is owned by its external pointer).

### 14.8 Empty inputs

`bin_unpack(raw(0), l)` is a zero-row result with the right columns and types.
`bin_pack(l)` with zero-length columns is `raw(0)`. `bin_decode(raw(0), "u32")` is
`double(0)`. An empty builder takes `raw(0)`.

## 15. Build, portability and CRAN

```
src/
├── init.c          R_init_zubin: registers .Call entry points, R_useDynamicSymbols(FALSE),
│                   R_forceSymbols(TRUE); nothing registered as C-callable
├── zubin_r.c       the .Call wrappers behind R/: arguments to kernels, columns, conditions
├── zubin_test.c    the always-compiled test harness (§16.3)
└── Makevars        PKG_CPPFLAGS = -I../inst/include ; PKG_CFLAGS = $(C_VISIBILITY)
                    OBJECTS listed explicitly; portable make only; no Makevars.win
```

The wrappers include `<zubin.h>` like any consumer, so CRAN's instrumented flavours
exercise the headers through the package's own tests (zufast §19.1). No `configure`, no
`install.libs.R`, no vendored code, no `SystemRequirements`.

Targets and compilers are zufast's (§19.2): Linux x86-64 and AArch64, macOS, Windows under
Rtools; GCC, clang, Apple clang; headers compile as C++11 for cpp11 and Rcpp consumers.
Byte order is handled by construction, and **the big-endian s390x leg of `arch.yml` is
load-bearing for this package**: the committed golden vectors (§16.4) must decode to the
same values there. zufast could say "no CI leg runs big-endian"; zubin cannot.

`Depends: R (>= 4.1)`, `LinkingTo: zufast (>= 0.1.0)`, `Suggests: bit64, testthat (>=
3.0.0), withr, knitr, rmarkdown`, `Language: en-GB`, `Config/roxygen2/version: 8.1.0`, no
`Config/testthat/parallel` (serial, so gctorture and valgrind legs run this package's C and
not a subprocess's: zujson's finding). During development `Remotes: pedrobtz/zufast@main`
lets CI resolve zufast, as zuxlsx does for its three siblings; the field is removed in the
release stage, which can only happen once zufast is on CRAN (roadmap, Stage 9).

## 16. Testing and gates

Correctness is bit-exact or it is wrong. Every gate is in CI except the benchmarks, and a
gate counts once it has been seen to fail (zuxml's rule).

### 16.1 The headers — `abi.yaml`

zufast's `tools/check-headers`, adapted: every header under `inst/include/zubin/` compiles
standalone as C99 under `-Wall -Wextra -Wpedantic -Werror` and as C++11, with GCC and
clang, at `-O0` and `-O2`; `tools/abi/probe-none.c` includes `<zubin.h>` and uses nothing;
`tools/abi/probe-all.c` calls every public function; `zubin-r.h` compiles against R's
headers in a third probe; a comment-stripped grep finds no `SEXP`, `Rf_`, `R.h` under
`zubin/`. `tools/run-symbol-audit` runs R's compiled-code `nm` scan over the full-use
probe.

### 16.2 The shared object — `test-abi.R`

`zubin.so` exports exactly `R_init_zubin`; `bin_info()` reports the `ZUFAST_VERSION`
compiled in.

### 16.3 The harness — `src/zubin_test.c`

Always compiled, `zubin_test_*`, not exported. It drives: the cursor at every truncation
point of a record (`ZB_ERR_EOF` and the cursor unchanged); the buffer through growth from
0 to past 64 MiB with a counted number of reallocations, the cap at every boundary, and
`ZB_BUF_HIT_LIMIT`; the kernels directly with caller-chosen strides and counts, including
the column-major array placement; `zb_layout_parse()` with every grammar error and its
position. It also exposes a live-buffer counter so `test-lifetime.R` can prove, after an
error and after a `setTimeLimit()` interrupt inside `bin_put()` and `bin_unpack()`, that
the count returns to its baseline after `gc()` (zucrypt's `zuc_live.h`, zucbor's interrupt
technique). The finalizer is broken once, locally, to show that test fail, and the PR
records it.

### 16.4 Correctness

- **Every row of §13.1 has a test**, both directions, including every error class in the
  row, and the roxygen table, this table and the tests are the same table three times.
- **Golden vectors.** `tests/testthat/fixtures/golden.tsv`: layout spec, hex input, and the
  expected columns, written by hand from the specifications of the formats they come from
  (WAV, BMP, PNG IHDR, Java class header, CFB header, an ITCH message, an SBE header) and
  from `readBin()`/`writeBin()` where base R covers the width. The same file is read on
  every CI leg, s390x included.
- **Round trips.** `bin_unpack(bin_pack(l, df), l)` is `identical()` to `df` over generated
  layouts and frames under `withr::local_seed()`; `bin_encode(bin_decode(b, t), t)` is `b`
  for every type over random bytes; f16 and bf16 over all 65 536 patterns.
- **Differential.** Against `readBin()`/`writeBin()` for every width and byte order base R
  supports; against `bit64` for `integer64`.
- **Alignment.** A fixture dumped from a C struct by the harness itself, read with `align
  = TRUE`, for a dozen field orders.
- **Strings.** Markus Kuhn's UTF-8 stress cases through `s<n>`; NUL handling at every
  position of the field; the three encodings.
- Tests are self-sufficient, byte-explicit (hex strings, never source literals: zukomp's
  lesson), assert on condition classes never messages, pass under `devtools::test(shuffle
  = TRUE)`, and finish in under a minute; exhaustive sweeps sit behind
  `ZUBIN_SLOW_TESTS=true`.

### 16.5 The consumer fixture — `consumer.yaml`

`tools/zubintest`: `LinkingTo: zubin, zufast`, no `Imports:`, nothing in `NAMESPACE` but
`useDynLib`, two translation units including `<zubin.h>` (so a non-`static` symbol would
collide), one including `<zubin-r.h>`, and a testthat suite that parses a layout, unpacks
a record, builds a buffer and borrows a raw vector. The workflow installs zufast, zubin
and the fixture; checks the fixture with `--as-cran` and fails on any compiled-code NOTE;
runs its tests; asserts with `nm` that its shared object defines no global `zb_` or `zuf_`
symbol; then removes zubin from the library path (with `R_LIBS_USER='-'`, zukomp's trap)
and proves the fixture still loads and passes.

### 16.6 Hardening — `hardening.yaml`, `native-checks.yaml`, `arch.yaml`

libFuzzer targets over the R-free headers with ASan and UBSan: `fuzz_layout` (the spec
parser on arbitrary strings), `fuzz_unpack` (a spec and a byte string; every field of every
record decoded, nothing may crash), `fuzz_buf` (an input-driven sequence of puts, resets
and reserves against a tiny cap; the length invariants hold throughout). Each has a canary
that must crash first. `native-checks.yaml` runs the r-actions sanitizers, valgrind, LTO,
gctorture and a blocking rchk; `arch.yaml` runs the suite on i386, musl and s390x.

## 17. Benchmarks

`tools/benchmarks.R`, in the repository and not in CI; results per machine in
`.agents/benchmarks.md` with the commit. Targets, to validate rather than promise:

| Operation | Baseline | Target |
|---|---|---|
| Unpack 10 M records, 4 fields, 24 bytes, to a data frame | 4 × `readBin()` with `n` then interleave | memory-bandwidth bound, one pass per field |
| Pack the same frame | `writeBin()` per column then `rawConnection` interleave | same |
| `bin_decode(x, "f32", n = 1e8)` | `readBin(x, "double", size = 4, n = 1e8)` | at least as fast; exact |
| 1 M `bin_put()` calls of 100 bytes | `c()` accumulation; `rawConnection(raw(0), "w")` | linear; dominated by `.Call` overhead |
| `bin_hexdump()` of 1 MiB | `sprintf("%02x")` over `as.integer()` | not a bottleneck in a test suite |

## 18. v0.1.0: deliverables and acceptance criteria

Deliverables:

1. the headers of §5 and every function of §7–§12, each documented in its header;
2. the fourteen `bin_` functions of §13 with roxygen, runnable examples, and the §13.1
   table in `?bin_layout`;
3. `tools/zubintest` and `consumer.yaml` (§16.5);
4. the gates of §16.1–§16.6, each with a log showing it exercised its target and a canary
   that failed;
5. `README.md` with a "Using zubin from C" section that is the consumer recipe in full,
   followed verbatim by the fixture;
6. the shipped vignette `zubin` (layouts, unpack and pack, codecs, builder, hexdump, the
   WAV and big-endian examples) and the pkgdown-only article `c-api` (§5, §9–§12 for a
   package author);
7. `cran-comments.md`, `inst/WORDLIST`, `NEWS.md` with a versioned heading;
8. the adoption issues of §3.2, filed and linked from the README;
9. `.agents/benchmarks.md` with first measurements.

Acceptance criteria:

1. Every header compiles standalone as C99 and C++11 under §16.1's flags on Linux and
   macOS; the no-use probe is warning-free; `zubin-r.h` compiles against R.
2. `zubin.so` exports exactly `R_init_zubin`.
3. The fixture builds, checks with no compiled-code NOTE, tests green, and loads with zubin
   uninstalled, on all three operating systems.
4. Every §13.1 row has a test in both directions; the golden vectors decode identically on
   every leg including s390x.
5. The round-trip properties of §16.4 hold; f16 and bf16 are exhaustive.
6. The lifetime test fails with the finalizer broken and passes with it restored.
7. Each fuzz target has run for at least one cumulative hour without a finding, and each
   canary was seen to crash.
8. `devtools::check(cran = TRUE)` is 0/0/0 on all three platforms; the suite passes
   shuffled and serially under gctorture.
9. zufast is on CRAN, `Remotes:` is gone, and `LinkingTo: zufast (>= 0.1.0)` resolves from
   CRAN.

Then: tag `v0.1.0`, submit, and move `main` to `0.1.0.9000`.

## 19. Decision log

Draft 1's eleven open questions, resolved, followed by the decisions revision 2 adds.

| # | Question | Decision, and why |
|---|---|---|
| 1 | Package name | `zubin`. Free on CRAN on 2026-10-04, as are the alternatives; the repository exists; the family prefix rule gives `bin_` and `zb_`, both free. |
| 2 | Spec syntax | Mnemonic widths (§11.2). A Python-letter translator is a later helper, not a second grammar. |
| 3 | `i32` and `NA` | Error by default, `na = "allow"` for both directions (§14.3). A file is not a place for silent missingness. |
| 4 | `u64` above 2^63 | Error under both `int64` modes (§14.4). A `bigint`-style class is in next.md with the trigger "someone has such a file". |
| 5 | Aligned layouts | In v0.1.0 (§11.4): thirty lines, needed to read any dumped C struct, and the rule is the same on every target R supports. |
| 6 | Views in a separate package | Not decided here; views are 0.3.0 and the decision is taken then (next.md). Nothing in v0.1.0 depends on it. |
| 7 | Variable-length fields | 0.2.0. They need the record-major path, which is a second kernel; v0.1.0 ships one kernel completely. `bin_put(b, s, "z")` covers the writer's case now. |
| 8 | Bitfields | Later, with CAN/DBC as the trigger (next.md). The grammar reserves `:` after the width. |
| 9 | Where `hash_object()` lives | zubin, later, and only if rdz is rewritten in C: the stream is the hard part and rdz is the consumer. Without that decision there is no consumer and `digest` exists. |
| 10 | Serialisation and connection API status | Checked against R 4.6.1's `tools:::nonAPI` on 2026-10-04: `R_InitOutPStream`, `R_InitInPStream`, `R_Serialize`, `R_Unserialize`, `R_new_custom_connection`, `R_ReadConnection`, `R_WriteConnection`, `R_make_altraw_class`, `R_new_altrep` are not on it; `DATAPTR` is. Re-check at the stage that uses each. |
| 11 | `split_at()` return type | Views, from the start, in 0.3.0: there is no version that returns copies. |
| 12 | R prefix | `bin_`, by the family rule and because `layout()` is `graphics::layout()` (§3.1). |
| 13 | Byte helpers | Built on zufast's `bits.h`, not re-implemented; consumers list `LinkingTo: zubin, zufast` (§4.1). The family keeps one copy of each primitive. |
| 14 | R glue header | A separate `zubin-r.h`, zukomp's `-r.h` pattern, outside the umbrella, so `zubin/` stays R-free and the header gate stays simple. |
| 15 | hexdump and diff | In v0.1.0 (§13.6): an afternoon, used in every test and example, first on ideas.md's list. |
| 16 | Search and split | Not in v0.1.0: `grepRaw(fixed = TRUE)` serves raw vectors; the gain needs views. |
| 17 | The buffer's cap and flag | Adopted from zuhttp (§9.1, §9.3): a limit that is not enforced where the buffer grows is advisory. |
| 18 | Ownership by flag, not by pointer identity | §4.5 rule 5: a header-only library cannot compare function pointers across translation units. |
| 19 | Array fields in R | Column-major kernels into a matrix (§11.5); data frames expand them as `data.frame()` does. No transposition anywhere. |
| 20 | Fixed strings | C semantics up to the first NUL, validated UTF-8, three encodings (§14.6). Exact bytes are `b<n>`. |
| 21 | 64-bit default | `double`, strict (§13.1): `integer64` without `bit64` attached prints as noise, and silent rounding is the one thing this package exists to refuse. |
| 22 | Default `as` | `"data.frame"`: records are rows. `"list"` is one word away. |
| 23 | Serial tests | No `Config/testthat/parallel`, so the gctorture and valgrind legs instrument this package's C (zujson's measurement). |
| 24 | The first consumer | None named for the C API; rdz is a candidate, not a commitment (§3.2). The fixture stands in and the design says so. The 1.0.0 gate is a real one (next.md). |
| 25 | Release plan | One release, v0.1.0, with everything in §2's first column; the mechanism (header-only delivery, kernels, builder ownership) is the risk, and it is proven by the fixture and the gates, not by breadth. |

## 20. Deferred

Everything in §2's "Later" column, with its trigger, is in [next.md](next.md).
