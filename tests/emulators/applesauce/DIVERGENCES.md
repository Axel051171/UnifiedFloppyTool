# Applesauce Emulator — Divergences from Real Hardware

Forensic-honesty ledger. Every place this emulator is known to differ
from, or make an unverified assumption about, real Applesauce hardware.
`SPEC_STATUS: REVERSE-ENGINEERED` — there is no official Applesauce
protocol SDK. Severity: **HIGH** = could mask a real bug or break on
silicon; **MEDIUM** = plausible but unverified; **LOW** = cosmetic /
tolerance.

The in-tree protocol-truth hierarchy this emulator follows (strongest
first):
1. `src/hardware_providers/applesauce_serial_runners.cpp` (MF-250) — the
   executable host-side command vocabulary UFT drives hardware with.
2. `src/hardware_providers/applesauce_provider_v2.h` — status-char table
   from the V1 audit against wiki.applesaucefdc.com.
3. `docs/M3_APPLESAUCE_TRANSPORT.md` — conflicts with 1+2 (see D-1).

| ID | Sev | Divergence |
|----|-----|------------|
| **D-1** | HIGH | `docs/M3_APPLESAUCE_TRANSPORT.md` documents a DIFFERENT command vocabulary ("info", "seek NN", "track NN hN capture R revolutions") and status semantics than the runner (`sync:on`, `head:track N`, `disk:readx R`). This emulator follows the **runner** (truth source #1, the code that actually talks to hardware). If the transport doc is authoritative instead, the entire command layer is wrong. **Must be resolved at the M3.3 bench** before wiring is called production. |
| **D-2** | HIGH | Binary flux framing (`disk:readx` → LE32 8 MHz tick deltas) is taken from the `ApplesauceReadResult` contract in `applesauce_provider_v2.h`, not from a wire capture. The real device's exact download framing (headers? length prefix? checksum trailer?) is unverified. |
| **D-3** | MEDIUM | Status characters (`.`/`+`/`-`/`!`/`?`/`v`) and their mapping to the response classes are from the V2 provider's audit notes, not a protocol spec. The exact payload format after `+` (e.g. RPM as `+300.00`) is this emulator's choice, documented for the bench to confirm. |
| **D-4** | MEDIUM | `data:< N` returns `AS_FW_RESP_BINARY` (no text line) and the host drains via `as_fw_pop_binary`. Whether the real device emits a leading acknowledgement line before the binary stream is unknown; modelled as no-line. |
| **D-5** | MEDIUM | Protocol desync (a host command sent while a binary download is pending) wedges the device into a sticky `ERROR` state recoverable only by `reset`. Real firmware may resync more gracefully; modelled pessimistically so tests must drain correctly. |
| **D-6** | LOW | `disk:readx R` revolution bounds (1..15) are inferred from the runner's host-side clamp; the firmware's own acceptance range is unverified. |
| **D-7** | LOW | `AS_FW_LINE_MAX` (64) is a modelled line-buffer limit; the real firmware buffer size is unknown. Over-length lines answer `!` (protocol error). |
| **D-8** | MEDIUM | Mac 3.5" data-field payload is structurally simplified — the generator emits correct address prologs, sync gaps, sector counts and speed-zone timing, but does not fully encode 12-and-... 3.5" GCR data-field bytes. Address-field structure and timing (the forensically load-bearing parts) are real. |
| **D-9** | LOW | Mac speed-zone nominal RPMs (~393.38 / 429.17 / 472.14 / 524.57 / 590.11) are the commonly documented Sony-drive nominals. Real drives vary within tolerance; the generator uses the nominal exactly (no spread). |
| **D-10** | MEDIUM | The write path (`disk:write`, `data:> N`) is ALWAYS refused with `!` regardless of media state — a forensic-safety guard matching the UFT HAL's write gate, NOT a model of what the real device does when asked to write. Real Applesauce hardware can write; the emulator deliberately never exercises that. |

## What is NOT divergent (bench-relevant confidence)

- Power/motor/sync state sequencing is a strict ordered climb; illegal
  transitions answer visibly (`v`/`-`), never silently. Matches the
  runner's connect sequence.
- The 8 MHz / 125 ns tick domain is cross-checked at test time against
  the HAL SSOT (`uft_as_get_sample_clock`, `uft_as_ticks_to_ns`) — the
  emulator cannot drift from the HAL on the one number that matters for
  flux timing.
- Apple II 5.25" 6-and-2 GCR address-field structure (D5 AA 96 prolog,
  16 sectors, DE AA epilog, self-sync gaps) is fully encoded and
  round-trips through the nibble decoder in the test.
- Medium-safety: the generator refuses out-of-spec intervals and track
  positions, so no test fixture could be fed back through a write path
  to damage media.
