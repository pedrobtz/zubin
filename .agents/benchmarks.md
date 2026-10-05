# zubin — benchmarks

Design §17's targets, measured with `tools/run-benchmarks` (`tools/benchmarks.R`): the
median of three runs, against the base-R way of doing the same thing. Not a CI gate;
`benchmarks.yaml` runs it on a Linux runner by hand. Add a section per machine and commit;
keep the old ones.

## 2026-10-05, Apple M1 (arm64), macOS, R 4.6.1, Apple clang 21

Commit: Stage 8 branch (after `96cdcc7`). Scale 1, the design's sizes.

| Operation | n | zubin | base R | Ratio |
|---|---|---|---|---|
| Unpack 4 fields, 24-byte records, to a data frame | 10 M | `bin_unpack()` 0.249 s | `readBin()` per field 0.593 s | 2.4× faster |
| Pack the same frame | 10 M | `bin_pack()` 0.47 s | `writeBin()` per field + interleave 0.417 s | 1.13× slower |
| Decode f32 | 100 M | `bin_decode()` 0.437 s | `readBin(size = 4)` 0.574 s | 1.3× faster |
| Append 100 bytes | 1 M | `bin_put()` 0.687 s | `rawConnection()` 1.926 s | 2.8× faster |
| Append 100 bytes | 20 k | | `c()` accumulation 19.4 s | quadratic |
| Hex dump | 1 MiB | `bin_hexdump()` 0.017 s | `sprintf("%02x")` 0.112 s (no layout) | 6.6× faster |

Against the targets:

- Unpack: one strided pass per field, 25 ns a record for four fields, and the only
  allocation is the columns. Met.
- Pack: **not met on this machine** (see the Linux section, where it is 8× faster than the
  baseline). 12% slower than four `writeBin()` calls and an interleave. All
  the time is in C (`Rprof`: 100% in the `.Call`); a fresh 240 MB output is first-touch
  page faults plus four strided write passes over it. Skipping the zero fill when the
  fields cover every byte (Stage 8) made no measurable difference, which says the cost is
  the faults and the passes, not the fill. A record-major pack loop is the next thing to
  try if it matters; it is not a correctness question.
- `bin_decode("f32")`: at least as fast as `readBin()`, and exact. Met.
- `bin_put()`: linear, 0.69 µs a call, dominated by the `.Call`. Met after Stage 8 moved
  the raw-vector path ahead of argument matching (it was 4.1 µs, slower than a raw
  connection).
- `bin_hexdump()`: not a bottleneck in a test suite. Met.

## 2026-10-05, GitHub Actions ubuntu-latest, AMD EPYC 9V45 (4 vCPU), R 4.6.1, GCC 13

Commit `4d4bfa3` (the Stage 8 branch; `benchmarks.yaml`, run 37263286053). Scale 1.

| Operation | n | zubin | base R | Ratio |
|---|---|---|---|---|
| Unpack 4 fields, 24-byte records, to a data frame | 10 M | `bin_unpack()` 0.049 s | `readBin()` per field 0.481 s | 9.8× faster |
| Pack the same frame | 10 M | `bin_pack()` 0.052 s | `writeBin()` per field + interleave 0.434 s | 8.3× faster |
| Decode f32 | 100 M | `bin_decode()` 0.062 s | `readBin(size = 4)` 0.203 s | 3.3× faster |
| Append 100 bytes | 1 M | `bin_put()` 0.715 s | `rawConnection()` 2.312 s | 3.2× faster |
| Append 100 bytes | 20 k | | `c()` accumulation 11.4 s | quadratic |
| Hex dump | 1 MiB | `bin_hexdump()` 0.016 s | `sprintf("%02x")` 0.064 s | 4× faster |

Every target is met here, pack included: 5 ns a record, which for 240 MB of output is about
4.6 GB/s, memory bandwidth for one core. So the macOS pack result above is the platform's
cost of first-touching a large fresh allocation, not the kernels': the same code is 9×
slower there for both unpack (whose output is also freshly allocated) and pack, while the
base-R baselines, which spend their time elsewhere, move much less between the two.
