# ARCH-7 — VID/PID Inconsistencies (verification + decision sheet)

Follow-up to `audit/MASTER_REPORT.md` ARCH-7 (task #121). The original
finding could not be *fixed* in the audit's Klasse-A pass because it
needs verification — you cannot guess which VID/PID is canonical. This
sheet does the verifiable part: every UFT-side VID/PID claim
firsthand-read with file:line, the contradictions pinned, and each
sub-finding sorted into **fixable-now / needs-hardware / needs-design**.

## Firsthand-verified table (UFT-side, this branch)

| Provider | UFT claims | Source (file:line) | Kind |
|----------|-----------|--------------------|------|
| Greaseweazle | `0x1209:0x4D69` | `uft_greaseweazle_full.h:37-38` (const) + `hardwaretab.cpp:446` (GUI hint) | **consistent** ✓ |
| SCP | header `0x16C0:0x0753` | `uft_scp_direct.h:34-35` (const) | — |
| SCP | GUI hint `0x16D0:0x0F8C` | `hardwaretab.cpp:448-449` | **HARD CONTRADICTION** — different VID *and* PID |
| KryoFlux | `0x0403:0x6001` | `hardwaretab.cpp:450-451` (GUI hint only) | generic FTDI FT232 ID — not KryoFlux-specific |
| FC5025 | `0x16C0:0x06D6` | `uft_fc5025.h:43`, `uft_usb.h:101`, `sync_backends.c:95` | **consistent across 3 sites** ✓ |
| XUM1541 | `0x16D0:0x0504` | `xum1541_usb.h:40-41` (const) | — |
| ZoomFloppy | `0x16D0:0x04B2` | `xum1541_usb.h:36-37` (const) | — |
| XUM1541 DIY | `0x16D0:0x0503` | `xum1541_usb.h:43-44` (const, `_ALT`) | — |
| XUM1541/ZF | GUI hint matches only `0x16D0:0x0504`, labels it "ZoomFloppy/XUM1541" | `hardwaretab.cpp:452-453` | **incomplete + mislabelled** vs the in-repo table |
| ADF-Copy | `0x16C0:0x0483` | `adfcopy_provider_v2.h:9`, `.cpp:124,128,196,398,602,627,653` (doc-comment / error-string text only — **no constant**) | = stock PJRC Teensy serial ID |
| Applesauce | `0x16C0:0x0483` | `applesauce_provider_v2.cpp:143,229,844,867` (doc-comment / error-string text only — **no constant**) | **IDENTICAL to ADF-Copy** |

Two structural facts the audit under-stated:

1. **`0x16C0` is a shared VID** (Van Ooijen / V-USB hobby-device range).
   SCP, FC5025, ADF-Copy and Applesauce all live under it — only the
   PID separates them. That is normal *if* the PIDs are unique.
2. **ADF-Copy and Applesauce both claim the identical `0x16C0:0x0483`** —
   and `0x16C0:0x0483` is the *stock, unmodified PJRC Teensy USB-Serial*
   identity. Two Teensy-based controllers shipping the default serial
   descriptor are **genuinely indistinguishable at the VID/PID layer**.
   This is a hardware reality, not a typo.
3. `git grep MF-146` → **empty**. The "MF-146 VID/PID disambiguation"
   the original audit brief assumed exists does **not** exist in `src/`
   or `include/` (it was V1-era and went with P1.18, or never existed
   in this form).

## Sub-finding A — XUM1541 GUI hint — **FIXABLE NOW (done in this commit)**

`hardwaretab.cpp:452-453` matched only `0x16D0:0x0504` and labelled it
"ZoomFloppy/XUM1541". But `0x0504` is the *XUM1541* PID; ZoomFloppy is
`0x04B2`; the DIY board is `0x0503`. The in-repo header
`xum1541_usb.h:36-44` already carries the authoritative 3-PID table and
an `isKnownXum1541()` helper.

This is fixable **without hardware**: it is an *internal-consistency*
fix — the GUI hint is brought in line with the existing in-repo
single-source table. No value is guessed. If the table values
themselves are later found wrong, that is a separate single-source fix
to `xum1541_usb.h`.

→ Applied: `hardwaretab.cpp` now matches all three PIDs (`0x04B2`,
`0x0504`, `0x0503`) and labels the hint `"XUM1541 / ZoomFloppy"`,
matching the single combo entry text.

## Sub-finding B — SCP header vs GUI — **RESOLVED (MF-212)**

`uft_scp_direct.h:34-35` said `0x16C0:0x0753`; the GUI hint in
`hardwaretab.cpp` said `0x16D0:0x0F8C`. Different VID **and** different
PID — at most one could be right.

**Verified** (Axel, real device): the descriptor reports
`USB\VID_16D0&PID_0F8C`. So the **GUI hint was correct** and the
**header was wrong**.

→ Applied (MF-212): `uft_scp_direct.h` now defines
`UFT_SCP_USB_VID 0x16D0` / `UFT_SCP_USB_PID 0x0F8C` (the verified
value), and `hardwaretab.cpp`'s SCP port-hint `#include`s that header
and references the macros — the VID/PID now lives in exactly one place,
so the contradiction structurally cannot recur.

## Sub-finding C — ADF-Copy / Applesauce share `0x16C0:0x0483` — **IMPLEMENTED (MF-213)**

Not fixable by changing a number — both *are* stock-ID Teensy devices,
genuinely indistinguishable at the VID/PID layer. Design proposal:
[`docs/proposals/ARCH7C_TEENSY_ID_DISAMBIGUATION.md`](../docs/proposals/ARCH7C_TEENSY_ID_DISAMBIGUATION.md)
(MF-198) — a two-tier scheme.

**`needs-source` gap resolved** (Axel device readout): ADF-Copy **and**
Applesauce both ship the **stock, unmodified Teensy string descriptors**.
So the string-descriptor heuristic cannot tell them apart either — the
proposal's optional Applesauce "Evolution Interactive" rule does not
apply to the real device. **Tier 2 (the protocol probe) is mandatory**,
exactly the §7 "both report stock strings" branch the proposal
anticipated.

→ Applied (MF-213):

- **Tier 1** — `detectSerialPorts()` now has a `0x16C0:0x0483` branch
  that emits the *explicit-ambiguity* hint
  `"ADF-Copy or Applesauce (0x16C0:0x0483 — probe on Connect)"`. No
  string rules (both stock) — honest ambiguity, never a guess.
- **Tier 2** — `src/hardware_providers/teensy_probe.{h,cpp}`:
  - `classify_teensy_probe()` — the pure, header-inline decision core.
    Conservative: only "this device answered its OWN identify command
    with plausible text" classifies; both-or-neither → `Unknown`,
    never a coin-flip. Exhaustively unit-tested by
    `tests/test_teensy_probe.cpp`.
  - `probe_teensy_serial()` — the QSerialPort I/O wrapper: opens the
    port, sends the two non-destructive identify queries (`?vers`,
    `0x00`), classifies. Returns `Unknown` if the port cannot be
    opened or the Qt build lacks QtSerialPort.
  - **Wired** into `HardwareTab::onConnect()`: connecting ADF-Copy or
    Applesauce runs the probe on the selected port and shows a warning
    (never a silent override) if the probe contradicts the combo
    selection.

KryoFlux's `0x0403:0x6001` (generic FTDI FT232 ID) is the same class of
problem — the same probe-pattern applies (its DTC subprocess can
identify the device); noted as a follow-up in the proposal, out of
ARCH-7-C scope.

## Status

| Sub-finding | Class | State |
|-------------|-------|-------|
| A — XUM1541 GUI hint | fixable-now | ✅ fixed (MF-190) |
| B — SCP header vs GUI | was needs-hardware | ✅ resolved (MF-212) — verified `0x16D0:0x0F8C`, single-sourced in `uft_scp_direct.h` |
| C — ADF-Copy/Applesauce shared ID | was needs-design | ✅ implemented (MF-213) — Tier-1 ambiguity hint + `classify_teensy_probe()` (pure, unit-tested) + `probe_teensy_serial()` + wired into `onConnect()`. **Verified (Axel readout): both devices ship the *stock* Teensy descriptors** → Tier-1 string heuristic is useless for them, the Tier-2 probe is the authoritative answer |

Task #121: **A + B + C all done.** The probe is wired into the connect
path — it opens its OWN serial port, so it does not depend on the
ADF-Copy / Applesauce providers having a production transport (that is
separate M3.x work). KryoFlux's generic-FTDI `0x0403:0x6001` is the
same class of problem and gets the same probe-pattern treatment as a
documented follow-up (out of ARCH-7 scope). No value was ever
guessed — "keine erfundenen Daten".

## Reproduce

```bash
git grep -nE "0x16C0|0x16D0|0x1209|0x0403" -- \
    include/uft/hal/uft_scp_direct.h include/uft/hal/uft_fc5025.h \
    src/hardware_providers/xum1541_usb.h src/hardwaretab.cpp \
    src/hardware_providers/adfcopy_provider_v2.cpp \
    src/hardware_providers/applesauce_provider_v2.cpp
git grep -nF MF-146 -- src/ include/    # expect: empty
```
