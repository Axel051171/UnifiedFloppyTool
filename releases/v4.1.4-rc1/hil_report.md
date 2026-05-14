# HIL Report — v4.1.4-rc1

- Generated: 2026-05-14 13:41
- Catalog: `tests\hil\golden_reference.yaml`
- **Overall: NOT_RUN**

Automated subset of `tests/HARDWARE_TRUTH_TESTS.md` (read→SHA-256, rpm±0.5). Physical-observation rows (motor, head-knock, GUI signal) stay manual — see the checklist.

> **NOT_RUN** — no `status: active` catalog entry produced a result. This is expected until the Golden-Reference disk set is assembled and the rig is connected. It is **not** a PASS.

## Disks

### TEMPLATE-gw-ibm-360k — greaseweazle — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-xum1541-1541-d64 — xum1541 — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

## Manual rows (not automatable)

The following require a human at the rig — fill in `tests/HARDWARE_TRUTH_TESTS.md` and sign off there:

- motor spin-up / stop audible
- seek 0/40/80 — no head-knock
- recalibrate — clean return to track 0
- detect_drive with no drive → `DriveAbsent`, not `ProviderError`
- bootloader-mode firmware → actionable `ProviderError`
- GUI: every Hardware-tab button emits exactly one signal

## Sign-off

This automated report does **not** replace the formal sign-off block in `RELEASE_NOTES.md` (HARDWARE_TRUTH_TESTS.md §Sign-off). Automation cannot replace the human gate.
