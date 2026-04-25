# UFT Performance Review Prompt — Hotpath Analysis

Use this for asking an LLM to review a hot-path function for
optimization opportunities. Customize the bracketed sections.

---

## Context

You are reviewing a UFT (UnifiedFloppyTool) hot-path function for
performance. UFT is a Qt6/C/C++ floppy-disk forensics tool, repository
github.com/Axel051171/UnifiedFloppyTool.

**Function under review:**
- File: `[src/path/file.c]`
- Function: `[function_name()]`
- Line range: `[approximately L100-L200]`
- Currently called from: `[list 1-3 callers with file:line]`

**Workload characteristics:**
- Called approximately `[N]` times per `[track / decode / capture]`
- Input size typically: `[describe — e.g. "500k flux transitions"]`
- Critical path: `[yes/no — does it block UI? gating throughput?]`

## Constraints (do not violate)

This code is dual-target:
- Desktop: x86-64, GCC ≥11
- Firmware: STM32H723ZGT6, Cortex-M7 @ 550MHz, single-precision FPU

Therefore:
- No `double` arithmetic (use `float` + `f`-suffix math: `sinf`, `sqrtf`)
- No x86 intrinsics without `#ifdef UFT_HOST_X86` + portable fallback
- No malloc in the function body
- No LUTs >16 KB
- No `size_t` in struct layouts (use explicit `uint32_t`/`uint16_t`)
- The function MUST remain a pure function of its inputs (no globals)

## Konfidenz-Regel (mandatory output format)

Performance claims MUST be ranged with workload context. Required for
EVERY speedup number you propose:

- **Range** ("3-5×" not "4×")
- **Workload** ("on a 500k-transition DD track")
- **Build flags** ("GCC 13.2 -O3 -march=native")
- **Variance** ("min/med/max over 11 runs, governor=performance")

If you don't have measurements, write "estimated 2-4× from cache miss
reduction, MEASURE BEFORE CLAIMING".

## Output format (mandatory)

```
## TL;DR
[2 sentences. The single highest-impact opportunity, expressed as a range.]

## Findings (impact order)

### 1. [Short title] — [LOC] — [Impact range with workload]
**Location:** [file:lines]
**Current code:**
```c
[paste exact existing code]
```
**Problem:** [what's wrong, in 1-2 sentences]
**Suggested change:**
```c
[concrete proposed code, complete enough to compile]
```
**Confidence:** [ranged speedup with workload]
**Risks:** [what could break, what edge cases need coverage]

### 2. ...
### 3. ...

## Not checked
- [list things outside the scope of this review — e.g. "callers'
   error handling", "interaction with PLL", "memory layout of caller"]

## Recommended order
1. [highest impact + lowest risk first]
2. ...
3. ...
```

## Anti-goals

- **No refactoring beyond performance.** If the function is ugly but
  fast, leave it ugly. Style is out of scope.
- **No new dependencies.** Don't propose linking ONNX, MKL, BLAS,
  Eigen. UFT is dependency-conservative.
- **No "while we're at it" changes.** One concern per finding.
- **No bulk rewrites.** Findings should be locally applicable patches,
  not "rewrite this function from scratch".

## What this task is NOT

- Not a code review for correctness — assume the function is currently
  correct, optimize without changing semantics
- Not API design — the function signature stays
- Not test coverage review — that's a separate review

## Reference materials available

- `docs/PERFORMANCE_REVIEW.md` — existing perf findings (avoid duplicates)
- `docs/AI_COLLABORATION.md` §3 (Konfidenz-Regel) — confidence rules
- `.claude/skills/uft-benchmark/` — benchmark harness for verification
