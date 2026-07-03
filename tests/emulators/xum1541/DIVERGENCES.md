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
- **RESOLVED** — the delta was answered by the MF-301 OpenCBM source
  audit; kept for the forensic record

---

## X-DELTA-1: HAL read 1 status byte; OpenCBM reference reads 3 — **RESOLVED (MF-301), was HIGH**

- **Was:** the pre-MF-301 UFT HAL read a **1-byte** status after bulk
  commands; OpenCBM's `xum1541_wait_status()` reads a **3-byte** buffer
  (`XUM_STATUSBUF_SIZE = 3`): `[status, val_lo, val_hi]`.
- **Resolution:** the MF-301 line-by-line OpenCBM source audit
  confirmed OpenCBM is correct. The HAL was rewritten:
  `src/hal/uft_xum1541.c::xum_wait_status()` now reads
  `UFT_XUM1541_STATUSBUF_SIZE` (3) bytes, loops on `STATUS_BUSY`, and
  extracts the little-endian extended value via
  `UFT_XUM1541_GET_STATUS_VAL`.
- **Sim:** modelled the 3-byte form from the start
  (`xum_fw_status_serialize`, pinned by test
  `status_buf_serialization_is_3_bytes_le`, which now also cross-checks
  the HAL accessor macros). Sim and HAL now agree.

---

## X-DELTA-2: WRITE/READ bulk header layout vs OpenCBM mode byte — **RESOLVED (MF-301), was HIGH**

- **Was:** the pre-MF-301 HAL sent `[opcode, len_lo, len_hi, 0]`;
  OpenCBM sends `[opcode, proto|flags, size_lo, size_hi]` — real
  firmware would have parsed `len_lo` as the protocol byte.
- **Resolution:** OpenCBM is correct. The HAL was rewritten
  (`xum_cbm_write`, `uft_xum_iec_read`) to the verified header layout
  with the protocol byte (`UFT_XUM1541_PROTO_*` upper nibble,
  `UFT_XUM1541_FLAG_WRITE_*` lower nibble).
- **Sim:** rewritten in this invocation to parse exactly that layout;
  unknown protocol nibbles are rejected with a sticky error
  (`unknown_proto_nibble_rejected_sticky`).

---

## X-DELTA-3: The pre-MF-301 UFT opcode table was FICTIONAL — **RESOLVED (MF-301), was HIGH**

- **What was wrong:** the old UFT constants declared bulk opcodes
  `WRITE_DATA=0, TALK=1, LISTEN=2, UNLISTEN=3, UNTALK=4, READ_DATA=7,
  OPEN=8, CLOSE=9`. None of these exist in the real firmware. Worse,
  the invented `OPEN=8`/`CLOSE=9` **collided with the real
  `READ=8`/`WRITE=9`** — an "open file" against real silicon would
  have triggered a bus READ.
- **Ground truth (OpenCBM xum1541_types.h + archlib.c):** the only
  bulk data opcodes are `XUM1541_READ=8` and `XUM1541_WRITE=9`. IEC
  addressing is NOT separate opcodes: `listen(dev,sec)` = WRITE with
  proto `CBM|ATN` and payload `{0x20|dev, 0x60|sec}`; `talk` = WRITE
  `CBM|ATN|TALK` with `{0x40|dev, 0x60|sec}`; `unlisten` = `{0x3F}`;
  `untalk` = `{0x5F}`.
- **Resolution:** HAL header + implementation rewritten (MF-301); this
  emulator rewritten to match. The fictional opcodes are now actively
  rejected: `unknown_opcode_rejected_sticky` sends every pre-MF-301
  fictional opcode value and asserts a sticky `BAD_OPCODE` error.
- **Forensic lesson:** the earlier emulator invocation modelled the
  fictional table because it treated the in-repo header as SSOT and
  only *flagged* the OpenCBM mismatch (X-DELTA-1/2) instead of
  treating the upstream source as authoritative. The
  DIVERGENCES-driven flags are what triggered the MF-301 audit —
  the process worked, but a full opcode-table cross-audit should have
  been part of the first invocation.

