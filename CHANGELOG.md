# Changelog

## [4.1.5] - 2026-06-05

### Added
- **M3.1 SCP-Direct read-flux full implementation** (MF-276): byte-for-byte
  port of samdisk `SuperCardPro::ReadFlux()` reference. CMD_READ_FLUX +
  CMD_GET_FLUX_INFO + per-revolution CMD_SENDRAM_USB; 16-bit big-endian
  samples decoded with 0x0000-overflow accumulator (resets per
  revolution per index-pulse semantics). 9-test mock-coverage including
  per-rev-reset regression-guard. `write_flux` remains intentionally
  blocked until real-HW bench verification (forensic safety).
- **Tier-2.5 hardware simulator system** (MF-267 + MF-270): 10/10
  SIMULATED coverage. KryoFlux/FluxEngine/FC5025 via Python fake-binary
  scripts; SCP-Direct + XUM1541 via novel libusb-mock framework
  (`tests/usb_mock/`, pre-defines `LIBUSB_H` guard); Greaseweazle via
  USB-CDC serial protocol simulator. SIMULATED is never reported as
  PASS — real hardware (Tier 3) remains the only PASS authority.
- **LOSS.preflight Phase 1 + 2** (MF-263 + MF-268): all 44 conversion
  paths gated through `uft_preflight_check()` at the
  `uft_convert_file()` chokepoint. `LOSSY_DOCUMENTED` paths emit a
  category-level `.loss.json` sidecar after successful convert.
  `LOSSY_DOCUMENTED` requires explicit `accept_data_loss=true`.
- **22/22 SCP USB opcodes byte-verified** (MF-261): cross-referenced
  against `samdisk/SuperCardPro.h`. Plus 4 new opcodes for read-flux
  payload (`CMD_GET_FLUX_INFO=0xA1`, `CMD_SENDRAM_USB=0xA9`, flag bits).
- **Plugin Prinzip-7 compliance 84/84** (MF-262 + MF-263): every plugin
  has populated `spec_status` (was 15/84) and `features` (was 5/84) via
  `scripts/populate_spec_status.py` + `scripts/populate_features.py`.
- **`uft_format_plugin_t` ABI gate** (MF-260): new `api_version` field
  + `_Static_assert(sizeof == 216)` + registrar reject for future-ABI
  plugins. Opt-in: legacy plugins with `api_version=0` accepted with
  one-shot stderr warning. Mandatory in v5.0.
- **ARCH-7-C Teensy-probe wrapper** (MF-213 + MF-263): probe-on-Connect
  with mismatch warning for ADFCopy ↔ Applesauce (both Teensy
  0x16C0:0x0483) — prevents disk-corrupting cross-wire.
- **Demo-ready docs bundle** (MF-273 + MF-274): `docs/SHOWCASE.md`,
  `docs/CAPABILITIES.md`, `docs/demo/QUICK_DEMO.md`,
  `docs/release/v4.1.5_github_release_body.md`,
  `docs/demo/TESTER_REPORTS.md` template.
- **Phase B/D release-operations infrastructure** (MF-275):
  `scripts/release/p2_4_squash_to_main.sh`,
  `scripts/release/release_v415_checklist.md` — both files were on disk
  but silently gitignored by an over-broad `release/` rule (qmake
  artifact pattern). Negation rules `!docs/release/` + `!scripts/release/`
  rescue them into the repo.
- **9/9 V2 providers live code path** (MF-249..MF-258): no silent
  no-op when user clicks "Connect" on a non-Greaseweazle controller.
