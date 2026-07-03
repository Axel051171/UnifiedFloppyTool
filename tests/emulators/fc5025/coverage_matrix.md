# FC5025 Emulator — Coverage Matrix

Aggregate ~80% — the highest of the sector-domain controllers, because
FC5025 is VendorDocumented (v1309 spec) and the forensically critical
behaviour (Rule F-3 divergent-read preservation) is directly tested.

Legend: ✓ modelled & tested · ~ partial · ✗ not modelled · n/a out of scope

## Layer 1 — USB CBW/CSW wire

| Aspect | Status | Note |
|--------|--------|------|
| CBW signature "USBC" + tag + length + flags | ✓ | |
| CMD_READ_FLEXIBLE CDB (opcode + CHS + size) | ✓ | FC-2 field order plausible |
| CSW signature "USBS" + tag + residue + status | ✓ | round-trips |
| CSW status OK / CRC_ERROR / NO_SECTOR / NO_DISK | ✓ | |
| CSW write-deny | ✓ | FC-7, read-only |
| Exact vendor sense codes | ~ | FC-1 — modelled, bench-gated |
| Full USB BOT phase/reset handling | ✗ | out of scope for the forensic surface |

## Layer 2 — device state machine

| Aspect | Status | Note |
|--------|--------|------|
| DISCONNECTED → READY → ERROR | ✓ | |
| Detect (presence only, no geometry) | ✓ | FC-4, faithful |
| Read-sector per (track, side, sector) | ✓ | |
| No-device / no-disk / no-sector faults | ✓ | |
| CRC-error retry loop | ✓ | preserves divergent copies |
| Write refusal | ✓ | FC-7 |
| Motor / seek / recalibrate | ✗ | FC-6 — implicit in CLI mode |
| measureRPM | ✗ | FC-8 — hardcoded, not a capability |

## Layer 3 — sector generator

| Aspect | Status | Note |
|--------|--------|------|
| Per-format geometry (4 formats) | ✓ | FC-5 — representative subset |
| Decoded sector payloads | ✓ | deterministic |
| Per-sector CRC status flags | ✓ | |
| **Rule F-3: divergent re-reads preserved** | ✓ | group E — the forensic core |
| Defect: CRC sector (diverges + status) | ✓ | |
| Defect: missing sector → NO_SECTOR | ✓ | |
| Defect: weak sector (OK but diverges) | ✓ | |
| Determinism (seeded xorshift64*) | ✓ | |
| Physical divergence distribution | ~ | FC-3 — uniform perturbation, not physical |
| All ~15 FC5025 formats | ✗ | FC-5 — subset only |
| Flux capture | n/a | FC5025 is sector-only (hard rule) |

## Test summary

`test_fc5025_emulator.c`: **39 assertions, 6 groups (A–F), 0 fail.**
Deterministic across runs. Sibling emulators unaffected (SCP 37, GW 37,
XUM1541 56, Applesauce 111, KryoFlux 51, FluxEngine 35).

## Bench-verification gate (M3 FC5025)

Byte-verify the CSW sense codes (FC-1) and CMD_READ_FLEXIBLE CDB (FC-2)
against a real device + the v1309 spec. Until then: SIMULATED
(FIRMWARE-REALISTIC), Tier-3 PASS is bench-only. The Rule F-3 property is
the one that most needs bench confirmation that the real retry loop
preserves divergent reads as modelled.
