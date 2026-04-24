---
name: uft-benchmark
description: |
  Use when measuring performance of decoder/PLL/CRC hotpath code, verifying
  speedup claims with before/after numbers, or detecting regressions.
  Trigger phrases: "benchmark für X", "messe wie schnell", "before/after
  speedup", "regression test performance", "is this really 2× faster",
  "micro-benchmark". Full working harness included. DO NOT use for: memory
  profiling (use valgrind/massif directly), Qt UI latency (use QTest),
  integration tests (use ctest).
---

# UFT Micro-Benchmark

Provides a **full working** benchmark harness for isolating and measuring
UFT hotpaths. Enforces the confidence-as-range rule from
`docs/AI_COLLABORATION.md`.

## When to use this skill

- Verifying a claimed speedup with real numbers
- Detecting regression after a refactor
- Comparing two implementations A/B
- Providing data for algorithm-hotpath-optimizer's next pass

**Do NOT use this skill for:**
- Memory profiling — use `valgrind --tool=massif`
- Qt UI latency — use `QElapsedTimer` inside QTest
- Integration/end-to-end timing — use `ctest --timeout`
- Cycle-accurate analysis of STM32 code — use hardware PMU on device

## Workflow

### Step 1: Identify the function under test

Candidates from `docs/PERFORMANCE_REVIEW.md`:
- `src/flux/uft_flux_decoder.c` — `flux_find_sync`, `flux_decode_mfm`
- `src/algorithms/bitstream/BitBuffer.cpp` — `read1`, `read_byte`
- `src/algorithms/uft_kalman_pll.c` — `uft_kalman_pll_decode`
- `src/crc/uft_crc16.c` — `uft_crc16`

### Step 2: Scaffold the benchmark

```bash
python3 .claude/skills/uft-benchmark/scripts/scaffold_benchmark.py \
    --function flux_find_sync \
    --workload-size 500000 \
    --unit "transitions"
```

Creates `tests/benchmarks/bench_flux_find_sync.c` from the template.

### Step 3: Run before/after

```bash
# BEFORE: baseline
git checkout main
make bench_flux_find_sync
./tests/benchmarks/bench_flux_find_sync > /tmp/before.txt

# AFTER: your change
git checkout feature/rolling-shift
make bench_flux_find_sync
./tests/benchmarks/bench_flux_find_sync > /tmp/after.txt

# Compare
paste /tmp/before.txt /tmp/after.txt | column -t
```

### Step 4: Report as a range, not a point

Correct reporting:
> `flux_find_sync` improved from **12.4 ms median (11.8–13.1 min/max)** to
> **1.7 ms median (1.6–1.9 min/max)** on a 500k-transition DD track →
> **6.5–7.5× speedup range** on this workload.

Wrong reporting:
> `flux_find_sync` is 7× faster.

## Verification

```bash
# 1. Compile check
gcc -std=c11 -Wall -Wextra -Werror -O3 -DNDEBUG \
    -I include/ \
    tests/benchmarks/bench_<name>.c \
    src/<impl>.c \
    -o /tmp/bench_<name> -lm

# 2. Run 3× — numbers should be stable (±10%)
for i in 1 2 3; do /tmp/bench_<name>; done

# 3. On battery-powered laptop? CPU frequency scaling matters
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# if "powersave" → plug in or switch to "performance" first:
# sudo cpupower frequency-set -g performance
```

## Measurement rules

- **Fixed workload size** per benchmark (500k transitions = "DD track")
- **Seeded pseudo-random data**, never `/dev/urandom`
- **Warmup iterations** — first 3 runs discarded
- **Odd number of measure iterations** (11) for unambiguous median
- **Report min / median / max**, never mean (outliers dominate mean)
- **Use `CLOCK_MONOTONIC`** on POSIX, never `clock()` or `time()`
- **Don't benchmark on battery** — CPU throttling invalidates results

## Confidence discipline (mandatory)

Every speedup claim must include:

| Field | Example |
|-------|---------|
| Range | "5-8× speedup" not "7×" |
| Workload | "on 500k-transition DD track" |
| Variance | "min 1.6ms, max 1.9ms" |
| Environment | "GCC 13.2, x86-64, -O3, perf governor" |

If any is missing, the number is not yet publishable.

## Common pitfalls

### Dead-code elimination discards your work

```c
/* WRONG — compiler removes the call entirely */
for (int i = 0; i < ITER; i++) flux_find_sync(buf, len, 0x4489);

/* RIGHT — force the compiler to keep the result */
volatile int result = 0;
for (int i = 0; i < ITER; i++) result ^= flux_find_sync(buf, len, 0x4489);
```

### Branch prediction warms up during measurement

Your warmup needs to be realistic — run the function on the actual workload
at least 3 times before measuring.

### Mean instead of median

A single outlier (context switch, cache flush) skews the mean by 2×.
Median is robust. If mean ≠ median by >20%, you have noise issues.

## Related

- `.claude/agents/algorithm-hotpath-optimizer.md` — consumes benchmark output
- `docs/PERFORMANCE_REVIEW.md` — 10 candidate optimizations to measure
- `docs/AI_COLLABORATION.md` §3 Konfidenz-Regel
- `.claude/skills/uft-flux-fixtures/` — deterministic input data
