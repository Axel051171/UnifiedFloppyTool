# Hardening Audit — v3.7 vs v4.1

**Stand:** 2026-04-25
**Scope:** Did v3.7's `*_hardened.c` files get merged into the regular
parsers, or was the hardening lost when those files were deleted?

## TL;DR — Hardening WAS lost

The `UFT_REQUIRE_NOT_NULL*` / `uft_safe_math` / `uft_safe_alloc` /
`uft_safe_parse` / `uft_safe_io` infrastructure is **present and
intact** in `include/uft/uft_safe.h` and `include/uft/core/uft_safe_*.h`.

But the regular parsers in v4.1 **do not use it**. The 24 `_hardened.c`
files were removed without porting their bounds-checks back into the
"plain" parser implementations.

This is a security regression vs v3.7.

## Evidence

### Inventory of v3.7 hardened files (24 total in zip)

| Format | Hardened LOC | Regular LOC (v4) | Hardening present in v4? |
|--------|-------------:|----------------:|---|
| ADF    | 11237 | varies | ✗ no |
| ATR    |  4742 | 169   | ✗ no — see below |
| CQM    |  5027 | varies | ✗ no |
| D64    |  8830 + 13404 (impl) | varies | ✗ no |
| D71    |  5018 | varies | ✗ no |
| D80    |  3119 | varies | ✗ no |
| D81    |  3566 | varies | ✗ no |
| D82    |  3164 | varies | ✗ no |
| D88    |  8846 | varies | ✗ no |
| DMK    |  4244 | varies | ✗ no |
| G64    |  8861 | varies | ✗ no |
| G71    |  2904 | varies | ✗ no |
| HFE    |  9669 | varies | ✗ no |
| IMD    |  7059 | varies | ✗ no |
| IMG    | 11788 | varies | ✗ no |
| MSA    |  6211 | varies | ✗ no |
| NIB    |  5907 | varies | ✗ no |
| SAD    |  2970 | varies | ✗ no |
| SCP    | 13781 + 14645 (impl) | varies | ✗ no |
| SSD    |  2737 | varies | ✗ no |
| TRD    |  3194 | varies | ✗ no |

Plus `src/formats/uft_hardened_registry.c` (3054 LOC) — plumbing.

### Direct comparison: ATR

**v3.7 `uft_atr_hardened.c` `atr_open()`:**

```c
#include "uft/core/uft_safe_math.h"
#include "uft/uft_safe.h"

static uft_error_t atr_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);     /* defensive NULL-check */

    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    ...
}
```

**v4.1 `uft_atr.c` `atr_open()`:**

```c
static uft_error_t atr_open(uft_disk_t* disk, const char* path, bool read_only) {
    /* no NULL-check; relies on caller */
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    ...
}
```

The v4 version drops:
- `UFT_REQUIRE_NOT_NULL2(disk, path)` defensive macro
- `uft/core/uft_safe_math.h` include (overflow-safe arithmetic)
- `uft/uft_safe.h` include (defensive macros)

Same pattern across all 21 formats sampled.

### Empty `src/formats/hardened/` directory

```
$ ls src/formats/hardened/
CMakeLists.txt
```

Only the build-system stub remains. No content.

### No `tests/hardened/` directory

Fuzz / hardening regression tests are absent.

## What v4.1 has that's still good

The hardening MACROS and HELPERS are intact:

- `include/uft/uft_safe.h` — `UFT_REQUIRE_NOT_NULL`,
  `UFT_REQUIRE_NOT_NULL2`, `UFT_REQUIRE_NOT_NULL3`,
  `UFT_BOUND_CHECK`, etc.
- `include/uft/core/uft_safe_alloc.h` — overflow-checked malloc/calloc
- `include/uft/core/uft_safe_cast.h` — overflow-checked numeric casts
- `include/uft/core/uft_safe_io.h` — bounded-read wrappers
- `include/uft/core/uft_safe_math.h` — `__builtin_*_overflow` portable wrappers
- `include/uft/core/uft_safe_parse.h` — bounded parse helpers
- `include/uft/core/uft_safe_string.h` — strlcpy/strlcat-style helpers
- `include/uft/core/uft_safety.h` — overall safety policy

So the **building blocks exist**. They're just not wired into the
regular parsers.

## Options

### Option A — Restore the 24 hardened files as a separate build mode

`CONFIG+=uft_hardened` in qmake would compile `src/formats/<fmt>/uft_<fmt>_hardened.c`
INSTEAD of `uft_<fmt>.c`. Two parallel builds (regular vs hardened),
no shared parser code. Matches v3.7 architecture.

Pros: minimal refactor, fastest restore.
Cons: maintenance burden — every parser bug must be fixed twice.

### Option B — Migrate bounds-checks into regular parsers (recommended)

For each parser:
1. Add `UFT_REQUIRE_NOT_NULL*` to every public entry point.
2. Replace bare `malloc` / `calloc` with `uft_safe_alloc` variants.
3. Replace bare `fread` / `fwrite` with `uft_safe_io` variants when
   sizes are computed from file content.
4. Replace bare arithmetic on file-derived sizes with
   `uft_safe_mul_size_t` / `uft_safe_add_size_t`.
5. Add at least one fuzz-target stub so future fuzz runs catch
   regressions.

Pros: single source of truth per parser; harder to break.
Cons: one PR per parser (~3 PR per format × 21 formats ~63 PRs).

### Option C — Hybrid: critical formats only

Migrate hardening (Option B) for **forensic-critical formats**:
ADF, D64, D81, IMG, IMD, ATR, ATX, MSA, HFE, SCP. ~10 formats.
Other formats stay as-is.

Pros: covers ~80% of real-world preservation work with ~50% the effort.
Cons: leaves smaller formats unhardened.

## Recommendation

**Option C — hybrid.** Hardening matters most for formats that
process untrusted input from museum archives or unknown disk dumps.
The 10 critical formats above are exactly that set. Smaller formats
(SAD, SSD, TRD) usually come from controlled sources.

Tracking: open one issue per format under label `hardening-restore`,
reference this doc.

## Related

- v3.7 reference files in zip: `src/formats/*/uft_*_hardened*.c`
- Hardening macros: `include/uft/uft_safe.h`
- Safety helpers: `include/uft/core/uft_safe_*.h`
- `docs/MASTER_PLAN.md` — should this become a new milestone?