---

## X-D1: IOCTL transport — bulk vs "control transfer" header comment — **RESOLVED (MF-301), was MEDIUM**

- **Was:** the UFT header comment described ioctls as control
  transfers. OpenCBM sends them as 4-byte **bulk** commands
  `[cmd, arg1, arg2, 0]` with the result in the status extended value.
- **Resolution:** header comment fixed (MF-301); the HAL's
  `xum_iec_command()` + `uft_xum_iec_poll()` implement the bulk form.
  Sim models the same; `iec_poll_returns_line_mask_in_status_value`
  round-trips the result through the HAL accessor macros.

---

## X-D2: Drive-internal DOS not modelled — **Severity: MEDIUM**

- **Real HW:** the 1541 is a computer. A real track read involves
  uploading a fastloader/nibbler routine (`M-W`), executing it (`M-E`),
  and the drive's 6502 streaming GCR over the parallel cable. Error
  channel (secondary 15) returns DOS status strings ("00, OK,00,00").
  OPEN/CLOSE ATN bytes (0xF0|sec / 0xE0|sec) trigger drive-side file
  handling.
- **Sim:** a talk-ATN + READ sequence drains a pre-generated GCR
  stream loaded by the test. OPEN/CLOSE ATN bytes are clocked through
  and counted (as the firmware does) but have no drive-side effect.
  No job queue, no DOS error channel, no ROM signatures (which
  `uft_xum_identify_drive` will eventually probe via `M-R`).
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

## X-D4: Error-reporting phase simplified for data commands — **Severity: LOW**

- **Real HW:** for a plain data WRITE the wire order is cmd → payload
  → status; a bus problem (no listener) is only reported at the status
  phase, after the firmware has swallowed the payload. A READ with no
  talker does not "error" at all — the firmware clocks a dead bus and
  the transfer times out at the USB layer.
- **Sim:** a data WRITE without a listener and a READ without a talker
  return `STATUS_ERROR` immediately at the command phase (sticky).
- **Why accept:** the deferred-error case would require the test to
  stream a payload nobody will consume (WRITE) or model USB timeouts
  (READ); the observable outcome — loud failure + sticky state — is
  strictly stricter than real HW and catches HAL sequencing bugs
  earlier. ATN-write errors (absent device, role conflict) ARE
  reported at the correct phase (payload/status) with the correct
  partial byte count.
- **Detection:** a byte-exact USB trace of an error case would show
  the status arriving one phase later (WRITE) or a bus hang (READ)
  where the sim errors immediately.

---

## X-D5: Strict bus-role switching — **Severity: LOW**

- **Real HW:** ATN is always under host control; a talk-ATN sequence
  while the drive is listener simply re-addresses the bus. Real
  firmware likely tolerates role switches without UNLISTEN/UNTALK.
- **Sim:** a listen-ATN byte while TALKING (and vice versa) → sticky
  `BUS_ROLE_CONFLICT` at that ATN byte.
