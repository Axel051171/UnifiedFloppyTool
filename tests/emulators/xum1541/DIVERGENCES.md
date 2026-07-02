# XUM1541 Emulator — Sim-vs-Real-HW Divergences

Honest list of every place this emulator simplifies away behaviour the
real XUM1541/ZoomFloppy firmware (and the attached Commodore drive)
exhibits. Forensic-honesty rule: no silent simplifications.

For each entry: **what real HW does → what sim does → why we accept →
how to detect the gap**, plus a **severity** for bench-session triage:

- **HIGH** — could hide a real-HW failure of the UFT HAL; must be
  resolved before/at the M3.2 bench session
- **MEDIUM** — behavioural gap; bench session will show a difference
  but the HAL's correctness is unlikely to depend on it
- **LOW** — cosmetic or deliberately-strict simplification

---

## X-DELTA-1: HAL reads 1 status byte; OpenCBM reference reads 3 — **Severity: HIGH**

This is a HAL-vs-upstream-reference delta the emulator SURFACED, not a
sim simplification. Recorded here because the sim had to pick a side.

- **OpenCBM reference:** after a bulk command, `xum1541_wait_status()`
  reads a **3-byte** status buffer (`XUM_STATUSBUF_SIZE = 3`):
  `[status, val_lo, val_hi]` — the 16-bit value carries transfer counts.
- **UFT HAL (`src/hal/uft_xum1541.c::xum_iec_command`,
  `uft_xum_iec_write`):** reads **1** byte via
  `xum_bulk_in(cfg, &status, 1, ...)`. If real firmware sends a 3-byte
  packet, libusb typically fails the 1-byte request with
  `LIBUSB_ERROR_OVERFLOW` — the IEC path would fail on first contact
  with silicon.
- **Sim:** models the OpenCBM-documented 3-byte form
  (`xum_fw_status_serialize`, pinned by test
  `status_buf_serialization_is_3_bytes_le`).
- **Why accepted in sim:** the upstream firmware source is the only
  status-format authority available; mirroring the HAL's 1-byte read
  would have baked a suspected bug into the test layer.
- **Detection:** first bench-session bulk command will answer this
  conclusively. Until then this is an OPEN question for M3.2 wiring —
  do NOT treat either side as verified.

---

## X-DELTA-2: WRITE/READ bulk header layout vs OpenCBM mode byte — **Severity: HIGH**

- **OpenCBM reference:** `xum1541_write()` sends
  `[XUM1541_WRITE, mode, len_lo, len_hi]` — byte 1 is a protocol/mode
  selector (serial / parallel-nibbler / ...), length in bytes 2..3.
- **UFT HAL:** sends `[opcode, len_lo, len_hi, 0]` — no mode byte,
  length shifted one position earlier. Real firmware would parse
  `len_lo` as the mode.
- **Sim:** follows the **UFT header layout** (the in-repo MF-255-audited
  SSOT), because the emulator's contract is to test the HAL against its
  documented protocol; the OpenCBM discrepancy is flagged here instead
  of silently "fixed".
- **Detection:** bench session, or a line-by-line re-audit of
  OpenCBM `xum1541.c` before M3.2 wiring continues. One of the two
  layouts is wrong on real HW.

---

## X-D1: IOCTL transport — bulk in sim, "control transfer" in the UFT header comment — **Severity: MEDIUM**

- **OpenCBM reference:** ioctls (GET_EOI/CLEAR_EOI/IEC_POLL/IEC_WAIT/
  SETRELEASE) are sent as 4-byte **bulk** commands; results come back
  in the 16-bit status value (`XUM_GET_STATUS_VAL`).
- **UFT header comment** (`uft_xum1541.h:101`): describes them as
  control transfers ("bRequest = 16+sub via control xfer") — which also
  conflicts with the constants already including the base (23..29).
- **Sim:** models the OpenCBM bulk form.
- **Why accept:** the HAL implements none of these ioctls yet; nothing
  is masked either way.
- **Detection:** when M3.2 implements ioctls, the header comment must
  be reconciled with whichever transport real firmware accepts.

