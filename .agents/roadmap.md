# zubin — Roadmap to 0.1.0 (first CRAN release)

Companion to [design.md](design.md) revision 2. Section references (§) point there.
[next.md](next.md) holds everything after 0.1.0.

Status: adopted 2026-10-04. Nothing below is implemented; the repository is the `usethis`
skeleton plus these documents.

## Sequencing principles

1. **The header gate lands before the first header has a consumer**, including zubin's
   own R glue. Every header is compiled standalone, as C99 and C++11, from the commit that
   creates it; a header that only ever compiled inside `zubin.so` has never been tested as
   what it is.
2. **Ownership and limits land before anything allocates.** The buffer's flags, cap and
   checked arithmetic (§9) are Stage 2; nothing in a later stage grows memory any other way.
   zukomp's version of this rule is "limits before the tree".
3. **The parser lands before the kernels.** Untrusted strings (layout specs) are checked
   and fuzzed before untrusted bytes are read through them.
4. **Unpack before pack.** Pack's oracle is unpack; the round-trip property arrives with
   Stage 5 and every later stage inherits it.
5. **Byte order is proven at Stage 4, not discovered later.** The s390x leg runs the golden
   vectors the day the first kernel exists (§15).
6. **Every stage ends with something runnable and tested**, and a stage is done when its
   exit criteria pass in CI on all three platforms, not when the code is written.
7. **Gates need canaries.** A gate is trusted once it has been seen to fail on purpose:
   a planted symbol, a broken finalizer, a fuzz canary that crashes.
8. **Change the design in the same commit as the contract.** A stage that changes a
   decision in §19 edits design.md in that commit, with the roxygen table and the tests
   that state it.

Sizes are relative: **S** ≈ a sitting, **M** ≈ a few, **L** ≈ the stage is the week.

**Status never goes in a heading.** A heading is `## Stage N — Title · Size` and nothing
else; the state is the **Status:** line under it. Status words in a heading change its
GitHub anchor and break every issue that links to it.

**Tracking.** A `v0.1.0` milestone, one umbrella issue, and one `stage`-labelled sub-issue
per stage linking to its heading here. Close a stage's issue when its exit criteria pass
and update its **Status:** line in the same PR. Each stage's section gains a **What
actually happened** block when it closes: what the plan got wrong is the most useful thing
these files record.

## The release order, and what it costs

zubin's headers include zufast's, so `LinkingTo: zufast` is unconditional and **zufast must
be on CRAN before zubin can be submitted**. On 2026-10-04 zufast is tagged 0.1.0 and not
on CRAN. Until it is, `DESCRIPTION` carries `Remotes: pedrobtz/zufast@main` so that
`setup-r-dependencies` and `pak` resolve it in CI (zuxlsx does the same for three
siblings), and Stage 9 removes the field. Every stage before 9 can proceed; only the
submission waits. If zufast's release slips indefinitely, the fallback is to inline the six
load/store functions zubin uses into `rw.h` under a documented exception to design decision
13; that is a one-day change and is not planned.

The family order is therefore: zufast 0.1.0 on CRAN, then zubin 0.1.0, then the adoption
issues of §3.2.

## Working rhythm

