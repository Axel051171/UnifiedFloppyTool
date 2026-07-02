# XUM1541 Emulator — Coverage Matrix

Quantified per-behaviour coverage of the real XUM1541/ZoomFloppy
firmware (OpenCBM reference; SPEC_STATUS: REVERSE-ENGINEERED — no
official SDK). "Covered" means this emulator can produce or react to
the behaviour in a test.

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

## Wire protocol — bulk commands

| Opcode | Value | Covered | Test / Reason |
|---|---|---|---|
| `BULK_WRITE_DATA` | 0 | PARTIAL | `listen_then_write_payload_ok`, `write_payload_without_listen_refused`, `write_length_overrun_is_sticky_error`, `write_payload_length_mismatch_is_sticky_error`; status-phase timing simplified (X-D4); header layout under X-DELTA-2 |
| `BULK_TALK` | 1 | YES | `talk_then_read_payload_drains_stream`, `listen_wrong_device_number_is_transient_error` |
| `BULK_LISTEN` | 2 | YES | `listen_then_write_payload_ok`, `listen_to_absent_device_is_transient_error`, `listen_while_talking_is_sticky_role_conflict` |
| `BULK_UNLISTEN` | 3 | YES | `unlisten_returns_to_ready`, `unlisten_when_idle_is_legal_noop` |
| `BULK_UNTALK` | 4 | YES | `untalk_returns_to_ready` |
| `BULK_READ_DATA` | 7 | PARTIAL | `talk_then_read_payload_drains_stream`, `read_payload_without_talk_refused`, `e2e_*`; same X-D4/X-DELTA-2 caveats |
| `BULK_OPEN_FILE` | 8 | PARTIAL (refusal only) | `open_close_file_refused_honestly` (X-D11) |
| `BULK_CLOSE_FILE` | 9 | PARTIAL (refusal only) | `open_close_file_refused_honestly` |
| `IOCTL_GET_EOI` | 23 | YES | `get_eoi_clear_eoi_roundtrip` |
| `IOCTL_CLEAR_EOI` | 24 | YES | `get_eoi_clear_eoi_roundtrip` |
| `IOCTL_IEC_POLL` | 27 | YES | `iec_poll_reports_line_mask` |
| `IOCTL_IEC_WAIT` | 28 | PARTIAL | `iec_wait_ready_on_match_busy_otherwise`; no time model (X-D7) |
| `IOCTL_IEC_SETRELEASE` | 29 | YES | `iec_setrelease_updates_lines` |
| Status phase (3-byte buf) | — | YES | `status_buf_serialization_is_3_bytes_le`; HAL delta flagged (X-DELTA-1) |

**Total: 13/13 documented opcodes exercised (9 modelled, 2 partial-with-caveat, 2 honest-refused) = 100% of the UFT-header opcode set.**

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
| LISTEN/TALK address validation (0..30) | YES |
| Absent-device timeout is transient (non-wedging) | YES |
| Strict role-conflict detection | YES (stricter than real, X-D5) |
| WRITE only while LISTENING / READ only while TALKING | YES |
| FIFO overrun (announced len > buffer) | YES |
| Announced-vs-delivered length mismatch | YES |
| EOI on stream exhaustion + GET_EOI/CLEAR_EOI | PARTIAL (boolean model, X-D3) |
| Bootloader-entry refusal | YES |
| Deferred status phase for write errors | NO (X-D4) |
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
| Wire protocol (control + bulk + status) | 100% of UFT-header opcodes exercised |
| Firmware state machine | ~85% (in-scope behaviours) |
| GCR generation | ~75% |
| Drive-internal DOS | 0% (out of scope, documented) |
| **Aggregate vs real-HW behaviour** | **~80%** |

Grounding note: unlike Greaseweazle (production HAL = ground truth),
the XUM1541 numbers rest on the OpenCBM source reference plus the
MF-255-audited UFT header. Two unresolved HAL-vs-reference deltas
(DIVERGENCES.md X-DELTA-1, X-DELTA-2) cap honest confidence until the
M3.2 bench session answers them. Real HW still catches the remaining
~20% (IEC timing margins, drive DOS choreography, INIT payload layout).
