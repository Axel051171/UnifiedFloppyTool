# Recovery Pipeline Restore — Deferred Files

Four v3.7.0 recovery modules were NOT restored in this pass because
their v3.7 implementations reference header types/constants that are
not present in the v4.1.x tree (or conflict with it). Restoration
requires designing updated headers compatible with v4.1.x's SSOT
error system — separate tasks.

## `uft_recovery_params.c` — deferred

References undefined: `UFT_RECOVERY_PARAMS_QUICK`, `UFT_RECOVERY_PARAMS_DEFAULT`,
`UFT_RECOVERY_PARAMS_AGGRESSIVE` enum values; `uft_recovery_params_t.version`
field; `UFT_RECOVERY_PARAMS_VERSION` constant. 269 errors under -Werror.

Current `include/uft/recovery/uft_recovery_params.h` (from v4.x) has a
different shape — needs either (a) extending the header to cover
v3.7's needs, or (b) rewriting params.c against v4.x's shape.

## `uft_forensic_recovery.c` — deferred

References undefined `uft_forensic_config_t`, `uft_forensic_session_t`,
`uft_forensic_result_t`. 42 errors. Needs a `uft_forensic_config.h`
that was not extracted — the forensic subsystem evolved structurally
between v3.7 and v4.x.

## `uft_forensic_track.c` — deferred

Same root cause as uft_forensic_recovery.c — needs the forensic
session types. 4 errors.

## `uft_recovery_advanced.c` — deferred

Platform-specific port issue: uses Win32 `HANDLE` as the file handle
type but the calling code in `uft_recovery_run` passes an `int fd`.
3 errors. Restoration requires either (a) a portable `HANDLE`/`int`
wrapper, or (b) splitting into platform-specific TUs. Separate task.

## Restored successfully (7 files)

| File | LOC | Role |
|---|---|---|
| `uft_bitstream_recovery.c` | 424 | Bitstream-level recovery |
| `uft_cross_track.c`        | 475 | Cross-track correlation recovery |
| `uft_flux_recovery.c`      | 422 | Flux-level recovery |
| `uft_multiread_pipeline.c` | 806 | Multi-read fusion pipeline |
| `uft_protection.c`         | 517 | Copy-protection-aware recovery |
| `uft_recovery_meta.c`      | 881 | Meta-recovery orchestration |
| `uft_sector_recovery.c`    | 432 | Sector-level recovery |

All compile clean under `-Wall -Wextra -Werror` (one `(void)` cast
added to `uft_protection.c` for an unused parameter in the flux
weak-bit detector).

Together with the pre-existing `uft_recovery_wizard.c` and M2's
`uft_salvage_fs.c`, that brings `src/recovery/` from 2 files up to 9.
The 13 `uft_recovery_*.h` headers in the v4.x tree now have
implementations for the majority of their surface.

## Integrity-lint sweep — 2026-04-26 (TODO-A7)

`bash .claude/skills/uft-recovery-integrity/scripts/lint_recovery.sh
src/recovery/*.c` — **clean (11 file(s) scanned)**.

The script checks the 5 invariants from the 2026-04-25 recovery-integrity
audit:

  - I1: linear interpolation of binary data
  - I2: first-match CRC correction without uniqueness check
  - I3: uses `passes[].data[]` but never reads `passes[].crc_ok`
  - I4: `uint8_t` counter arrays that overflow at 256 inputs
  - I5: write to `output[]` without paired `confidence_map[]` update

All 11 .c files in `src/recovery/` (including `uft_recovery_wizard.c`
and `uft_protection.c` per the v4.1.4 audit Finding 17) pass without
warnings — no `INTEGRITY-OK` annotations needed.

Re-run on every PR that touches this directory (a CI gate is a
candidate follow-up; for now the pre-push hook covers SSOT/parity but
not domain-specific recovery invariants).
