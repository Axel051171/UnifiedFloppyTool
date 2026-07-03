# FC5025 Firmware-Realistic Emulator (7/9)

Seventh controller in the `hardware-emulation-author` sequence, after
SCP, Greaseweazle, XUM1541, Applesauce, KryoFlux and FluxEngine.

## What is different about FC5025

FC5025 is a **sector-only, read-only** USB device (Device Side Data,
VID:PID 0x16C0:0x06D6). CLAUDE.md hard rule: *"FC5025 kann keinen Flux
lesen — nur Sektordaten"*. The firmware decodes the disk internally and
delivers decoded sector payloads via USB Bulk-Only Transport
(CMD_READ_FLEXIBLE CBW on EP 0x01, sector data + CSW on EP 0x81). So this
emulator is sector-domain, not flux-domain.

It is also the richest-documented controller so far: `SPEC_STATUS:
VendorDocumented`, the FC5025 Command Set Specification v1309 (Device
Side Data, deviceside.com/fc5025.html).

Three layers:

1. **USB CBW/CSW wire** (`firmware_state_machine.{h,c}`) — standard
   Bulk-Only Transport framing ("USBC"/"USBS" signatures, tag, residue,
   status) with the FC5025 CMD_READ_FLEXIBLE opcode. CSW status codes for
   OK / CRC-error / no-sector / no-disk / write-deny.
2. **Device state machine** — detect (presence only; FC5025 cannot
   auto-detect geometry), read-sector with a CRC-error retry loop,
   read-only enforcement (no write opcode exists).
3. **Sector generator** (`../../flux_gen/fc5025/`) — per-format geometry
   (Apple DOS 3.3 = 35×16×256, DOS 3.2 = 13 sec, IBM 3740 FM = 77×26×128,
   TRS-80 SSSD), decoded sector payloads with per-sector CRC status,
   deterministic xorshift64*.

## The forensic core: Rule F-3 (divergent-read preservation)

The provider's Rule F-3 requires that a CRC-error sector's re-reads are
PRESERVED — a marginal sector read multiple times yields DIFFERENT bytes,
and those divergent copies are kept (`SectorMarginal::divergent_reads`
>= 2), never collapsed to one "resolved" value, and the CRC status is
never silently cleared to OK. Group E asserts exactly this:

- a CRC-error sector reports `FC_CSW_CRC_ERROR`,
- keeps **>= 2 distinct** re-read copies that genuinely differ,
- and re-reading it still reports the error (no silent "repair").

A clean sector, by contrast, returns a single stable copy.

## Forensic invariants

- No fabricated data: a failed read returns a non-zero status and no
  usable sector; a marginal read keeps its divergent copies.
- Read-only is structural: `fc_write_sector` always returns
  `FC_CSW_WRITE_DENY` — no silent no-op (there is no write opcode).
- CRC status survives re-reads — never silently cleared.

## Build & run

Registered in `tests/CMakeLists.txt` as `test_fc5025_emulator`
(self-contained: no HAL/parser link; no libm — the MF-305 lesson).

```
ctest -R test_fc5025_emulator --output-on-failure
```

Standalone:

```
gcc -Wall -Wextra -I include -I src \
    -I tests/emulators/fc5025 -I tests/flux_gen/fc5025 \
    tests/emulators/fc5025/test_fc5025_emulator.c \
    tests/emulators/fc5025/firmware_state_machine.c \
    tests/flux_gen/fc5025/flux_gen.c -o fc_test && ./fc_test
```

## Status

**39 assertions, 6 groups (A–F), 0 fail. Deterministic.** Coverage ~80%
aggregate — the highest of the sector-domain controllers, thanks to the
v1309 spec and the directly-tested Rule F-3 property. Bench gate:
byte-verify the CSW sense codes (FC-1) and CMD_READ_FLEXIBLE CDB (FC-2)
against a real device. Until then SIMULATED (FIRMWARE-REALISTIC).

See also: `DIVERGENCES.md`, `coverage_matrix.md`.
