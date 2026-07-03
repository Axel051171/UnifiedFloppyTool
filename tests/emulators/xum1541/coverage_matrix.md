# XUM1541 Emulator — Coverage Matrix

Quantified per-behaviour coverage of the real XUM1541/ZoomFloppy
firmware. SPEC_STATUS: SOURCE-VERIFIED (MF-301) — the wire protocol was
line-verified against OpenCBM master (xum1541/xum1541_types.h,
opencbm/lib/plugin/xum1541/xum1541.c, archlib.c); no official ZoomFloppy
SDK exists. "Covered" means this emulator can produce or react to the
behaviour in a test.

Status legend:
- **YES** — fully modelled, tests exist
- **PARTIAL** — modelled with documented simplification (see DIVERGENCES.md)
- **NO** — not modelled, see "Reason"

## Wire protocol — control transfers

| bRequest | Value | Covered | Test / Reason |
|---|---|---|---|
| `CTRL_ECHO` | 0 | YES | `echo_legal_before_init` |
| `CTRL_INIT` | 1 | PARTIAL | `init_returns_8_byte_info_and_enters_ready`, `init_doing_reset_flag_reported_exactly_once`, `init_reports_no_device_flag_when_drive_absent`; info layout best-effort (X-D9) |
| `CTRL_RESET` | 2 | PARTIAL | `ctrl_reset_clears_sticky_error_and_bus_role`; pre-INIT behaviour inferred (X-D6) |
| `CTRL_SHUTDOWN` | 3 | YES | `shutdown_blocks_further_commands` |
| `CTRL_ENTER_BOOTLOADER` | 4 | PARTIAL (refusal only) | `enter_bootloader_always_refused` — bricking guard, never emulated |
| `CTRL_TAP_BREAK` | 5 | PARTIAL (refusal only) | Tape mode not modelled; honest refusal |
| `CTRL_GITREV` | 6 | YES | `version_strings_answered` |
| `CTRL_GCCVER` | 7 | YES | `version_strings_answered` |
| `CTRL_LIBCVER` | 8 | YES | `version_strings_answered` |

**Total: 9/9 control requests exercised (7 modelled, 2 honest-refused).**

## Wire protocol — bulk data commands (MF-301-verified: READ/WRITE only)

| Behaviour | Covered | Test / Reason |
|---|---|---|
| `WRITE(9)` header `[9, proto\|flags, size_lo, size_hi]` | YES | all group-B tests build this exact header |
| `WRITE` status = 3 bytes, extended value = IEC byte count | YES | `listen_then_data_write_reports_byte_count`, `atn_listen_sets_listening_and_secondary` |
| `READ(8)` header `[8, proto, size_lo, size_hi]` | YES | `talk_then_read_drains_stream_no_status_phase`, `e2e_*` |
| `READ` has NO status phase; short read = IEC EOI | YES | `e2e_short_read_reports_partial_len_and_eoi` |
| ATN addressing: listen = `{0x20\|dev, 0x60\|sec}` via CBM\|ATN write | YES | `atn_listen_sets_listening_and_secondary` |
| ATN addressing: talk = `{0x40\|dev, 0x60\|sec}` via CBM\|ATN\|TALK | YES | `atn_talk_sets_talking_and_secondary` |
| ATN: unlisten `{0x3F}` / untalk `{0x5F}` | YES | `atn_unlisten_returns_to_ready`, `atn_untalk_returns_to_ready`, `atn_unlisten_untalk_idle_is_legal_noop` |
| Device 31 unencodable (0x20\|31 == UNLISTEN) | YES | `atn_byte_0x3F_is_unlisten_not_device_31` |
| OPEN/CLOSE as ATN payload bytes (0xF0/0xE0\|sec) | PARTIAL | `open_close_atn_bytes_clocked_through`; drive-side semantics out of scope (X-D2) |
| Talk turnaround flag (`FLAG_WRITE_TALK`) enforced | PARTIAL | `talk_atn_without_talk_flag_is_sticky_bad_protocol`; real HW hangs instead of erroring (X-D12) |
| Protocol S1/S2/PP/P2/NIB | PARTIAL (refusal only) | `unmodelled_real_protos_refused_transiently` (X-D13) |
| Unknown protocol nibble rejected | YES | `unknown_proto_nibble_rejected_sticky` |
| Unknown opcode rejected (incl. all pre-MF-301 fictional opcodes) | YES | `unknown_opcode_rejected_sticky` (X-DELTA-3) |
| FIFO overrun (announced len > buffer) | YES | `write_length_overrun_is_sticky_error` |
| Announced-vs-delivered length mismatch | YES | `write_payload_length_mismatch_is_sticky_error` |
| Payload without command header | YES | `write_payload_without_command_is_sticky_error` |

## Wire protocol — IOCTLs (4-byte bulk `[cmd, arg1, arg2, 0]`, result in status value)

