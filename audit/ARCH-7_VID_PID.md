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

## Sub-finding B — SCP header vs GUI — **NEEDS HARDWARE / VENDOR DOC**

`uft_scp_direct.h:34-35` says `0x16C0:0x0753`; `hardwaretab.cpp:448-449`
says `0x16D0:0x0F8C`. Different VID **and** different PID — at most one
is right, possibly neither. Cannot be resolved by reading the repo.

**Question for Axel** (2-minute answer with the real device plugged in):
```
Linux:   lsusb | grep -i supercard      # or: lsusb -v on the SCP port
Windows: Device Manager → SuperCard Pro → Properties → Details →
         "Hardware Ids"  (shows USB\VID_xxxx&PID_xxxx)
```
The SuperCard Pro is an FTDI FT240X-based device — its USB identity
depends on what the vendor programmed into the FT240X EEPROM, so neither
the V-USB-style `0x16C0` nor `0x16D0` is obviously right. Once known,
**both** sites get the verified value and a single-source `#define` in
`uft_scp_direct.h` should drive the GUI hint too.

## Sub-finding C — ADF-Copy / Applesauce share `0x16C0:0x0483` — **NEEDS DESIGN**

Not fixable by changing a number — both *are* stock-ID Teensy devices.
Discrimination has to happen one level up, on the USB **string
descriptors** (manufacturer / product), which the error text already
hints at ("manufacturer 'Evolution Interactive'",
`applesauce_provider_v2.cpp:229`). That is a small detection *feature*,
not a constant correction:

- when enumerating `0x16C0:0x0483`, read the iManufacturer / iProduct
  strings and route to ADF-Copy vs Applesauce accordingly;
- until then, a `0x16C0:0x0483` device is ambiguous and the GUI must
  not silently pick one.

Also note KryoFlux's `0x0403:0x6001` (`hardwaretab.cpp:450`) is the
*generic* FTDI FT232 ID — the same class of "not actually unique"
problem: any FT232 cable matches it. Same design answer (string
descriptor) applies.

**Question for Axel:** when an Applesauce and an ADF-Copy are both
plugged in, what do their iManufacturer / iProduct strings read?
```
Linux:   lsusb -v -d 16c0:0483 | grep -iE 'iManufacturer|iProduct'
```

## Status

| Sub-finding | Class | State |
|-------------|-------|-------|
| A — XUM1541 GUI hint | fixable-now | ✅ fixed (this commit) |
| B — SCP header vs GUI | needs-hardware | ⬜ open — blocked on Axel `lsusb` / vendor doc |
| C — ADF-Copy/Applesauce shared ID + KryoFlux generic FTDI | needs-design | ⬜ open — string-descriptor disambiguation feature |

Task #121 stays open for B and C. Neither may be "fixed" by guessing a
value — that would violate "keine erfundenen Daten".

## Reproduce

```bash
git grep -nE "0x16C0|0x16D0|0x1209|0x0403" -- \
    include/uft/hal/uft_scp_direct.h include/uft/hal/uft_fc5025.h \
    src/hardware_providers/xum1541_usb.h src/hardwaretab.cpp \
    src/hardware_providers/adfcopy_provider_v2.cpp \
    src/hardware_providers/applesauce_provider_v2.cpp
git grep -nF MF-146 -- src/ include/    # expect: empty
```