One pull request per stage (zucrypt's rhythm): branch `stage-N-<slug>` from `main`;
`devtools::document()`, `devtools::test()`, `devtools::test(shuffle = TRUE)` and
`devtools::check(cran = TRUE)` clean at 0/0/0 locally before pushing; the PR body states
the stage and its exit criteria as a checklist; every CI leg green before merging, not
"green except the container ones"; then update CLAUDE.md's current-state paragraph and the
**Status:** line.

Two local traps, known today: roxygen2 on this machine is 8.0.0 and the family pins
`Config/roxygen2/version: 8.1.0`, so upgrade before the first `document()` or `man/` is
rewritten; and `devtools::check()` offline NOTEs on the system clock, silenced with
`_R_CHECK_SYSTEM_CLOCK_=0`.

## Testing strategy, fixed once

Inherited from the family and stated in §16; every stage adds its tests under these rules.

- Self-sufficient tests: inputs built inside each `test_that()`; byte-explicit, as hex
  strings through `bytes("1a 2b")` in `helper-bytes.R`, never from source-file string
  literals; `withr::local_seed()` for anything random.
- Assert on condition classes, never message text; wording is covered by snapshots.
- Serial testthat, no `Config/testthat/parallel` (§15). `devtools::test(shuffle = TRUE)` is
  in every stage's definition of done.
- Helpers: `helper-bytes.R` (`bytes()`, `hex()`), `helper-expect.R`
  (`expect_roundtrip(layout, df)`, `expect_bytes(x, hex)`, `expect_zubin_error(expr,
  class, ...)` asserting the condition's fields), `helper-abi.R` from zufast,
  `helper-skip.R` (`skip_if_no_slow_tests()` on `ZUBIN_SLOW_TESTS`, `skip_heavy()` on
  `ZUBIN_SKIP_HEAVY` for the gctorture leg, zucbor's pattern).
- The always-compiled harness `src/zubin_test.c` (§16.3) is the main lever: it drives the
  headers at caller-chosen sizes, strides, truncation points and caps, and exposes the
  live-buffer counter.
- Fixtures: `tests/testthat/fixtures/golden.tsv` (§16.4), read with `colClasses =
  "character"` (zukomp's trap); generated nothing at test time.
- CRAN budget: the suite finishes in under a minute; sweeps and 64 MiB buffers behind
  `ZUBIN_SLOW_TESTS=true`, run in CI.

## CI, and the stage each workflow lands in

Reusable workflows from `pedrobtz/r-actions` at `@v1`, except `coverage.yml`, pinned by
commit with the tag in a comment, because it holds a write token (zucrypt's rule; the
skeleton's `coverage.yaml` uses `@v1` today and Stage 0 pins it). Bespoke workflows are
copied from zufast and adapted. A workflow lands at the stage where it has something to
check; a job that is green because it inspected nothing is worse than none.

| Workflow | Stage | What it checks |
|---|---|---|
| `R-CMD-check.yaml` (exists) | 0 | runners and the CRAN-like containers; quick on PRs, full on `main` and with the `full-ci` label |
| `coverage.yaml` (exists) | 0 | badge on `main`; `native: true` from Stage 1 |
| `pkgdown.yaml` (exists) | 0 | the site; `development: mode: auto` |
| `abi.yaml` | 1 | `tools/check-headers` with GCC and clang on Linux and macOS; `tools/run-symbol-audit` |
| `native-checks.yaml` | 2 | r-actions sanitizers, valgrind, LTO, gctorture (quick step on PRs, step 20 on `main`), blocking rchk, analyzers |
| `hardening.yaml` | 3 | `tools/run-fuzz`: canaries, then each libFuzzer target; r-actions `fuzz.yml` with a cached corpus; nightly long runs |
| `arch.yaml` | 4 | i386, musl, s390x; `error-on: warning`, `require-tests: true`, testthat installed in every leg (zucrypt's finding) |
| `consumer.yaml` | 7 | `tools/zubintest` on three operating systems, with and without zubin installed |
| `alloc-failure.yaml` | 8 | r-actions allocation-failure sweep over `bin_put()` and `bin_unpack()`, if the interposer fits; informational first |
| `vendor.yaml`, `vendor-upstream.yaml` | — | not used: zubin vendors nothing |

CI stands in for win-builder and macbuilder (zukomp's rule): the Windows R-devel and
macOS release legs are CRAN's own builds under `--as-cran`. Neither builder is a release
step; both remain available to reproduce a CRAN failure.

## Stage map

| Stage | Size | Needs | Delivers |
|---|---|---|---|
| 0 — Package identity and a clean baseline | S | — | a package that checks 0/0/0 with zufast resolved |
| 1 — Layer 0: status, version, rw, cursor; the header gate | M | 0 | `zubin.h`, `abi.yaml`, `bin_info()`, the harness |
| 2 — The buffer and the builder | M | 1 | `buf.h`, `zubin-r.h`, `bin_builder()` and friends, `native-checks.yaml` |
| 3 — Layouts and the spec parser | M | 1 | `layout.h` types and parser, `bin_layout()`, `hardening.yaml` |
| 4 — Unpack and decode | L | 2, 3 | kernels, `bin_unpack()`, `bin_decode()`, golden vectors, `arch.yaml` |
| 5 — Pack and encode | M | 4 | inverse kernels, `bin_pack()`, `bin_encode()`, typed `bin_put()`, round trips |
| 6 — Hexdump and diff | S | 1 | `bin_hexdump()`, `bin_diff()` |
| 7 — The consumer fixture and the C contract | M | 5 | `tools/zubintest`, `consumer.yaml`, the README recipe, the C-API article |
| 8 — Hardening, documentation, benchmarks | M | 6, 7 | vignette, WORDLIST, cran-comments, benchmarks, full gates |
| 9 — Release 0.1.0 | S | 8, and zufast on CRAN | the submission |

Stage 6 is off the critical path and can be done in any spare sitting after Stage 1.

---

## Stage 0 — Package identity and a clean baseline · S

**Status:** done (#4).

**Goal:** the `usethis` skeleton becomes a package with the right metadata, registration
and build hygiene, so every later stage is measured against a clean 0/0/0.

**Do**

- `DESCRIPTION`: `Title: Read and Write Structured Binary Data`; a Description naming
  layouts, vectorised unpack and pack, typed codecs at every width and byte order, the byte
  builder, and the header-only C API through `LinkingTo`; `Authors@R` Pedro Baltazar
  (`aut`, `cre`, `cph`); `Depends: R (>= 4.1)`; `LinkingTo: zufast (>= 0.1.0)`;
  `Remotes: pedrobtz/zufast@main` (development only, see above); `Suggests: bit64, knitr,
  rmarkdown, testthat (>= 3.0.0), withr`; `VignetteBuilder: knitr`; `Language: en-GB`;
  `Config/roxygen2/version: 8.1.0` (upgrade roxygen2 first); `URL` and `BugReports`;
  `Config/testthat/edition: 3` and **no** `Config/testthat/parallel`. No `Copyright:`
  field: nothing is vendored.
- `LICENSE` and `LICENSE.md` name the real holder.
- `.Rbuildignore`: add `^\.agents$`, `^tools$`, `^cran-comments\.md$`,
  `^vignettes/articles$`, `^CRAN-SUBMISSION$`, `^\.claude$`, and the `src/` patterns for
  `*.o`, `*.so`, `*.dll`, `*.dylib`; the same object patterns in `.gitignore`.
- `src/init.c` with `R_registerRoutines()` over an empty table, `R_useDynamicSymbols(dll,
  FALSE)`, `R_forceSymbols(dll, TRUE)`; delete `src/zubin.c`; `src/Makevars` with
  `PKG_CPPFLAGS = -I../inst/include`, `PKG_CFLAGS = $(C_VISIBILITY)`, `OBJECTS = init.o`.
- `R/zubin-package.R` with the `@useDynLib zubin, .registration = TRUE` block;
  `R/conditions.R` with `zubin_abort()` and `invalid_argument()` in zufast's shape, the call
  reduced to the function name (§13.7).
- `NEWS.md`: `# zubin 0.0.0.9000` (a bare "development version" heading is a check NOTE
  once it is the only one).
- Replace the template test with `tests/testthat/test-init.R`: the DLL is loaded and
  registered. An empty `tests/testthat/` beside `tests/testthat.R` is a hard check error.
- `coverage.yaml`: pin `coverage.yml` by commit with the tag in a trailing comment.
- `.claude/CLAUDE.md` (not the root, which pkgdown would render): what zubin is, the two
  documents as the spec, the commands, the naming table, the invariants of §4.5 and §12,
  and a current-state paragraph that every stage updates.

**Exit**

- `devtools::check(cran = TRUE)` is 0/0/0 locally, apart from the CRAN-incoming
  version NOTE.
- `R-CMD-check.yaml` is green on every leg with zufast installed from the `Remotes` field.
- `NAMESPACE` carries `useDynLib(zubin, .registration = TRUE)`.

**Not this stage:** any header, any C beyond `init.c`.

**What actually happened**

- The usethis template's `src/zubin.c` was not a registration table; deleting it left
  `tests/testthat/` empty for a moment, and `git rm` removes the directory with the last
  file, so the replacement test has to be written after recreating it.
- `init.c` needs `<stddef.h>` for `NULL`: `R_ext/Rdynload.h` does not bring it in under
  clang.
- `devtools::check(cran = TRUE)` was 0/0/0 locally without the CRAN-incoming NOTE, because
  `check()` skips the incoming feasibility checks unless `remote = TRUE`.
- The tracking issues were created with this stage: milestone `v0.1.0`, umbrella #2, stage
  issues #4–#13. (#3 was created by mistake and closed.)

---

## Stage 1 — Layer 0: status, version, rw, cursor; the header gate · M

**Status:** done (#5).

**Goal:** the first four headers exist, compile standalone under the gate, and are
exercised through the package's own shared object; the delivery mechanism of §4 is proven
before any feature rides on it.

**Do**

- `inst/include/zubin/version.h`, `status.h` (§7), `detail/portability.h` (over zufast's),
  `rw.h` (§8), `cursor.h` (§10); `inst/include/zubin.h` including them, with the
  `#error` on `ZUFAST_VERSION_NUMBER < 100`.
- `tools/check-headers` from zufast, adapted: standalone compiles, `tools/abi/probe-none.c`,
  `tools/abi/probe-all.c` (every function of these four headers, extended by each later
  stage), the forbidden-identifier grep; `tools/run-symbol-audit`; `.github/workflows/abi.yaml`.
- `src/zubin_r.c` with `zubin_info()`; `R/info.R` with `bin_info()` (§13.7).
- `src/zubin_test.c`: `zubin_test_rw(type, endian, bytes)` and its inverse, driving every
  `zb_rd_*`/`zb_wr_*`; `zubin_test_cursor(bytes, plan)` reading a sequence of typed fields
  and reporting the status and position after each.
- Tests: `test-abi.R` (exports exactly `R_init_zubin`; `bin_info()$zufast` is the pinned
  version), `test-info.R`, `test-rw.R` (differential against `readBin()`/`writeBin()` for
  every width and order base R has; f16 and bf16 over all 65 536 patterns against zufast's
  own conversion and against an R reference), `test-cursor.R` (every truncation point of a
  mixed record returns `ZB_ERR_EOF` and leaves `pos` unchanged; `seek` and `skip` past the
  end refuse).
- `coverage.yaml` gains `native: true`.

**Exit**

- `abi.yaml` green with GCC and clang on Linux and clang on macOS: every header C99 and
  C++11 clean at `-O0` and `-O2`, both probes warning-free, no R identifier under `zubin/`.
- `tools/run-symbol-audit` passes, and was seen to fail with a planted `printf`.
- `test-abi.R` shows exactly `R_init_zubin`.
- The cursor is unchanged on every failure, at every position, for every type.

**Not this stage:** `buf.h`, `layout.h`, anything a user calls except `bin_info()`.

**What actually happened**

- The zufast version check lives in `detail/portability.h`, which every header includes
  first, not only in `zubin.h`: a consumer that includes `<zubin/cursor.h>` alone gets it too.
- `zb_status_string()` returns the enumerator's name (`"ZB_ERR_EOF"`), so the R side maps
  statuses to classes by name (`zb_status_class` in `R/conditions.R`) with no table of
  numbers to keep in step.
- `rw.h` gained `zb_host_big_endian()` and an internal double-to-float narrowing that
  clamps out-of-range finite doubles itself (the C conversion is undefined there); design §8
  now says so.
- Both gate scripts carry their canaries inside them, so every CI run re-proves them: the
  header gate compiles an unused plain `static` function (must fail) beside a `static
  inline` control (must pass) and fires its R-API and allocation greps on a planted header;
  the symbol audit builds the probe with a planted `printf()` and must fail before it audits
  the real one. The gate also enforces design §5's "no allocation outside `buf.h`".
- The harness returns 64-bit integers as their bit pattern in a double, which is bit64's
  representation; that exposed that **bit64 reads −2^63 as `NA_integer64_`**. The reader is
  right to return the bits; what `int64 = "integer64"` does with −2^63 is a Stage 4 decision
  (§14.3 names only `i32` and `f64` today).
- `devtools::test(shuffle = TRUE)` shuffles top-level code too, so objects shared by tests
  (`base_endian`, the mixed cursor record) belong in `helper-*.R`, not at file scope.
- The planted-`printf` canary failed on its first Linux run: glibc's `_FORTIFY_SOURCE`
  turns `printf()` into `__printf_chk`, which zufast's hand-written list of forbidden names
  does not contain (nor `sprintf`). The audit now asks R itself,
  `tools:::check_so_symbols()`, which is what `R CMD check` runs, with the platform's own
  table. The canary earned its keep on day one.
- The native coverage build links libgcov, which exports `__gcov_*` and also `mangle_path`;
  `test-abi.R` skips on an instrumented object (one exporting any gcov or LLVM profile
  symbol) rather than guessing the runtime's names, since every `R-CMD-check` leg runs it on
  the object CRAN builds.
- `abi.yaml` checks out `pedrobtz/zufast` for the header gate and installs it for the symbol
  audit; both go away only when zufast's headers are reachable another way (never: the gate
  needs them).

---

## Stage 2 — The buffer and the builder · M

**Status:** done (#6).

**Goal:** `zb_buf` with its ownership model, limits and checked growth, the R-owned
variant in `zubin-r.h`, and the R builder over them; the heap discipline of §12 is in place
and proven before any kernel allocates.

**Do**

- `inst/include/zubin/buf.h` (§9): the struct and flags, `init`, `alloc`, `borrow`,
  `release`, `reset`, `detach`, `reserve` with `zb_int_add()`/`zb_int_mul()`/`zb_int_grow()`,
  `zb_put_bytes()`, `zb_put_zeros()`, `zb_put_raw()`, and the scalar `zb_put_<type><order>()`
  and vectorised `_n` appends for every type of §8.
- `inst/include/zubin-r.h` (§12): `zb_r_buf_new()` with the finalizer registered before the
  buffer exists and `onexit = TRUE`, `zb_r_buf_get()`, `zb_r_buf_borrow()`,
  `zb_r_buf_to_raw()`. `tools/check-headers` gains the probe that compiles it against R's
  headers.
- `R/builder.R`: `bin_builder()`, `bin_put()` for raw input and for `"z"` and `"s<n>"`
  strings, `bin_reserve()`, `bin_reset()`, `bin_size()` (the generic, with the builder
  method), `bin_take()`, `as.raw()`, `print()`. Conditions `zubin_limit_error` (`size`,
  `max`), `zubin_memory_error`, and the finalized-builder error.
- Harness: `zubin_test_buf_growth(sizes)` reporting reallocation counts;
  `zubin_test_buf_cap(max, puts)`; `zubin_test_live_buffers()`, the live counter;
  `zubin_test_put_then_error()` and a put loop long enough to interrupt.
- Tests: growth from 0 past 64 MiB in `ZUBIN_SLOW_TESTS` with the expected reallocation
  count; exactly `max` succeeds, `max + 1` is `zubin_limit_error`, the buffer is unchanged
  and `ZB_BUF_HIT_LIMIT` is set; `bin_take()` empties and keeps capacity; a taken builder
  accepts new puts; a borrowed buffer cannot grow; `test-lifetime.R`: after an error inside
  a put and after a `setTimeLimit()` interrupt of a long put loop, the live counter returns
  to its baseline after `gc()`; `gctorture(TRUE)` over the builder tests.
- `.github/workflows/native-checks.yaml`: sanitizers, valgrind, LTO, gctorture (quick on
  PRs), blocking rchk, analyzers, from r-actions.

**Exit**

- Every test above green; `native-checks.yaml` green including rchk.
- The lifetime test was shown to fail with the finalizer broken, and the PR records it.
- `abi.yaml` still green with `buf.h` and `zubin-r.h` added to both probes.

**Not this stage:** typed `bin_put(b, x, type = "u32")`, which shares the pack kernels and
is Stage 5.

**What actually happened**

- `zb_r_buf_new()` gained a `zb_status *` out-parameter and returns `R_NilValue` on failure,
  and `zb_r_buf_free()` was added for the eager release §12 describes but did not name: a
  glue header that raised `Rf_error()` would break "C never raises" for every consumer.
  Design §12 says so now. The external pointer is tagged `zubin_buf`, so `zb_r_buf_get()`
  refuses a foreign one.
- `zb_put_raw(b, 0)` returns a non-`NULL` dummy slot even on a buffer with no storage, so
  `NULL` always means failure (design §9.4).
- The `.Call` convention: an entry point that can fail returns, on failure, the status's
  enumerator name as a `zubin_status`-classed string (with `index` when an element is at
  fault), and `R/conditions.R` maps it by name. No status crosses as a number.
- `bin_put()` reserves the whole append first and advances `len` only after the last
  64 MiB chunk is copied, so an interrupt between chunks leaves the builder unchanged; the
  slow test interrupts a 1 GiB append to show it.
- The finalizer canary, run locally with `R_RegisterCFinalizerEx()` removed from
  `zubin-r.h`: all three lifetime tests failed (live count 1, 2, 3 against 0, 1, 2), and
  passed again with it restored. The PR records the output.
- rchk: r-actions' `rchk.yml` could not install a `LinkingTo` dependency that is on neither
  CRAN nor Bioconductor, since the rchk image resolves only those. A bespoke job proved the
  fix (install zufast into the image's library, which lives in the mounted directory, with
  the image's `R` passthrough), and it then went into r-actions as the `github-packages`
  input of `rchk.yml` and `fuzz.yml`; zubin uses the reusable workflow, blocking on findings.
  The input goes at Stage 9, when zufast is on CRAN.
- CI found three things the local build could not. GCC's `-Walloc-size-larger-than`
  proved a `realloc(SIZE_MAX)` reachable after overflow in `zb_int_grow()` (a WARNING on
  every GCC leg, Windows included): growth is now bounded by `ZB_BUF_MAX_CAP`
  (`PTRDIFF_MAX`) and refuses before asking. rchk found an unprotected result across
  `zb_r_buf_free()` in the harness, because `zb_r_buf_get()` called `Rf_install()`; it now
  compares the tag by name and allocates nothing, so consumers need not protect around it.
  And it flagged `zubin_int_status()`'s unprotected `Rf_setAttrib()` arguments.
- The first bespoke rchk run failed on "too many states" lines, which name R's own
  functions rchk gives up on; the gate now parses findings as r-actions' `rchk.yml` does.
- R's headers on 4.6 use a C23 fixed-underlying-type enum (`R_ext/Boolean.h`), so the
  `zubin-r.h` probe compiles as gnu17 without `-Wpedantic`, in C and C++11; the `abi.yaml`
  header jobs now set up R for it.

---

## Stage 3 — Layouts and the spec parser · M

**Status:** done (#7).

**Goal:** the layout descriptor and its grammar, parsed without allocating, fuzzed from the
day it exists, and exposed as the `zubin_layout` object.

**Do**

- `inst/include/zubin/layout.h`: `zb_type`, `zb_field`, `zb_layout` (§11.1);
  `zb_layout_count_fields()`, `zb_layout_parse()` with every rule of §11.2 and the byte
  position of each error; the alignment rule of §11.4.
- `R/layout.R`: `bin_layout()` with the string and named-vector forms, the normalised
  `spec`, the `fields` data frame, `size`, `align`; methods `print`, `format`, `length`,
  `names`, `as.data.frame`, `bin_size()`; `zubin_spec_error` with `position`.
- Harness: `zubin_test_layout(spec, endian, align)` returning the C-side field table, so
  the R object and the C parse are compared field by field; `zubin_test_struct_offsets()`
  returning `offsetof()` and `sizeof()` for a dozen C structs compiled into the harness, the
  oracle for `align = TRUE`.
- `tools/fuzz/fuzz_layout.c` over the parser, `tools/fuzz/canary.c`, `tools/run-fuzz`
  (canaries first, exit status captured, not a pipe's); `.github/workflows/hardening.yaml`
  with the r-actions `fuzz.yml` and a cached corpus; seeds from every example in design
  §11.2 and §13.8.
- Tests: one test per grammar rule and per error, each asserting the class and the
  position; the WAV, BMP, PNG IHDR, Java class, CFB and ITCH layouts' sizes and offsets
  against their specifications; alignment against the harness oracle; the named-vector form
  equals the string form; a `zubin_layout` passes through unchanged; `print` snapshot.

**Exit**

- Every grammar rule and error has a test; `abi.yaml` green with `layout.h` in the probes.
- `hardening.yaml` green: the canary crashed, `fuzz_layout` ran ten minutes on the PR with
  no finding.
- The alignment oracle agrees on every struct, on all three operating systems.

**Not this stage:** reading or writing a single byte through a layout.

**What actually happened**

- The first R-level run caught a parser bug the C-only tests would also have caught: the
  count parser never stored its value, so every `b`, `s`, `x` width and array count was 0.
  Comparing the R table against the C harness table field by field is what makes such a bug
  loud rather than a wrong size somewhere downstream.
- The normalised spec's prefix must be the order the spec *declares*, not the `endian`
  argument: the round-trip test `bin_layout(l$spec) == l` caught it.
- **32-bit x86 aligns 8-byte struct members to 4.** Design §11.4 claimed the rule held on
  every target R supports; it holds on every 64-bit one. The oracle structs carry a flag, and
  the eight-byte cases skip on a 32-bit build (the i386 leg of Stage 4 runs the rest). The
  design and `?bin_layout` say so.
- The canary is zufast's mechanism, a `-DZB_FUZZ_CANARY` build that overflows on the input
  `ZB-CANARY`, not a separate `canary.c`. r-actions' `fuzz.yml` could not fetch zufast's
  headers; it gained a `github-packages` input for that (with `rchk.yml`, Stage 2), and
  `hardening.yaml` uses it: a small canaries job (`CANARY_ONLY=1 tools/run-fuzz`, since the
  reusable workflow has no canary step), then each target through `fuzz.yml` with its
  corpus cached, ten minutes on every PR and push, an hour nightly. `cflags` carries
  `-fno-sanitize-recover=undefined`, or a UBSan report would print and the run go on.
- `zb_type_width()`, `zb_type_name()` and `ZB_LAYOUT_MAX` became public (design §11.3); the
  R glue and the fuzz target needed them, and a consumer will too.
- `run-fuzz` reads `FUZZ_SECONDS`, not `SECONDS`, which is a special variable in bash.

---

## Stage 4 — Unpack and decode · L

**Status:** done (#8).

**Goal:** the field-major kernels, `bin_unpack()` and `bin_decode()`, the whole §13.1 type
model in the reading direction, and the golden vectors proven on a big-endian host.

**Do**

- `layout.h`: the unpack kernels of §11.5 with the column-major array rule.
- `src/zubin_r.c`: `zubin_unpack()` and `zubin_decode()`: argument re-validation in C,
  bounds checks with offsets, one column allocated per field, `R_CheckUserInterrupt()`
  between fields, `b<n>` into a list of raw, `s<n>` through `zuf_utf8_valid()` into
  `CE_UTF8`/`CE_LATIN1`/`CE_BYTES` CHARSXPs made in one function, the `integer64` class on
  request, the `i32` NA rule.
- `R/unpack.R`, `R/decode.R`: `bin_unpack()` and `bin_decode()` with every argument of
  §13.3–§13.4; the data frame assembly with `I()` on byte columns; conditions
  `zubin_bounds_error` (`offset`, `length`), `zubin_range_error` (`field`, `index`),
  `zubin_encoding_error` (`field`, `index`).
- `tests/testthat/fixtures/golden.tsv`: spec, hex, expected values, source, for the formats
  of §16.4 and for every `readBin()` width.
- Harness: `zubin_test_unpack_kernel(bytes, spec, n, stride)` driving each kernel directly,
  including odd strides and `n = 0`.
- `tools/fuzz/fuzz_unpack.c`: a spec and bytes; every field of every whole record is
  decoded; nothing may crash or read out of bounds.
- Tests: one per §13.1 row in the reading direction, including every error in the row;
  golden vectors; `offset`, `n`, `stride`, trailing bytes, explicit `n` past the end; array
  fields as matrices and as expanded columns; empty input; a 2^31 + 16 byte raw decoded in
  `ZUBIN_SLOW_TESTS`; the interrupt test (`setTimeLimit()` inside the unpack expression,
  behind `skip_heavy()`), after which the same input unpacks in full; Markus Kuhn's UTF-8
  cases through `s<n>`.
- `.github/workflows/arch.yaml` from zufast's: i386, musl, s390x, with testthat installed
  and `error-on: warning`, `require-tests: true`. Dispatch it by hand and record the test
  counts in the PR.

**Exit**

- The golden vectors decode identically on every `R-CMD-check` leg and on i386, musl and
  s390x; the arch legs report a non-zero test count.
- Every §13.1 row has a reading test; `fuzz_unpack` ran ten minutes with no finding.
- Sanitizers, valgrind and rchk clean over the new code; the interrupt test passes.

**Not this stage:** writing.

**What actually happened**

- Every unpack kernel returns a status, and the `i32` kernel gained `*bad`: a C consumer that
  dispatches a field to the wrong kernel is told, and the R glue needs the first offending
  record to name it. Design §11.5 has the signatures.
- −2^63 read as `integer64` is an error unless `na = "allow"` (design §14.3), the decision
  Stage 1 deferred.
- The golden vectors are generated once by a script outside the repository, with Python's
  `struct` and `zlib.crc32` as an encoder independent of R and of zubin, and committed as
  data; the test only reads them. Expected values are R expressions, and two had to be
  written exactly (`-2^1000`, `2^-24`, `2^-1074`): R's parser on macOS arm64 turns some
  decimal literals into a neighbouring double, zucbor's finding again.
- A failed field is reported by its position in the layout, not by name, because an unnamed
  field has no name in C; R names it from the layout.
- `arch.yaml` cannot use r-actions' dependency resolution for zufast either: each leg
  downloads zufast's `main` tarball from GitHub and installs it, which needed
  `ca-certificates` in the images.
- The s390x leg's first run: all 34 golden vectors decoded identically on the big-endian
  host, and it caught one test comparing little-endian output with `writeBin()`'s default,
  which is the *native* order. The i386 leg (R 4.2) caught three portability bugs in tests:
  `DLLInfo` has no `forceSymbols` before R 4.3, `sample()` cannot hold the whole `int`
  range on a 32-bit build, and `rawToChar()` gives a native string that the check's C
  locale cannot translate to UTF-8. `arch.yaml` sets `_R_CHECK_TESTS_NLINES_=0` so a failing
  leg shows its whole test output.
- The interrupt test interrupts an unpack of 64 string fields over 200 000 records (one
  interrupt check per field, never inside a kernel), then unpacks the same input in full.

---

## Stage 5 — Pack and encode · M

**Status:** done (#9).

**Goal:** the inverse kernels with range and NA checks, `bin_pack()`, `bin_encode()` and
typed `bin_put()`, and the round-trip properties that make every later change cheap to
trust.

**Do**

- `layout.h`: the pack kernels of §11.5 with `*bad`; `zb_pack_zeros()` for padding and
  alignment gaps.
- `src/zubin_r.c`: `zubin_pack()` (one exact-size `RAWSXP`, zeros first, then each field),
  `zubin_encode()`, and `zubin_put_typed()` into a builder through `zb_put_raw()` plus the
  same kernels, so there is one conversion path.
- `R/pack.R`, `R/encode.R`, `R/builder.R`: `bin_pack()` with named vectors or one frame,
  the recycling rule of §14.5, `integer64` input by class; `bin_encode()`; `bin_put()`
  gains `type`; `zubin_na_error` (`field`, `index`).
- `tools/fuzz/fuzz_buf.c`: an input-driven sequence of typed puts, resets and reserves
  against a tiny cap; `len <= cap <= max` and the flag hold after every step.
- Tests: one per §13.1 row in the writing direction, including every error; `bin_pack()`
  twice gives identical bytes and no byte is uninitialised (valgrind leg); round trips
  `expect_roundtrip(layout, df)` over generated layouts and frames under
  `withr::local_seed()`; `bin_encode(bin_decode(b, t), t)` is `b` for every type over random
  bytes; f16 and bf16 exhaustive in both directions; differential against `writeBin()`;
  `bin_put(b, x, "u32")` equals `bin_put(b, bin_encode(x, "u32"))`; recycling that does not
  divide is an error.

**Exit**

- Every §13.1 row has tests in both directions; the roxygen table, design §13.1 and the
  tests agree.
- Round-trip properties green on every platform; `fuzz_buf` ran ten minutes with no
  finding; valgrind reports no uninitialised byte in packed output.

**Not this stage:** any new field type.

**What actually happened**

- Every pack kernel takes `allow_na`, not only `zb_pack_i32`: R's NA markers reach `i32`
  from a double column and `i64` from an integer64 one as well (design §11.5).
- `zb_pack_f64` writes every numeric type, integers included, because R's doubles carry most
  integers; it checks the range before casting, since casting an out-of-range double is
  undefined.
- In the C glue, `zb_field.count` for `b` and `s` is the byte width, not an element count;
  both the pack glue and typed `bin_put()` tripped on it before the first test passed. The
  R table already says `count = 1` for them, which is why the C name confused the C author.
- Typed `bin_put()` uses the pack kernels in place, chunked by 64 MiB with an interrupt
  check between chunks, and commits `len` only at the end; on failure nothing is appended.
- `fuzz_buf` keeps a shadow copy of the buffer by plain `memcpy` and checks the bytes,
  `len <= cap <= max` and `ZB_BUF_HIT_LIMIT` after every step. Its first crash was the
  harness's own mistake (a refused reserve checked against the append size), and so was its
  first CI finding: the shadow copy did `memcpy(shadow, b.data + len, 0)` while `b.data` was
  still `NULL`, which UBSan reports once it cannot recover. The input is now a seed.
- `bin_pack()` accepts the data frame `bin_unpack()` returns, array fields as `name.k`
  columns included, so `expect_roundtrip()` checks both directions in both shapes.

---

## Stage 6 — Hexdump and diff · S

**Status:** done (#10).

**Goal:** `bin_hexdump()` and `bin_diff()` (§13.6), used from here on in every example and
test that shows bytes.

**Do**

- `src/zubin_r.c`: `zubin_hexdump()` formatting lines in C (offset, hex groups, ASCII
  column) with `offset`, `n`, `width`; `zubin_diff()` returning the first `n` differing
  offsets and both bytes.
- `R/hexdump.R`: `bin_hexdump()` returning a `zubin_hexdump` character vector with a
  `print` method; `bin_diff()` returning a data frame with the two lengths as attributes.
- Tests: snapshots for a known 64-byte input at widths 8 and 16 and at a non-zero offset;
  `n` beyond the end; empty input; `bin_diff()` on equal, prefix, and differing inputs.

**Exit**

- Snapshots green on all three operating systems (line endings included).

**What actually happened**

- Both formatters are C (`zubin_hexdump()`, `zubin_diff()`), writing each line into one
  `R_alloc` buffer; offsets widen from 8 to 16 hex digits past 4 GiB.
- `n` past the end shows what there is, as presentation should; an `offset` past the end
  is still a `zubin_bounds_error`, as everywhere else. `bin_diff()` compares over the
  common length and reports both lengths as attributes, so a prefix is zero rows and two
  lengths.

---

## Stage 7 — The consumer fixture and the C contract · M

**Status:** done (#11).

**Goal:** the header-only delivery is proven by a package in the exact shape every consumer
will have, with and without zubin installed, on three operating systems.

**Do**

- `tools/zubintest` (§16.5): `LinkingTo: zubin, zufast`, no `Imports`, `useDynLib` only;
  two translation units including `<zubin.h>`, one including `<zubin-r.h>`; a suite that
  parses a layout, unpacks a record, builds a buffer through `zb_r_buf_new()`, borrows a raw
  vector, and reads it with a cursor.
- `.github/workflows/consumer.yaml` from zufast's: install zufast and zubin, then the
  fixture; `R CMD check --as-cran` of the fixture must show `checking compiled code ... OK`
  and no ERROR or WARNING; run its tests; `nm` shows no global `zb_` or `zuf_` symbol in
  its shared object; move zubin out of the library path with `R_LIBS_USER='-'` and run the
  tests again.
- `README.md`: the full consumer recipe, "Using zubin from C", followed verbatim by the
  fixture; `vignettes/articles/c-api.Rmd` (pkgdown-only): §4, §5, §9–§12 for a package
  author, quoting the fixture rather than inventing examples.
- `tools/abi/probe-all.c` covers every public function now in the headers.

**Exit**

- `consumer.yaml` green on Linux, macOS and Windows, including the zubin-uninstalled step.
- The README recipe and the fixture's `DESCRIPTION`, `NAMESPACE` and `Makevars` agree
  line for line.

**What actually happened**

- "Agree line for line" is a gate, not a promise: the README marks each recipe block with
  `<!-- recipe: FILE -->`, and `tools/check-recipe` (first step of `consumer.yaml`) compares
  each block with the fixture's file, and was seen to fail on a planted edit. The README
  quotes two whole fixture sources, `cursor.c` and `buffer.c`, as its C examples.
- The `c-api` article reads its C from the fixture at build time instead of carrying copies.
- The fixture has three translation units: `layout.c` and `cursor.c` include `<zubin.h>`
  (so a non-`static` symbol would collide), `buffer.c` includes `<zubin-r.h>`. Its
  `buffer.c` raises with `Rf_error()` while a buffer is live, which is the case the R glue
  exists for.
- Its first CI run failed for two reasons, neither the fixture's. A fix committed to Stage 5
  with `git add -A` had swept up the fixture's macOS build products, untracked in the
  working tree and not yet ignored on that branch, and they reached `main`: Linux then
  found an up-to-date `zubintest.so` with a Mach-O header. This stage removes them and
  ignores object files repository-wide. And Windows converts line endings on checkout, so
  `tools/check-recipe` now ignores carriage returns.
- `consumer.yaml` uninstalls zufast as well as zubin before the last run: neither is needed
  at run time, and only removing both proves it.

---

## Stage 8 — Hardening, documentation, benchmarks · M

**Status:** done (#12). On `main` after merge: an hour of fuzzing per target with no finding
(dispatched run 37267269936: `fuzz_unpack` 684 M executions, `fuzz_buf` 355 M, `fuzz_layout`
217 M), every canary crashed first, and `native-checks` passed with gctorture at step 20
(38 minutes).

**Goal:** everything a user, a CRAN reviewer or a sanitizer reads or runs is in place and
matches the code.

**Do**

- The shipped vignette `vignettes/zubin.Rmd`: layouts, `bin_unpack()` and `bin_pack()`,
  `bin_decode()` and `bin_encode()`, the builder, `bin_hexdump()`, the WAV and big-endian
  examples of §13.8 executed into `tempfile()`s, the alignment example, and the type model
  table.
- Roxygen: `?bin_layout` carries the §11.2 grammar and the §13.1 table; every function
  states "offsets are 0-based" in its first paragraph where it takes one; runnable examples
  on every export; pkgdown reference index grouped into layouts, records, codecs, builder,
  inspection, package.
- `inst/WORDLIST` through `spelling::update_wordlist()`; `urlchecker::url_check()`;
  the `cran-extrachecks` and `review-cran-submission` passes.
- `tools/benchmarks.R` and `tools/run-benchmarks` against the baselines of §17;
  `.agents/benchmarks.md` with first results on this machine and one Linux x86-64 runner.
- Gates at full strength: a `full-ci` run (gctorture step 20); the nightly fuzz schedule
  enabled; `alloc-failure.yaml` added if r-actions' interposer reaches `zb_buf_reserve()`
  (`target-pattern: ZB_ERR_MEMORY`), informational until it reads clean.
- `cran-comments.md` as a first submission: only checks that have run; one paragraph on the
  header-only C API and the `LinkingTo` consumer shape; nothing about vendored code.
- `NEWS.md` 0.1.0 entry: the fourteen functions, the C headers and their contract, the
  explicit non-goals, the pointer to next.md's items.
- CLAUDE.md current state; `.agents/design.md` amended wherever Stages 1–7 found it wrong.

**Exit**

- Every deliverable of design §18 except criterion 9 is met and linked from the PR.
- Each fuzz target has accumulated an hour; each canary was seen to crash.
- `devtools::check(cran = TRUE)` 0/0/0 on all three platforms; spelling and URLs clean.

**What actually happened**

- The benchmarks found two misses against §17. `bin_put()` cost 4.1 µs a call, slower
  than a raw connection, because argument matching ran before the raw-vector path; it is
  now 0.69 µs. `bin_pack()` was 40% slower than `writeBin()` per field: stripping
  attributes from each column (`attributes(x) <- NULL`) copied every column, `storage.mode<-`
  copied even when the type was already right, and `zb_pack_f64()` decided the type per
  value. With all three fixed it is 12% behind on macOS, bound by first-touch page faults,
  and 8× ahead on the Linux runner; `.agents/benchmarks.md` has both.
- r-actions grew `github-packages` (v1.20.0) for zubin's sake (Stages 2 and 3), and every
  r-actions workflow here is now pinned to that release by commit, `R-CMD-check.yaml`
  excepted (`@v1`, as the roadmap's CI section says).
- `R-CMD-check.yaml` sets `ZUBIN_SLOW_TESTS=true` on the full profile (`main`, and PRs
  labelled `full-ci`), so the sweeps and the 2 GiB input run in CI.
- `alloc-failure.yaml` is weekly and informational, with `tools/alloc-exercise.R` as its
  workload and `zubin_memory_error` as the target pattern. Its first run, on the PR, killed
  R four times out of 300 injected failures, every one a segfault inside R's regex engine:
  `grepl()` in `bin_put()`'s type parsing, when the failed allocation was TRE's. That is a
  finding about R, as r-actions' documentation warns, but `bin_put()` no longer uses a
  regex, and the sweep no longer runs on pull requests, which it would otherwise gate.
- `cran-comments.md` is written for the submission at Stage 9, when zufast is on CRAN; it
  claims only checks the CI runs.
- A `zubin-conditions` help page lists every class and its fields; `?bin_layout` carries
  the §13.1 table in both directions.

---

## Stage 9 — Release 0.1.0 · S

**Status:** prepared, waiting for zufast on CRAN (checked 2026-10-05: not there). The branch
`stage-9-release` holds the release commit: `Version: 0.1.0` with `version.h` and the NEWS
heading, no `Remotes:`, the CRAN install in the README, and rchk and the arch legs
resolving zufast from CRAN. Rebase it on `main`, open its PR with `full-ci`, and once
every leg is green (that run is the win-builder and macbuilder result), submit.

**Entry:** zufast 0.1.0 is on CRAN.

**Do**

- Remove `Remotes:`; keep `LinkingTo: zufast (>= 0.1.0)`; `Version: 0.1.0` and the
  `NEWS.md` heading together.
- A green run of every workflow on the release commit: that run is the win-builder and
  macbuilder result (zukomp's rule).
- Submit (the maintainer's step); answer reviewers; on acceptance tag `v0.1.0`, publish the
  GitHub release, move `main` to `0.1.0.9000` with a matching NEWS heading.
- File the adoption issues of design §3.2 (zuhttp, zucbor, zuxlsx, rdz) and link them from
  the README; propose zubin's cells for the family table at the next all-repository change.

**Exit:** zubin 0.1.0 on CRAN; `main` at `0.1.0.9000`; the issues filed.

---

## Acceptance criteria against stages

| Design §18 criterion | Stage | Verified by |
|---|---|---|
| 1 headers compile standalone; probes clean; `zubin-r.h` against R | 1, 2 | `abi.yaml` |
| 2 `zubin.so` exports exactly `R_init_zubin` | 1 | `test-abi.R` |
| 3 fixture builds, checks clean, runs without zubin | 7 | `consumer.yaml` |
| 4 every type-model row tested; golden vectors identical on s390x | 4, 5 | `test-unpack.R`, `test-pack.R`, `arch.yaml` |
| 5 round trips; f16/bf16 exhaustive | 1, 5 | `test-rw.R`, `test-roundtrip.R` |
| 6 lifetime test fails with the finalizer broken | 2 | `test-lifetime.R`, the PR record |
| 7 an hour of fuzzing per target; canaries crashed | 3, 4, 5, 8 | `hardening.yaml` |
| 8 check 0/0/0 on three platforms; shuffled; gctorture | 0–8 | `R-CMD-check.yaml`, `native-checks.yaml` |
| 9 zufast on CRAN; no `Remotes:` | 9 | the submission |

## Explicitly not in 0.1.0

Variable-length fields, a cursor-style reader from R, byte search and splitting, views,
memory mapping, connections, serialisation streams and object hashing, the nanoarrow
bridge, bitfields, `blob` and `float` outputs, a `bigint` for `u64`, zero-copy
`bin_take()`. Each is in [next.md](next.md) with the trigger that admits it. None is made
harder by shipping 0.1.0 first: every one is an addition to the headers and a new function
in R.
