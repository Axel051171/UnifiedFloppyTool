# ADFCopy Emulator — Coverage Matrix

Aggregate ~75%. Strength: the binary serial protocol (the surface that
differs from every other controller) is fully framed and tested. The gap
is ADFC-2 — how the real firmware handles a track larger than the 16-bit
flux reply — which only a bench can close.

Legend: ✓ modelled & tested · ~ partial · ✗ not modelled · n/a out of scope

## Layer 1 — binary serial protocol (the wire)

| Aspect | Status | Note |
|--------|--------|------|
| CMD_INIT 0x01 → 'O' | ✓ | motor + head-home |
| CMD_SEEK 0x02 + track → 'O'/'E' | ✓ | range 0..159 |
| CMD_GET_STATUS 0x0B → bitmask | ✓ | 4 status bits |
| CMD_READ_FLUX 0x06 → 3-byte hdr + flux | ✓ | LE16 length matches |
| Responses 'O'/'E'/'D' | ✓ | distinct, visible |
| No-disk 'D' header (read) | ✓ | 3-byte, zero flux |
| Full command vocabulary | ~ | ADFC-1 — SSOT reconstruction |
| >64KB-track reply handling | ~ | ADFC-2 — 16-bit cap, bench-gated |
| USB-CDC enumeration / DTR | ✗ | host OS concern |

## Layer 2 — Teensy firmware state machine

| Aspect | Status | Note |
|--------|--------|------|
| IDLE → READY (via INIT) → ERROR | ✓ | |
| Seek-before-init rejected | ✓ | 'E' |
| Motor state (on after INIT) | ✓ | reflected in status |
| Disk-present / write-prot faults | ✓ | |
| Read-only (no write frame) | ✓ | ADFC-6 |
| INIT extra handshake | ~ | ADFC-5 — single 'O' modelled |

## Layer 3 — Amiga-MFM flux generator

| Aspect | Status | Note |
|--------|--------|------|
| 2/3/4 µs DD cell family | ✓ | 300 RPM revolution |
| 40 MHz / 25 ns ticks | ✓ | matches SCP + SSOT |
| 1-byte-per-interval flux | ✓ | ADFC-3 — clamps at 6.375 µs |
| 3-byte header (status + LE16 len) | ✓ | |
| Deterministic (seeded xorshift64*) | ✓ | byte-identical across runs |
| Defect: weak bits (≤10%) | ✓ | medium-safe |
| Defect: short (sub-revolution) | ✓ | |
| Defect: long damaged cell | ✓ | within MAX |
| Medium-safety refusal | ✓ | out-of-spec track/revs rejected |
| Long gaps > 6.375 µs | ✗ | ADFC-3 — 1-byte field limit |
| Copy-protection structures | ✗ | ADFC-4 — clean DD only |

## Test summary

`test_adfcopy_emulator.c`: **48 assertions, 6 groups (A–F), 0 fail.**
Deterministic across runs. Sibling emulators unaffected (SCP 37, GW 37,
XUM1541 56, Applesauce 111, KryoFlux 51, FluxEngine 35, FC5025 39).

## Bench-verification gate (M3 ADFCopy)

ADFC-2 (>64KB-track reply handling) and ADFC-1 (full firmware command
vocabulary) are the gate before the ADFCopy path is called "production".
Until then: SIMULATED (FIRMWARE-REALISTIC), Tier-3 PASS is bench-only.
