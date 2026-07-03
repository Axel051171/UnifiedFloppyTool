# Applesauce Emulator — Coverage Matrix

What the three emulator layers cover versus real hardware, per the
`hardware-emulation-author` contract. Aggregate ~75% — dominated by the
D-1 command-vocabulary and D-2 flux-framing uncertainties that only a
bench session can close.

Legend: ✓ modelled & tested · ~ partial · ✗ not modelled · n/a out of scope

## Layer 1 — Wire protocol (ASCII line over USB-CDC serial)

| Aspect | Status | Note |
|--------|--------|------|
| `psu:on/off/?` | ✓ | state + truthful query |
| `motor:on/off` | ✓ | requires PSU (`v` otherwise) |
| `sync:on/off` | ✓ | requires motor (`-` otherwise) |
| `sync:?speed` | ✓ | requires index; RPM payload |
| `head:track N` | ✓ | 0..83, range-checked, needs PSU |
| `head:side N`, `head:zero` | ✓ | |
| `disk:readx R` | ✓ | needs sync+disk+index; 1..15 revs |
| `disk:?write` | ✓ | truthful write-protect report |
| `data:?size`, `data:?max` | ✓ | |
| `data:< N` (binary download) | ✓ | streams via `as_fw_pop_binary` |
| `data:clear` | ✓ | |
| `?kind`, `?vers`, `?pcb` | ✓ | |
| `disk:write`, `data:> N` | ✓ (refused) | forensic-safety guard, D-10 |
| Over-length / unknown lines | ✓ | `!` / `?` |
| Exact binary download framing | ~ | D-2 — LE32 contract, not wire-verified |
| Real command vocabulary | ~ | D-1 — runner vs transport-doc conflict |
| USB-CDC enumeration / DTR/RTS | ✗ | host OS concern, not modelled |

## Layer 2 — Firmware state machine

| Aspect | Status | Note |
|--------|--------|------|
| 7-state ordered climb (POWER_ON→…→BINARY_TX) | ✓ | |
| Illegal-transition rejection (visible) | ✓ | never silent |
| Sticky desync ERROR (reset-only recovery) | ✓ | D-5, pessimistic |
| Timeout / cable-pull drill (`silent_next`) | ✓ | |
| Binary-pending desync wedge | ✓ | host must drain |
| Write-protect / no-disk / no-index faults | ✓ | |
| Graceful resync after desync | ✗ | D-5 — modelled as unrecoverable |
| Real firmware line-buffer size | ~ | D-7 — assumed 64 |

## Layer 3 — Synthetic flux generator (Apple GCR)

| Aspect | Status | Note |
|--------|--------|------|
| Apple II 5.25" 6-and-2, 16-sector | ✓ | D5 AA 96 / D5 AA AD prologs |
| Address-field 4-and-4 + checksum | ✓ | round-trips in test |
| Self-sync 10-bit FF gaps | ✓ | |
| Deterministic (seeded xorshift64*) | ✓ | byte-identical across runs |
| LE32 8 MHz framing, no endian leak | ✓ | |
| Defect: weak bits (≤10% jitter) | ✓ | medium-safe |
| Defect: checksum error | ✓ | |
| Defect: sync anomaly | ✓ | |
| Defect: missing address field | ✓ | |
| Defect: fake/self-sync bits (protection) | ✓ | ≥5-cell zero run |
| Mac 3.5" 5 speed zones (12/11/10/9/8) | ✓ | zone table = SSOT |
| Mac 3.5" data-field GCR bytes | ~ | D-8 — simplified payload |
| Mac cross-zone speed defect | ✓ | |
| 8 MHz tick domain vs HAL SSOT | ✓ | cross-checked in test |
| Medium-safety refusal (interval/track) | ✓ | |
| 13-sector DOS 3.2 | ✗ | 5.25" generator is 16-sector only |

## Test summary

`test_applesauce_emulator.c`: **111 assertions, 6 groups (A–F), 0 fail.**
Deterministic across runs. Sibling emulators (SCP 37, GW 37, XUM1541 56)
unaffected.

## Bench-verification gate (M3.3)

Before the Applesauce HAL is called "production", a real-hardware bench
session (UFT-008 pattern) must resolve D-1 (command vocabulary) and D-2
(binary framing) — the two HIGH divergences. Until then: SIMULATED
(FIRMWARE-REALISTIC), Tier-3 PASS is bench-only.
