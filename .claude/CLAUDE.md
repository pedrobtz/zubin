# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`zubin` is an R package for structured binary data, and two products in one:

1. **An R API over raw vectors**, prefixed `bin_`: record layouts (`bin_layout()`), vectorised
   `bin_unpack()`/`bin_pack()`, homogeneous codecs `bin_decode()`/`bin_encode()`, a byte
   builder (`bin_builder()`, `bin_put()`, `bin_take()`), and `bin_hexdump()`/`bin_diff()`.
2. **A header-only C99 library** under `inst/include/zubin/`, consumed through
   `LinkingTo: zubin, zufast` alone: a buffer with ownership and limits, a checked cursor,
   typed reads and writes at any alignment and byte order, and layout kernels.

The headers are built on zufast's (`<zufast/bits.h>`, `<zufast/utf8.h>`); zubin never
re-implements a primitive zufast has. It is a member of the `zu*` family (sibling checkouts
in `../`: zufast is the closest model, then zukomp, zucrypt, zucbor).

## The specification

- [.agents/design.md](../.agents/design.md) is the spec, numbered §1–§20; every statement in
  it is a decision, and open questions live only in its §19 decision log.
- [.agents/roadmap.md](../.agents/roadmap.md) sequences it into Stages 0–9, each with a
  **Status:** line under its heading (never status in a heading: it changes the anchor that
  the stage issues link to).
- [.agents/next.md](../.agents/next.md) holds everything after 0.1.0.

Tracking: milestone `v0.1.0`, umbrella issue #2, stage issues #4 (Stage 0) to #13 (Stage 9).
One pull request per stage, branch `stage-N-<slug>`; close the stage issue and update its
**Status:** line in that PR. A stage that changes a decision edits design.md in the same
commit.

## Current state

Stages 0 and 1 are done. Layer 0 exists: `inst/include/zubin.h` and `zubin/{version,status,
rw,cursor}.h`, compiled standalone by `tools/check-headers` (needs `ZUFAST_INCLUDE` or an
installed zufast) and audited by `tools/run-symbol-audit`, both in `abi.yaml`. The only user
function is `bin_info()`. `src/zubin_test.c` drives the headers from `tests/testthat/` via
`zubin_test_*` entry points. `zufast` is resolved through `Remotes: pedrobtz/zufast@main`
(removed at Stage 9, once zufast is on CRAN). Next: Stage 2, the buffer and the builder.

## Commands

```sh
Rscript -e 'devtools::document()'
Rscript -e 'devtools::test()'
Rscript -e 'devtools::test(shuffle = TRUE)'
_R_CHECK_SYSTEM_CLOCK_=0 Rscript -e 'devtools::check(cran = TRUE)'
Rscript -e 'pkgdown::build_site()'                 # site -> docs/ (gitignored)
```

zufast is not on CRAN: install it from the sibling checkout (`R CMD INSTALL ../zufast`) or
with `pak::pak("pedrobtz/zufast")`. roxygen2 must be 8.1.0 or newer
(`Config/roxygen2/version`); an older one rewrites `man/`. Never hand-edit `NAMESPACE` or
`man/`.

## Naming

| Layer | Prefix | Examples |
|---|---|---|
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

1. Every function defined in a header is `static inline` (`ZB_INLINE`), never plain `static`.
2. Consumers, and zubin itself, compile with `PKG_CFLAGS = $(C_VISIBILITY)`; `zubin.so`
   exports `R_init_zubin` and nothing else (`test-abi.R`).
3. The headers reference nothing `R CMD check` forbids in compiled code: no `printf`,
   `abort`, `exit`, `assert`, `rand`, `stdout`, `stderr` (`tools/run-symbol-audit`).
4. No R header and no `SEXP` under `inst/include/zubin/`; only `zubin-r.h` includes R.
5. Never compare function pointers taken from a header-only function: each translation unit
   has its own copy. Ownership and growability are flags in `zb_buf`.

Allocation and errors:

- No allocation anywhere in the headers except `buf.h`, and there only via `malloc`,
  `realloc`, `free`. No bare size arithmetic: `zb_int_add()`/`zb_int_mul()`.
- On any failure the inputs are unchanged: a cursor that fails has not advanced, a buffer
  that fails holds what it held. Tests check this at every truncation point.
- Heap state that must survive a longjmp is owned by R before the first call that can jump
  (design §12): `zb_r_buf_new()` registers the finalizer before the buffer exists.
- C never raises below the outermost `.Call`; statuses map to condition classes by
  enumerator name in `R/conditions.R`, and the call is reduced to the function name.
- Offsets are 0-based everywhere (§14.1). Byte order is never guessed (§14.2).

## Tests

- Self-sufficient, byte-explicit (hex strings through `bytes()`), assert on condition
  classes never messages, pass under `devtools::test(shuffle = TRUE)`, serial (no
  `Config/testthat/parallel`, so gctorture and valgrind instrument this package's C).
- Shuffling reorders top-level code as well as tests: anything two tests share goes in a
  `helper-*.R` file, never at file scope in a `test-*.R`.
- Compare doubles bit for bit with `bits()` when NaN payloads or `-0` matter.
- Exhaustive sweeps and big buffers sit behind `ZUBIN_SLOW_TESTS=true`; tests that
  allocate heavily call `skip_heavy()` (`ZUBIN_SKIP_HEAVY`, set by the gctorture leg).
- Design §13.1, the roxygen table and the tests are the same table three times.

## CI

Reusable workflows from `pedrobtz/r-actions`; `coverage.yaml` is pinned by commit with the
tag in a comment, because it holds a write token. `R-CMD-check.yaml` is quick on PRs and
full on `main`; the `full-ci` label runs the full set on a PR. `pkgdown.yaml` deploys with
`development: mode: auto`. Each later workflow lands at the stage that gives it something
to check (roadmap, "CI, and the stage each workflow lands in").

This file lives in `.claude/`, not the package root, because pkgdown renders every
root-level `*.md` as a site page.

Prose is en-GB.