---

## X-D2: Drive-internal DOS not modelled — **Severity: MEDIUM**

- **Real HW:** the 1541 is a computer. A real track read involves
  uploading a fastloader/nibbler routine (`M-W`), executing it (`M-E`),
  and the drive's 6502 streaming GCR over the parallel cable. Error
  channel (secondary 15) returns DOS status strings ("00, OK,00,00").
- **Sim:** TALK + READ_DATA drains a pre-generated GCR stream loaded by
  the test. No job queue, no DOS error channel, no ROM signatures
  (which `uft_xum_identify_drive` will eventually probe via `M-R`).
- **Why accept:** modelling the drive DOS is a 6502-emulator project,
  not a controller emulator. The GCR *payload* semantics are fully
  covered by the generator; the *transport* semantics by the state
  machine. The gap is the command choreography in between.
- **Detection:** `uft_xum_read_track_gcr` (currently honest-stub) will
  need choreography this sim cannot verify — bench-only.

---

## X-D3: IEC bit-level timing not modelled — **Severity: MEDIUM**

- **Real HW:** ATN handshake, ~70 µs bit cells, EOI signalled by an
  intra-byte timing gap (>200 µs before the last byte), ATN-response
  window of 1 ms. Cable length and drive ROM revisions shift margins.
- **Sim:** line states are an instantaneous byte mask
  (`xum_fw_set_iec_lines` / IEC_POLL / SETRELEASE); EOI is a boolean
  flag set when the talk stream is exhausted.
- **Why accept:** the XUM1541 firmware exists precisely to hide these
  timings from the host; the host-visible contract is command + status.
- **Detection:** timing-margin failures (long cables, weak pull-ups)
  will only ever appear on a bench.

---

## X-D4: WRITE/READ status-phase timing simplified — **Severity: LOW**

- **Real HW:** for WRITE_DATA the wire order is cmd → payload → status;
  a state violation is only reported at the status phase (after the
  firmware has swallowed the payload).
- **Sim:** a WRITE_DATA/READ_DATA command in the wrong bus state
  returns `STATUS_ERROR` immediately at the command phase.
- **Why accept:** the deferred-error case would require the test to
  stream a payload nobody will consume; the observable outcome (error
  + sticky state) is the same.
- **Detection:** a byte-exact USB trace of an error case would show
  the status arriving one phase later than the sim's.

---

## X-D5: Strict bus-role switching — **Severity: LOW**

- **Real HW:** ATN is always under host control; a TALK ATN sequence
  while the drive is listener simply re-addresses the bus. Real
  firmware likely tolerates role switches without UNLISTEN/UNTALK.
- **Sim:** LISTEN while TALKING (and vice versa) → sticky
  `BUS_ROLE_CONFLICT`.
