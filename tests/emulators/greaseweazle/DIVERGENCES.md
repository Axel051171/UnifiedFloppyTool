# Greaseweazle Emulator — Sim-vs-Real-HW Divergences

This is the honest list of every place this emulator simplifies away
behaviour the real Greaseweazle firmware exhibits. Forensic-honesty
rule (see `.claude/agents/hardware-emulation-author.md` §Hard rules):
no silent simplifications.

For each entry: **what real HW does → what sim does → why we accept →
how to detect the gap**.

---

## D-1: Sample clock is exact in sim

- **Real HW:** crystal-controlled 72 MHz (F7) or 84 MHz (F7-Plus)
  oscillator with ±50 ppm tolerance. Two captures of the same disk on
  two GW units can disagree by a few ticks per sample.
- **Sim:** every "tick" is exactly `1 / sample_freq_hz` seconds. Two
  simulated captures are byte-identical.
- **Why accept:** the tolerance is orders of magnitude smaller than the
  decoder's PLL bandwidth; no realistic decoder failure is masked.
- **Detection:** a real-HW bench session reading the same disk twice
  on the same GW will produce slightly different tick values within
  noise — sim will not.

---

## D-2: USB transport not exercised

- **Real-HW path:** every CMD has to traverse libusb / USB-CDC-ACM →
  STM32 firmware → drive → back. Transport adds packet fragmentation,
  baud-rate resets (the `ClearComms` 10000-baud trick), and rare
  byte loss on flaky cables (which the HAL retries via
  `GW_READ_RETRIES = 5`).
- **Sim:** the firmware-state-machine is invoked directly by the test.
  There is no libusb path; `Cmd.GetInfo` returns the 32-byte struct
  in-memory, not over a serial port.
- **Why accept:** separation of concerns. The HAL itself exercises the
  serial-port path on real hardware (production-tested). The emulator
  is for firmware-state-machine correctness only.
- **Detection:** none — this is an architectural split, not a wrong
  result. The HAL's `serial_*` helpers are the only place that
  touches the wire.

---

## D-3: GetInfo subindex 7 (CurrentDrive) not modelled

- **Real HW:** `GW_GETINFO_CURRENT_DRV (7)` returns the currently-
  selected drive's index — used by some HAL paths to verify selection.
- **Sim:** only `GW_GETINFO_FIRMWARE (0)` is implemented. Subindex 7
  (and `BW_STATS=1`) return `ACK_BAD_COMMAND`.
- **Why accept:** the production HAL only invokes subindex 0 inside
  `uft_gw_get_info()`. Other subindices are unused on the read path.
- **Detection:** if a future HAL change starts using subindex 7, this
  test will catch the unsupported case immediately — by failing.
- **Next step:** implement subindex 7 when it joins the HAL's
  read-path needs.

---

## D-4: Motor unit-mismatch is strict; real HW may be permissive

- **Real HW:** `Cmd.Motor(unit, on)` accepts a unit parameter. The
  firmware probably allows control of any unit's motor independently
  of which is "selected" (the bus has multiple device-select lines).
- **Sim:** we refuse `Cmd.Motor(unit, on)` if `unit != selected_unit`
  with `ACK_BAD_UNIT`.
- **Why accept:** the UFT HAL always passes the selected unit, so
  enforcing the stricter rule catches a "did the HAL forget to
  re-select before motor?" regression sooner.
- **Detection:** if a multi-drive bench session ever uses both drive 0
  and drive 1 simultaneously, this sim would refuse where real HW
  allows. The HAL doesn't currently support that pattern.

---

## D-5: SetPin is always refused

- **Real HW:** `Cmd.SetPin` allows the host to drive output pins
  (used in advanced setups like serial test mode).
- **Sim:** always returns `ACK_BAD_PIN` regardless of input.
- **Why accept:** the production HAL never calls `Cmd.SetPin`; it
  only reads pins via `Cmd.GetPin(TRK0, WRPROT)`. Modelling
  arbitrary writable pins would require a per-pin state vector that
  no test currently needs.
- **Detection:** any future HAL path that uses SetPin will fail this
  emulator's tests immediately — honest fail, not silent success.

---

## D-6: Bus-type auto-recovery missing from firmware layer

- **Real HW:** firmware refuses `Cmd.Select` with `ACK_NO_BUS` if no
  bus type was set. The HAL has an auto-recovery in
  `uft_gw_select_drive()` that sets bus type to IBM_PC and retries.
- **Sim:** the FIRMWARE LAYER refuses with `ACK_NO_BUS` and STAYS
  in ERROR; there is no auto-retry inside the emulator. Tests must
  call `gw_fw_cmd_set_bus_type()` explicitly first.
- **Why accept:** the auto-recovery is HAL-level glue, not firmware
  behaviour. Modelling it inside the emulator would conflate the
  layers.
- **Detection:** any test that wants to assert "the HAL's auto-bus-set
  trick works" must wrap the emulator at a higher level (HAL-mock
  pattern). Out of scope for this emulator.

---

## D-7: No motor spin-up delay modelled

- **Real HW:** after `Cmd.Motor(on)`, the firmware waits for the
  spindle to reach speed (typically 500 ms via `motor_delay_ms`)
  before subsequent reads succeed. The HAL sleeps for that duration.
