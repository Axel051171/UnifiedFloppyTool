# FluxEngine Firmware-Realistic Emulator (6/9)

Sixth controller in the `hardware-emulation-author` sequence, after SCP,
Greaseweazle, XUM1541, Applesauce and KryoFlux.

## What is different about FluxEngine

FluxEngine has **no C HAL** in UFT. The C++ provider
(`src/hardware_providers/fluxengine_provider_v2.cpp`) runs the
`fluxengine` command-line tool via an injectable runner. On read it asks
fluxengine to write an **SCP file**, then decodes it with UFT's own
vetted SCP parser (`src/flux/uft_scp_parser.c`). RPM/detect run
`fluxengine rpm` and parse stdout with the regex `(\d+\.?\d*)\s*rpm`.

So the three layers model the fluxengine CLI + device pair:

1. **fluxengine CLI contract** (`firmware_state_machine.{h,c}`) — read /
   write / rpm requests, exit codes, and the RPM stdout shapes. Write is
   refused (forensic read-only policy).
2. **Device state machine** — DISCONNECTED → READY → BUSY → ERROR, with
   no-device / no-disk faults that fail with no file (never a partial
   read), and a soft-fault retry model.
3. **SCP file generator** (`../../flux_gen/fluxengine/`) — builds a
   minimal but REAL single-track SCP file (16-byte header + 168-entry
   offset table + TRK header + per-rev entries + 16-bit BE flux),
   deterministic xorshift64*, with defect classes that map to specific
   SCP parser errors.

## The distinguishing feature

The generated SCP reads are decoded by the **production parser**
`uft_scp_read_track_memory()`, and each container defect asserts the
exact `uft_scp` error code:

| Defect | Parser code |
|--------|-------------|
| clean | `UFT_SCP_OK` (flux count, RPM ~300) |
| BAD_SIG (corrupt "SCP") | `UFT_SCP_ERR_SIGNATURE` |
| BAD_TRK_SIG (corrupt "TRK") | `UFT_SCP_ERR_SIGNATURE` |
| ZERO_OFFSET (track offset 0) | `UFT_SCP_ERR_TRACK` |
| TRUNCATED (flux cut short) | `UFT_SCP_ERR_READ` |

That makes the generator a **live conformance fixture for the SCP
reader** on the FluxEngine path — the same strength KryoFlux has with
`uft_kf_decode()`.

## Forensic invariants

- No fabricated flux: a successful read returns the generator's SCP
  bytes verbatim; a failed read returns a non-zero exit AND no file.
- Write is always refused (read-only policy).
- The `fluxengine rpm` stdout shapes all satisfy the provider's
  documented regex; the value round-trips.

## Build & run

Registered in `tests/CMakeLists.txt` as `test_fluxengine_emulator`
(links `src/flux/uft_scp_parser.c` for the real parser; no libm — the
test avoids fabs, per the MF-305 lesson).

```
ctest -R test_fluxengine_emulator --output-on-failure
```

Standalone:

```
gcc -Wall -Wextra -I include -I src \
    -I tests/emulators/fluxengine -I tests/flux_gen/fluxengine \
    tests/emulators/fluxengine/test_fluxengine_emulator.c \
    tests/emulators/fluxengine/firmware_state_machine.c \
    tests/flux_gen/fluxengine/flux_gen.c \
    src/flux/uft_scp_parser.c -o fe_test && ./fe_test
```

## Status

**35 assertions, 6 groups (A–F), 0 fail. Deterministic.** Coverage ~70%
aggregate. Three HIGH divergences (FE-1 exit-code/stdout vocabulary, FE-2
SCP byte-capture, FE-3 CLI flag dialect) are the M3-FluxEngine bench
gate; until then this is SIMULATED (FIRMWARE-REALISTIC), not a substitute
for the bench.

See also: `DIVERGENCES.md`, `coverage_matrix.md`.
