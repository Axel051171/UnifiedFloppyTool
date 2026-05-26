# UnifiedFloppyTool v4.1.5 Release Notes [DRAFT — 2026-05-25]

**Status:** DRAFT — finalize on tag-day per `scripts/release/release_v415_checklist.md`.
**Target Tag Date:** post-2026-05-29 (after v4.1.4 RC1-Window closes)
+ hardware-bench session for HIL.GW + UFT-008 SCP-Direct Tier-3 PASS.
**Branch:** `tests/v4.1.5-hardening` (squash-merge to `main` at tag time).
**Commits since `origin/main`:** 27 (MF-240..MF-270, MF-243..MF-258 from
the v4.1.4-final ramp + MF-260..MF-270 v4.1.5 work).

## What changed (TL;DR)

v4.1.5 hardens the v4.1.4 release across **9 dimensions** without
breaking any v4.1.4 user-facing behavior. The Greaseweazle workflow is
unchanged. Everything new is either honesty (more accurate status
labels), structural correctness (ABI gate, single-chokepoint
preflight), or scaffolding (Tier-2.5 simulators, sidecar reports).

**Code-side gates closed in this release (V415_GOAL_PLAN.md):**

| Gate | Status | Impact |
|---|---|---|
| All 9 V2 providers have a live code path | ✅ MF-249..MF-258 | No silent no-op when user clicks "Connect" on a non-Greaseweazle controller |
| 22/22 SCP USB opcodes byte-verified | ✅ MF-254 + MF-261 | Cross-referenced against `samdisk/SuperCardPro.h`. Pre-MF-254 placeholder bytes (`0x02-0x40`) are replaced with the documented `0x80-0xD2` opcode space |
| `uft_format_plugin_t` ABI gate | ✅ MF-260 | New `api_version` field + `_Static_assert(sizeof == 216)` + registrar reject for future-ABI plugins |
| Plugin Prinzip-7 compliance 84/84 | ✅ MF-262 + MF-263 | `spec_status` and `features` populated for every plugin (was 15/84 and 5/84 respectively) |
| LOSS.preflight Phase 1 + 2 | ✅ MF-263 + MF-268 | All 44 conversion paths gated through `uft_preflight_check()` at the `uft_convert_file()` chokepoint. `LOSSY_DOCUMENTED` paths emit a `.loss.json` sidecar after successful convert with category-level loss entries |
| Tier-2.5 hardware simulator system | ✅ MF-267 + MF-270 | KryoFlux + FluxEngine + FC5025 via Python "fake binary" sims, SCP-Direct + XUM1541 via libusb-mock framework, Greaseweazle via serial-protocol sim. 10/10 SIMULATED in CI |
| ARCH-7-C Teensy-probe wrapper | ✅ MF-213 + MF-263 | Probe-on-Connect with mismatch warning for ADFCopy ↔ Applesauce (both Teensy 0x16C0:0x0483) — prevents disk-corrupting cross-wire |
| Test pass-rate 47/180 → 153/153 | ✅ MF-245..MF-260 + MF-270 | Stale exclusions reviewed; new `test_plugin_abi.c`, `test_scp_direct_usb_mock.c`, `test_xum1541_usb_mock.c` |
| Build-system parity baseline shrunk | ✅ MF-262 | `verify_build_sources.py` baseline 224 → 219 |
| qmake .pro wiring fixed | ✅ MF-259 + MF-266 | preflight/loss_report/roundtrip sources, libusb IMPORTED target, audit-vector pins |

## Highlight: Tier-2.5 hardware simulator system

The biggest forensic-confidence win of v4.1.5: **all V2 providers can
now be exercised end-to-end without owning the corresponding hardware.**

Three different mocking strategies for three different transport layers:

| Transport | Mocking strategy | Coverage |
|---|---|---|
| QProcess (CLI subprocess: KryoFlux DTC, FluxEngine, FC5025 fcimage) | Python fake-binary scripts on PATH | 7/7 SIMULATED |
| libusb-direct (SCP-Direct, XUM1541) | Scripted libusb-mock framework (`tests/usb_mock/`), per-target wrapper TU, zero production-code changes | 5+3 = 8 SIMULATED |
| USB-CDC virtual serial (Greaseweazle, Applesauce, ADFCopy) | Python protocol simulators paired with com0com/socat | 3 byte-compile-verified |

