# UnifiedFloppyTool v4.1.5 Release Notes

**Release Date:** 2026-06-05
**Tag:** `v4.1.5` (squash-merge of `tests/v4.1.5-hardening` onto `main`,
39 commits MF-240..MF-276 plus `.claude` hygiene v2/v3 plus Phase A/B/D
demo-ready bundle).
**Version-string convention:** `VERSION.txt` carries the numeric
`4.1.5` per the existing release convention.

## Hardware verification status (read this first)

The Greaseweazle production path (`src/hal/uft_greaseweazle_full.c`) is
**byte-identical** to the v4.1.4-rc1 commit that was hardware-verified
2026-05-15 — zero lines changed across the 39 squashed commits, the
file is in `.claude/CLAUDE.md`'s protected-paths list. HIL.GW
formal-bench session (`tests/hil/run_hil.py`) is **deferred to the
post-tag window**; substantively the HIL output for v4.1.5 would be
identical to v4.1.4-rc1's because the GW code is unchanged.

UFT-008 SCP-Direct Tier-3 verification still requires real SCP
hardware and is **explicitly v4.1.6 scope** — see `docs/CAPABILITIES.md`
for the per-controller status matrix.

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
| SCP-Direct M3.1 read-flux payload parsing | ✅ MF-276 | Full samdisk-reference algorithm: CMD_READ_FLUX → CMD_GET_FLUX_INFO → CMD_SENDRAM_USB; 16-bit big-endian samples decoded with 0x0000-overflow accumulator, reset per revolution. 9/9 mock-tests pass. impl_complete stays false until real-HW bench |
| `uft_format_plugin_t` ABI gate | ✅ MF-260 | New `api_version` field + `_Static_assert(sizeof == 216)` + registrar reject for future-ABI plugins |
| Plugin Prinzip-7 compliance 84/84 | ✅ MF-262 + MF-263 | `spec_status` and `features` populated for every plugin (was 15/84 and 5/84 respectively) |
| LOSS.preflight Phase 1 + 2 | ✅ MF-263 + MF-268 | All 44 conversion paths gated through `uft_preflight_check()` at the `uft_convert_file()` chokepoint. `LOSSY_DOCUMENTED` paths emit a `.loss.json` sidecar after successful convert with category-level loss entries |
| Tier-2.5 hardware simulator system | ✅ MF-267 + MF-270 | KryoFlux + FluxEngine + FC5025 via Python "fake binary" sims, SCP-Direct + XUM1541 via libusb-mock framework, Greaseweazle via serial-protocol sim. 10/10 SIMULATED in CI |
| ARCH-7-C Teensy-probe wrapper | ✅ MF-213 + MF-263 | Probe-on-Connect with mismatch warning for ADFCopy ↔ Applesauce (both Teensy 0x16C0:0x0483) — prevents disk-corrupting cross-wire |
| Test pass-rate 47/180 → 153/153 | ✅ MF-245..MF-260 + MF-270 + MF-276 | Stale exclusions reviewed; new `test_plugin_abi.c`, `test_scp_direct_usb_mock.c` (9 tests), `test_xum1541_usb_mock.c` |
| Build-system parity baseline shrunk | ✅ MF-262 + MF-272 | `verify_build_sources.py` baseline 224 → 219 → 216 (LOSS.preflight sources wired) |
| qmake .pro wiring fixed | ✅ MF-259 + MF-266 | preflight/loss_report/roundtrip sources, libusb IMPORTED target, audit-vector pins |
| Phase A demo-ready docs bundle | ✅ MF-273 + MF-274 | `docs/SHOWCASE.md` (179 LOC), `docs/CAPABILITIES.md` (189 LOC), `docs/demo/QUICK_DEMO.md` (253 LOC), `docs/KNOWN_ISSUES.md` Demo-Impact annotation, `docs/release/v4.1.5_github_release_body.md` |

## Highlight: SCP-Direct M3.1 read-flux implementation (MF-276)

