# UFT Declaration-Completeness Inventory

Scan date: 2026-04-17.
Branch: `main` (post-HAL H7 + P2a/P2b + FC5025 enumerate).

## Real numbers (measured, not claimed)

| Category | Spec claim | Measured | Notes |
|----------|-----------|----------|-------|
| Declared (non-inline) | — | 5,426 | From `include/` via the spec's regex |
| Implemented | — | 1,830 | From `src/*.c` |
| Called | — | 2,245 | From `src/*.c` and `src/*.cpp` |
| **Kat. A ALL** (called, no impl) | — | 638 | Raw count |
| **Kat. A real** (called+declared, no impl) | 105 | **72** | Intersects with declared set |
| **Kat. B** (declared, never called) | ~3,400 | 4,314 | Dead-ish declarations |
| **Kat. C** (impl, never declared) | 572 | 591 | Silent API |

The gap between 638 "Kat. A ALL" and 72 "Kat. A real" is dominated by
declarations the regex doesn't match (return type with pointer/qualifier
prefix, e.g. `const uft_sector_t* uft_track_get_sector(...)`), macros,
and calls to function-pointer fields. The "72" number is the realistic
linker-gap target.

## Kat. A real — clusters by prefix

```
 19 uft_fat_*        Filesystem: no real implementation anywhere
  8 uft_amiga_*      AmigaDOS: declared API, zero backend (confirmed earlier)
  6 uft_params_*     Parameter store
  4 uft_read_*       Endian helpers (ALL are false positives — inline defs exist)
  4 uft_detect_*     Format detection public API
  3 uft_track_*      Track accessors (two fixed in this commit)
  3 uft_cbm_*        CBM disk API
  2 uft_jv3_*        JV3 format
  2 uft_imd_*        IMD format
  2 uft_disk_*       Top-level disk_open / disk_save_as
  + 10 singletons    (scp, kf, kfx, pll, ml, ir, list, hal, fc, xum, as, td0)
```

## Known false positives

- `uft_read_le16/le32/be32` — defined `static inline` in 3–4 headers, picked up
  because one declaration in `tracks/uft_floppy_utils.h` is NOT inline.
- `uft_c64_sectors_per_track` — same pattern, 5 static-inline definitions + one
  forward declaration in `uft_flux_decoder.h:827`.
- `uft_dmk_crc16` — both declaration and static implementation exist, but the
  only caller (`src/uft_dmk_analyzer_panel.cpp`) is not in the .pro build.

A future K1 refinement should widen the regex to catch pointer-prefixed return
types and filter out callers in unbuilt source files.

## Kat. A real — actually fixed in this commit

Three track accessors declared in `include/uft/uft_core.h`:

- `uft_track_get_sector_count(track)` → `track->sector_count`
- `uft_track_has_flux(track)` → `track->flux && track->flux_count > 0`
- `uft_track_get_sector(track, idx)` → bounds-checked array lookup

Implementations live in `src/core/uft_core_stubs.c` next to the existing
`uft_track_find_sector` and `uft_track_get_status`. All three are direct
field reads — no plugin delegation, no subsystem dependencies.

## Kat. A real — NOT attempted in this commit (why)

The other ~69 gaps fall into three risk classes:

**Class 1 — Whole-subsystem gaps** (`uft_fat_*`, `uft_amiga_*`, `uft_cbm_*`):
implementing these is a multi-day port of a complete filesystem, not a
single-commit job. Spec §2.4 calls out these as stop-and-ask cases.

**Class 2 — HAL/controller gaps** (`uft_fc_open`, `uft_xum_open`, `uft_as_open`,
`uft_scp_read`, `uft_kfx_read_stream`, `uft_hal_open`): require libusb / CLI
subprocess infrastructure that the HAL backend files themselves don't yet
pull in. The FC5025 enumerate probe from commit `ac29c2b` is the template.

**Class 3 — Top-level orchestrators** (`uft_disk_open`, `uft_disk_save_as`,
`uft_format_detect`, `uft_read_disk`): these have multiple competing
signatures in the tree (spec §2.4 warning). Requires the signature-reconciliation
step called out in §10 "not-alone decisions".

The honest scope for a per-session K2 is 3–6 functions where the semantics
are unambiguous from context. That's what landed here.

## Next K2 candidates (when someone picks this back up)

Low-risk single-function jobs, ordered by isolation:

1. `uft_detect_result_init` / `uft_detect_result_free` — allocator pair, ~10
   lines each.
2. `uft_ir_track_to_json` — JSON serializer, mechanical.
3. `uft_triage_level_name` (actually in Kat. C? needs check) — enum-to-string.
4. `uft_prot_platform_name` / `uft_prot_scheme_name` — enum-to-string lookups.
5. `uft_kf_ticks_to_ns` (marked impl=1 in quick check — verify if the one
   impl is actually in the build).

Each of those should be a single commit with a targeted test.

## Data files

- `.audit/declared.txt` — 5,426 declared function names
- `.audit/implemented.txt` — 1,830 implemented names
- `.audit/called.txt` — 2,245 called names
- `.audit/gaps_A_declared.txt` — the 72 real gaps
- `.audit/dead_B.txt` — 4,314 dead declarations
- `.audit/silent_C.txt` — 591 silent-API candidates