- **Why accept:** deliberately stricter than real HW (same stance as
  the GW emulator's motor-unit strictness): it catches "HAL forgot to
  release the bus" regressions at the earliest point.
- **Detection:** a multi-role bench script would succeed on real HW
  where the sim refuses. The UFT HAL never issues that pattern.

---

## X-D6: CTRL_RESET requires prior INIT — **Severity: LOW**

- **Real HW:** unverified; OpenCBM only ever sends RESET after INIT,
  so the pre-INIT behaviour is undocumented.
- **Sim:** refuses RESET from CONNECTED (pre-INIT).
- **Why accept:** inferred from the only documented call order; a
  permissive model would invent behaviour with no source.
- **Detection:** trivial bench probe (RESET before INIT) if it ever
  matters.

---

## X-D7: IEC_WAIT has no time model — **Severity: LOW**

- **Real HW:** IEC_WAIT blocks inside the firmware until the line
  condition is met (or a timeout fires).
- **Sim:** condition already true → READY; otherwise → `STATUS_BUSY`
  immediately. No clock exists in the emulator.
- **Why accept:** same clockless stance as GW D-7; tests control line
  state explicitly via `xum_fw_set_iec_lines`.
- **Detection:** any test needing "line changes while waiting" cannot
  be expressed — that is bench/transport territory.

---

## X-D8: Weak bits are a deterministic single snapshot — **Severity: MEDIUM**

- **Real HW:** a weak/unformatted region returns DIFFERENT bytes on
  every revolution — that instability IS the copy-protection check.
- **Sim/generator:** `UFT_XUM_DEFECT_WEAK_BITS` produces ONE
  deterministic corruption (CI reproducibility beats per-read
  variance). Additionally, RNG bytes of 0xFF are mapped to 0x7F so no
  fake sync run can appear.
- **Why accept:** determinism is a hard rule of this test layer. A
  multi-read weak-bit test can call the generator with N different
  seeds to model N revolutions.
- **Detection:** any consumer that verifies "weak bits differ between
  consecutive reads of the same capture" will see identical bytes.

---

## X-D9: INIT info layout + capability values are best-effort — **Severity: MEDIUM**

- **Real HW:** the 8-byte INIT response layout (version @0,
  capabilities @1, status flags @2) and the capability/flag values are
  taken from OpenCBM `xum1541_types.h` from memory of the source —
  NOT verified against a live device or a vendored copy in this repo.
  Promicro/ZoomFloppy variants report different capability sets.
- **Sim:** implements exactly that layout with defaults
  `version=8, caps=CAP_CBM|CAP_NIB`.
- **Why accept:** best-effort reverse-engineering is documented as
  such; the UFT HAL currently ignores the INIT payload entirely, so
  nothing downstream depends on the layout yet.
- **Detection:** first bench INIT dump; also flagged for verification
  when `uft_xum_open` starts parsing device info.

---

## X-D10: Half-track crosstalk is an approximation — **Severity: MEDIUM**

- **Real HW:** at track N+0.5 the head reads a superposition of the two
  adjacent tracks' magnetisation; the result depends on write history,
  azimuth and signal amplitude — partially decodable structure from
  BOTH neighbours can appear.
- **Sim/generator:** clean track N data with ~30% deterministic byte
  corruption and every second sync run wiped.
- **Why accept:** the consumer-visible property tests need — "GCR
  stream with degraded-but-nonzero structure, reproducible" — is
  achieved without a magnetic superposition model (which the agent
  contract explicitly excludes).
- **Detection:** protection schemes that store VALID data on half
  tracks (e.g. some V-MAX! variants) are not representable yet;
  a future defect class (`HALF_TRACK_FORMATTED`) would cover that.

---

## X-D11: OPEN_FILE / CLOSE_FILE refused — **Severity: LOW**

- **Real HW:** opens a named channel on the drive (ATN + secondary
  0xF0/0xE0 + filename bytes).
- **Sim:** always `STATUS_ERROR` with reason `NOT_MODELLED` (transient,
  non-wedging).
- **Why accept:** the UFT HAL never sends them; honest refusal beats
  fake success (same stance as GW `SetPin`).
- **Detection:** any future HAL path using file channels fails this
  emulator immediately — loud, not silent.

---

## Coverage summary

| Layer | Coverage vs real-HW |
|---|---|
| Control transfers | 7/9 modelled + 2 honest-refused (bootloader, tap) |
| Bulk commands (IEC path) | 6/8 modelled + 2 honest-refused (open/close file) |
| IOCTLs | 5/5 modelled (transport form per OpenCBM, see X-D1) |
| Firmware state machine | ~85% (7 states; missing: time model, deferred status phase) |
| GCR generation | ~75% (7 defect classes; missing: formatted half-tracks, per-read weak-bit variance, bit-shifted off-sync reads) |
| Drive-internal DOS | 0% — out of scope, see X-D2 |
| Capability-flag claim | "SIMULATED (FIRMWARE-REALISTIC, ~80% aggregate)" |

The ~80% aggregate is weaker-grounded than Greaseweazle's ~85% (no
production-tested HAL as ground truth) and comparable to SCP's ~80%:
the OpenCBM firmware source is authoritative for the protocol, but two
HAL-vs-reference deltas (X-DELTA-1/2) remain unresolved until silicon
answers. Real-HW bench catches the remaining ~20%.
