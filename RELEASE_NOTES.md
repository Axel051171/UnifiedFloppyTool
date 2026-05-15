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