- **`.claude` hygiene-pass v2 + v3**: agent-count sync (21 = 13 + 8
  refactor), EOL normalization, model-string alias normalization,
  htb-mentor flatten. v3 adds constitutional section
  "Eigenständigkeit, Eigenverantwortung, Vollständigkeit" overriding
  all other rules: no new stubs in newly-written code (sole exception:
  honest-stub for unwired hardware providers); 7-anti-pattern table;
  Definition-of-Done per code-type; scope-rule (>150 LOC → STOP);
  Hard-Rule #6 STOP-Gründe enumerated; CONSULT-Protocol autonomy
  boundary; stub-eliminator dual-role (reactive + proactive
  diff-review); quick-fix anti-stub-as-fix; team-onboarding
  knowledge-check.

### Fixed
- **SCP read_flux accumulator reset per revolution** (MF-276 follow-up):
  initial implementation persisted overflow accumulator across
  revolution boundaries; reference samdisk impl resets per revolution
  because each capture starts fresh after its index pulse. Regression-
  guard test locks in the correct semantic.
- **MFM/AmigaMFM/Apple/C64-GCR decoder shields** (MF-260 / UFT-005):
  `test_transitions_ns_contract` extended with KryoFlux + FluxEngine
  contract-shield FFI.
- **stray-comma after trailing comment in 79 plugin literals**
  (MF-264).
- **`UFT_FORMAT_ID_T_DEFINED` guard** in `roundtrip.h` +
  `format_validate.h` (MF-265).
- **qmake .pro wiring**: preflight/loss_report/roundtrip sources,
  libusb IMPORTED target, audit-vector pins (MF-259 + MF-266).
- **HardwareTab honest-stub visual** (MF-247 / UFT-003): orange
  "Disconnect (Beta)" Button distinct from green production GW.
- **CMakeLists.txt version comment stale** (MF-247 / UFT-002): removed
  numeric; refers to VERSION.txt SSOT.
- **`<threads.h>` MinGW build** (UFT-T01): `__has_include` guard.
- **4 tests phantom-symbol link errors** (UFT-T02): per-test
  `target_sources` wired correctly.
- **build-system baseline drift** (MF-262 + MF-272): rebaseline
  224 → 219 → 216.

### Changed
- **Test pass-rate 47/180 → 153/153** (100%). Stale exclusions
  reviewed; new test executables: `test_plugin_abi`,
  `test_scp_direct_usb_mock` (9 tests), `test_xum1541_usb_mock` (3 tests).
- **Excluded tests reduced 43 → 38** (UFT-T04): re-enabled
  `test_scp_direct_hal`, `test_applesauce_hal`, `test_fnmatch_shim`,
  `test_whdload_resload`. Remaining 38 reference MF-011-deleted impls,
  documented per-line.
- **`.claude/CLAUDE.md` Agent-Liste** (UFT-006): 6 → 9 V2-providers
  reflected.
- **CHANGES file naming**: `.claude/CHANGES_v2.md` → `.claude/CHANGES.md`
  with v3 delta appended (single audit trail).

### Documentation
- **SCOPE decision**: `src/switch/` + `src/cart7/` + Switch GUI tab
  deletion deferred to MF-271 post-tag (per Axel 2026-05-25). Backup
  tag `archive/pre-mf271-switch-removal` planned before delete.
- **KNOWN_ISSUES.md Demo-Impact annotation**: 10 open issues annotated
  with `Demo-Impact: keiner | Workaround | vermeiden` field.
- **hermes-agent PoC park** (post-v4.1.5-tag):
  `.claude/goals/v4.1.6-hermes-agent-poc.md` documents the GO/NO-GO
  evaluation plan. Bootstrap done under `proto/hermes-eval/` (skeleton +
  prompts + rubric + benchmark templates); actual benchmark runs are
  v4.1.6+.

### Hardware verification status
- Greaseweazle production code (`src/hal/uft_greaseweazle_full.c`):
  **zero lines changed** since v4.1.4-rc1 hardware verification
  (2026-05-15). HIL.GW formal bench session deferred to post-tag —
  substantively identical output expected to v4.1.4-rc1.