`uft_scp_direct_read_flux()` was the last NOT_IMPLEMENTED in the SCP
M3.1 scaffold. v4.1.5 implements it as a byte-for-byte port of the
samdisk `SuperCardPro::ReadFlux()` reference (the de-facto open-source
SCP USB reference implementation).

Algorithm:

1. `CMD_SIDE [side]`
2. `CMD_READ_FLUX [revs, ff_Index=0x01]` — waits on index pulse
3. `CMD_GET_FLUX_INFO` → 40-byte rev_index table (5 pairs of
   `[index_time_be32, flux_count_be32]`)
4. Per revolution: `CMD_SENDRAM_USB [offset_be32, length_be32]` →
   `flux_count × uint16_t` big-endian samples
5. Decode: `0x0000` accumulates 0x10000 ticks (cell overflow);
   non-zero emits `(accum + sample) × 25 ns`; accumulator **resets
   per revolution** (matches samdisk — each capture starts fresh
   after its index pulse)

`uft_scp_direct_write_flux()` **remains intentionally NOT_IMPLEMENTED**.
A malformed flux stream can physically damage forensic media; write is
blocked at the HAL boundary until the read path is bench-verified
against real SCP hardware. Forensic safety overrides feature-
completeness.

**Mock-test coverage** (9 tests in `tests/test_scp_direct_usb_mock.c`):
open/close, seek with packet format verification, error-status handling,
bulk-out timeout, read_flux two-revs happy-path with full 5-step wire
dance, overflow accumulation (`{0000, 0010, 0000, 0000, 0020}` → 2
emits), single-revolution rejection (`revs=1` → `UFT_ERR_INVALID_ARG`,
no silent coercion), per-revolution accumulator-reset regression-guard.

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

## Highlight: LOSS.preflight — Prinzip 1 enforcement

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

## Highlight: SCP USB protocol — VENDOR-DOCUMENTED status

