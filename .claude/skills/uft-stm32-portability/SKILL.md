---
name: uft-stm32-portability
description: |
  Use when reviewing or writing UFT code that must also compile for UFI
  firmware (STM32H723ZGT6, Cortex-M7). Trigger phrases: "läuft das auf
  STM32", "firmware-kompatibel", "dual-target", "firmware-safe", "FPU
  limit", "Intrinsics portabel", "SRAM budget", "runs on UFI firmware".
  Enforces Hardware-Dualität from AI_COLLABORATION.md §5. Includes
  executable portability check script. DO NOT use for: pure GUI/Qt code
  (desktop-only), hardware-provider code (desktop-only), code explicitly
  marked `#ifdef UFT_DESKTOP_ONLY`.
---

# UFT STM32H723 Portability

Use this skill when touching code that might run on UFI firmware. The
STM32H723ZGT6 is NOT a Linux box — getting this wrong silently breaks the
firmware build or burns the stack.

## When to use this skill

- Reviewing a PR that touches `src/flux/`, `src/algorithms/`, `src/crc/`,
  or `src/analysis/otdr/`
- Before adding a new hot-path function
- Before committing SIMD or intrinsics usage
- Porting a desktop-only feature to firmware

**Do NOT use this skill for:**
- Pure GUI code in `src/gui/` — that's desktop-only
- Hardware provider code — that's Qt-coupled and desktop-only
- Code explicitly guarded with `#ifdef UFT_DESKTOP_ONLY`
- Tests — `tests/` are desktop-only by convention

## STM32H723ZGT6 budget

| Resource | Limit | Comment |
|----------|-------|---------|
| CPU | Cortex-M7 @ 550 MHz | No SMT, no OOO |
| FPU | Single-precision only | `float` OK; `double` → soft-float |
| SIMD | DSP instructions (SMLAD) | No SSE2/AVX2/NEON |
| SRAM | 564 KB | ~2 KB stack/ISR, ~8 KB per task |
| Flash | 1 MB | Code + const data |
| I-cache | 16 KB | LUTs >16 KB kill perf |
| D-cache | 16 KB | Hot structs should fit |

## Workflow

### Step 1: Run the automated check

```bash
bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh src/flux/
```

The script reports:
- `double` arithmetic in hot paths
- x86 intrinsics (`_mm_`, `__m128/256/512`, `__builtin_ia32_*`)
- `malloc`/`free` in potential ISR paths
- `printf("%f")` and `%zu` (firmware-unsafe)
- LUTs larger than 16 KB
- `size_t` in binary struct layouts
- Recursive functions without bounded depth

Exit code 0 = clean, 1 = issues found.

### Step 2: Understand which files are dual-target

ALL code under these paths is potentially firmware-bound:
- `src/flux/` — decoder, PLL, sync
- `src/algorithms/` — Kalman, Viterbi, CRC
- `src/crc/` — CRC engines
- `src/analysis/otdr/` — OTDR booster (some modules)

Pure desktop:
- `src/gui/`, `src/hardware_providers/`, `tests/`, anything `*.cpp`
- Anything with `QObject`, `Q_OBJECT`, Qt includes
- Files with `#ifdef UFT_DESKTOP_ONLY` guard at top

When in doubt, check the module's `#ifdef` at the top or ask.

### Step 3: Fix forbidden patterns

```c
/* ===== FORBIDDEN on firmware ===== */
double x = 3.14;                   /* soft-float, slow */
__m256 v = _mm256_load_ps(p);      /* x86-specific */
printf("%f\n", x);                 /* pulls in FP printf */
static uint32_t LUT[8192];         /* 32 KB > 16 KB I-cache */
malloc(1024);                      /* in ISR: non-deterministic */
size_t field;                      /* in struct: 32 vs 64 bit mismatch */

/* ===== Correct ===== */
float x = 3.14f;                   /* 0.5f not 0.5 */
/* SIMD: guard with #if, provide fallback */
printf("%u %d %s\n", ...);         /* no %f, no %zu */
static const uint16_t LUT[256];    /* 512 B — fits cache */
static uint8_t buffer[1024];       /* static allocation */
uint32_t field;                    /* explicit width */
```

### Step 4: Optional SIMD fast-path (correctly guarded)

```c
static inline uint32_t fast_popcount(uint64_t x) {
#if defined(__AVX2__) && defined(UFT_HOST_X86)
    return (uint32_t)__builtin_popcountll(x);
#else
    /* Portable — works on Cortex-M7 */
    uint32_t c = 0;
    while (x) { c += (uint32_t)(x & 1); x >>= 1; }
    return c;
#endif
}
```

Rules:
- Portable fallback MUST exist and be correct standalone
- Guard macro is `UFT_HOST_X86`, NOT `__x86_64__` alone
- Measure that the fast-path actually helps — see `uft-benchmark`

### Step 5: Stack awareness in ISR paths

```bash
# Estimate per-function stack use
arm-none-eabi-gcc -fstack-usage -O2 -c src/flux/flux_decode.c
cat flux_decode.su
# expect: all functions < 512 bytes
```

Large locals → move to `static` storage. Recursive parsers → convert
to iterative. `printf` debugging → kills stack, use lightweight
logging macros.

## Verification

```bash
# 1. Full portability check
bash .claude/skills/uft-stm32-portability/scripts/check_firmware_portability.sh
# expect: "OK: no portability issues found"

# 2. Desktop simulation build
make CFLAGS="-DUFT_SIMULATE_STM32 -fno-builtin-ctzll -Wdouble-promotion" test

# 3. Cross-compile check (requires arm-none-eabi toolchain)
arm-none-eabi-gcc -mcpu=cortex-m7 -mfpu=fpv5-sp-d16 -mfloat-abi=hard \
                   -Wdouble-promotion -Werror \
                   -fsyntax-only src/flux/uft_flux_decoder.c -Iinclude/

# 4. Stack usage per function
arm-none-eabi-gcc -fstack-usage -O2 -c src/flux/*.c 2>/dev/null
grep -E "^[^:]+\.c:[0-9]+:[^:]+\s+[0-9]{4,}\s" *.su
# expect: empty (no function >9999 bytes stack)
```

## Common pitfalls

### Accidental `double` promotion

```c
/* WRONG — 0.5 is double, forces soft-float */
float y = x * 0.5;

/* RIGHT */
float y = x * 0.5f;
```

`-Wdouble-promotion` catches these. Enable it in the firmware build.

### `sinf` vs `sin`

```c
/* WRONG — calls double sin() */
float y = sin(x);

/* RIGHT */
float y = sinf(x);
```

Math functions are `double` by default. Firmware must use `f`-suffix
variants.

### `size_t` in protocol structs

```c
/* WRONG — 8 bytes on desktop, 4 bytes on M7 */
typedef struct {
    size_t length;  /* binary layout mismatch! */
} wire_frame_t;

/* RIGHT */
typedef struct {
    uint32_t length;
} wire_frame_t;
```

Never use `size_t`, `int`, `long` in binary protocol/file-format structs.
Always explicit-width (`uint16_t`, `int32_t`, etc.).

### Recursive descent parser

Parsers that recurse unboundedly overflow stack fast. Convert to
iterative with explicit state stack on heap (or static).

## Related

- `.claude/skills/uft-benchmark/` — measure fast-path gains
- `.claude/agents/abi-bomb-detector.md` — reviews binary layouts
- `.claude/agents/algorithm-hotpath-optimizer.md` — asks "runs on firmware?"
- `docs/AI_COLLABORATION.md` Hard-Rule #5 (Hardware-Dualität)
- `CLAUDE.md` Hardware Targets (STM32H723 profile)