| IOCTL | Value | Covered | Test / Reason |
|---|---|---|---|
| `GET_EOI` | 23 | YES | `get_eoi_clear_eoi_roundtrip` |
| `CLEAR_EOI` | 24 | YES | `get_eoi_clear_eoi_roundtrip` |
| `PP_READ` | 25 | YES | `pp_write_read_roundtrip` |
| `PP_WRITE` | 26 | YES | `pp_write_read_roundtrip` |
| `IEC_POLL` | 27 | YES | `iec_poll_returns_line_mask_in_status_value` (result round-tripped through HAL `UFT_XUM1541_GET_STATUS_VAL`) |
| `IEC_WAIT` | 28 | PARTIAL | `iec_wait_ready_on_match_busy_otherwise`; no time model (X-D7) |
| `IEC_SETRELEASE` | 29 | YES | `iec_setrelease_updates_lines` |
| `PARBURST_READ` | 30 | PARTIAL (refusal only) | `parburst_refused_honestly` (X-D13) |
| `PARBURST_WRITE` | 31 | PARTIAL (refusal only) | `parburst_refused_honestly` |
| Status phase (3-byte buf, LE value) | — | YES | `status_buf_serialization_is_3_bytes_le` (X-DELTA-1 RESOLVED) |

**Total: 2/2 real bulk data opcodes + 9/9 IOCTLs exercised = 100% of the
MF-301-verified opcode set. CBM protocol fully modelled; 5 real transfer
protocols + 2 PARBURST ioctls honest-refused (X-D13).**

## Firmware state-machine coverage

| Behaviour | Covered |
|---|---|
| Power-on DISCONNECTED (no USB response) | YES |
| CONNECTED → READY via CTRL_INIT | YES |
| DOING_RESET flag reported exactly once | YES |
| NO_DEVICE flag when drive absent | YES |
| Bulk command before INIT → sticky error | YES |
| INIT recovers from sticky ERROR | YES |
| CTRL_RESET recovers + clears bus role + lines | YES |
| SHUTDOWN terminal state | YES |
| ATN-payload-driven LISTENING/TALKING transitions | YES |
| Secondary address captured from 0x60\|sec ATN byte | YES |
| Absent-device timeout is transient (non-wedging) + partial byte count | YES |
| Strict role-conflict detection (per ATN byte) | YES (stricter than real, X-D5) |
| Data WRITE only while LISTENING / READ only while TALKING | YES (error phase simplified, X-D4) |
| WRITE status extended value = IEC byte count | YES |
| EOI on stream exhaustion + GET_EOI/CLEAR_EOI | PARTIAL (boolean model, X-D3) |
| Bootloader-entry refusal | YES |
| Deferred status phase for data-write errors | NO (X-D4) |
| IEC bit-cell timing / ATN windows | NO (X-D3) |
| Drive-internal DOS (job queue, error channel, M-R/M-E) | NO (X-D2 — out of scope) |
| Concurrent command while BUSY | PARTIAL (IEC_WAIT returns BUSY; no async engine) |

**Total: 16/20 firmware behaviours modelled or partially modelled = ~80%; excluding the out-of-scope drive DOS: ~85%.**

## GCR-generation coverage

| Capability | Covered |
|---|---|
| Clean 1541 track (sync/header/gap/data layout) | YES |
| 4 density zones, correct sector counts | YES (cross-checked vs HAL SSOT) |
| Zone byte budgets (7692/7142/6666/6250) | YES |
| CBM GCR 4→5 encode + decode (all byte values) | YES |
| Header + data checksums (1541 DOS rules) | YES |
| Deterministic RNG (cross-platform identical) | YES |
| Medium-safety: stepper bounds (1..42, half ≤ 41.5) | YES |
| Medium-safety: budget overrun refusal | YES |
| `WEAK_BITS` | PARTIAL (deterministic snapshot, X-D8) |
| `CHECKSUM_ERROR` (DOS error 23) | YES |
| `SYNC_LONG` (40-byte run) | YES |
| `SYNC_SHORT` (sub-threshold) | YES |
| `KILLER_TRACK` (zero syncs) | YES |
| `DENSITY_MISMATCH` (zone-3 on inner track) | YES |
| `HALF_TRACK` (crosstalk) | PARTIAL (approximation, X-D10) |
| Formatted half-tracks (V-MAX!-style) | NO (v4.1.6+ candidate) |
| Per-revolution weak-bit variance | NO (X-D8; use N seeds) |
| Bit-shifted off-sync reads | NO (nibbler output is byte-synced; off-sync only matters for killer tracks) |
| Header interleave variants (custom formatters) | NO (sequential layout only) |

**Total: 15/19 payload capabilities = ~75%.**

## Aggregate

| Layer | Quantified |
|---|---|
| Wire protocol (control + bulk + ioctl + status) | 100% of the MF-301-verified opcode set exercised |
| Firmware state machine | ~85% (in-scope behaviours) |
| GCR generation | ~75% |
| Drive-internal DOS | 0% (out of scope, documented) |
| **Aggregate vs real-HW behaviour** | **~80%** |

Grounding note: post-MF-301 the protocol layer rests on a line-verified
OpenCBM source audit (X-DELTA-1/2/3 RESOLVED) — comparable grounding to
SCP's vendored-reference numbers, still below Greaseweazle's
production-HAL ground truth. Remaining honest caps: INIT payload layout
unverified on silicon (X-D9), 5 transfer protocols + PARBURST refused
(X-D13), drive DOS out of scope (X-D2). Real HW still catches the
remaining ~20% (IEC timing margins, drive DOS choreography, INIT
payload layout).