- UFT-008 SCP-Direct Tier-3 verify: explicitly v4.1.6 (needs real SCP
  hardware bench session). `impl_complete` flag stays `false` in
  `uft_scp_direct_get_capabilities()` until then.

### Known Limitations (deferred to v4.1.6)
- M3.2 XUM1541 real-HW Tier-3 (libusb wired, opencbm needed for bench)
- M3.3 Applesauce `?disk` read state-machine completion
- UFI Windows + macOS backends (DeviceIoControl + IOKit)
- ARCH-9 XUM1541 macOS `.dylib` loader
- 5-and-3 Apple GCR (DOS 3.2 13-sector)
- FM-decoder completion (`flux_decode_fm`)
- Per-track exact loss counts (current Phase 2 is category-level)
- `uft-decode` CLI build wiring (scaffold ships; CMake/qmake enable
  v4.1.6)
- USBFloppy SG_IO mock
- MF-271 Switch/cart7 delete (decision done, execution post-tag)

## [4.1.4-rc1] - 2026-05 (pre-release)

### Internal Refactor — Type-Driven HAL (MF-150 … MF-172)

This release replaces UFT's dual-architecture hardware abstraction
(C HAL fast-path + Qt provider class hierarchy in parallel) with a
single type-driven HAL. The change is mechanical inside the codebase
but invisible to the working Greaseweazle workflow.

**Architecture (additions):**
- Foundation headers `include/uft/hal/{outcomes,concepts,mixins}.h`
  introducing `std::variant` sum-type outcomes (SectorOutcome,
  FluxOutcome, MotorOutcome, …), C++20 capability concepts
  (ReadsRawFlux, ControlsMotor, …), and CRTP mixin templates
  (`ReadsRawFluxVia<Backend>` …) for provider composition (MF-150).
- 10 mixin-composed V2 providers (one per controller + a synthetic
  MockProviderV2): GreaseweazleProviderV2, SCPProviderV2,
  KryoFluxProviderV2, FluxEngineProviderV2, FC5025ProviderV2,
  XUM1541ProviderV2, ApplesauceProviderV2, ADFCopyProviderV2,
  USBFloppyProviderV2 (MF-154, MF-161–MF-168).
- GUI action wiring is now code-generated from
  `forms/tab_hardware.actions.yaml` via `tools/wiring_codegen.py`
  into `generated/tab_hardware_wiring.gen.cpp`. The runtime template
  `wire_action<cap::X>` (in `include/uft/gui/wiring_runtime.h`)
  enables/disables the button based on compile-time capability
  membership and dispatches outcomes through `std::visit` (MF-152,
  MF-155, MF-156, MF-157).
- Conformance test harness `tests/test_hal_conformance.cpp` runs 65
  forensic-invariant sections across all 10 V2 providers — every
  per-variant invariant (rule F-3 divergent-reads preservation, rule
  F-4 3-part-error contract, etc.) is now compile-time-checked +
  runtime-validated (MF-158).
- Three transport mocks in `tests/mock_hardware/` (usb_loopback,
  subprocess, serial) provide deterministic byte-stream fixtures
  for the conformance loop without requiring real hardware (MF-159).

**Removals (V1 hierarchy):**
- The V1 `HardwareProvider` base class, the `HardwareManager`
  dispatcher, and 11 V1 provider classes (`*hardwareprovider.{h,cpp}`)
  are gone — 29 files, ~12 700 LOC deleted (MF-169).
- The `unified_hal_bridge` Qt↔C bridge that converted
  `uft_track_t` to the V1 `TrackData` DTO is gone with the V1
  hierarchy.
- The X1541-family controller-combo entries (XA1541, XAP1541,
  XM1541, XE1541, X1541) and their parallel-port enumeration
  helper are removed — they were phantom-features without a
  matching provider class (MF-170).
- `uft_gw_*` C-API calls are now ONLY in
  `GreaseweazleProviderV2::open()/close()/do_*()`. HardwareTab has
  zero direct `uft_gw_*` references (MF-171).