- **Sim:** `Cmd.Motor(on)` returns OK immediately and `Cmd.ReadFlux`
  is then accepted without any delay.
- **Why accept:** modelling time would require a clock abstraction
  that no test currently needs. The HAL's sleep is real and
  preserved; the emulator's role is the firmware reply, not the
  physical drive's spin-up timing.
- **Detection:** stress test that issues `ReadFlux` immediately after
  `Motor(on)` on real HW will return `ACK_NO_INDEX` (motor not yet
  spinning); sim will succeed.

---

## D-8: Index pulse jitter only modelled in flux-gen, not firmware

- **Real HW:** mechanical photo-interrupter; index timing jitters
  ±0.1-0.5% revolution-to-revolution.
- **Sim:** the firmware state machine reports an exact index period
  (whatever the flux-gen encoded). The `UFT_GW_DEFECT_INDEX_JITTER`
  flag in the flux-gen DOES introduce ±0.5% jitter into the encoded
  Index opcodes — but the firmware emulator itself does not
  independently jitter them.
- **Why accept:** consistent layering — flux-gen owns the timing
  model, firmware owns the state model.
- **Detection:** running the flux-gen output through the production
  PLL gives the same per-rev confidence; real flux gives slightly
  varying confidence per rev. Bench session will show this.

---

## D-9: NFA (no-flux-area) modelled in flux-gen only, decode untested

- **Real HW:** firmware writes Space+Astable opcode pairs to mark
  no-flux areas during long-format PC reads. The HAL's
  `uft_gw_decode_flux_stream` decodes these by accumulating ticks
  but emitting no transition.
- **Sim flux-gen:** `UFT_GW_DEFECT_NFA_BURST` emits the correct
  opcode pair (verified bit-for-bit against the HAL's encoder).
- **Sim firmware:** never autonomously inserts NFA opcodes; they
  only appear in flux-gen output.
- **Why accept:** the firmware emulator just streams whatever was
  loaded via `gw_fw_load_read_stream`. NFA-generation is the
  flux-gen's concern.
- **Detection:** the existing test `flux_gen_nfa_burst_emits_*`
  asserts opcode presence. A full firmware-level NFA-during-read
  test would require modelling on-the-fly opcode injection, which
  is firmware-internal and untestable without the silicon.

---

## D-10: Flux-generator omits Gaussian magnetic noise

- **Real-HW captured flux:** every interval has a small (~0.5%)
  Gaussian noise on timing — a side-effect of the head amplifier
  and PLL preamp.
- **Sim:** clean MFM generator produces exact 8/12/16 µs intervals
  (or the weak-bits jittered versions). No Gaussian noise.
- **Why accept:** clean intervals are what the test author wants —
  they match the decoder's NOMINAL input. Adding Gaussian noise
  would couple the test's pass/fail to the decoder's noise
  tolerance, which is a decoder test concern not a flux-gen concern.
- **Detection:** running the generator's output through the
  production PLL gives a flat-perfect confidence map; real flux
  gives a varying one.

---

## D-11: No Greaseweazle bootloader-mode response modelled

- **Real HW:** if `is_main_fw == 0`, the bootloader firmware still
  answers `GetInfo` but rejects every other command. The HAL refuses
  to open such a device (`UFT_GW_ERR_BOOTLOADER`).
- **Sim:** `gw_fw_set_bootloader_mode(true)` makes GetInfo return
  `is_main_fw = 0`, but the emulator does NOT then refuse other
  commands — they proceed as if main firmware were running.
- **Why accept:** the HAL's rejection happens at open-time, before
  any state-machine command is issued. The emulator's role is to
  test post-open behaviour.
- **Detection:** the test `bootloader_mode_flagged_in_info` verifies
  the `is_main_fw=0` flag. Anything beyond that is HAL-policy, not
  firmware behaviour.

---

## Coverage summary

| Layer | Coverage vs real-HW |
|---|---|
| Wire-protocol opcodes (read path) | 100% of the 11 opcodes UFT's read-flux path uses |
| Wire-protocol opcodes (write path) | 0% (intentionally refused — forensic safety) |
| Wire-protocol opcodes (full GW set) | ~75% (Drive A modelled; Drive B / TestMode / MemRead absent) |
| Firmware state-machine | ~90% (10 states + transitions; missing: motor spin-up timing, real-HW NFA-during-read) |
| Flux generation (clean MFM) | ~80% (correct format, deterministic, F7 + F7+; missing: Gaussian noise) |
| Flux defect classes | 5 classes (weak_bits, crc, nfa_burst, index_jitter, long_track); coverage ~70% (no Rob Northen, no copylock yet) |
| Capability-flag claim | "SIMULATED (FIRMWARE-REALISTIC, ~85% aggregate)" |

These numbers are estimates from feature-by-feature review against
keirf/greaseweazle v1.23 and the production HAL. Calibration against
the only production HAL UFT has = aggregate ~85% (vs SCP's ~80%) because
the GW HAL itself is ground-truth — every divergence above is
genuinely measurable against silicon, not against another reference
implementation.
