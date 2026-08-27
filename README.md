# UnifiedFloppyTool

[![Release](https://img.shields.io/github/v/release/Axel051171/UnifiedFloppyTool)](https://github.com/Axel051171/UnifiedFloppyTool/releases)
[![Build](https://github.com/Axel051171/UnifiedFloppyTool/actions/workflows/ci.yml/badge.svg)](https://github.com/Axel051171/UnifiedFloppyTool/actions)
[![License: GPL-2.0](https://img.shields.io/badge/License-GPL%202.0-blue.svg)](LICENSE)

**„Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."**
Open-source forensic floppy disk preservation tool. Qt6 C/C++ desktop
application targeted at archives, museums, retrocomputing enthusiasts,
digital forensics, and copy-protection research.

138 disk-image format IDs, 88 registered plugin parsers. **Verification
honesty:** most parsers are currently validated only against synthetic
round-trip tests and/or specs verified against authoritative reference
implementations — **not yet against a real-disk reference corpus**. The
per-format verification-tier table now exists and is kept current:
[`docs/VERIFICATION_TIERS.md`](docs/VERIFICATION_TIERS.md) — **56 of 88
formats are T3 (unverified)**; proven: T1=2, T1b=12, T2=18. 9 hardware controllers
via a type-driven HAL (Greaseweazle fully wired, **hardware bench pass
pending** — still open in 4.1.6, and it needs a machine we do not have
(see "Please break it" below); SCP-Direct M3.1 mock-validated; KryoFlux
/ FluxEngine / FC5025 via subprocess wrappers; XUM1541 / Applesauce /
ADF-Copy / USB-Floppy as honest scaffolds — see
[`docs/CAPABILITIES.md`](docs/CAPABILITIES.md)). No controller has a
documented real-hardware bench pass in this release.

---

## Downloads

**[Latest Release](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest)** — currently **v4.1.6** (2026-08-26)

| Platform | File | Notes |
|----------|------|-------|
| Linux | `unifiedfloppytool_4.1.6_amd64.deb` | Ubuntu/Debian |
| Linux | `UnifiedFloppyTool-4.1.6-linux-amd64.tar.gz` | Portable binary |
| macOS | `UnifiedFloppyTool-4.1.6-macOS-arm64.dmg` | Apple Silicon |
| macOS | `UnifiedFloppyTool-4.1.6-macOS.tar.gz` | Universal portable |
| Windows | `UnifiedFloppyTool-4.1.6-windows-x64.tar.gz` | Portable, Qt DLLs included |
| Checksums | `SHA256SUMS.txt` | Verify with `sha256sum -c` |

> **macOS:** Falls "App ist beschädigt" erscheint: `xattr -cr UnifiedFloppyTool.app`

---

## What's New in v4.1.6 (2026-08-26)

**Theme: every statement measured — including the statement of what the
tool cannot do.**

No new formats (those are under a moratorium). Instead: a sweep through
everything the tool *says* about preserved data. What it found, measured:

| Where | Before | After |
|---|---|---|
| `SCP→D64` | **0 of 683 sectors**, reported success | 683 of 683 |
| In-memory conversion | bypassed the preflight gate entirely — **3.7 MB of fabricated flux from 4 KB of noise**, no consent asked | gate applies, 32 pairs refused |
| GUI **Convert** button | `QFile::copy()` + "Conversion complete!" | goes through `uft_convert_file()` |
| File browser | **13 invented directory entries** (`devs`, `libs`, `Startup-Sequence`, `GAME 17280`, …) | says it does not read the filesystem |
| Allocation map | every disk shown as entirely **free**, colour-coded | grey `?`, caveat on screen |
| Forensic report | `Filesystem: ✓ Valid` **without reading a byte** — and it goes into the PDF export | `— not checked`, with the reason |
| Copy protection | `✓ None detected` after three heuristics | `— nothing matched`, plus why that is not an all-clear |
| Licence header | `SPDX: MIT` on a GPLv2+ port, inside a GPLv2 project | `GPL-2.0-or-later` |

**Also new**

- **Multi-revolution voting for the C64 path** — the CBM data checksum is
  a single XOR byte; a corrupt sector passes it with probability 1/256
  (measured: 3 of 683). Agreement across revolutions beats one check
  byte, and it uses the *same* voter the Amiga path already had.
- **The write-safety gate finally has callers.** Six in-place image
  modifications (rename, delete, mkdir, import ×2, save) now take a
  verified snapshot into `.uft-snapshots` next to the image first. It was
  filed as "needs a hardware session" — the signature takes image bytes
  and a snapshot directory, no hardware at all.
- **Six headless Qt tests** — every tab now has one. Before this release:
  two, both on the same tab.
- **Three new consistency gates** (37 categories total, all at 0) so the
  discovered shapes cannot come back: a display that admits in the source
  that it is a placeholder must say so *on screen*; an integrity verdict
  that cannot fail is not a verdict; and a **test that cannot fail** is
  worse than no test — see below. The last two ship with an **empty
  baseline**: the next such claim fires.

### The test harness itself was the biggest find

**"266/266 green" was never true.** In **32 test files** the runner macro
counted success unconditionally after the call, while `ASSERT` merely
returned from the test function on failure. `main()` returned
`(tests_passed == tests_run) ? 0 : 1` — so always 0. These tests printed
`PASSED` on the line directly below `FAILED at line N`, and **could not
go red**.

Behind them sat **seven tests with 18 checks**. Four were faulty
fixtures. **Three were real bugs in the format layer**, each fixed
against a *named* reference that now sits in the code:

| Bug | Reference |
|---|---|
| **Every Game Gear image was reported as Master System** — the region code lives in the *upper* nibble of `$7FFF`, the parser read the lower one; the ROM-size code came from the wrong byte entirely | [`maxim-zhao/sega8bitheaderreader`](https://github.com/maxim-zhao/sega8bitheaderreader) `source/Unit1.pas`, confirmed by [SMS Power!](https://www.smspower.org/Development/ROMHeader) |
| **The Game Boy header was overlaid as a memory struct** although its fields overlap. Measured with `offsetof`: `sizeof` = 86 instead of 80, `cartridge_type` landed at `$014C` instead of `$0147`, the global checksum **six bytes past the end of the header** | [Pan Docs — The Cartridge Header](https://gbdev.io/pandocs/The_Cartridge_Header.html) |
| **The magic-less Z80 guess ran before TAP and DSK** and swallowed both. It amounts to "file ≥ 30 bytes and bytes 6..7 not both zero" — which is nearly every file | — (ordering fix, no spec claim) |

An eighth surfaced only on Windows (`/tmp` hard-wired; MSYS bends the
path locally, CI does not).

**Where no reference could be found, nothing was changed.** The Action
Replay detector demands `66816 ≤ size ≤ 67584`; the test fixture is
66 664 bytes. Neither number has a source — the header calls its own
offset table "typical layout". Bending one side to match the other would
be fabrication, so those 8 checks are **skipped with a printed reason**,
not made green. The reference image is on the procurement list.

- **The leak backlog went from 58 to 0**, every step measured in CI:
  **58 → 15 → 2 → 0** of 266 under LeakSanitizer, and UBSan **0 of 266**.
  The largest cause was not test-code debt, as two earlier audits had
  concluded: `uft_track_add_sector()` **copies**, and the shared helper
  freed its own buffer only on the error path — so **every sector read
  leaked its buffer**.

**What this release still cannot do** — this list is part of the release,
not a footnote:

- **56 of 88 tier-tracked formats are unverified (T3).** Proven: T1=2,
  T1b=12, T2=18. (Was 57/17 — `mfi` moved up in v4.1.7 after a real
  parser bug was fixed against MAME's `mfi_dsk.h`.)
- **12 of 44 conversion paths are offered**, 4 of them lossless *with a
  measurement*. The rest the preflight gate refuses as UNTESTED — on
  purpose.
- **The copy-protection catalogue (55+ schemes) has no caller.** What runs
  automatically is signal detection plus three heuristically named
  schemes.
- **The filesystem is not read.** Directory listing and allocation map now
  say so.
- **One test skips 8 of its 14 checks** (`test_freezer`): there is no
  named reference for the Action Replay format, and bending either side
  to match the other would be fabrication. Skipped, not made green.
  (The ASan leak backlog that stood here — 58 tests — is now **0 of 266**,
  measured in CI: MF-592/595/598/599.)
- **No hands-on acceptance test.** The headless Qt tests cover logic and
  display, not look, flow, or anything behind a modal dialog.
- **No controller has a documented real-hardware bench pass.**

---

## Please break it — and tell us how

This project has a specific, unusual gap: **there is no physical hardware
behind it.** Every Tier-3 bench result has to come from someone else's
desk. And 56 of 88 formats have never met a real disk — only synthetic
round-trips.

That makes your report worth more than any test we can write ourselves.

**What helps most, roughly in order:**

1. **A real disk of a format marked T3.** Read it, convert it, and tell us
   whether the result opens in the tool you normally use (VICE, WinUAE,
   Applesauce, an emulator, the original machine). A single verified
   format moves it out of "unproven".
2. **A hardware controller session.** Greaseweazle, SuperCard Pro,
   KryoFlux, FC5025, XUM1541, Applesauce — any of them. Even "it
   connected and read track 0" is data we do not have.
3. **Anything that claims something.** If a number, a status, a
   checkmark or a message looks wrong — that is exactly the bug class
   this release was about. Six of them were found this way.
4. **A crash.** Attach the input file if you can share it.

**What makes a report useful:**

- the **exact** version (`Help → About`, or `VERSION.txt`)
- OS and how you installed it
- what you clicked, what you expected, what happened
- the image file, or its size and format, if you can share it
- for conversions: the source format, the target format, and the message
  the tool printed

**Where:**
[Issues](https://github.com/Axel051171/UnifiedFloppyTool/issues) for bugs
and format reports ·
[Discussions](https://github.com/Axel051171/UnifiedFloppyTool/discussions)
for "is this supposed to work like that?"

**What we will not do with it:** guess. If a report cannot be reproduced
or measured, it goes on the open list as unproven rather than being
quietly closed — and if it turns out the tool was right, that gets
written down too.

> A note on trust: this release deliberately *removed* several things
> that looked like features — an invented file listing, a green
> allocation map, three "✓ Valid" certificates. If something you relied
> on now says "not checked", that is not a regression. It is the tool
> admitting it never knew.

---

## What's New in v4.1.5 (2026-06-05)

### M3.1 SCP-Direct read-flux full implementation (MF-276)

Byte-for-byte port of [samdisk's `SuperCardPro::ReadFlux()`](https://github.com/simonowen/samdisk/blob/main/src/SuperCardPro.cpp)
reference implementation. The full SCP USB flux-capture protocol is now
wired: `CMD_READ_FLUX → CMD_GET_FLUX_INFO → per-revolution CMD_SENDRAM_USB`;
16-bit big-endian samples decoded with 0x0000-overflow accumulator that
resets per revolution (index-pulse semantics). 9 mock-tests including
a per-revolution-reset regression-guard.

`uft_scp_direct_write_flux()` **remains intentionally NOT_IMPLEMENTED**.
A malformed flux stream can physically damage forensic media; write is
blocked at the HAL boundary until the read path is bench-verified
against real SCP hardware. Forensic safety overrides feature-completeness.

### Tier-2.5 hardware simulator system

The biggest forensic-confidence win of v4.1.5: **all V2 providers can
now be exercised end-to-end without owning the corresponding hardware.**

Three mocking strategies for three transport layers:

| Transport | Mocking strategy | Coverage |
|---|---|---|
| QProcess (KryoFlux DTC, FluxEngine, FC5025 fcimage) | Python fake-binary scripts on PATH | 7/7 SIMULATED |
| libusb-direct (SCP-Direct, XUM1541) | Scripted libusb-mock framework (`tests/usb_mock/`) | 5+3 = 8 SIMULATED |
| USB-CDC virtual serial (Greaseweazle, Applesauce, ADF-Copy) | Python protocol simulators (com0com/socat) | 3 byte-compile-verified |

CI runs the simulator gate on every push. **SIMULATED is never reported
as PASS** — real hardware (Tier 3) remains the only PASS authority.

### LOSS.preflight — Prinzip 1 enforcement

`uft_convert_file()` is the single chokepoint for all 44 conversion
paths. v4.1.5 wires `uft_preflight_check()` into that entry point plus
`.loss.json` sidecar emission for successful `LOSSY_DOCUMENTED`
conversions:

- **LOSSLESS** runs silently as before.
- **LOSSY_DOCUMENTED** requires explicit `accept_data_loss=true` — the
  GUI/CLI must obtain user consent. On success, a category-level
  `<target>.loss.json` is written.
- **IMPOSSIBLE** and **UNTESTED** abort with a diagnostic.

### Plugin Prinzip-7 compliance 84/84

Before v4.1.5 only 5 plugins had a hand-curated `features` array and 15
had a non-UNKNOWN `spec_status`. Now **every** plugin has both
populated. Validated by `audit_plugin_compliance.py` in CI.
**Note:** this audit checks *metadata completeness* (`spec_status` +
`features` fields), not functional correctness — it does not exercise
open/read against reference data.

### ABI gate for plugin format

`uft_format_plugin_t` gained an `api_version` field plus
`_Static_assert(sizeof == 216)` plus registrar reject for future-ABI
plugins. Opt-in until v5.0; legacy plugins with `api_version=0` are
accepted with a one-shot stderr warning.

### 22/22 SCP USB opcodes byte-verified

All command bytes in `include/uft/hal/uft_scp_direct.h` are
byte-for-byte verified against samdisk's `SuperCardPro.h` (the de-facto
open-source SCP reference implementation since 2017). Pre-MF-254
placeholder opcodes (`0x02-0x40`) are replaced with the documented
`0x80-0xD2` opcode space.

### `.claude` agent suite hardened

Constitutional section "Eigenständigkeit, Eigenverantwortung,
Vollständigkeit" overriding all other rules: no new stubs in
newly-written code (sole exception: `honest-stub` for unwired hardware
providers with explicit milestone). Definition-of-Done per code-type.
Scope-rule (>150 LOC ballooning → STOP, deliver biggest complete
chunk). 21 agents, 21 skills (20 UFT + 1 HTB).

### Hardware verification status

The Greaseweazle production path (`src/hal/uft_greaseweazle_full.c`) is
**byte-identical** to the v4.1.4-rc1 commit that was hardware-verified
2026-05-15 — zero lines changed across the 39 commits in this release.
HIL.GW formal-bench session is deferred to v4.1.6 (substantively
identical to v4.1.4-rc1 because the GW code is unchanged).

UFT-008 SCP-Direct Tier-3 verification still requires real SCP
hardware (v4.1.6 scope). `impl_complete` flag stays `false` in
`uft_scp_direct_get_capabilities()` until then.

[Full Changelog](CHANGELOG.md) · [Release Notes](RELEASE_NOTES.md) · [Showcase](docs/SHOWCASE.md)

---

## Features

### Disk-image format coverage

138 format IDs registered, 88 of them backed by a Plugin-B parser
(read + probe). Every plugin has populated `spec_status` and `features`
metadata (Prinzip 7 — metadata completeness, not functional
verification).

**Honest verification status** (script-generated, see
[`docs/VERIFICATION_TIERS.md`](docs/VERIFICATION_TIERS.md) for the
per-format table and [`docs/VERIFICATION_PLAN.md`](docs/VERIFICATION_PLAN.md)
for the tier definitions): **T1=2, T1b=12, T2=18, T3=56** of 88.

- **T1** (real reference image): FDI — a real 1994 TR-DOS disk magazine
  (83-cylinder, partially formatted last track; image local-only for
  copyright hygiene, sha256 + provenance in `tests/corpus_manifest/`)
- **T1b** (image produced by a canonical third-party tool, read by UFT):
  D64/D71/D81/D67/D80/D82/G64/G71 (VICE c1541), ADF (amitools xdftool),
  ATR (atrcopy template) plus XFD derived from it, HFE + SCP flux
  (greaseweazle encoding the rights-free ADF)
- **T2** (synthetic round-trip + spec verified against an authoritative
  reference implementation): listed per format in
  [`docs/VERIFICATION_TIERS.md`](docs/VERIFICATION_TIERS.md), with the
  evidence for each
- **T3** (unverified — no test, or a synthetic test without external spec
  verification): the remaining majority

A freeze rule is in force: no new format/decoder code until existing
formats are lifted (enforced by CI gate, see
[`docs/VERIFICATION_PLAN.md`](docs/VERIFICATION_PLAN.md)).

| Platform | Formats |
|----------|---------|
| Commodore | D64, D71, D81, G64, G71, NIB, NB2, T64, CRT, PRG, P00 |
| Amiga | ADF, ADZ, DMS, HDF, IPF (own implementation, no libcaps) |
| Atari ST/8-bit | ATR, ATX, XFD, ST, MSA, STX/Pasti, DCM |
| Apple | DSK, DO, PO, NIB, WOZ (v1/v2/2.1), 2IMG, DC42, EDD |
| IBM PC | IMG, IMA, IMD, TD0, CQM, DMK |
| Amstrad/Spectrum | DSK, EDSK, TRD, SCL, MGT, TAP, TZX |
| BBC/Acorn | SSD, DSD, ADF, UEF |
| Japanese | D88, D77, NFD, HDM, XDF, DIM, FDX |
| Flux | SCP, KryoFlux stream, HFE (v1/v2/v3), A2R, DFI |
| Other | MSX, Thomson, TI-99, Roland, HP LIF, CP/M, Micropolis |

### Hardware Controllers (9 V2 providers)

Full per-controller capability matrix: [`docs/CAPABILITIES.md`](docs/CAPABILITIES.md).

Status legend:
- ✅ **production** — Tier-3 hardware-verified, read+write
- 🟢 **read-real** — subprocess wrapper to vendor CLI, hardware-tested
- 🟡 **mock-only** — libusb wired, byte-protocol validated, no Tier-3 yet
- 🟠 **scaffold** — code path live, transport pending
- 🐧 **Linux-only** — works under Linux, Windows/macOS backends pending

| Controller | Status | Read | Write | Flux | Notes |
|---|:---:|:---:|:---:|:---:|---|
| Greaseweazle | ✅ | Yes | Yes | Yes | Protocol v1.23, 72 MHz capture. Bench-verified **2026-05-15** (v4.1.4-rc1); the production path is byte-identical since — that is what the ✅ rests on, not a new bench |
| KryoFlux | 🟢 | Yes | — | Yes | DTC subprocess (proprietary protocol); read-only by design |
| FluxEngine | 🟢 | Yes | Yes | Yes | `fluxengine` CLI subprocess wrapper |
| FC5025 | 🟢 | Yes | — | — | fcimage subprocess (read-only hardware) |
| SCP-Direct | 🟡 | Yes\* | (safety-blocked) | Yes\* | M3.1 full libusb impl, 22/22 opcodes byte-exact vs samdisk; write blocked until real-HW read-verify |
| XUM1541 / ZoomFloppy | 🟡 | Yes\* | Yes\* | — | M3.2 libusb wired. Wire protocol **rewritten against the OpenCBM source** (MF-301) — see below. Emulator 56/56. Silicon-untested |
| Applesauce | 🟡 | Yes\* | — | Yes\* | M3.3 `?vers` handshake wired; `?disk` state machine **still open** |
| ADF-Copy | 🟠 | — | — | — | QSerialPort transport wired, Teensy-probe (MF-213) |
| USB-Floppy | 🐧 | Linux | Linux | — | SG_IO ioctl; Win/Mac backends **still open** |

`\*` = libusb-mock-validated, no Tier-3 hardware-bench yet. GUI shows
an orange "Preview" badge instead of the green Production badge. Any
provider not in Tier-3 PASS reports `not_implemented` for unwired
calls — **never a silent no-op**.

**What changed here in 4.1.6 — and what did not.** No controller changed
status, and no hardware bench happened; there is still no machine behind
this project. One substantive fix landed:

> **XUM1541: the bulk-opcode table was invented** (MF-301). An audit
> against the OpenCBM source — `xum1541_types.h`,
> `lib/plugin/xum1541/xum1541.c`, `archlib.c`, read verbatim — found that
> UFT's `OPEN=8` / `CLOSE=9` **collided with the real `READ=8` /
> `WRITE=9`**, and that IEC addressing is not a separate opcode family at
> all but a WRITE with the ATN flag. Also corrected: the 3-byte status
> read (the old 1-byte read would have failed with
> `LIBUSB_ERROR_OVERFLOW` on real silicon), the header layout, the IOCTL
> transport, and an EOI `bytes_read` out-param so a shortened transfer is
> distinguishable from a full one — forensic length preservation. The
> emulator was rewritten to the same verified wire format: 56/56.
>
> This is verification against a **named reference**, not against
> silicon. The status stays 🟡 for exactly that reason.

Also fixed: the macOS build broke on a one-sided libusb include (MF-470).
Everything else in this layer was header hygiene.

### Copy Protection Analysis

**What runs automatically** is *signal* detection over the flux: fuzzy
bits, long and short tracks, no-flux areas, track overlap, desync, weak
bits and illegal GCR. Three schemes are named heuristically from those
signals (RapidLok, weak-bit protection, FAT/long track) — the UI labels
them as heuristics, not as measurements.

**What exists in the source but is not reachable:** a catalogue of 55+
*named* schemes across 10 platforms in `src/protection/`. Those ~200
functions have **no caller** — see `docs/OPEN_ITEMS.md` P0-2. The earlier
wording here counted `uft_*_detect_*` entry points and presented the count
as a capability; a function that exists is not a feature until something
calls it.

The catalogue covers (as source, not as a reachable feature):

- **Commodore** — V-MAX!, RapidLok, Vorpal, Pirate Slayer, GEOS
- **Amiga** — Rob Northen Copylock, custom MFM
- **Atari ST** — Copylock, Speedlock, Macrodos, Fuzzy sectors
- **Apple II** — Spiral tracking, nibble count, half-tracks
- **PC** — Weak sectors, long tracks, non-standard formats

### Forensic Analysis Tools

- Flux timing histogram with encoding auto-detection
- PLL phase analysis and clock recovery
- Track alignment (V-MAX!, RapidLok, Pirate Slayer)
- Weak bit and copy-protection mapping
- Sector-level hex editor
- Side-by-side disk comparison
- **8 DeepRead modules** — adaptive decode, weighted voting, encoding
  boost, write-splice detection, magnetic aging profile, cross-track
  correlation, revolution fingerprint, soft-decision LLR
- **LOSS.preflight** at the `uft_convert_file()` chokepoint —
  category-level `.loss.json` sidecar for every lossy conversion

---

## Quick Start

### Run the GUI

```bash
./UnifiedFloppyTool          # Linux
open UnifiedFloppyTool.app   # macOS
UnifiedFloppyTool.exe        # Windows
```

1. **Hardware** tab — select controller, click Connect (orange "Preview"
   badge = not yet hardware-verified at Tier 3)
2. **Workflow** tab — configure source / destination
3. Click **Start**

### 10-minute hands-on demo (no hardware needed)

The Tier-2.5 simulator system lets you exercise the full pipeline
without owning a single floppy controller. Detailed script:
[`docs/demo/QUICK_DEMO.md`](docs/demo/QUICK_DEMO.md).

```bash
python3 tests/hil/run_simulated.py
# 10 SIMULATED · 0 FAIL · 0 NOT_RUN
```

---

## Building from Source

### Requirements

- Qt 6.5+ (Core, Widgets, SerialPort, Charts)
- C++20 compiler (GCC 13+, Clang 15+, MSVC 2022+)
- libusb 1.0 (for SCP-Direct + XUM1541 hardware support)
- Python 3 (for build-system audit scripts)

### Linux (Ubuntu/Debian)

```bash
sudo apt install build-essential qt6-base-dev qt6-tools-dev \
    libqt6serialport6-dev libqt6charts6-dev \
    libusb-1.0-0-dev libgl1-mesa-dev python3

git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool

mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j$(nproc)
```

### macOS

```bash
brew install qt@6 libusb

git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
mkdir build && cd build
qmake ../UnifiedFloppyTool.pro CONFIG+=release
make -j$(sysctl -n hw.ncpu)
```

### Windows (MinGW)

```batch
:: Install Qt 6.5+ from qt.io (MinGW kit)
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
mkdir build && cd build
qmake ..\UnifiedFloppyTool.pro CONFIG+=release
mingw32-make -j%NUMBER_OF_PROCESSORS%
```

CMake is also supported for tests — see [`CMakeLists.txt`](CMakeLists.txt).
Build-system parity between qmake (release) and CMake (test) is gated
by `scripts/verify_build_sources.py` in CI.

---

## udev Rules (Linux)

For direct USB access to floppy controllers without root:

```bash
sudo cp tools/99-floppy-devices.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
```

---

## Forensic Guarantees

| Guarantee | Status |
|---|---|
| MD5 / SHA1 / SHA256 / SHA512 parallel hashing | ✅ |
| Hash-Chain for integrity proof | ✅ |
| Audit-Trail (40+ event types, CHS context) | ✅ |
| Export: JSON / HTML / PDF / Markdown / XML / Plain | ✅ |
| Risk scoring (0-100) with recovery recommendation | ✅ |
| LOSS.preflight gate (all 44 converters) | ✅ MF-263 |
| `.loss.json` sidecar (LOSSY_DOCUMENTED paths) | ✅ MF-268 |
| Per-track exact loss counts | v4.1.6 |
| Prinzip-7: `spec_status` for every plugin | ✅ 80/80 |
| Prinzip-7: `features` matrix for every plugin | ✅ 80/80 |

---

## Documentation

| Document | Description |
|----------|-------------|
| [Showcase](docs/SHOWCASE.md) | One-page pitch — what UFT is and what it can do today |
| [Capabilities Matrix](docs/CAPABILITIES.md) | Per-controller honest capability matrix (Tier-3 / Tier-2.5 / mock-only / scaffold) |
| [Design Principles](docs/DESIGN_PRINCIPLES.md) | 7+4 binding principles for the project |
| [Open Items](docs/OPEN_ITEMS.md) | **The** single list of open work — everything else points here |
| [Verification Tiers](docs/VERIFICATION_TIERS.md) | Per-format proof level (T1 / T1b / T2 / T3) with sources |
| [Known Issues](docs/KNOWN_ISSUES.md) | Archive: where the tool does not meet its own design principles |
| [Bench Protocol](docs/BENCH_PROTOCOL.md) | How to run a Tier-3 hardware bench and report it |
| [Master Plan](docs/MASTER_PLAN.md) | M-milestone roadmap (M3 hardware, M4 emulator-CI) |
| [Refactor Brief](docs/REFACTOR_BRIEF.md) | Type-Driven HAL architecture spec |
| [10-min Demo](docs/demo/QUICK_DEMO.md) | Hands-on demo script (no hardware required) |
| [Release Notes](RELEASE_NOTES.md) | All releases v4.1.3..v4.1.6 |
| [Changelog](CHANGELOG.md) | Full version history |
| [Contributing](CONTRIBUTING.md) | How to contribute |

---

## Acknowledgments

UFT builds upon the work of these projects:

- [Greaseweazle](https://github.com/keirf/greaseweazle) — Keir Fraser (production protocol)
- [samdisk](https://simonowen.com/samdisk/) — Simon Owen (SCP USB protocol reference)
- [OpenCBM](https://github.com/opencbm/opencbm) — CBM community (XUM1541 protocol)
- [FluxEngine](https://github.com/davidgiven/fluxengine) — David Given
- [HxC Floppy Emulator](https://hxc2001.com/) — Jean-François DEL NERO
- [Pauline](https://github.com/jfdelnero/Pauline) — Jean-François DEL NERO
- [libdsk](https://github.com/lipro-cpm4l/libdsk) — John Elliott
- [MAME](https://github.com/mamedev/mame) floppy subsystem
- [Pan Docs](https://gbdev.io/pandocs/) — gbdev community (Game Boy cartridge header)
- [SMS Power!](https://www.smspower.org/) and
  [sega8bitheaderreader](https://github.com/maxim-zhao/sega8bitheaderreader) —
  Maxim (SMS/Game Gear ROM header)
- [VICE](https://vice-emu.sourceforge.io/) — CBM emulator team (D64 geometry, `c1541`)

---

## License

GPL-2.0 — see [LICENSE](LICENSE) for details.
