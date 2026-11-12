# CLAUDE.md

<!-- markdownlint-disable-next-line MD013 -->
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Shared conventions

The portfolio's shared rules are synced into `.claude/shared/` from
`pedrobtz/packages:claude/shared/`. Never edit them here. Where this file and
a shared file disagree, this file wins for this package.

@.claude/shared/workflow.md
@.claude/shared/r-package.md
@.claude/shared/ci.md
@.claude/shared/correctness.md
@.claude/shared/assurance.md

## What this is

`zubin` is an R package for structured binary data, and two products in one:

1. **An R API over raw vectors**, prefixed `bin_`: record layouts
   (`bin_layout()`), vectorised `bin_unpack()`/`bin_pack()`, homogeneous
   codecs `bin_decode()`/`bin_encode()`, a byte builder (`bin_builder()`,
   `bin_put()`, `bin_take()`), `bin_hexdump()`/`bin_diff()`, and from
   Stage 10 `bin_serialize()`, `bin_unserialize()` and `bin_hash_object()`
   over `zubin-r.h`'s `zb_serialize()`, `zb_unserialize()` and
   `zb_serialize_to_sink()`.
2. **A header-only C99 library** under `inst/include/zubin/`, consumed
   through `LinkingTo: zubin, zufast` alone: a buffer with ownership and
   limits, a checked cursor, typed reads and writes at any alignment and byte
   order, and layout kernels.

The headers are built on zufast's (`<zufast/bits.h>`, `<zufast/utf8.h>`);
zubin never re-implements a primitive zufast has. Nothing is vendored. It is
a member of the `zu*` family (sibling checkouts in `../`: zufast is the
closest model, then zukomp, zucrypt, zucbor).

The specification outranks this file:

- [.agents/design.md](.agents/design.md) is the spec, numbered §1–§20; every
  statement in it is a decision, and open questions live only in its §19
  decision log. A stage that changes a decision edits `design.md` in the
  same commit.
- [.agents/roadmap.md](.agents/roadmap.md) sequences it into stages, each
  with a **Status:** line under its heading. Never put status in a heading:
  it changes the anchor that the stage issues link to.
- [.agents/next.md](.agents/next.md) holds everything after 0.1.0.
- `.agents/benchmarks.md` holds the benchmark results; `ideas.md` and
  `domains.md` feed `next.md`.

## Current state

