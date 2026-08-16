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
implementations — **not yet against a real-disk reference corpus** (a
per-format verification-tier table is in progress; see
[`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md)). 9 hardware controllers
via a type-driven HAL (Greaseweazle fully wired, **hardware bench pass
pending** — deferred to v4.1.6; SCP-Direct M3.1 mock-validated; KryoFlux
/ FluxEngine / FC5025 via subprocess wrappers; XUM1541 / Applesauce /
ADF-Copy / USB-Floppy as honest scaffolds — see
[`docs/CAPABILITIES.md`](docs/CAPABILITIES.md)). No controller has a
documented real-hardware bench pass in this release.

---

## Downloads

**[Latest Release](https://github.com/Axel051171/UnifiedFloppyTool/releases/latest)** — currently **v4.1.5** (2026-06-05)

| Platform | File | Notes |
|----------|------|-------|
| Linux | `unifiedfloppytool_4.1.5_amd64.deb` | Ubuntu/Debian |
| Linux | `UnifiedFloppyTool-4.1.5-linux-amd64.tar.gz` | Portable binary |
| macOS | `UnifiedFloppyTool-4.1.5-macOS-arm64.dmg` | Apple Silicon |
| macOS | `UnifiedFloppyTool-4.1.5-macOS.tar.gz` | Universal portable |
| Windows | `UnifiedFloppyTool-4.1.5-windows-x64.tar.gz` | Portable, Qt DLLs included |
| Checksums | `SHA256SUMS.txt` | Verify with `sha256sum -c` |

> **macOS:** Falls "App ist beschädigt" erscheint: `xattr -cr UnifiedFloppyTool.app`

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
for the tier definitions): **T1=0, T1b=3, T2=14, T3=71** of 88.

- **T1** (real reference image): none yet
- **T1b** (image produced by a canonical third-party tool, read by UFT):
  D64 (VICE c1541), ADF (amitools xdftool), ATR (atrcopy template)
- **T2** (synthetic round-trip + spec verified against an authoritative
  reference implementation): 15 formats
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
| Greaseweazle | ✅ | Yes | Yes | Yes | Protocol v1.23, 72 MHz capture, byte-identical to v4.1.4-rc1 |
| KryoFlux | 🟢 | Yes | — | Yes | DTC subprocess (proprietary protocol); read-only by design |
| FluxEngine | 🟢 | Yes | Yes | Yes | `fluxengine` CLI subprocess wrapper |
| FC5025 | 🟢 | Yes | — | — | fcimage subprocess (read-only hardware) |
| SCP-Direct | 🟡 | Yes\* | (safety-blocked) | Yes\* | M3.1 full libusb impl, 22/22 opcodes byte-exact vs samdisk; write blocked until real-HW read-verify |
| XUM1541 / ZoomFloppy | 🟡 | Yes\* | Yes\* | — | M3.2 libusb wired; opencbm bus-timing pending |
| Applesauce | 🟡 | Yes\* | — | Yes\* | M3.3 `?vers` handshake wired; `?disk` state machine v4.1.6 |
| ADF-Copy | 🟠 | — | — | — | QSerialPort transport wired, Teensy-probe (MF-213) |
| USB-Floppy | 🐧 | Linux | Linux | — | SG_IO ioctl; Win/Mac backends v4.1.6 |

`\*` = libusb-mock-validated, no Tier-3 hardware-bench yet. GUI shows
an orange "Preview" badge instead of the green Production badge. Any
provider not in Tier-3 PASS reports `not_implemented` for unwired
calls — **never a silent no-op**.

### Copy Protection Analysis

Automatic detection and preservation of 55+ historical protection
schemes across 10 platforms (122 unique `uft_*_detect_*` entry points
in `src/protection/`).

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
| [Known Issues](docs/KNOWN_ISSUES.md) | Open principle-compliance gaps |
| [Master Plan](docs/MASTER_PLAN.md) | M-milestone roadmap (M3 hardware, M4 emulator-CI) |
| [Refactor Brief](docs/REFACTOR_BRIEF.md) | Type-Driven HAL architecture spec |
| [10-min Demo](docs/demo/QUICK_DEMO.md) | Hands-on demo script (no hardware required) |
| [Release Notes](RELEASE_NOTES.md) | All releases v4.1.3..v4.1.5 |
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

---

## License

GPL-2.0 — see [LICENSE](LICENSE) for details.
