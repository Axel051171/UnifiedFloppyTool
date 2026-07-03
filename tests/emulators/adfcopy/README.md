# ADFCopy Firmware-Realistic Emulator (8/9)

Eighth controller in the `hardware-emulation-author` sequence, after SCP,
Greaseweazle, XUM1541, Applesauce, KryoFlux, FluxEngine and FC5025.

## What is different about ADFCopy

ADFCopy is a Teensy-based (PJRC VID:PID 0x16C0:0x0483) USB-CDC serial
controller for Amiga 3.5" drives. Unlike Applesauce's ASCII line
protocol, ADFCopy uses **binary framing**: a 1-byte command + variable
payload, a 1-byte `'O'`/`'E'`/`'D'` response, a status bitmask, and a
CMD_READ_FLUX 3-byte header (status + LE16 length) + flux payload. Sample
clock 40 MHz / 25 ns (same as SCP). Protocol SSOT:
`src/hardware_providers/adfcopy_serial_runners.h` (MF-252).

Three layers:

1. **Binary serial protocol** (`firmware_state_machine.{h,c}`):
   - `CMD_INIT 0x01` → `'O'` (spin motor + home head)
   - `CMD_SEEK 0x02 + track` → `'O'`/`'E'` (track = cyl*2 + head, 0..159)
   - `CMD_GET_STATUS 0x0B` → 1-byte bitmask (DISK/WPROT/MOTOR/FLUX)
   - `CMD_READ_FLUX 0x06 + track + revs` → 3-byte header + flux
2. **Teensy firmware state machine** — IDLE → READY (after INIT) → ERROR;
   seek-before-init rejected, disk/write-protect faults visible, no write
   frame (read-only).
3. **Amiga-MFM flux generator** (`../../flux_gen/adfcopy/`) — 2/3/4 µs DD
   cell family at 40 MHz / 25 ns, packed as the CMD_READ_FLUX reply,
   deterministic xorshift64*, with weak-bit / short / long-cell defects.

## Forensic invariants

- No fabricated flux: a READ_FLUX success returns the generator's bytes
  verbatim; a no-disk read returns a 3-byte `'D'`-status header with zero
  flux — never a silent success.
- Read-only: there is no write command frame; write-protect does not
  block reading (forensic capture never writes).
- Errors are visible: every failure path returns a distinct non-`'O'`
  byte.
- The CMD_READ_FLUX header's LE16 length always equals the flux byte
  count.

## Build & run

Registered in `tests/CMakeLists.txt` as `test_adfcopy_emulator`
(self-contained; no libm — MF-305 lesson).

```
ctest -R test_adfcopy_emulator --output-on-failure
```

Standalone:

```
gcc -Wall -Wextra -I include -I src \
    -I tests/emulators/adfcopy -I tests/flux_gen/adfcopy \
    tests/emulators/adfcopy/test_adfcopy_emulator.c \
    tests/emulators/adfcopy/firmware_state_machine.c \
    tests/flux_gen/adfcopy/flux_gen.c -o adfc_test && ./adfc_test
```

## Status

**48 assertions, 6 groups (A–F), 0 fail. Deterministic.** Coverage ~75%
aggregate. The bench gate is ADFC-2 (how the real firmware handles a
track larger than the 16-bit flux reply — the generator caps at 0xFFFF,
a real protocol limit) and ADFC-1 (full firmware command vocabulary).
Until then SIMULATED (FIRMWARE-REALISTIC).

See also: `DIVERGENCES.md`, `coverage_matrix.md`.