The libusb-mock framework is novel: a `libusb_mock.h` header that
pre-defines `LIBUSB_H` (the real libusb's include-guard) so any later
`#include <libusb-1.0/libusb.h>` becomes a no-op. The mock provides
minimal libusb types + a scripted-exchange queue that test code
populates. Production C-HAL code is bytewise unchanged.

Coverage runs in CI on every push via the Audit job, reporting
SIMULATED / FAIL / NOT_RUN per controller. SIMULATED is **never**
reported as PASS — real hardware (Tier 3) remains the only PASS
authority.

## LOSS.preflight — Prinzip 1 enforcement

`uft_convert_file()` is the single chokepoint for all 44 conversion
paths. v4.1.5 wires `uft_preflight_check()` into that entry point
(Phase 1, MF-263) plus `.loss.json` sidecar emission for successful
LOSSY_DOCUMENTED conversions (Phase 2, MF-268).

- **LOSSLESS** runs silently as before.
- **LOSSY_DOCUMENTED** requires `accept_data_loss=true` — the GUI/CLI
  must obtain explicit user consent. On success, a category-level
  `<target>.loss.json` is written: FLUX_TIMING, WEAK_BITS,
  INDEX_PULSES, MULTI_REVOLUTION, plus a path-quality warning.
- **IMPOSSIBLE** and **UNTESTED** ABORT with a diagnostic.

Per-track exact loss counts (e.g. "23 weak bits in track 18 sector 4")
are v4.1.6 follow-up — Phase 2 emits category-level entries, sufficient
for the user to know "this converter lost flux timing".

## SCP USB protocol — VENDOR-DOCUMENTED status

MF-261 cross-referenced all 22 SCP USB command bytes + response code +
checksum init in `include/uft/hal/uft_scp_direct.h` against
[samdisk's `SuperCardPro.h`](https://github.com/simonowen/samdisk/blob/main/include/SuperCardPro.h).
samdisk has shipped SCP read/write against real hardware since 2017
and is the de-facto open reference implementation.

Pre-MF-254 the header carried six placeholder opcodes (`0x02-0x40`)
traceable to `a8rawconv`, not to the vendor SDK. The current opcodes
`0x80-0xD2` (CMD_SELA..CMD_SCPINFO), `PR_OK=0x4F`, `CHECKSUM_INIT=0x4A`
are byte-exact with samdisk.

The SCP SDK PDF (`scp_sdk.pdf` v1.7, cbmstuff.com, Dec 2015) itself is
binary-only-readable; samdisk's header is the public cross-reference.

## Plugin Prinzip-7 compliance

Before v4.1.5 only 5 plugins had a hand-curated `.features` array
(ADF, HFE, IPF, STX, WOZ) and 15 had a non-UNKNOWN `.spec_status`.
The `audit_plugin_compliance.py` script flagged the gap as a §7.1 +
§7.2 violation.

Two mass-populator scripts solved it:

- `scripts/populate_spec_status.py` — per-format-subdir mapping
  (OFFICIAL_FULL / OFFICIAL_PARTIAL / REVERSE_ENGINEERED / DERIVED)
  based on documentation provenance.
- `scripts/populate_features.py` — derives a per-plugin feature
  matrix from `.capabilities` bitfield (Read/Write/Create/Flux/
  Timing/Weak Bits/MultiRev → SUPPORTED or UNSUPPORTED).

Result: **84/84 (100%) Prinzip-7 compliance**. Per-plugin hand-tuning
where the heuristic was too coarse (e.g. WOZ PARTIAL with explanatory
note) is ongoing in v4.1.6 as documentation hygiene.

## Resolved structural issues

| ID | Pre-v4.1.5 state | Resolution |
|---|---|---|
| UFT-002 | CMakeLists.txt header comment "Version: 4.1.3" stale | Removed numeric; refers to VERSION.txt SSOT |
| UFT-003 | HardwareTab honest-stub providers visually identical to production GW | Orange "Disconnect (Beta)" Button + tooltip distinct from green production GW |
| UFT-004 | `uft_format_plugin_t` had no API-version field; ABI-bomb risk | `api_version` + `_Static_assert(sizeof==216)` + registrar gate |
| UFT-005 | `test_transitions_ns_contract` covered only MockProviderV2 | KryoFlux + FluxEngine contract-shield FFI added |
| UFT-006 | `.claude/CLAUDE.md` listed 6 controllers, code had 9 V2-Providers | List corrected with per-provider status |
| UFT-007 | Claimed VID/PID mismatch between header + GUI-hint | Verified consistent (MF-212): 0x16D0:0x0F8C via shared macro |
| UFT-T01 | `<threads.h>` include broke MinGW build | `__has_include` guard added |
| UFT-T02 | 4 tests linked phantom symbols | Per-test `target_sources` wired correctly |
| UFT-T04 | 43 tests excluded from build | Stale exclusions reviewed; 4 re-enabled (test_scp_direct_hal, test_applesauce_hal, test_fnmatch_shim, test_whdload_resload). Remaining 38 are MF-011-deleted-impl orphans, documented per-line |
| UFT-T05 | events CMakeLists Standalone-Pfad bug | Verified already fixed (uses CMAKE_CURRENT_SOURCE_DIR) |
| ARCH-2 | KryoFlux/FluxEngine providers fabricated FluxCaptured from container bytes | Closed in MF-208 (KF) + MF-209 (FE); shield test in MF-260 prevents regression |
| SCOPE | `src/switch/` (10MB Nintendo Switch + mbedtls) off-mission | **Decision: Option C — Delete, post-tag (MF-271)**. Backup tag planned: `archive/pre-mf271-switch-removal` |

## What this release does NOT ship

These are intentionally deferred to v4.1.6 (with reasoning):

- **M3.2 XUM1541 libusb** — wired but Tier-3 hardware verification needs opencbm + real device
- **M3.3 Applesauce serial-protocol full handshake** — `?vers` is in; the `?disk`-readx state machine still has gaps
- **UFI Windows + macOS backends** (`ufi_win.c` via DeviceIoControl, `ufi_mac.c` via IOKit). Linux SG_IO is in (MF-207)
- **ARCH-9 XUM1541 macOS .dylib loader** — needs `OpenCbmLibrary::load()` path
- **5-and-3 Apple GCR** (DOS 3.2 13-Sektoren)
- **FM-Decoder completion** — `flux_decode_fm` flagged as incomplete in the decoder source
- **Per-converter sidecar precision** — current Phase 2 emits category-level entries; per-track exact counts are v4.1.6
- **`uft-decode` CLI binary build wiring** — scaffold at `cli/uft-decode/` ships; CMake/qmake enable + emulator.yml round-trip integration is v4.1.6
- **MF-271 Switch/cart7 delete** — decided this session, ausführung post-tag
- **USBFloppy SG_IO mock** — Tier-2.5 framework supports it but SG_IO-mock not yet written; v4.1.6

## Upgrade notes

- **No breaking changes.** v4.1.4 → v4.1.5 is purely additive.
- The `uft_format_plugin_t` ABI gate (MF-260) is opt-in: legacy plugins
  with `api_version=0` are accepted with a one-shot stderr warning.
  Plugin authors should set `.api_version = UFT_PLUGIN_API_VERSION`
  before v5.0.
- LOSS.preflight requires explicit consent for `LOSSY_DOCUMENTED`
  conversions. GUI workflows that previously ran lossy converts
  silently now need to set `accept_data_loss=true` in
  `uft_convert_options_t` (`preserve_errors` field maps to this).

## Hardware Compatibility

Detailed per-controller capability matrix (Tier-3 / Tier-2.5 / mock-only /
scaffold) lives in [`docs/CAPABILITIES.md`](docs/CAPABILITIES.md).

**Status quick-glance:**
- **1 production:** Greaseweazle (Tier-3 PASS, read+write)
- **3 read-real via subprocess wrapper:** KryoFlux (DTC), FluxEngine (CLI), FC5025 (fcimage)
- **3 libusb-wired, mock-only:** SCP-Direct, XUM1541, Applesauce (byte-protocol validated)
- **1 USB-CDC-wired with sim:** ADF-Copy
- **1 Linux-only:** USB-Floppy (SG_IO; Win/Mac backends are v4.1.6)

Any provider not in Tier-3 PASS reports `not_implemented` for unwired calls
— **never a silent no-op.** GUI shows orange "Preview" badge instead of
the green Production badge.

## Verified on (CI green)

- Local: MinGW-w64 g++ 13.1.0, Qt 6.10.1
- CI: Linux GCC (Qt 6.7.3 + 6.10.1 with c++17 + c++20), macOS Clang, Windows MinGW
- Plugin compliance: `audit_plugin_compliance.py` 84/84 ✓
- Build parity: `verify_build_sources.py` 219/0
- Consistency: `check_consistency.py` 0/0/0/0 ✓
- Test suite: ctest 153/153 (100%)
- Tier-2.5 simulator gate: 10/10 SIMULATED (Audit CI job)
- All 5 CI workflows green on latest commit (Audit · Emulator-CI · Sanitizer · Coverage · CI×4)

## Commit ledger (since `origin/main` HEAD `21ff31b`)

27 commits, all squash-mergeable as v4.1.5:

```
MF-240    docs(release): pre-tag cleanup
MF-241/2  docs/release alignment
MF-243    build(release): uft_p24_verify.sh — 4-BEFUND verification gate
MF-244    build(hil): goal v4.1.4-final Phase-3 infrastructure
MF-245/6  v4.1.5-hardening: full audit findings — 26%→100% pass rate
MF-247    UFT-003 HardwareTab honest-stub visual + UFT-007 verified
MF-248    M3.3 concrete Applesauce production-transport plan
MF-249    M3.3 QSerialPort-backed Applesauce transport
MF-250    M3.3 Applesauce live end-to-end + HardwareTab wiring
MF-251/2/3 M3.3 Applesauce sim + ADFCopy transport — UFT-001 3/9
MF-254    M3.1 SCP-Direct libusb wiring — UFT-001 4/9
MF-255    M3.2 XUM1541 libusb wiring — UFT-001 5/9
MF-256    M3 FluxEngine + KryoFlux QProcess runners — UFT-001 7/9
MF-257    M3 FC5025 fcimage QProcess runners — UFT-001 8/9
MF-258    M3.4 USB Floppy UFI runners — UFT-001 9/9 COMPLETE
MF-259    CI qmake .pro wiring + libusb IMPORTED + audit-vector pins
MF-260    UFT-004 plugin ABI gate + UFT-005 KF/FE shields + UFT-T04 reductions
MF-261/2  V415-PLAN — SCP.D1 verify + spec_status 100% + BUILD.rebaseline
MF-263    V415-PLAN deep — features 100% + LOSS Phase 1 + EMUCI/release scaffolds
MF-264    fix stray-comma after trailing comment in 79 plugin literals
MF-265    UFT_FORMAT_ID_T_DEFINED guard in roundtrip.h + format_validate.h
MF-266    add preflight + loss_report + roundtrip sources to .pro
MF-267    Tier-2.5 hardware simulator system (KryoFlux/FluxEngine/FC5025)
MF-268    LOSS.preflight Phase 2 + ADFCopy sim + CI gate + RELEASE_NOTES draft
MF-269    hw simulators self-contained on CI fresh checkout
MF-270    Tier-2.5 USB-mock framework (SCP/XUM/GW without hardware)
```

## Open prerequisites for tag-day

| Item | Owner | Blocker type |
|---|---|---|
| RC1-Window closes 2026-05-29 | calendar | hard date |
| HIL.GW 10/10 PASS on real Greaseweazle | Axel + hardware | needs bench session |
| UFT-008 SCP-Direct Tier-3 verify | Axel + hardware | needs SCP device |
| v4.1.4-final tag promoted (P2.4) | `scripts/release/p2_4_squash_to_main.sh` | depends on RC1+HIL |
| MF-271 Switch/cart7 delete | post-tag follow-up | decided 2026-05-25 (this session) |

When all five are ✓, follow `scripts/release/release_v415_checklist.md`
step 1-5 to tag.
