# HIL Report — v4.1.4-rc1

- Generated: 2026-05-15 00:29
- Catalog: `tests\hil\golden_reference.yaml`
- **Overall (hardware tier): NOT_RUN**

Automated subset of `tests/HARDWARE_TRUTH_TESTS.md` (read→SHA-256, rpm±0.5). Physical-observation rows (motor, head-knock, GUI signal) stay manual — see the checklist.

> **NOT_RUN** — no `status: active` catalog entry produced a result. This is expected until the Golden-Reference disk set is assembled and the rig is connected. It is **not** a PASS.

## Software golden reference (CI tier — no hardware)

- Status: **active**  (6 disk classes)
- Harness: `tests/differential/test_gw_parity.py`
- Manifest: `tests/differential/corpus/MANIFEST.sha256`

Each class is decoded by BOTH the UFT flux engine and `gw convert` and asserted byte-identical — this tier runs in GitHub Actions on every push, unlike the hardware tier.

| Disk class | gw format | encoding |
|------------|-----------|----------|
| ibm_dd | ibm.720 | mfm |
| ibm_hd | ibm.1440 | mfm |
| atarist_dd | atarist.720 | mfm |
| c64_gcr | commodore.1541 | gcr_c64 |
| apple2_525 | apple2.appledos.140 | gcr_apple |
| amiga_dd | amiga.amigados | amiga |

## Disks (hardware tier)

### TEMPLATE-gw-ibm-360k — greaseweazle — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-scp-amiga-880k — scp — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-kryoflux-atarist-720k — kryoflux — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-fluxengine-apple2-140k — fluxengine — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-fc5025-ibm-360k — fc5025 — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-xum1541-1541-d64 — xum1541 — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-applesauce-apple2-protected — applesauce — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-adfcopy-amiga-880k — adfcopy — **NOT_RUN**

| Check | Status | Detail |
|-------|--------|--------|
| catalog-entry | NOT_RUN | entry status is 'template', not 'active' |

### TEMPLATE-usbfloppy-pc-1440k — usbfloppy — **NOT_RUN**

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
