---
name: uft-benchmark
description: |
  Use when you need to measure the performance of a decoder/PLL/CRC
  hotpath, verify a claimed speedup with before/after numbers, or detect
  regressions. Trigger phrases: "benchmark für X", "messe wie schnell",
  "before/after speedup", "regression test für performance", "is this
  really 2× faster", "micro-benchmark". Enforces confidence-as-range
  per AI_COLLABORATION.md (§ "Konfidenz-Regel").
---

# UFT Micro-Benchmark Harness

Use this skill when a performance claim needs real measurements — not
guesses. UFT rule: every speedup estimate must be a measured range
("5–8×"), never a point value ("7×"). This skill provides the harness.

## Step 1: Locate the hotpath

The three performance-relevant decoder paths (see `PERFORMANCE_REVIEW.md`):

- `src/flux/uft_flux_decoder.c` (977 LOC) — UFT's canonical
- `src/algorithms/bitstream/FluxDecoder.cpp` (119 LOC) — mid-layer
- `src/flux/fdc_bitstream/mfm_codec.cpp` (530 LOC) — FDC emulator

Plus PLL: `src/algorithms/uft_kalman_pll.c`, `src/decoder/uft_pll.c`.
CRC: `src/crc/`.

## Step 2: Create the benchmark file

Location: `tests/benchmarks/bench_<subject>.c`. Convention:

```c
/**
 * @file bench_flux_find_sync.c
 * @brief Hotpath benchmark: flux_find_sync before/after rolling-shift
 *        register optimisation.
 *
 * Reference workload: 500k-transition DD track (approximately one
 * Amiga disk side at standard PLL).
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* The actual function(s) under test */
#include "uft/flux/uft_flux_decoder.h"

#define WORKLOAD_TRANSITIONS  500000
#define WARMUP_ITERATIONS          3
#define MEASURE_ITERATIONS        11   /* odd → clean median */

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

static void build_workload(uint8_t *bits, size_t bit_count) {
    /* Deterministic pattern: avoid random data so runs are comparable. */
    uint32_t seed = 0x4E554654u;
    for (size_t i = 0; i < (bit_count + 7) / 8; i++) {
        seed = seed * 1103515245u + 12345u;
        bits[i] = (uint8_t)(seed >> 16);
    }
}

int main(void) {
    static uint8_t workload[WORKLOAD_TRANSITIONS / 8];
    build_workload(workload, WORKLOAD_TRANSITIONS);

    /* Warmup: populate caches, JIT, page-fault, etc. */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        (void)flux_find_sync(workload, sizeof(workload) * 8, 0x4489);
    }

    /* Measure */
    uint64_t samples[MEASURE_ITERATIONS];
    for (int i = 0; i < MEASURE_ITERATIONS; i++) {
        uint64_t t0 = now_ns();
        volatile int result = flux_find_sync(workload, sizeof(workload) * 8, 0x4489);
        uint64_t t1 = now_ns();
        (void)result;
        samples[i] = t1 - t0;
    }

    qsort(samples, MEASURE_ITERATIONS, sizeof(samples[0]), cmp_u64);
    uint64_t min_ns = samples[0];
    uint64_t med_ns = samples[MEASURE_ITERATIONS / 2];
    uint64_t max_ns = samples[MEASURE_ITERATIONS - 1];

    double tps_med = (double)WORKLOAD_TRANSITIONS / (med_ns / 1e9);
    printf("flux_find_sync @ 500k transitions:\n");
    printf("  min:      %7.2f ms\n", min_ns / 1e6);
    printf("  median:   %7.2f ms  (%.1f Mtransitions/sec)\n",
           med_ns / 1e6, tps_med / 1e6);
    printf("  max:      %7.2f ms\n", max_ns / 1e6);
    return 0;
}
```

## Step 3: Workload rules

- **Fixed workload size** per benchmark — the "500k-transition DD
  track" is the suite's standard unit.
- **Seeded pseudo-random data** (LCG as above), no `/dev/urandom`.
- **Warmup iterations** (3+) discarded before measurement.
- **Odd number of measure iterations** (11) so median is unambiguous.
- **Report min/median/max**, not mean (outliers on Windows are
  catastrophic for mean).
- **Use `CLOCK_MONOTONIC`** on POSIX, `QueryPerformanceCounter` on
  Windows. Do NOT use `time()` or `clock()` — resolution is too coarse.

## Step 4: Before/after discipline

Baseline first, one change at a time:

```bash
# BEFORE
git checkout main
make bench_flux_find_sync && ./bench_flux_find_sync > before.txt

# AFTER
git checkout feature/rolling-shift-register
make bench_flux_find_sync && ./bench_flux_find_sync > after.txt

# DIFF
paste before.txt after.txt | column -t
```

Confidence reporting (MANDATORY):
- Speedup as RANGE: `(median_before / median_after)` → report 3–5×
  across measurement runs, never a single number.
- Tie to workload: "5–8× on 500k-transition DD track" — never
  just "5–8×".
- Report min/median/max separately. If min and max diverge >2×,
  system noise is dominating; re-run with higher N_ITER.

## Step 5: Integrate with CMake

```cmake
# tests/CMakeLists.txt (bottom)
if(UFT_ENABLE_BENCHMARKS)
    file(GLOB BENCH_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/bench_*.c")
    foreach(bench_source ${BENCH_SOURCES})
        get_filename_component(bench_name ${bench_source} NAME_WE)
        add_executable(${bench_name} ${bench_source})
        target_link_libraries(${bench_name} PRIVATE uft_core uft_crc)
        if(UNIX)
            target_link_libraries(${bench_name} PRIVATE m)
        endif()
    endforeach()
endif()
```

Build with `-DUFT_ENABLE_BENCHMARKS=ON`. Do not run by default in
`ctest` — benchmarks belong to a separate workflow.

## Step 6: Performance regression CI (future)

When benchmarks exist for ≥3 hotpaths, add `.github/workflows/bench.yml`
that:
1. Builds with `-O3 -DNDEBUG`.
2. Runs each `bench_*` binary.
3. Parses `median:` line and diffs against a committed baseline.
4. Fails if regression > 5% for >N runs (algorithm-hotpath-optimizer
   auto-invoke trigger per its frontmatter).

## Anti-patterns

- **Don't report mean timings** — skewed by outliers.
- **Don't benchmark on a laptop on battery** — CPU throttling makes
  numbers meaningless.
- **Don't skip warmup** — first run is 2–10× slower due to cold cache.
- **Don't claim "7× faster"** without the measurement variance.
- **Don't benchmark inside the Qt thread** — GC pauses and UI redraws
  pollute numbers. Always pure C harness.

## Related

- `docs/PERFORMANCE_REVIEW.md` — 10 findings awaiting measurement
- `docs/AI_COLLABORATION.md` §3 Konfidenz-Regel
- `.claude/agents/algorithm-hotpath-optimizer.md` — consumes these
  benchmark outputs to prioritise fixes
