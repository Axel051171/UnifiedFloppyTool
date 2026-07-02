# XUM1541/ZoomFloppy Firmware-Realistic Emulator + Synthetic GCR Generator

**Status:** SIMULATED (FIRMWARE-REALISTIC, ~80% coverage vs. real-HW spec;
gaps documented in [`DIVERGENCES.md`](DIVERGENCES.md)).
Real-hardware bench session still required for Tier-3 PASS.

## Why XUM1541 third

Third invocation of the hardware-emulation-author agent, after SCP
(commit `2e2a36f`) and Greaseweazle. The XUM1541 is the first
**non-flux** controller in the sequence: it is an AVR-based USB ↔
Commodore-IEC bridge, and its M3.2 HAL (`src/hal/uft_xum1541.c`) has
libusb wiring that has **never been driven by real hardware**. The
emulator pins the firmware sequencing semantics the M3.2 wiring will
have to satisfy — and it has already surfaced two HAL-vs-OpenCBM
protocol deltas that must be verified before the bench session
(DIVERGENCES.md X-DELTA-1 / X-DELTA-2).

## Scope

Three layers, per `.claude/agents/hardware-emulation-author.md`:

| Layer | File | Purpose |
|---|---|---|
| Wire protocol | `firmware_state_machine.c::xum_fw_ctrl` + `xum_fw_bulk_command` + `xum_fw_status_serialize` | Control transfers (ECHO/INIT/RESET/SHUTDOWN/version strings), 4-byte bulk command headers, 3-byte status phase `[code, val_lo, val_hi]` |
| Firmware state machine | `firmware_state_machine.{h,c}` | 7 states + IEC bus-role sequencing (LISTEN/TALK/UNLISTEN/UNTALK), EOI flag, IOCTLs, sticky-vs-transient error semantics |
| Payload generation | `tests/flux_gen/xum1541/flux_gen.{h,c}` | Raw-GCR track images as a 1541 drive delivers them over the parallel nibbler, 7 defect classes |

```
DISCONNECTED -> CONNECTED -(CTRL_INIT)-> READY <-(UNLISTEN/UNTALK)- LISTENING/TALKING
                                          |  \
                              (violation) |   `-(LISTEN/TALK)-> LISTENING / TALKING
                                          v
                                        ERROR -(CTRL_INIT / CTRL_RESET)-> READY
CTRL_SHUTDOWN (any state) -> SHUTDOWN (terminal: no response to anything)
```

Sticky vs. transient errors mirror real IEC behaviour: a protocol
violation (bulk before INIT, WRITE without LISTEN, role conflict,
FIFO overrun) wedges the firmware until INIT/RESET; a bus timeout
(absent drive, bad device number) returns `STATUS_ERROR` but leaves
the adapter usable.

## The "flux" layer is GCR, not flux

The XUM1541 never samples flux. What a nibbler transfer delivers is a
**byte-aligned raw-GCR track** (the 1541 read channel byte-syncs on the
sync mark in hardware). The generator produces exactly that, per
Commodore density zone (21/19/18/17 sectors, 7692/7142/6666/6250 bytes
per revolution), with these defect classes — each individually
toggleable:

- `WEAK_BITS` — unstable region inside one data block
- `CHECKSUM_ERROR` — 1541 DOS error 23 (data checksum)
- `SYNC_LONG` — 40-byte copy-protection sync run
- `SYNC_SHORT` — sub-threshold sync (8 bits < 10-bit minimum)
- `KILLER_TRACK` — zero syncs (RapidLok-style; wedges a real 1541 reader)
- `DENSITY_MISMATCH` — zone-3 bit density written on an inner track
- `HALF_TRACK` — crosstalk-degraded read at track N + 0.5

Zone/sector tables are cross-checked at test time against the HAL SSOT
`uft_xum_sectors_for_track()` — the test executable links
`src/hal/uft_xum1541.c` (without libusb) for exactly that purpose.

## Non-scope

- **Drive-internal DOS** — the 1541 is itself a computer (6502 + DOS
  ROM). Job queue, `M-R`/`M-W`/`U1` commands and fastloader upload are
  NOT modelled; the TALK/READ pipeline drains a pre-generated GCR
  stream (DIVERGENCES.md X-D2).
- **IEC bit-level timing** — ATN/CLK/DATA handshake cells (~70 µs)
  are not modelled; only line-state polling (X-D3).
- **Write path to media** — LISTEN payloads are accepted and counted
  (drive command channel), but nothing is ever persisted as disk
  content. `CTRL_ENTER_BOOTLOADER` is always refused (bricking guard).
- **Other controllers** — Applesauce etc. get their own invocations.

## Reference

- HAL under test: [`src/hal/uft_xum1541.c`](../../../src/hal/uft_xum1541.c)
  (M3.2 partial scaffold) + [`include/uft/hal/uft_xum1541.h`](../../../include/uft/hal/uft_xum1541.h)
  (MF-255-audited constants — in-repo SSOT for opcodes)
- Upstream firmware reference: [OpenCBM/OpenCBM](https://github.com/OpenCBM/OpenCBM)
  xum1541 firmware + host lib (BSD-2). SPEC_STATUS: REVERSE-ENGINEERED —
  no official ZoomFloppy SDK exists.
- GCR layout: Commodore 1541 service manual + VIC-1541 DOS ROM listings.

## Build + run

```bash
cmake -B build
cmake --build build --target test_xum1541_emulator
./build/tests/test_xum1541_emulator        # on Windows: .exe
# or:
cd build && ctest -R test_xum1541_emulator -V
```

## Forensic honesty

- Every byte is traceable: clean GCR follows the documented 1541
  format; defect bytes come from the seeded xorshift64* RNG and are
  reproducible from `(seed, params)` alone. Output is byte-identical
  across runs and platforms (verified: two consecutive runs diff-clean).
- Medium-safety guards: the generator refuses tracks outside stepper
  bounds (1..42, half-track ≤ 41.5) and tracks exceeding the density
  zone's byte budget — an overlong track written back to real media
  would destroy its own write splice.
- The HAL is linked read-only for table cross-checks, never modified.
- Sim-vs-real divergences: [`DIVERGENCES.md`](DIVERGENCES.md).
  Coverage: [`coverage_matrix.md`](coverage_matrix.md).