- **Why accept:** deliberately stricter than real HW (same stance as
  the GW emulator's motor-unit strictness): it catches "HAL forgot to
  release the bus" regressions at the earliest point. Re-addressing
  the SAME role is allowed (matches real ATN).
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
  state explicitly via `xum_fw_set_iec_lines`. The BUSY answer matches
  the real busy-loop protocol shape (the HAL's `xum_wait_status` loops
  on BUSY), just without elapsed time.
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
  taken from OpenCBM `xum1541_types.h` — the MF-301 audit verified the
  bulk/ioctl/status protocol but did NOT re-verify the INIT payload
  byte layout against a live device. Promicro/ZoomFloppy variants
  report different capability sets.
- **Sim:** implements exactly that layout with defaults
  `version=8, caps=CAP_CBM|CAP_NIB`.
- **Why accept:** best-effort with documented source; the UFT HAL
  currently ignores the INIT payload beyond success/failure, so
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

## X-D11: (retired) OPEN_FILE / CLOSE_FILE bulk opcodes — **superseded by X-DELTA-3**

The opcodes this entry documented never existed on real hardware
(fictional table, X-DELTA-3). Real IEC OPEN/CLOSE are ATN payload
bytes (0xF0|sec / 0xE0|sec); the sim clocks them through and counts
them, with drive-side file semantics out of scope per X-D2. Test:
`open_close_atn_bytes_clocked_through`.

---

## X-D12: Talk-ATN without FLAG_WRITE_TALK is a sticky sim error — **Severity: LOW**

- **Real HW:** `XUM_WRITE_TALK` tells the firmware to perform the bus
  turnaround (host releases, drive takes over as talker) after the ATN
  bytes. A talk-address ATN write without the flag would leave the bus
  without a turnaround — the subsequent READ hangs until timeout.
- **Sim:** rejects the talk-ATN byte with sticky `BAD_PROTOCOL`.
- **Why accept:** a hang is not expressible in a clockless sim; the
  loud sticky error catches a HAL that forgets the flag (the failure
  mode that matters) at the earliest point.
- **Detection:** bench trace would show a timeout instead of an error
  status. Test: `talk_atn_without_talk_flag_is_sticky_bad_protocol`.

---

## X-D13: Only the CBM protocol is modelled; S1/S2/PP/P2/NIB + PARBURST refused — **Severity: MEDIUM**

- **Real HW:** the firmware implements the serial-1 (S1), serial-2
  (S2), parallel (PP), 2-byte-parallel (P2) and burst-nibbler (NIB)
  transfer protocols, plus PARBURST_READ/WRITE ioctls — all of which
  require a matching drive-side routine uploaded via `M-W`/`M-E`.
- **Sim:** protocol nibbles S1..NIB and the PARBURST ioctls return an
  honest transient `NOT_MODELLED` error (never a wedge, never fake
  success). Only PROTO_CBM has full semantics.
- **Why accept:** the UFT HAL currently only emits PROTO_CBM; the
  other protocols are meaningless without the drive-side 6502 routine
  (X-D2). Modelling them would be confabulation.
- **Detection:** when the HAL grows nibbler support (M3.2 follow-up),
  these tests fail loudly and the sim must be extended. Tests:
  `unmodelled_real_protos_refused_transiently`, `parburst_refused_honestly`.

---

## Coverage summary

| Layer | Coverage vs real-HW |
|---|---|
| Control transfers | 7/9 modelled + 2 honest-refused (bootloader, tap) |
| Bulk data commands | 2/2 real opcodes (READ/WRITE); CBM proto full, 5 real protos honest-refused (X-D13) |
| IEC ATN addressing | listen/talk/unlisten/untalk/secondary/open/close via ATN payload parsing |
| IOCTLs | 9/9 exercised (7 modelled, 2 honest-refused PARBURST) |
| Firmware state machine | ~85% (7 states; missing: time model, deferred data-write status phase) |
| GCR generation | ~75% (7 defect classes; missing: formatted half-tracks, per-read weak-bit variance, bit-shifted off-sync reads) |
| Drive-internal DOS | 0% — out of scope, see X-D2 |
| Capability-flag claim | "SIMULATED (FIRMWARE-REALISTIC, ~80% aggregate)" |

Post-MF-301 the protocol layer is grounded in a line-verified OpenCBM
source audit (X-DELTA-1/2/3 all RESOLVED); the remaining honest gaps
are the INIT payload layout (X-D9), the unmodelled transfer protocols
(X-D13) and everything drive-side (X-D2). Real-HW bench still catches
the remaining ~20% (IEC timing margins, drive DOS choreography, INIT
payload layout).
