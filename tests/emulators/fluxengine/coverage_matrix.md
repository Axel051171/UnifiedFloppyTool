# FluxEngine Emulator — Coverage Matrix

Aggregate ~70%. Like KryoFlux, the strength is that the flux layer is
validated against a PRODUCTION parser (`uft_scp_read_track_memory()`);
the gaps are the FE-1/FE-2/FE-3 CLI/SCP-dialect questions that only a
bench can close.

Legend: ✓ modelled & tested · ~ partial · ✗ not modelled · n/a out of scope

## Layer 1 — fluxengine CLI contract

| Aspect | Status | Note |
|--------|--------|------|
| `read` request (cyl/head/revs/profile) | ✓ | range-validated |
| `write` request | ✓ (refused) | FE-8, always denied |
| `rpm` request | ✓ | stdout line emitted |
| Exit code OK / NO_DEVICE / NO_DISK | ✓ | |
| Exit code BAD_ARGS / WRITE_DENY / IO | ✓ | |
| RPM stdout shape parsing (3 variants) | ✓ | provider regex contract |
| Real exit-code vocabulary | ~ | FE-1 — modelled, bench-gated |
| Exact argv flag dialect | ~ | FE-3 — device-response modelled, not argv |
| Motor / seek / recalibrate | ✗ | FE-7 — no CLI primitive |

## Layer 2 — fluxengine + device state machine

| Aspect | Status | Note |
|--------|--------|------|
| DISCONNECTED→READY→BUSY→ERROR | ✓ | |
| No-device / no-disk faults | ✓ | fail empty, never partial file |
| Retry model | ✓ | FE-6 — soft-fault retry |
| Retry exhaustion → IO | ✓ | |
| Hard/soft fault classification | ~ | FE-6 — simplified |

## Layer 3 — SCP file generator

| Aspect | Status | Note |
|--------|--------|------|
| 16-byte SCP header ("SCP", res, revs) | ✓ | matches parser |
| 168-entry track offset table | ✓ | |
| TRK header + per-rev entries | ✓ | index_time / length / offset |
| 16-bit BE flux samples | ✓ | |
| **Decodes via `uft_scp_read_track_memory()`** | ✓ | flux count, RPM |
| Deterministic (seeded xorshift64*) | ✓ | byte-identical across runs |
| Defect BAD_SIG → ERR_SIGNATURE | ✓ | group F |
| Defect BAD_TRK_SIG → ERR_SIGNATURE | ✓ | |
| Defect ZERO_OFFSET → ERR_TRACK | ✓ | |
| Defect TRUNCATED → ERR_READ | ✓ | |
| Defect weak bits (≤10%) | ✓ | medium-safe |
| Medium-safety refusal | ✓ | out-of-spec track/revs rejected |
| SCP footer / string table | ✗ | not needed for single-track read |
| GCR / FM / HD cell families | ✗ | FE-5 — DD-MFM only |

## Test summary

`test_fluxengine_emulator.c`: **35 assertions, 6 groups (A–F), 0 fail.**
Deterministic across runs. Sibling emulators unaffected (SCP 37, GW 37,
XUM1541 56, Applesauce 111, KryoFlux 51).

## Bench-verification gate (M3 FluxEngine)

Three HIGH divergences (FE-1 exit-code/stdout vocabulary, FE-2 SCP
byte-capture, FE-3 CLI flag dialect) are the gate before the FluxEngine
path is called "production". Until then: SIMULATED (FIRMWARE-REALISTIC),
Tier-3 PASS is bench-only.
