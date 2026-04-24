---
name: uft-stm32-portability
description: |
  Use when reviewing or writing code that must also compile for the
  UFI firmware target (STM32H723ZGT6, Cortex-M7). Trigger phrases:
  "läuft das auf STM32", "firmware-kompatibel", "dual-target check",
  "firmware-safe", "FPU limit", "intrinsics portabel", "SRAM budget".
  Enforces the Hardware-Dualität Hard-Rule from AI_COLLABORATION.md §5.
---

# UFT STM32H723 Portability Check

Use this skill when editing or reviewing code that might run on UFI
firmware. The H723ZGT6 is NOT a Linux box — getting this wrong
silently breaks the firmware build or burns the stack.

## STM32H723ZGT6 constraints (memorize)

| Resource | Limit | Comment |
|---|---|---|
| CPU | Cortex-M7 @ 550 MHz | No SMT, no out-of-order |
| FPU | Single-precision only | `float` OK, **`double` → soft-float** |
| SIMD | Limited DSP (SMLAD, etc.) | No SSE2/AVX2/NEON |
| SRAM | 564 KB total | ~2 KB stack per ISR, ~8 KB per task |
| Flash | 1 MB | Code + const data |
| Instruction cache | 16 KB | LUTs >16 KB kill perf |
| Data cache | 16 KB | Hot struct should fit |
| Branch predictor | 8-entry BTAC | Tight loops matter |

## Which code is dual-target?

ALL code under these paths is potentially firmware-bound:

- `src/flux/` — decoder (PLL, sync, flux→bitstream)
- `src/algorithms/` — Kalman, Viterbi, CRC, encoding detection
- `src/crc/` — CRC engines
- `src/analysis/otdr/` — OTDR booster (some parts UFI-bound)

Pure desktop (NO firmware concern):
- `src/gui/`, `src/*.cpp` with Qt deps
- `src/hardware_providers/`
- `tests/`
- anything `*.cpp` (firmware is C11, no C++)

When in doubt, ASK: "läuft das auf firmware?"

## Hard forbidden on firmware

```c
double x = ...;              // NO — soft-float emulation, slow
x86 intrinsics (_mm_, __m256)// NO — architecture-specific
__builtin_ctzll (64-bit)     // NO on thumb without extension
malloc() in ISR              // NO — heap is not real-time
printf() with %f on firmware // NO — pulls in soft-FPU printf
LUT > 16 KB                  // NO — busts I-cache
recursion with >2 depth      // NO — unbounded stack
C++ features (templates, new)// NO — firmware is pure C
```

## Hard required on firmware

```c
#include <stdint.h>           // explicit-width types only
                              // never `int`/`long` in binary layouts
static const uint8_t TABLE[256] = { ... };  // const → goes to flash
__attribute__((always_inline)) // for hot helpers
memcpy / memset from libc     // firmware libc has these
```

## Checklist per pull request

Before merging code that touches `src/flux/`, `src/algorithms/`,
`src/crc/`, or `src/analysis/otdr/`:

- [ ] No `double` arithmetic in hot path (`grep -n 'double ' file.c`)
- [ ] No `_mm_*`, `__m256*`, or `__builtin_ia32_*` — check with:
      `grep -nE '_mm_|__m(128|256|512)|__builtin_ia32' src/flux/ src/algorithms/`
- [ ] No `malloc/free` inside functions called from ISR context
- [ ] Static LUTs sized ≤ 16 KB (prefer ≤ 4 KB for multi-table cases)
- [ ] Recursion bounded and explicitly annotated with `/* max depth N */`
- [ ] `printf` calls use `%u`/`%d`/`%s`/`%x` only (no `%f`, no `%zu`
      on firmware's printf stub)

## Optional fast-paths (correctly guarded)

The correct pattern for dual-target SIMD acceleration:

```c
static inline uint32_t fast_popcount(uint64_t x) {
#if defined(__AVX2__) && defined(UFT_HOST_X86)
    return (uint32_t)__builtin_popcountll(x);
#else
    /* Portable fallback — works on Cortex-M7 */
    uint32_t c = 0;
    while (x) { c += (uint32_t)(x & 1); x >>= 1; }
    return c;
#endif
}
```

Rules:
- Portable fallback MUST exist and be correct standalone.
- Guard macro is `UFT_HOST_X86` (set by Makefile), NOT `__x86_64__`
  alone (cross-compile scenarios use x86_64 host, arm target).
- Measure the fast-path actually helps — see `uft-benchmark` skill.

## Float budget (single-precision only on firmware)

```c
/* OK — float throughout */
float weight = confidence * 0.5f;   /* note: 0.5f, not 0.5 */
float pll_error = (float)(measured_ns - expected_ns);

/* WRONG — silent promotion to double */
float weight = confidence * 0.5;    /* 0.5 is double, forces soft-float */
float y = sinf((double)x);          /* double cast round-trip */

/* WRONG — math functions double-by-default */
float y = sin(x);                   /* sin() is double, use sinf() */
```

## Stack awareness

Each ISR has a small stack. Inside ISR call chains:

- Large local arrays → move to `static` storage or allocate from
  caller's stack.
- Printf debugging → kills stack, use lightweight logging macros.
- Recursive descent parsers → convert to iterative.

Estimate stack use with GCC's `-fstack-usage`:
```bash
arm-none-eabi-gcc -fstack-usage -O2 -c flux_decode.c
cat flux_decode.su   # reports per-function stack bytes
```

## Firmware-side testing

UFI firmware testing is a separate workflow (out of M2 scope). For
now, a desktop "STM32 emulation mode":

```bash
make CFLAGS="-DUFT_SIMULATE_STM32 -fno-builtin-ctzll" test
```

The `UFT_SIMULATE_STM32` macro should:
- Disable x86 intrinsic fast-paths
- Warn on `double` arithmetic (via `-Wdouble-promotion`)
- Use 32-bit `size_t` (M7 is 32-bit)

## Escalation

If a fix requires firmware changes you don't have access to — STOP
and flag via CONSULT block to `deep-diagnostician` or `human`.
Firmware/desktop coupled changes cannot be reviewed in one PR.

## Anti-patterns

- **Don't write "TODO: make portable later"** — either portable now
  or exclude from firmware build.
- **Don't duplicate a function for firmware and desktop** — parameterise
  with `#ifdef` fast-paths, single implementation.
- **Don't use `size_t` in protocol structs** — binary-layout bug waiting
  to happen (it's 64-bit on desktop, 32-bit on M7). Use `uint32_t`/`uint64_t`.

## Related

- `CLAUDE.md` Hardware Targets (STM32H723 profile)
- `docs/AI_COLLABORATION.md` Hard-Rule #5 (Hardware-Dualität)
- `.claude/agents/abi-bomb-detector.md` — review layouts for dual-target
- `.claude/agents/algorithm-hotpath-optimizer.md` — always asks
  "runs on firmware?"