**User-visible changes:**
- Greaseweazle workflow (read, write, detect, motor, seek, RPM,
  recalibrate): structurally hardened, behavior preserved.
- 8 non-Greaseweazle controllers (SCP, KryoFlux, FluxEngine, FC5025,
  XUM1541, Applesauce, ADFCopy, USBFloppy) are now routed through
  their V2 providers (P1.23, MF-205/MF-206). On Connect each
  constructs its V2 provider into the `ProviderV2Variant` and its
  capability buttons are gated by the codegen `wire_action<cap::X>`.
  The providers are constructed honest-stub (null transports) — every
  operation returns a truthful `ProviderError` ("backend not wired /
  M3.x pending") rather than a silent no-op or a fabricated success.
  The production transports (libusb / QProcess / QSerialPort /
  OpenCBM) are the M3.x follow-up (`docs/M3_HAL_PLAN.md`).
- 5 X1541-family combo entries are gone (never had a working
  backend; selected workflows always errored).

**Known-issue resolutions:**
- KNOWN_ISSUES H-1 (GUI freezes during hardware imaging) resolved
  by the V2 worker-thread routing introduced in MF-147.
- KNOWN_ISSUES H-2 (silent stubs in HardwareProvider derivatives)
  resolved structurally — every silent stub became a missing
  mixin, no longer expressible at the type level.
- KNOWN_ISSUES H-9 (deprecated `TrackData::errorMessage` alias)
  resolved by removing the V1 DTOs entirely.

**Quality gates that ship with this RC:**
- 14 test executables green (test_hal_foundation,
  test_hal_conformance, test_greaseweazle_v2, test_scp_provider_v2,
  test_kryoflux_provider_v2, test_fluxengine_provider_v2,
  test_fc5025_provider_v2, test_xum1541_provider_v2,
  test_applesauce_provider_v2, test_adfcopy_provider_v2,
  test_usbfloppy_provider_v2, test_mock_provider_v2,
  test_mock_hardware, test_wiring_runtime).
- 65 conformance sections / 0 failed.
- `scripts/check_consistency.py` 0/0/0/0.
- `scripts/verify_build_sources.py` no new divergence.
- `tools/wiring_codegen_tests/run_tests.py` 6/6 (incl. drift gate
  comparing committed `generated/tab_hardware_wiring.gen.cpp`
  against a fresh regeneration).
- C++20 mandatory (was C++17).

**Carry-overs for v4.1.5+ (documented in REFACTOR_TASKS.md):**

The original carry-over list (P1.20–P1.23, P3.1–P3.4) is now **all
done and shipped in this RC** — the RC was re-cut to cover MF-150 …
MF-232 (see "External-Compat Audit + Tester Strategy" above): P1.20/21
migrated the flux jobs to the V2 outcome surface (MF-199–MF-201),
P1.22 removed the `raw_handle()`/`gwDevice()` escape hatch (MF-202),
P1.23 routed all 9 providers through the `ProviderV2Variant`
(MF-205/MF-206), and P3.1–P3.4 landed the full Tester Strategy.

What genuinely remains for v4.1.5+:
- **M3.x — HAL production transports.** The 8 non-GW V2 providers are
  routed and GUI-reachable but constructed honest-stub; their
  production transports (M3.1 SCP-Direct, M3.2 XUM1541, M3.3
  Applesauce, …) are unwired. M3.1 additionally needs the SuperCard
  Pro SDK v1.7 vendored before its protocol can be trusted (audit
  SCP-D1-1). See `docs/M3_HAL_PLAN.md`.
- **Loss-report / round-trip wiring** — the `.loss.json` writer and
  round-trip registry exist but are not yet wired into all 44
  conversion paths (`docs/KNOWN_ISSUES.md` §1.1/§1.2/§5.1).

**Forensic-mission status:**
- Rule F-3 (preserve divergent reads, never collapse) — enforced
  at every Sum-Type Marginal/VerifyFailed alternative.
- Rule F-4 (3-part error messages) — type-enforced via
  `ProviderError`'s throwing constructor.
- Rule H-1 (no enabled action without backing capability) —
  structural via codegen's Phase 2 disable path.
- Rule H-3 (action without provider) — structural via codegen
  validation against `KNOWN_CAPABILITIES`.
- Rule D-2 (SpecStatus per provider) — type-mandated.

### External-Compat Audit + Tester Strategy (MF-181 … MF-231)

The RC was re-cut to include the full post-P1 work: the external-
compatibility audit, the Tester-Strategy test infrastructure (P3), and
the **real decoder bugs that infrastructure uncovered**.

**Greaseweazle-compatibility audit (P2.x):**
- Per-provider audit of all nine controllers against the real
  Greaseweazle 1.23 host tools — protocol constants, USB IDs, command
  bytes, timing — synthesised into `audit/MASTER_REPORT.md`.
- Class-A safe fixes applied; ARCH-7 VID/PID inconsistencies verified
  against real device descriptors and corrected (SCP `0x16D0:0x0F8C`,
  the GUI port-hint was right; the header was wrong).
- Native audit-vector tests (`audit/*/test_*_vectors.c`) make the
  protocol-constant contract a compile-time build gate.

**Differential conformance — P3.2 (MF-217 … MF-225):**
- `tests/differential/` decodes a shared synthetic flux corpus with
  BOTH the UFT flux engine and `gw convert` and asserts the decoded
  sector images are byte-identical. **6/6 disk classes pass** —
  IBM-DD/HD, Atari ST, Commodore 1541 GCR, Apple II GCR, AmigaDOS MFM.
- The corpus pattern is a per-index avalanche hash, so every block is
  distinct — the differential tests sector *placement*, not just count.

**Decoder bugs the differential harness uncovered (correctness fixes):**
- **MF-218** — the IBM-MFM decoder skipped only one of the three `0xA1`
  sync words System-34 writes before each address mark, landing inside
  the sync run instead of on the IDAM. It decoded **zero sectors** from
  any standard IBM flux. Fixed with `mfm_skip_sync_run()`.
- **MF-224** — the Apple II 6-and-2 GCR decoder did not undo Apple's
  bit-reversed 2-bit auxiliary groups; every data byte's low two bits
  came out swapped.
- **MF-225** — UFT had **no** Amiga MFM flux decoder: `FLUX_ENC_AMIGA`
  silently routed to the IBM-MFM decoder, which cannot parse Amiga's
  whole-track layout. A real `flux_decode_amiga()` was written.
- **MF-227** — `uft_longtrack_type_name()` / `uft_longtrack_get_def()`
  were declared but never defined, and the internal helpers indexed the
  scheme table by enum value where the table is 0-packed — every
  longtrack scheme was named as its neighbour.

**Improvement test suite — P3.3 (MF-214 … MF-228):**
- `tests/improvement/` proves behaviours `gw` expectedly cannot match:
  forensic provenance + marginal-data preservation + destructive-op
  consent, GUI capability-gating, multi-device provider-switch,
  concurrent multi-device sessions, copy-protection scheme detection
  (longtrack + Dungeon Master fuzzy bits), and format-extension decode
  (Amiga HDF/RDB).

**HIL infrastructure — P3.4 (MF-185, MF-230):**
- `tests/hil/` Hardware-in-the-Loop runner with a **two-tier**
  Golden-Reference catalog: a CI-runnable software tier (the P3.2
  differential corpus) and a hardware tier (one template per
  controller, for Axel's rig). `releases/v4.1.4-rc1/hil_report.md`
  carries both tiers.

**CI fixes:** MF-222 (SCP audit-vector pins aligned to the verified
VID/PID), MF-231 (removed a duplicate `uft_longtrack_type_name` stub
that broke the qmake link once MF-227 added the real definition).

### Hot-Fixes Included (predate the refactor — MF-129, MF-141, MF-145, MF-146)

These fixes were tracked as the "planned 4.1.4 hot-fix" before the
Type-Driven HAL refactor was decided; they ship in the same RC.

- **Windows COM-port-prefix bug across four QSerialPort-based hardware
  providers** (MF-145 / FB user-report). Greaseweazle, SuperCard Pro,
  Applesauce, and ADF-Copy each pre-applied the Win32 device-namespace
  prefix `\\.\` before calling `QSerialPort::setPortName()`. Qt's
  `QSerialPort` adds that prefix internally; the pre-application caused
  `DeviceNotFoundError` on Qt 6.7+ or silently broken handles on older
  Qt. Symptom matched the user report: "hardware will not access the
  floppy drive" on Windows. The bug never surfaced for Greaseweazle on
  the production code path (UI → C-HAL `uft_gw_open()` uses
  `CreateFileA` directly, where the prefix is correct), but it did
  affect SCP / Applesauce / ADF-Copy which route through the Qt
  provider. The bug-fix code is now inherent in the V2 providers
  introduced by the refactor — the V1 sites that had it were deleted
  along with the V1 hierarchy (MF-169). USB-Floppy provider's
  `\\.\A:` and `\\.\B:` paths are intentionally retained: that
  provider uses `DeviceIoControl` SCSI pass-through where the prefix
  is the correct Win32 convention for direct volume access.
- **Applesauce ↔ ADF-Copy VID/PID disambiguation** (MF-146). Both
  controllers ship with the generic PJRC Teensy USB ID
  (VID `0x16C0` / PID `0x0483`). Previously, both providers' detect
  paths candidate-matched any 0x16C0:0x0483 device, then attempted
  their own protocol handshake — racing for the same port during
  auto-detect. The new ApplesauceProviderV2 + ADFCopyProviderV2
  detect-runners skip ports whose USB manufacturer / description
  string identifies the *other* controller. Eliminates the
  double-claim on systems where only one device is attached.
- **Greaseweazle bootloader-mode detection** (MF-129). `uft_gw_open()`
  refuses bootloader-firmware boards with an actionable error
  (`UFT_GW_ERR_BOOTLOADER`), surfaced via `ProviderError.fix` as
  recovery hints instead of a generic "I/O error".
- **MFM IDAM/DAM standalone sector parser** (MF-141 / AUD-002).
  SCP→ADF/IMG/D64 conversion path decodes sectors via a reusable
  API with explicit CRC validation and CHRN-keyed LBA mapping.
  Previously the inline parser dropped sectors after IDAM, ignored
  CRCs, and placed payloads in scan order instead of by sector ID.

### Build / Tooling

- `uft_version.h` is regenerated from `VERSION.txt` at every qmake
  parse / CMake configure (MF-131). Removes the `DEFINES+=
  UFT_VERSION_STRING=\\\"...\\\"` quoting fragility from
  `release.yml` (MF-145).
- `scripts/check_consistency.py --check version` scans
  `RELEASE_NOTES.md` headline + Doxygen `@version` tags too
  (MF-128). Pre-push gate now blocks 5 distinct version-drift
  classes.
- `tools/wiring_codegen_tests/run_tests.py` is a new CI smoke
  with 6 cases: happy-path / H-3 surface (missing widget) / H-4
  surface (unknown capability) / idempotence / production-YAML
  validation / committed-`.gen.cpp`-is-fresh drift detector.

## [4.1.3] - 2026-04-16

### Added
- CRC / status flag propagation across the PLL → sector pipeline
- IMD read_track (ImageDisk sector extraction with metadata)
- FDI read_track (Formatted Disk Image with track offset table)
- Plugin-B parsers for DO, PO, ADL, V9T9, VDK (Apple / Atari / Acorn / TI / Dragon)
- STX (Atari Pasti) + PRO (Atari 8-bit) Plugin-B — all 5 Tier-1 formats complete (27 plugins)
- ATX + ADF + SCL Plugin-B + restored registry (25 plugins)
- write_track for TRD, D64, ATR, SAD, SSD, HFE, D80, D82, D71, D81
- Probe-quality improvements for 6 formats (fewer false positives)

### Fixed
- 3 silent I/O errors in SAD, SSD, HFE
- NULL-checks + silent-error fixes in 8 registry plugins
- Error aliases, total_sectors, SSD write, protection includes
- Memory allocation limits, bounds checks, forensic fill (security pass)
- Struct compat aliases, write-verify types, track_init signature
- CMake Sanitizer / Coverage builds — dead file references removed
- Test include paths repaired

### Changed
- Format registry: 138 → 149 fully implemented parsers
- Cleanup: 674 orphaned source files deleted (~386 000 LOC dead code)
- 109 dead format files removed from disk
- 165 non-floppy stubs removed (32 active plugins after pruning)

## [4.1.2] - 2026-04-15

### Added
- D64 Plugin-B wrapper (Commodore 1541, variable SPT zones)
- TD0 read_track (Teledisk: raw/repeat/pattern decode)
- FDI read_track (track offset table + sector descriptors)
- MSA Plugin-B (Atari ST RLE decompression)
- WOZ Plugin-B (Apple II v1/v2/v2.1 via woz_load API)
- IPF Plugin-B (CAPS/SPS identification)
- G71 Plugin-B (Commodore 1571 GCR raw)
- Format registry: 22 plugins, all with read_track
- MFM sector extraction in SCP→sector flux converter

### Fixed
- 4 legacy MFM encoder CRC placeholders → real CRC-CCITT (0xCDB4)
- 86F CRC verification (was always true, now computed)
- CRC32 combine XOR bug → sentinel value
- uft_track_add_sector return type harmonized (int → uft_error_t)
- Duplicate uft_ufi_read_sectors removed from backend
- USBFloppyHardwareProvider interface mismatch with base class
- 10 broken tests repaired (include paths: 78→88 OK)
- 3 missing forwarding headers created

### Changed
- Format registry expanded from 21 to 22 plugins (D64 added)
- IPF read_track returns NOT_IMPLEMENTED honestly
- Test compilation: 88 of 94 tests compile cleanly

## [4.1.1] - 2026-04-14

### Added
- 11 new Plugin-B format parsers: ST, D77, DIM, DC42, FDS, CAS, MFI, PRI, EDSK, KFX
- A2R parser (1104 LOC) activated from existing code
- Decode Pipeline: unified Flux → PLL → Bits → Sectors → OTDR session
- Canonical PLL API header (uft_pll.h) with 11 platform presets

### Fixed
- WOZ 1.0 TRKS chunk: track bitstream data now accessible (was empty)
- 10 macro redefinition warnings eliminated (#ifndef guards)
- Qt Charts made optional (CMake builds without Charts module)
- 4 previously disabled tests re-enabled (ti99, fat_extensions, mega65_fat32, new_formats)

### Changed
- PLL consolidation: vfo_fixed removed, vfo_experimental + kalman_pll optional (CONFIG flags)
- Format count: 138 → 149 fully implemented parsers

## [Unreleased] (planned 4.2.0)

### Added
- DeepRead forensic modules: write-splice, aging, cross-track, fingerprint, soft-decode
- OTDR adaptive decode, weighted voting, encoding boost
- ML anomaly detection and copy protection classifier
- 9 format parsers: FDS, CHD, NDIF, EDD, DART, Aaru, HxCStream, 86F, SaveDskF
- FC5025, XUM1541/ZoomFloppy, Applesauce hardware providers
- Unified copy protection API (55+ schemes, 10 platforms)
- Triage mode, sector compare, provenance chain, recovery wizard, format suggestion
- GUI: state machine, smart export dialog, recovery wizard dialog, compare dialog
- GUI: drag & drop, keyboard shortcuts, status bar info
- CI: sanitizers.yml (ASan+UBSan), coverage.yml (Codecov)
- 25 specialized agent definitions

### Fixed
- ~610 silent fseek/fread/fwrite error handling fixes
- Integer overflow guards in 9 parsers
- Real SHA-256 replacing fake FNV1a-based hash
- SIMD rounding mismatch (AVX2 vs SSE2/scalar)
- Multi-revolution negative alignment offset blocking
- Amiga MFM header buffer overread
- Apple II DOS-to-ProDOS sector map (3 conflicting tables → 1 canonical)
- Atari ST boot detection (proper 0x1234 checksum)
- C64 40-track D64 spt table incomplete
- Path traversal security (component-level walk)
- Thread safety: uft_safe_alloc, UftParameterModel, HardwareManager
- Compiler hardening: -fstack-protector-strong, -D_FORTIFY_SOURCE=2

### Changed
- God-file split: uft_format_convert.c (3944 lines) → 7 modular files
- 33 protection source files added to build (were dead code)
- SCP Extension Footer parsing implemented
- WOZ 2.1 FLUX chunk decoding implemented
- Format detection improved: D88/D77, IMG/IMA, Atari ST, Apple DO/PO, HFE v2

## [4.1.0] - 2026-02-08

### Fixed – Hardware Protocol Audit
- **SuperCard Pro**: Complete rewrite from SDK v1.7 reference
  - Replaced fabricated ASCII protocol with correct binary commands (0x80-0xD2)
  - Fixed checksum (init 0x4A), endianness (big-endian), USB interface (FTDI FT240-X)
  - Implemented proper RAM model (512K SRAM, SENDRAM_USB/LOADRAM_USB)
  - Corrected flux read flow: READFLUX→GETFLUXINFO→SENDRAM_USB
  - Corrected flux write flow: LOADRAM_USB→WRITEFLUX
- **FC5025**: Complete rewrite from driver source v1309
  - Replaced fabricated opcodes with SCSI-like CBW/CSW protocol (sig 'CFBC')
  - Correct opcodes: SEEK=0xC0, FLAGS=0xC2, READ_FLEXIBLE=0xC6, READ_ID=0xC7
  - Device correctly marked as read-only (no write support in hardware)
  - Density control via FLAGS bit 2
- **KryoFlux**: Replaced invented USB commands with DTC subprocess wrapper
  - Proprietary USB protocol acknowledged, official tool used instead
  - Stream format parser (OOB 0x08-0x0D) verified and retained
- **Pauline**: Replaced fabricated binary protocol (0x01-0x90) with HTTP/SSH
  - Matches real architecture: embedded Linux web server on DE10-nano FPGA
  - SSH commands for drive control, HTTP for data transfer
- **Greaseweazle**: 16 protocol bugs fixed against gw_protocol.h v1.23
  - Fixed USB command codes, bandwidth negotiation, SET_BUS_TYPE
  - Corrected checksum and packet framing

### Added
- GitHub Actions CI/CD release workflow (macOS/Linux/Windows)
- Linux .deb packaging with desktop integration
- macOS .dmg with universal binary support
- CMakeLists.txt for cmake-based builds alongside qmake
- Linux .desktop entry and udev rules for floppy devices

### Changed
- Version bump: 4.0.0 → 4.1.0
- Cleaned up development-only files from distribution

## [4.0.0] - 2026-01-16

### Added
- DMK Analyzer, Flux Histogram
- MAME MFI & 86Box 86F formats
- Documentation suite
- CI/CD pipeline

## [3.9.0] - 2026-01-15

### Added
- TI-99/4A format support
- FAT filesystem extensions
- 43 CP/M disk definitions
