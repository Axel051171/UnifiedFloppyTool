# Applesauce Firmware-Realistic Emulator (4/9)

Fourth controller in the `hardware-emulation-author` sequence, after
SCP, Greaseweazle and XUM1541. Models the Applesauce disk-imaging
controller so bugs in the UFT Applesauce HAL surface in CI instead of
only at a hardware bench.

## What it is

A 3-layer emulator, same convention as the sibling controllers:

1. **Wire protocol** (`firmware_state_machine.{h,c}`) — Applesauce
   speaks an ASCII line protocol over USB-CDC serial (115200 8N1). The
   host feeds one command line into `as_fw_process_line()` and gets back
   the response line plus a classification. Binary flux downloads drain
   through `as_fw_pop_binary()`.
2. **Firmware state machine** — a 7-state ordered climb
   (POWER_ON → PSU_ON → MOTOR_ON → SYNC_ON → CAPTURED → BINARY_TX, plus
   a sticky ERROR). Illegal transitions answer visibly; protocol desync
   wedges the device.
3. **Synthetic flux generator** (`../../flux_gen/applesauce/`) — Apple
   II 5.25" 16-sector 6-and-2 GCR and Mac 3.5" 5-zone GCR, deterministic
   xorshift64*, 8 MHz tick domain, six injectable defect classes
   including Apple-specific fake-bit/self-sync protection patterns.

## Protocol truth source

There is **no official Applesauce SDK**. This emulator follows, strongest
first:

1. `src/hardware_providers/applesauce_serial_runners.cpp` — the
   executable host-side command vocabulary UFT drives hardware with.
2. `src/hardware_providers/applesauce_provider_v2.h` — status-char table
   from the V1 audit against wiki.applesaucefdc.com.
3. `docs/M3_APPLESAUCE_TRANSPORT.md` — **conflicts** with 1+2; see
   `DIVERGENCES.md` D-1. Resolving that conflict is a bench-session gate.

`SPEC_STATUS: REVERSE-ENGINEERED`. Every uncertainty is logged in
`DIVERGENCES.md` with a severity, never guessed silently.

## Forensic invariants

- The state machine never fabricates flux bytes — flux comes only from
  the synthetic generator, wired in via `as_fw_load_capture()`.
- The write path (`disk:write`, `data:> N`) is **always refused** with
  `!`, regardless of media state — matching the UFT HAL's write gate.
- No silent state changes: sequencing violations answer `v`/`-`/`!`
  visibly; a host line during a pending binary download wedges the
  device into a reset-only ERROR state.
- The 8 MHz / 125 ns tick domain is cross-checked against the HAL SSOT
  (`uft_as_get_sample_clock`, `uft_as_ticks_to_ns`) at test time — the
  emulator cannot silently drift from the HAL.

## Build & run

Registered in `tests/CMakeLists.txt` as `test_applesauce_emulator`
(links `src/hal/uft_applesauce.c` for the SSOT cross-check; serial-only
HAL, so no libusb; `-lm` for the tick-math asserts).

```
ctest -R test_applesauce_emulator --output-on-failure
```

Standalone:

```
gcc -Wall -Wextra -I include -I src \
    -I tests/emulators/applesauce -I tests/flux_gen/applesauce \
    tests/emulators/applesauce/test_applesauce_emulator.c \
    tests/emulators/applesauce/firmware_state_machine.c \
    tests/flux_gen/applesauce/flux_gen.c \
    src/hal/uft_applesauce.c -lm -o as_test && ./as_test
```

## Status

**111 assertions, 6 groups (A–F), 0 fail. Deterministic.** Coverage
~75% aggregate vs real hardware — see `coverage_matrix.md`. The two HIGH
divergences (D-1 command vocabulary, D-2 binary framing) are the
bench-verification gate for M3.3; until then this is SIMULATED
(FIRMWARE-REALISTIC), not a substitute for the bench.

See also: `DIVERGENCES.md`, `coverage_matrix.md`.
