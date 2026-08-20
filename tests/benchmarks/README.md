# Micro-Benchmarks

Baselines for hotpaths, so a later change can be **judged** instead of
believed. Rules and reporting discipline: `.claude/skills/uft-benchmark`.

Benchmarks are not part of `ctest`. They measure, they do not assert, and a
number that varies with the machine has no business failing a build. Build
them explicitly:

```bash
cmake -DUFT_ENABLE_BENCHMARKS=ON ..
cmake --build . --target bench_decode_hotpath
./tests/benchmarks/bench_decode_hotpath
```

---

## `bench_decode_hotpath` — the two per-transition decode paths

**Baseline, 2026-08-20 (MF-435).** GCC 13.1.0 MinGW-w64, x86-64, `-O3
-DNDEBUG`, mains power. Median of 11 measurements after 3 warmups, three
independent runs:

| | min | median | throughput |
|---|---|---|---|
| `uft_pll_process_flux_mfm` | 1.285–1.338 ms | 1.320–1.380 ms | **72.5–75.7 M transitions/s** |
| `flux_find_sync` (full scan) | 0.283–0.330 ms | 0.317–0.330 ms | **1514–1578 M bits/s** |

Workload: 100 000 flux transitions — one revolution of one DD track side at
300 rpm — and a 500 000-bit full-track sync scan whose only sync word sits in
the last 16 bits, so the number is the complete scan and not a lucky early hit.

### What the numbers say: do not optimise these

Projected onto a whole DD disk — 80 cylinders × 2 heads × 3 revolutions =
480 revolutions:

```
PLL decode      480 × 1.35 ms   ≈  0.65 s
sync search     worst case, ten full scans per track side
                160 × 10 × 0.33 ms ≈  0.53 s
                                    ────────
                                    ≈  1.2 s of CPU

drive time      160 track sides × 3 revolutions × 200 ms ≈ 96 s
                plus seeks
```

The decode arithmetic is on the order of **one percent** of the wall clock a
real capture spends waiting for the disk to turn. Making it twice as fast
would save under a second per disk and would be, by any honest accounting,
wasted effort — plus risk in code where a wrong bit is a wrong archive.

That is the point of measuring first. Both functions look like candidates:
the PLL does `double` arithmetic once per transition, and `flux_find_sync`
walks a bitstream one bit at a time, reloading the same byte eight times. Both
would have been tempting to rewrite. Neither is worth it.

### Not measured

The plausible real costs are untouched by this file: end-to-end conversion,
file I/O, the OTDR/DeepRead analysis pipeline (12 stages), and the GUI. If
something is slow in practice, look there — and measure it before changing it.

### A finding that came out of building this (MF-435)

There were no benchmarks in this tree, and the reason turned out to be
structural rather than neglectful: **timing inside UFT silently did not
work.** `include/uft/uft_platform.h` carried

```c
#define clock_gettime uft_clock_gettime
```

whose replacement returned `time(NULL)` with `tv_nsec = 0` — one-second
resolution. Whether a translation unit got it depended on include order (the
guard tested `CLOCK_MONOTONIC`, which `<time.h>` defines). Every elapsed
measurement under a second read as exactly 0, including
`uft_capture_result_t::elapsed_seconds`. Removed; MinGW-w64's real
`clock_gettime` has 100 ns granularity, measured.