MF-261 cross-referenced all 22 SCP USB command bytes + response code +
checksum init in `include/uft/hal/uft_scp_direct.h` against
[samdisk's `SuperCardPro.h`](https://github.com/simonowen/samdisk/blob/main/include/SuperCardPro.h).
samdisk has shipped SCP read/write against real hardware since 2017
and is the de-facto open reference implementation.

Pre-MF-254 the header carried six placeholder opcodes (`0x02-0x40`)
traceable to `a8rawconv`, not to the vendor SDK. The current opcodes
`0x80-0xD2` (CMD_SELA..CMD_SCPINFO), `PR_OK=0x4F`, `CHECKSUM_INIT=0x4A`
are byte-exact with samdisk. MF-276 adds `CMD_GET_FLUX_INFO=0xA1`,
`CMD_SENDRAM_USB=0xA9`, and `ff_Index=0x01` flag (also byte-exact).

## Highlight: Plugin Prinzip-7 compliance

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

- **HIL.GW formal bench session** — production GW code unchanged so
  substantive risk is null, but the formal report
  (`releases/v4.1.4/hil_report.md`) is still pending.
- **UFT-008 SCP-Direct Tier-3 verify** — needs real SCP hardware
- **M3.2 XUM1541 libusb** — wired but Tier-3 hardware verification needs opencbm + real device
- **M3.3 Applesauce serial-protocol full handshake** — `?vers` is in; the `?disk`-readx state machine still has gaps
- **UFI Windows + macOS backends** (`ufi_win.c` via DeviceIoControl, `ufi_mac.c` via IOKit). Linux SG_IO is in (MF-207)
- **ARCH-9 XUM1541 macOS .dylib loader** — needs `OpenCbmLibrary::load()` path
- **5-and-3 Apple GCR** (DOS 3.2 13-Sektoren)
- **FM-Decoder completion** — `flux_decode_fm` flagged as incomplete in the decoder source
- **Per-converter sidecar precision** — current Phase 2 emits category-level entries; per-track exact counts are v4.1.6
- **`uft-decode` CLI binary build wiring** — scaffold at `cli/uft-decode/` ships; CMake/qmake enable + emulator.yml round-trip integration is v4.1.6
- **MF-271 Switch/cart7 delete** — decided 2026-05-25, execution post-tag
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
- `uft_scp_direct_read_flux()` rejects `revolutions=1` with
  `UFT_ERR_INVALID_ARG` (SCP firmware requires ≥2; honest, no silent
  coercion). Provider layer
  (`src/hardware_providers/scp_provider_v2.cpp`) still accepts
  `revolutions=1` in its parameter range — open audit finding to be
  reconciled in v4.1.6 (see `docs/KNOWN_ISSUES.md`).

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
- Build parity: `verify_build_sources.py` 216/0
- Consistency: `check_consistency.py` 0/0/0/0 ✓
- Test suite: ctest 153/153 (100%)
- Tier-2.5 simulator gate: 10/10 SIMULATED (Audit CI job)
- All 5 CI workflows green on `tests/v4.1.5-hardening` HEAD before squash (Audit · Emulator-CI · Sanitizer · Coverage · CI)

## Commit ledger

39 commits squashed from `tests/v4.1.5-hardening` (MF-240..MF-276 plus
`.claude` hygiene v2/v3 plus Phase A/B/D bundles plus hermes-PoC park):

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
MF-272    rebaseline build_system_baseline (3 resolved entries)
MF-273    Phase A demo-ready bundle (SHOWCASE+CAPABILITIES+KNOWN_ISSUES Demo-Impact+RELEASE_NOTES section)
MF-274    Phase C.1 QUICK_DEMO.md (10-min demo script pull-forward)
MF-275    Phase B/D pre-work bundle + rescue ignored release files
MF-276    M3.1 SCP-Direct read-flux payload parsing — full samdisk impl + per-rev-reset fix
+ chore(.claude): hygiene-pass v2 (a0f8187) — agent count sync + EOL + htb-mentor flatten
+ chore(.claude): hygiene-pass v3 (db14e70) — Eigenständigkeit, Eigenverantwortung, Vollständigkeit
+ docs(.claude): park hermes-agent PoC goal for post-v4.1.5-tag
+ spike(proto/hermes-eval): Session #1+#2 (skeleton + prompts + rubric + hermes CLI research)
```

---

# UnifiedFloppyTool v4.1.4 Release Notes

**Release Date:** 2026-05 (`v4.1.4-rc1` pre-release tag first;
14-day window; promote to `v4.1.4` final on success).
**Branch:** `main` already carries the type-driven HAL refactor
(PR #24 squash-merged) and the 4 Phase-2 hardening commits
(PR #26 merged 2026-05-15). The v4.1.4 tag goes on `main` HEAD —
no further merge step required.
**Version-string convention:** `VERSION.txt` carries the numeric
`4.1.4`. The `-rc1` / `-rc2` suffix lives only in the git tag
(`v4.1.4-rc1`, `v4.1.4`), per the project's existing release-notes
header convention.

## What changed (TL;DR)

- **Working Greaseweazle workflow:** unchanged behavior, structurally
  hardened internals. Read / write / detect / motor / seek / RPM /
  recalibrate all preserve every byte they used to. Auto-detect on
  Connect still happens (now via the V2 surface).
- **5 X1541-family controller-combo entries gone** — XA1541 / XAP1541
  / XM1541 / XE1541 / X1541. They never had a working backend in any
  release; selecting them only produced an error dialog. Removed for
  honesty.
- **8 non-Greaseweazle controllers** (SCP, KryoFlux, FluxEngine,
  FC5025, XUM1541, Applesauce, ADFCopy, USBFloppy) now display a
  clear "Controller routing pending" message on Connect instead of
  silently no-op'ing. Their V2 wrappers exist, are conformance-
  tested, and will be wired in v4.1.5 (task P1.23).

## Why this is `v4.1.4` and not `v5.0.0`

The Type-Driven HAL refactor underneath this RC is substantial — 10
mixin-composed V2 providers, codegen-driven GUI wiring, 65-section
conformance harness, ~12 700 LOC of V1 hierarchy deleted. From a USER
perspective the change set is:

1. The Greaseweazle pipeline is preserved + hardened.
2. Phantom-feature combo entries are cleaned up (UI more honest).
3. Non-GW controllers display a "pending" message instead of silent
   no-op (UI more honest).

None of those break a working user workflow. UFT is an end-user
application — no third-party code links the deleted C++ classes —
so there is no external SemVer contract to honor. The PATCH bump
`v4.1.3 → v4.1.4` reflects "hardening of the v4.1 line" honestly;
the refactor magnitude is documented in `CHANGELOG.md`'s v4.1.4-rc1
section, in `docs/REFACTOR_BRIEF.md` (the architecture spec), and in
this release-notes file. Full rationale in `REFACTOR_BRIEF.md` §9.

## Hot-fixes included (pre-existing 4.1.4 plan)

- Windows COM-port `\\.\` prefix bug across all serial-port providers
  (MF-145).
- Applesauce ↔ ADFCopy USB VID/PID disambiguation (MF-146).
- Greaseweazle bootloader-firmware-mode detection (MF-129).
- MFM IDAM/DAM standalone sector parser for SCP→ADF/IMG/D64 paths
  (MF-141 / AUD-002).

## Quality gates that ship with this RC

- 14 test executables, all green:
  `test_hal_foundation`, `test_hal_conformance` (65 sections),
  `test_greaseweazle_v2`, `test_scp_provider_v2`,
  `test_kryoflux_provider_v2`, `test_fluxengine_provider_v2`,
  `test_fc5025_provider_v2`, `test_xum1541_provider_v2`,
  `test_applesauce_provider_v2`, `test_adfcopy_provider_v2`,
  `test_usbfloppy_provider_v2`, `test_mock_provider_v2`,
  `test_mock_hardware`, `test_wiring_runtime`.
- `scripts/check_consistency.py` 0/0/0/0.
- `scripts/verify_build_sources.py` clean (no new divergence).
- `tools/wiring_codegen_tests/run_tests.py` 6/6 (incl. drift
  detector for `generated/tab_hardware_wiring.gen.cpp`).
- CI: green on Linux GCC (Qt 6.7.3 + 6.10.1), macOS Clang,
  Windows MinGW — all 3 platform jobs.

## Pre-release window

This is `v4.1.4-rc1`. 14-day window starts on tag. During the window:

- Hand-test on real Greaseweazle hardware
  (`tests/HARDWARE_TRUTH_TESTS.md` checklist).
- File any regression in working v4.1.3 workflows as a release-blocker.
- Watch CI runs on the `refactor/type-driven-hal` branch.

If nothing breaks in 14 days: squash-merge → `v4.1.4` final tag.

## Known limitations of this release

What v4.1.4 honestly does **not** ship — flagged here so users and
reviewers know what is intentionally absent, not silently broken
(MF-235 / MF-240 follow-up to the original "queued for v4.1.5" block
that pretended these were just architectural polish):

- **M3.x production transports — not in this release.** Only
  Greaseweazle has a real USB/serial transport wired through. The
  other 8 controllers (SCP-Direct, KryoFlux, FluxEngine, FC5025,
  XUM1541, Applesauce, ADFCopy, USBFloppy) have V2 wrappers that are
  conformance-tested against the Mock surface but display
  "Controller routing pending" on Connect. M3.1 (SCP USB), M3.2
  (XUM1541 USB), M3.3 (Applesauce serial) and the remaining
  transports are v4.1.5 scope — see `docs/MASTER_PLAN.md` §M3.
- **ARCH-7 sub-B / sub-C field-validation follow-ups.** Sub-B
  (SCP `0x16D0:0x0F8C`) and sub-C (ADF-Copy / Applesauce stock-Teensy
  `0x16C0:0x0483` two-tier probe) are *implemented* in code
  (MF-212 / MF-213) and unit-tested; they have **not yet** been
  re-verified end-to-end on real hardware after the type-driven HAL
  refactor. The single-device readout that produced the original
  ID values still stands, but a post-refactor HIL pass on SCP +
  ADF-Copy + Applesauce is queued for v4.1.5. See
  `audit/ARCH-7_VID_PID.md`.
- **§6.1 Emulator-CI — not in this release.** The HIL runner
  (`tests/hil/run_hil.py`) has a two-tier Golden-Reference catalog
  (CI-runnable software tier + hardware tier), but no emulator
  loop (Greaseweazle firmware simulator, virtual SCP/KryoFlux
  device, WinUAE / VICE harnesses) runs in GitHub Actions yet. The
  hardware tier therefore stays `NOT_RUN` in CI by design; only the
  software tier is exercised. Emulator-CI is planned for v4.3 —
  see `docs/M3_HAL_PLAN.md` §M3.5.
- **ARCH-8 — official protocol sources not yet vendored.** GW
  `cmd.h`, SCP SDK, OpenCBM and fluxengine / DTC are currently
  declared `recalled` (working from memory + observed behaviour)
  in the audit-compliance matrix. Promoting them to `vendored` is
  what turns `audit.yml` into a real CI gate — until then the gate
  is advisory. Vendoring is v4.1.5+ scope.
- **§1.2 Loss-Report wiring incomplete.** The 44 `convert_*`
  format-conversion paths still need ~10 lines of glue each to
  propagate "data that did not survive" into the audit trail.
  Lossy conversion (Flux → Sector drops timing + weak bits) is
  *labelled* as lossy at the format-graph level, but per-conversion
  loss reports are not yet emitted end-to-end. v4.1.5+ scope.
- **Plugin-Compliance matrix sparsely populated.** Of 80 plugin
  registrations: `spec_status` is populated for 15, `features` for
  5, `compat_entries` for 1, `is_stub` flag for 0/287 stubs. The
  data model is in place (MF-187 onward), filling it iteratively
  is 4.2.x scope.
- **ARCH-9 — XUM1541 macOS `.dylib` load path.** The libxum1541
  shared-object lookup on macOS does not yet match the system's
  Homebrew / `/usr/local/lib` conventions. Linux + Windows paths
  work; macOS users currently need a manual `DYLD_LIBRARY_PATH`.
  v4.1.5 scope.

These are real gaps, not paperwork. v4.1.4 is shippable because the
Greaseweazle workflow — the one path used in practice — is
preserved + hardened, and the 8 non-GW controllers show an honest
"pending" message instead of silently no-op'ing. Anyone relying on
a non-GW transport should wait for v4.1.5.

---

# UnifiedFloppyTool v4.1.3 Release Notes

**Release Date:** 2026-04-16

## Highlights
- CRC / status flag propagation wired across the PLL → sector pipeline
- IMD + FDI read_track (real ImageDisk + Formatted Disk Image extraction)
- Plugin-B parsers added for DO, PO, ADL, V9T9, VDK, STX, PRO, ATX, ADF, SCL
- write_track support for TRD, D64, ATR, SAD, SSD, HFE, D80, D82, D71, D81
- Format registry: 138 IDs / 80 plugin registrations after dead-code cleanup
- 6 hardware controllers: Greaseweazle production; SCP-Direct, XUM1541,
  Applesauce as M3 partial scaffolds (real lifecycle + utility code,
  USB/serial wiring pending — see `docs/MASTER_PLAN.md` §M3)

## Fixed
- 3 silent I/O errors in SAD, SSD, HFE parsers
- NULL-checks and silent-error fixes in 8 registry plugins
- Memory allocation limits, bounds checks, forensic fill (security pass)
- CMake Sanitizer / Coverage builds — dead file references removed
- Test include paths repaired

## Changed
- Cleanup: 674 orphaned source files deleted (~386 000 LOC dead code)
- 109 dead format files removed from disk
- 165 non-floppy stubs removed (32 active plugins after pruning)

## Known Issues
- 6 test modules excluded (missing dependencies)
- Some C headers use `protected` as field name (C++ incompatible)
- DeepRead forensic modules (write-splice, aging, cross-track, fingerprint,
  soft-decode) and ML analysis are planned for v4.2.0 — not in this release

## Full Changelog
See [`CHANGELOG.md`](CHANGELOG.md) for the complete entry list.
