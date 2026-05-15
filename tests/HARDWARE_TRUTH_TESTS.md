# Hardware Truth Tests

The compile-time guarantees and conformance suite ensure the *shape* of
the type-driven HAL is correct. Whether the bytes coming off the wire
match a real disk is a question only real hardware can answer.

This document is the manual checklist Axel runs against physical
hardware before promoting `v4.1.4-rc1` → `v4.1.4`. It is the *only*
thing in the refactor that cannot be automated.

---

## Pre-refactor baseline (capture once on `main` @ MF-149)

Run these on `main` BEFORE switching to `refactor/type-driven-hal` and
keep the artefacts. Every post-refactor result must match byte-for-byte
(disk images) or within tight tolerance (RPM ±0.5).

```bash
git checkout main
mkdir -p tests/baseline_artifacts
# Replace ${DEV} with the real device path on your machine.

# Greaseweazle — full-track flux capture of a known disk
./uft read --hal greaseweazle --device ${DEV} \
    --image tests/baseline_artifacts/gw_known_disk.scp \
    --tracks 0-79 --revs 3
sha256sum tests/baseline_artifacts/gw_known_disk.scp \
  > tests/baseline_artifacts/gw_known_disk.sha256

# Greaseweazle — RPM measurement
./uft rpm --hal greaseweazle --device ${DEV} \
    --revolutions 50 \
    > tests/baseline_artifacts/gw_rpm.txt

# Per controller you actually own, repeat with the equivalent command.
```

---

## Greaseweazle — mandatory verification

**Pre-condition:** v4.1.4-rc1 build available, same physical disk in
the drive that produced the baseline.

| Check                                                                          | Expected                                          | OK? |
|--------------------------------------------------------------------------------|---------------------------------------------------|-----|
| `read_raw_flux` of baseline disk → SHA-256 match against baseline              | identical hash                                    | ☐   |
| `write_raw_flux` of a blank, then `read_raw_flux`, then byte-compare           | identical                                         | ☐   |
| `set_motor(true)` audible spin-up; `set_motor(false)` audible stop             | both                                              | ☐   |
| `seek(0)`, `seek(40)`, `seek(80)` — no head-knock, head visibly moves          | quiet, smooth                                     | ☐   |
| `recalibrate()` — head returns to track 0 cleanly                              | yes                                               | ☐   |
| `measure_rpm()` for 50 revolutions                                             | within ±0.5 RPM of baseline                       | ☐   |
| `detect_drive()` reports correct drive type (3.5" HD vs 5.25" HD vs DD)        | matches physical drive                            | ☐   |
| `detect_drive()` with no drive attached                                        | `DriveAbsent` outcome, not `ProviderError`        | ☐   |
| Bootloader-mode firmware → `ProviderError` with the actionable hint            | what/why/fix message present                      | ☐   |
| GUI: clicking each Hardware-tab button emits one signal, no silent no-op       | every button responds                             | ☐   |

If any row fails: STOP. The refactor is not green. File details in
`docs/REFACTOR_NOTES.md` under "Hardware Truth Failure".

---

## SuperCard Pro — verification (if hardware available)

The SCP HAL is partial scaffold (M3.1). For pre-release:

| Check                                                                | Expected                                              | OK? |
|----------------------------------------------------------------------|-------------------------------------------------------|-----|
| `detect_drive()` returns `DriveDetected` with correct firmware string| matches `scp.exe info` output                         | ☐   |
| `read_raw_flux` of baseline disk → SHA match                         | identical                                             | ☐   |
| Capability surface: `WritesRawFlux<SCPProviderV2>` is true           | static_assert in conformance                          | ☐   |
| `set_motor` / `seek` / `measure_rpm` calls do **not** compile        | the type lacks those mixins (intentional)             | ☐   |

---

## KryoFlux — verification (if hardware available)

DTC is a subprocess; treat it as an external dependency.

| Check                                                                | Expected                                              | OK? |
|----------------------------------------------------------------------|-------------------------------------------------------|-----|
| DTC binary on PATH; version pinned in CI matches                     | reported in detect output                             | ☐   |
| `read_raw_flux` of baseline disk → SHA match                         | identical                                             | ☐   |
| `detect_drive()` honest about read-only capability surface           | `WritesRawFlux<KryoFluxProviderV2>` is false          | ☐   |
| KryoFlux removed mid-read → `HardwareDisconnected` outcome           | not silent failure                                    | ☐   |

---

## FluxEngine — verification (if hardware available)

| Check                                                                        | Expected                                  | OK? |
|------------------------------------------------------------------------------|-------------------------------------------|-----|
| `fluxengine` binary on PATH                                                  | yes                                       | ☐   |
| `read_raw_flux` of baseline disk → SHA match                                 | identical                                 | ☐   |
| `write_raw_flux` + verify-read → byte-identical                              | identical                                 | ☐   |
| Capability surface mirrors what fluxengine-CLI actually supports             | conformance tests reflect this            | ☐   |

---

## FC5025 — verification (if hardware available)

| Check                                                                        | Expected                                  | OK? |
|------------------------------------------------------------------------------|-------------------------------------------|-----|
| `read_sector` on a known DOS-formatted disk → byte-match against baseline    | identical                                 | ☐   |
| Calling `write_sector` on `FC5025ProviderV2` is a **compile error**          | yes — read-only by hardware design         | ☐   |
| `detect_drive()` distinguishes 5.25" SD vs DD vs HD correctly                | matches physical drive                    | ☐   |

---

## XUM1541 / ZoomFloppy — verification (if hardware available)

| Check                                                                        | Expected                                  | OK? |
|------------------------------------------------------------------------------|-------------------------------------------|-----|
| `read_sector` from a 1541-formatted D64 → match baseline                     | identical                                 | ☐   |
| `write_sector` of a known D64 then read-back → identical                     | identical                                 | ☐   |
| `detect_drive()` reports correct CBM drive model (1541 vs 1571 vs 1581)      | matches device probe                      | ☐   |

---

## Applesauce — verification (if hardware available)

| Check                                                                        | Expected                                  | OK? |
|------------------------------------------------------------------------------|-------------------------------------------|-----|
| `read_raw_flux` of an Apple II disk → byte-match against baseline (.a2r)     | identical                                 | ☐   |
| Auto-detect avoids ADF-Copy port (VID/PID disambiguation MF-146 still works) | only Applesauce ports candidate           | ☐   |

---

## ADF-Copy — verification (if hardware available)

| Check                                                                        | Expected                                  | OK? |
|------------------------------------------------------------------------------|-------------------------------------------|-----|
| `read_raw_flux` of an Amiga disk → byte-match against baseline               | identical                                 | ☐   |
| Auto-detect avoids Applesauce port (MF-146 inverse)                          | only ADF-Copy ports candidate             | ☐   |

---

## USB Floppy (UFI) — verification (if hardware available)

| Check                                                                        | Expected                                  | OK? |
|------------------------------------------------------------------------------|-------------------------------------------|-----|
| `read_sector` of a 1.44 MB DOS floppy → byte-match against baseline image    | identical                                 | ☐   |
| `write_sector` then read-back → identical                                    | identical                                 | ☐   |

---

## Sign-off

Once all applicable rows are ☐ → ☑:

```
Hardware Truth verification complete: 2026-MM-DD
Verifier:  Axel Fuchs (MysticFoxDE)
Result:    {ALL GREEN | partial — see notes}
Tag:       v4.1.4
```

Append this block to `RELEASE_NOTES.md` under the v4.1.4 section. The
sign-off is the formal release gate — automation cannot replace it.
