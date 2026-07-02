# Greaseweazle Firmware-Realistic Emulator + Synthetic Flux Generator

**Status:** SIMULATED (FIRMWARE-REALISTIC, ~85% coverage vs. real-HW spec).
Real-hardware bench session still required for Tier-3 PASS — see
`docs/MASTER_PLAN.md` and `tests/hil/`.

## Why Greaseweazle second

The Greaseweazle HAL (`src/hal/uft_greaseweazle_full.c`) is the only
PRODUCTION-tested hardware path in UFT — every other provider is
honest-stub or partial scaffold. Building the GW emulator second
(after SCP) **calibrates the firmware-realistic methodology against
ground truth**: the only HW path where we already know what "right"
looks like.

The SCP emulator (commit `2e2a36f`) verified the agent's three-layer
template against a non-production HAL. THIS emulator verifies the
template against a HAL that has been driven by real F7 and F7-Plus
silicon in actual recovery sessions. If the methodology can fit
both, it's broadly applicable.

## Scope

This emulator goes *beyond* a hypothetical byte-mock. The HAL itself
is already production-driven, so we don't need to *verify wire bytes*
— we need to verify the **firmware sequencing semantics**, the **bus/
select/motor/seek prerequisite ordering**, and **flux-format correctness
of synthetic test patterns**:

```
DISCONNECTED -> CONNECTED -> BUS_CONFIGURED -> DRIVE_SELECTED
                  ^                                  |
                  | Cmd.Reset                        v
                                              MOTOR_ON -> SEEKED
                                                            |
                                                            v
                                              STREAMING_READ -> FLUX_STATUS_READY
```

Sequencing violations (e.g. `Cmd.Seek` before `Cmd.Motor`) transition
the firmware emulator into `GW_FW_STATE_ERROR` with a sticky ACK code
(`ACK_BAD_COMMAND` / `ACK_NO_UNIT` / `ACK_NO_BUS` / `ACK_NO_TRK0`) —
exactly as the real firmware would.

Three layers, per `.claude/agents/hardware-emulation-author.md`:

| Layer | File | Purpose |
|---|---|---|
| Wire protocol | `firmware_state_machine.c::gw_fw_build_packet` | Emits `[CMD, LEN, params...]` packets identical to `uft_gw_command()` |
| Firmware state machine | `firmware_state_machine.{h,c}` | 10 states + per-command transitions matching keirf/greaseweazle v1.23 |
| Flux generation | `tests/flux_gen/greaseweazle/flux_gen.{h,c}` | Synthetic GW USB stream bytes (1-byte / 2-byte / opcode encoding) with opt-in defect injection |

## Non-scope

- **Write path** — `Cmd.WriteFlux` and `Cmd.EraseFlux` are ALWAYS refused
  (`ACK_WRPROT`). Forensic-safety guard: we never emulate writes in tests
  because emulated-but-buggy writes would mask real-HW write bugs the
  bench session needs to catch. The production HAL DOES write; this
  emulator is for the read pipeline and error-handling tests only.
- **USB transport / serial-port physics** — the emulator is invoked
  directly by the test, with no `libusb` or serial-port between them.
  Transport semantics (latency, packet fragmentation, baud-rate reset)
  are HAL-internal concerns.
- **Magnetic-medium physics** — no aging, no fluxometric Gaussian noise,
  no cross-track bleed. The flux generator emits clean intervals + the
  defects in `flux_gen.h::uft_gw_defect_flags_t`, nothing else.
- **Other controllers** — XUM1541, Applesauce etc. each get their own
  invocation per the agent contract.

## Reference

- Production C-HAL (ground truth): [`src/hal/uft_greaseweazle_full.c`](../../../src/hal/uft_greaseweazle_full.c)
- HAL header (opcodes + ACKs): [`include/uft/hal/uft_greaseweazle_full.h`](../../../include/uft/hal/uft_greaseweazle_full.h)
- Upstream firmware reference: [keirf/greaseweazle](https://github.com/keirf/greaseweazle)
  v1.23 (`src/greaseweazle/usb.py`, `flux.py`)

## Build + run

The test binary `test_greaseweazle_emulator` is wired into the main
`tests/CMakeLists.txt`. Build:

```bash
cmake -B build
cmake --build build --target test_greaseweazle_emulator
./build/tests/test_greaseweazle_emulator       # on Windows: .exe
```

Or via ctest:

```bash
cd build
ctest -R test_greaseweazle_emulator -V
```

## Forensic honesty

- Every byte this emulator emits is traceable to the synthetic-flux
  generator (deterministic xorshift64* seed) OR a documented packet
  builder. No silent fabrication.
- The flux generator REFUSES to emit out-of-spec intervals (<2 µs or
  >10 ms) — this guard exists so that if a future path ever fed
  generated flux back to real hardware, the medium would not be
  damaged.
- The production HAL is referenced but NEVER MODIFIED by this emulator
  (per `.claude/CLAUDE.md` protected-paths rule).
- Sim-vs-real divergences are listed in [`DIVERGENCES.md`](DIVERGENCES.md).
  Coverage breakdown in [`coverage_matrix.md`](coverage_matrix.md).