**0.1.0 is on CRAN** (published 2026-11-11; tag and release `0.1.0`), and
`main` is at `0.1.0.9000`, working towards v0.1.1. Stages 0–9 and 10–12 are
done (Stage 10: serialisation streams, #25; Stage 11: #26's small items;
Stage 12: what zunpy, zumsgpack, zuint and zuarrow call, #38). Stage 13, the
foreign-buffer contract (#40), is planned for v0.1.1 and lands before any
sibling's zero-copy stage. A release is recorded by committing the
`CRAN-SUBMISSION` that `devtools::submit_cran()` writes: `cran-release.yaml`
tags that commit and opens the `.9000` bump. #32
and `.agents/next.md` hold what follows.

- **Headers:** `zubin.h` and
  `zubin/{version,status,rw,buf,cursor,layout,column}.h` (layout.h: types,
  spec parser, unpack and pack kernels; column.h: bitmaps, validity, swaps),
  `zubin/detail/portability.h`, plus `zubin-r.h`. Stage 12 added `u128`,
  `i128`, `c64`, `c128` (type values 32–35), native-order kernels and the
  foreign buffer.
- **R API:** `bin_info()`, `bin_layout()`, `bin_size()`, `bin_unpack()`,
  `bin_pack()`, `bin_decode()`, `bin_encode()`, the builder (`bin_builder()`,
  `bin_put()` raw/typed/`"z"`/`"s<n>"`, `bin_reserve()`, `bin_reset()`,
  `bin_take()`, `as.raw()`), `bin_hexdump()`/`bin_diff()`, and the
  serialisation streams.
- **zufast comes from CRAN**; the workflows without R (the header gate, the
  fuzzers) check out its `0.1.0` tag, never its `main`.
- **`tools/zubintest` is the consumer fixture** (`consumer.yaml`); the
  `c-api` article renders its recipe from the fixture's files, so the
  fixture is the one place to edit.

## Versions and dependents

The current version's parent issue is #2, `v0.1.0` (milestone `v0.1.0`),
with stage issues #4 (Stage 0) to #13 (Stage 9), then #25, #26, #38 and #40
for Stages 10–13. One pull request per stage, branch `stage-N-<slug>`; close
the stage issue and update its **Status:** line in that pull request.

Sibling packages that link to zubin before CRAN does, through `LinkingTo`
and `Remotes:`: zunpy (`pedrobtz/zubin@main`), zumsgpack and rdz. zuint and
zuarrow are planned consumers of the foreign buffer (Stage 13). A change to
a header in `inst/include/` breaks them even before 0.1.0.

## Commands

<!-- markdownlint-disable MD013 -->
```sh
tools/check-headers                                # header gate (CC/CXX, ZUFAST_INCLUDE)
tools/run-symbol-audit                             # R CMD check's compiled-code scan, on the probe
CC=/opt/homebrew/opt/llvm/bin/clang tools/run-fuzz # libFuzzer; Apple clang has none
tools/run-benchmarks                               # the numbers in .agents/benchmarks.md
_R_CHECK_SYSTEM_CLOCK_=0 Rscript -e 'devtools::check(cran = TRUE)'
Rscript -e 'devtools::build_readme()'              # README.Rmd -> README.md
Rscript -e 'pkgdown::build_site()'                 # site -> docs/ (gitignored)
air format R/                                      # format R sources (air defaults)
```
<!-- markdownlint-enable MD013 -->

zufast is on CRAN (`install.packages("zufast")`); test unreleased zufast
changes from the sibling checkout (`R CMD INSTALL ../zufast`). roxygen2 must
be 8.1.0 or newer (`Config/roxygen2/version`); an older one rewrites `man/`.

The R fuzz targets (`fuzz_serialize`, `fuzz_sink`) embed R through
`tools/fuzz/rfuzz.c`; run them with `R_HOME` set (`tools/run-fuzz` does).
`tools/alloc-exercise.R` is what the allocation-failure sweep runs.

## Architecture

- `inst/include/zubin/` is the product most consumers see; `src/` is thin.
  `src/zubin_r.c` holds the `.Call` entry points over the headers,
  `src/zubin_r.h` defines the `ZB_INT_R_ON_NEW/FREE` hooks the harness counts
  live buffers through, and `src/zubin_test.c` holds the test-only
  `zubin_test_` symbols the C-level tests call.
- `R/` is one file per area: `layout.R`, `unpack.R`, `pack.R`, `builder.R`,
  `hexdump.R`, `serialize.R`, `info.R`, with `checks.R` (argument checks)
  and `conditions.R` (status names to classes).
- `tools/abi/` holds the ABI probes (`probe-none.c`, `probe-all.c`,
  `probe-r.c`) that `abi.yaml` compiles; `tools/fuzz/` the libFuzzer targets
  and seeds; `tools/zubintest/` the consumer fixture package.

In C, `zb_field.count` is the byte width for `b`, `s` and `x` fields; the
element count of a column is 1 for them. Alignment is `zb_type_align()`, not
the width: a complex type aligns as its part. A 128-bit value is a
`zb_u128` (`lo` first), a container with no arithmetic.

**zubin-r.h's serialisation streams are the one place R raises:** R's errors
during `R_Serialize`/`R_Unserialize` (a malformed stream, a refhook) reach
the caller unchanged; only EOF (`zb_unserialize`, via `R_tryCatch` on a
private condition class) and a builder that cannot grow (`zb_serialize`,
with `R_UnwindProtect` restoring its length on a jump) are statuses.

**The `.Call` convention:** an entry point that can fail returns, on
failure, a `zubin_status`-classed string holding the `zb_status` enumerator
name (attribute `index` when an element is at fault); R checks `is_status()`
and raises through `zb_fail()`, which maps the name to a class with
`zb_status_class`. C never calls `Rf_error()` except in the test harness.

## Naming

| Layer | Prefix | Examples |
| --- | --- | --- |
| R exports | `bin_` | `bin_layout()`, `bin_unpack()`, `bin_info()` |
| R classes | `zubin_` | `zubin_layout`, `zubin_builder`, `zubin_hexdump` |
| R condition classes | `zubin_` | `zubin_error`, `zubin_range_error` |
| Public C | `zb_` / `ZB_` | `zb_buf`, `zb_put_u32le()`, `ZB_OK` |
| Internal C in the headers | `zb_int_` / `ZB_INT_` | `zb_int_grow()` |
| `.Call` entry points | `zubin_` | `zubin_unpack` |
| Test-only `.Call` symbols | `zubin_test_` | `zubin_test_cursor()` |
| Fixture consumer package | `zubintest` | `tools/zubintest` |

## Invariants that are easy to break

Linkage (design §4.5):

1. Every function defined in a header is `static inline` (`ZB_INLINE`),
   never plain `static`.
2. Consumers, and zubin itself, compile with `PKG_CFLAGS = $(C_VISIBILITY)`;
   `zubin.so` exports `R_init_zubin` and nothing else (`test-abi.R`).
3. The headers reference nothing `R CMD check` forbids in compiled code: no
   `printf`, `abort`, `exit`, `assert`, `rand`, `stdout`, `stderr`
   (`tools/run-symbol-audit`).
4. No R header and no `SEXP` under `inst/include/zubin/`; only `zubin-r.h`
   includes R.
5. Never compare function pointers taken from a header-only function: each
   translation unit has its own copy. Ownership and growability are flags in
   `zb_buf`.

Allocation and errors:

- No allocation anywhere in the headers except `buf.h`, and there only via
  `malloc`, `realloc`, `free`. No bare size arithmetic:
  `zb_size_add()`/`zb_size_mul()`.
- No `static` object under `inst/include/zubin/`: every header promises any
  thread may call it (design §15), and `tools/check-headers` enforces it.
  `zubin-r.h` is main-thread only.
- On any failure the inputs are unchanged: a cursor that fails has not
  advanced, a buffer that fails holds what it held. Tests check this at
  every truncation point.
- Heap state that must survive a longjmp is owned by R before the first call
  that can jump (design §12): `zb_r_buf_new()` registers the finalizer
  before the buffer exists. The harness counts live buffers through the
  `ZB_INT_R_ON_NEW/FREE` hooks defined in `src/zubin_r.h`; `test-lifetime.R`
  asserts the count returns to baseline after `gc()`.
- C never raises below the outermost `.Call`; statuses map to condition
  classes by enumerator name in `R/conditions.R`, and the call is reduced to
  the function name.
- Offsets are 0-based everywhere (§14.1). Byte order is never guessed
  (§14.2).

## Testing

- **Byte-explicit:** inputs are hex strings through `bytes()`.
- **Serial:** no `Config/testthat/parallel`, so gctorture and valgrind
  instrument this package's C.
- **No file-scope objects, because shuffling reorders top-level code as
  well as tests:** anything two tests share goes in a `helper-*.R` file
  (`helper-abi.R`, `helper-bytes.R`, `helper-expect.R`, `helper-layout.R`,
  `helper-rw.R`, `helper-serialize.R`, `helper-skip.R`), never at file scope
  in a `test-*.R`.
- Compare doubles bit for bit with `bits()` when NaN payloads or `-0`
  matter.
- Never write an expected double as a long decimal literal: R's parser on
  macOS arm64 rounds some to a neighbouring double. Use exact forms
  (`2^-24`, `-2^1000`) or bytes.
- `tests/testthat/fixtures/golden.tsv` is data: generated once outside the
  repository from an independent encoder, read with
  `colClasses = "character"`, never regenerated by a test. The `arch` legs
  depend on it: the golden vectors must decode to the same values on i386,
  musl and big-endian s390x (design §15).
- Exhaustive sweeps and big buffers sit behind `ZUBIN_SLOW_TESTS=true`,
  which the full R CMD check profile sets; tests that allocate heavily call
  `skip_heavy()` (`ZUBIN_SKIP_HEAVY`, set by the gctorture leg).
- Design §13.1, the roxygen table and the tests are the same table three
  times.

## Continuous integration

**A deviation from `ci.md`:** every r-actions workflow except
`R-CMD-check.yaml` is pinned by commit to a release, with the tag in a
trailing comment, not only those that hold a write token; Dependabot bumps
them. `R-CMD-check.yaml` and `native-checks.yaml` do not pass
`profile: auto`: they run quick on a pull request and full on every push to
`main`, and the `full-ci` label runs the full set on a pull request. Each
workflow landed at the stage that gave it something to check (roadmap, "CI,
and the stage each workflow lands in").

<!-- markdownlint-disable MD013 -->
| Workflow | What it checks |
| --- | --- |
| `R-CMD-check.yaml` | r-actions' runners plus Windows R-devel (CRAN's flavour, which the Linux containers miss), `nosuggests`; the full profile sets `ZUBIN_SLOW_TESTS` |
| `native-checks.yaml` | UBSan, ASan, valgrind, LTO, gctorture, analyzers and a blocking rchk (through r-actions with `github-packages`) |
| `coverage.yaml` | coverage, and the badge under `.github/badges/` |
| `pkgdown.yaml` | the site, `development: mode: auto` |
| `abi.yaml` (own) | headers compile standalone as C99 and C++11, the include-everything and call-everything probes are warning-free under GCC and clang, then the symbol audit (design §16.1) |
| `consumer.yaml` (own) | `tools/zubintest` checks with no compiled-code NOTE, defines no global `zb_` or `zuf_` symbol, and still loads with zubin and zufast uninstalled (design §16.5) |
| `hardening.yaml` | canaries first, then `fuzz_layout`, `fuzz_unpack`, `fuzz_buf`, `fuzz_serialize`, `fuzz_sink` through r-actions' `fuzz.yml`, corpus cached: ten minutes a target per push, an hour nightly (design §16.6) |
| `arch.yaml` | the suite on i386, musl and s390x under QEMU, weekly |
| `alloc-failure.yaml` | r-actions' allocation-failure sweep over `tools/alloc-exercise.R`; informational, weekly and by hand, never on a pull request |
| `benchmarks.yaml` (own) | `tools/run-benchmarks` on Linux; by hand, and on pull requests that change the benchmarks |
<!-- markdownlint-enable MD013 -->

The fuzzers and the header gate fetch zufast's headers at its CRAN tag
(`github-packages` in r-actions), not its `main`. Issue #94 tracks gating
the jobs that still run on documentation-only pull requests.

## Editing rules specific to zubin

- Prose is en-GB (`Language: en-GB`); domain terms go in `inst/WORDLIST`.
- `README.md` is generated: edit `README.Rmd` and run
  `devtools::build_readme()`.
- The `c-api` article renders from `tools/zubintest`; edit the fixture, not
  the article's code.
