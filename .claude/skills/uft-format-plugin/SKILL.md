---
name: uft-format-plugin
description: |
  Use when adding a new disk-image format plugin to UFT. Trigger phrases:
  "füge plugin für X format", "neuer format plugin", "add X plugin", "X
  format unterstützen", "plugin für X schreiben". Follows the 80-plugin
  template pattern with probe + open/close/read_track + registry + test.
  Enforces MF-007 (test coverage) and Principle 7 (honesty matrix).
---

# UFT Format Plugin Template

Use this skill when adding a NEW format plugin (e.g. Roland MC-500 `.roland`,
Sinclair `.mdv`, obscure format). Always start from the template pattern of
an existing plugin — do NOT invent your own structure.

## Step 1: Identify a template

Pick the closest-matching existing plugin:

| If your format is… | Use as template |
|---|---|
| Size-based detection (no magic) | `src/formats/commodore/d64.c` |
| Magic at offset 0 (ASCII) | `src/formats/commodore/crt.c` |
| Magic + suffix guard (Applesauce family) | `src/formats/apple/uft_moof_parser.c` |
| Chunk-based (RIFF-style) | `src/formats/atx/uft_atx.c` |
| Header + track table | `src/formats/fdi/uft_fdi_plugin.c` |
| Compressed (zlib/LZSS) | `src/formats/td0/uft_td0.c` |

## Step 2: Write the plugin file

Location: `src/formats/<family>/uft_<name>.c` where `<family>` is the
platform dir (commodore/apple/atari/...). Include pattern:

```c
#include "uft/uft_format_common.h"

/* Your constants */
#define FOO_MAGIC      "..."
#define FOO_HEADER_SIZE ...

/* Plugin private data */
typedef struct { FILE *file; /* ... */ } foo_pd_t;

/* Four required callbacks */
static bool foo_probe(const uint8_t *d, size_t s, size_t fs, int *c) { ... }
static uft_error_t foo_open(uft_disk_t *disk, const char *path, bool ro) { ... }
static void foo_close(uft_disk_t *disk) { ... }
static uft_error_t foo_read_track(uft_disk_t *disk, const uft_track_coord_t *coord,
                                   uft_track_t *out) { ... }

/* Registration — MUST be at file scope, MUST use this macro */
static uft_format_plugin_t foo_plugin = {
    .id = UFT_FORMAT_FOO,
    .name = "FOO",
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = foo_probe, .open = foo_open,
    .close = foo_close, .read_track = foo_read_track,
    .spec_status = UFT_SPEC_STATUS_VERIFIED,  /* or STUB / PARTIAL */
    .is_stub = false,
};
UFT_REGISTER_FORMAT_PLUGIN(foo)
```

## Step 3: Honesty matrix (Principle 7)

Populate `features[]` and `compat_entries[]` truthfully. Unknown ≠ supported:

```c
static const uft_format_feature_t foo_features[] = {
    { "Weak sectors",    UFT_FEATURE_NOT_SUPPORTED, NULL },
    { "Timing data",     UFT_FEATURE_SUPPORTED,     NULL },
    { "Write support",   UFT_FEATURE_PLANNED,       "v4.2" },
};
/* Then in foo_plugin struct: */
.features = foo_features, .feature_count = sizeof(foo_features)/sizeof(*foo_features),
```

## Step 4: Register the format ID

Add to `include/uft/core/uft_format_registry.h`:
```c
UFT_FORMAT_FOO,  /* Your format description */
```

## Step 5: Add to build system

- `UnifiedFloppyTool.pro`: append `src/formats/<family>/uft_<name>.c \` to
  SOURCES. Check for basename collision first:
  `grep -c "uft_<name>\.c" UnifiedFloppyTool.pro` — must return 0.
- CMake picks it up automatically via `file(GLOB_RECURSE)`.
- Run `python3 scripts/verify_build_sources.py` — must pass.

## Step 6: Write the probe test (MANDATORY, MF-007)

Location: `tests/test_<name>_plugin.c`. Use the self-contained template
pattern from `tests/test_atr_plugin.c`:

- Replica the probe logic verbatim (no external linking)
- Cover: valid magic, wrong magic, too small, boundary conditions,
  format-neighbour confusion (e.g. D64 vs D71 vs D81)
- Framework: the TEST/RUN/ASSERT macros at top
- Compile check: `gcc -std=c11 -Wall -Wextra -Werror`

## Verification checklist

- [ ] `gcc -fsyntax-only` on the new .c clean
- [ ] `python3 scripts/verify_build_sources.py`: OK
- [ ] Test passes locally
- [ ] Plugin appears in `python3 scripts/audit_plugin_compliance.py` output
- [ ] Principle 7 feature matrix reflects reality (no "supported" lies)

## Anti-patterns (don't)

- Don't modify `src/formats_v2/` — it's DEAD CODE, see CLAUDE.md
- Don't skip the test ("will add later" → MF-007 regression)
- Don't claim `is_stub = false` if any of the read/open paths return
  `UFT_ERR_NOT_IMPLEMENTED`
- Don't fabricate magic bytes — if the format has no magic, use size-based
  probe with `confidence ≤ 60` (so magic-based plugins win ties)

## Related

- `docs/DESIGN_PRINCIPLES.md` Principle 7 (Ehrlichkeit)
- `docs/MASTER_PLAN.md` M1 MF-007 (plugin coverage)
- `tests/test_atr_plugin.c` — canonical probe-test template
- `scripts/audit_plugin_compliance.py` — CI compliance check
