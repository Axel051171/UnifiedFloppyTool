# UnifiedFloppyTool v4.1.4 Release Notes

**Release Date:** 2026-05 (`v4.1.4-rc1` pre-release tag first;
14-day window; promote to `v4.1.4` final on success).
**Branch:** `refactor/type-driven-hal` → squash-merge to `main` at
v4.1.4 final.
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

## Known limitations / queued for v4.1.5

- `FluxCaptureJob` and `FluxWriteJob` still call `uft_gw_*` directly
  through a non-owning handle accessor — they will be migrated to
  the V2 outcome surface in P1.20 / P1.21.
- The 8 non-Greaseweazle controllers' V2 routing in `HardwareTab`
  is queued as P1.23 (a `std::variant<unique_ptr<*>>`-shaped
  dispatch over all V2 provider types).
- `GreaseweazleProviderV2::raw_handle()` legacy escape hatch will
  be removed once P1.20 / P1.21 land (P1.22).

These are queued architecture refinements, not user-visible
regressions. The v4.1.4 release is shippable today as-is.

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
