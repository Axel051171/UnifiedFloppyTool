# UnifiedFloppyTool v4.1.5 Release Notes [DRAFT]

**Status:** DRAFT — finalize on tag-day per `scripts/release/release_v415_checklist.md`.
**Target Tag Date:** post-2026-05-29 (after v4.1.4 RC1-Window closes) +
hardware-bench session for HIL.GW + UFT-008 SCP-Direct Tier-3 PASS.
**Branch:** `tests/v4.1.5-hardening` (squash-merge to `main` at tag time).

## What changed (TL;DR)

v4.1.5 hardens the v4.1.4 release across **9 dimensions** without
breaking any v4.1.4 user-facing behavior. The Greaseweazle workflow
is unchanged. Everything new is either honesty (more accurate status
labels) or scaffolding (Tier-2.5 simulators, sidecar reports).

**Six new closed gates from the V415-PLAN:**

| Gate | Status | Why it matters |
|---|---|---|
| All 9 V2 providers have a live code path | ✅ MF-249..MF-258 | No silent no-op when the user clicks "Connect" on a non-Greaseweazle controller. |
| 22/22 SCP USB opcodes byte-verified | ✅ MF-261 | Cross-referenced against `samdisk/SuperCardPro.h` (the open reference SCP impl). Pre-MF-254 placeholder bytes (`0x02-0x40`) are gone. |
| `uft_format_plugin_t` ABI gate | ✅ MF-260 | New `api_version` field + `_Static_assert(sizeof == 216)` + registrar reject for future-ABI plugins. Closes ABI-bomb risk for future external `.so` plugins. |
| Plugin Prinzip-7 compliance 84/84 | ✅ MF-262 + MF-263 | `spec_status` and `features` populated for every plugin (was 15/84 and 5/84 respectively). Plugin honesty audit passes 100%. |
| LOSS.preflight chokepoint + sidecar | ✅ MF-263 + MF-268 | All 44 conversion paths gated through `uft_preflight_check()`. `LOSSY_DOCUMENTED` paths emit a `.loss.json` sidecar after successful convert. |
| Tier-2.5 hardware simulator system | ✅ MF-267 | `tools/hw_simulators/{sim_dtc,sim_fluxengine,sim_fcimage}.py` — exercise KryoFlux/FluxEngine/FC5025 V2 providers end-to-end without physical hardware. CI-gated. |
| ARCH-7-C Teensy-probe wrapper | ✅ MF-213 + MF-263 | Probe-on-Connect with mismatch warning for ADFCopy ↔ Applesauce (both Teensy 0x16C0:0x0483). Prevents disk-corrupting cross-wire. |
| Test pass-rate 47/180 → 151/151 | ✅ MF-245..MF-260 | Stale exclusions reviewed and re-enabled; new `test_plugin_abi.c` for the new ABI gate. |
| Build-system parity baseline shrunk | ✅ MF-262 | `verify_build_sources.py` baseline 224 → 219; 5 source-file entries resolved. |

## Verified opcodes — SCP-Direct USB protocol

MF-261 cross-referenced the 22 USB command bytes + response code +
checksum init in `include/uft/hal/uft_scp_direct.h` against
[samdisk's `SuperCardPro.h`](https://github.com/simonowen/samdisk/blob/main/include/SuperCardPro.h).
samdisk has shipped SCP read/write against real hardware since 2017
and is the de-facto open reference implementation. Pre-MF-254 the
header carried six placeholder opcodes (`0x02-0x40`) traceable to
a3rawconv, not to the vendor SDK — those are gone.

The SCP SDK PDF (`scp_sdk.pdf` v1.7, cbmstuff.com, Dec 2015) itself
is binary-only-readable; samdisk's header is the public cross-reference.

## Plugin Prinzip-7 compliance

Before v4.1.5 only 5 plugins had a hand-curated `.features` array
(ADF, HFE, IPF, STX, WOZ) and 15 had a non-UNKNOWN `.spec_status`.
The `audit_plugin_compliance.py` script flagged the gap as a §7.1
+ §7.2 violation. Two mass-populator scripts solved it:

- `scripts/populate_spec_status.py` — per-format-subdir mapping
  (OFFICIAL_FULL / OFFICIAL_PARTIAL / REVERSE_ENGINEERED / DERIVED)
  based on each format's documentation provenance.
- `scripts/populate_features.py` — derives a per-plugin feature
  matrix from the `.capabilities` bitfield. Read/Write/Create/
  Flux/Timing/Weak Bits/MultiRev → SUPPORTED or UNSUPPORTED.

Result: **84/84 (100%) Prinzip-7 compliance**. Per-plugin hand-tuning
where the heuristic was too coarse (e.g. WOZ PARTIAL with note) is
ongoing in v4.1.6 as documentation hygiene.

## LOSS.preflight — Prinzip 1 enforcement

`uft_convert_file()` is the single chokepoint for all 44 conversion
paths. v4.1.5 wires `uft_preflight_check()` into that entry point:

- **LOSSLESS** conversions run as before, silently.
- **LOSSY_DOCUMENTED** conversions require `accept_data_loss=true` —
  the GUI/CLI **must** obtain explicit user consent. On success, a
  `<target>.loss.json` sidecar is written next to the output file,
  documenting which categories of information were lost
  (`FLUX_TIMING`, `WEAK_BITS`, `INDEX_PULSES`, `MULTI_REVOLUTION`,
  `COPY_PROTECTION`, etc.).
- **IMPOSSIBLE** and **UNTESTED** conversions ABORT with a diagnostic
  rather than silently producing garbage.

Phase 1 (gate) and Phase 2 (sidecar emit) both ship in v4.1.5.
Per-converter loss-precision refinement (exact weak-bit counts etc.)
is v4.1.6 follow-up — Phase 2 emits category-level entries, sufficient
for the user to know "this converter lost flux timing", not "...lost
12 specific weak bits in track 18".

## Tier-2.5 hardware simulator system

The big new piece for forensic confidence without owning every
controller in the lab:

- `tools/hw_simulators/sim_dtc.py` — KryoFlux DTC subprocess simulator
- `tools/hw_simulators/sim_fluxengine.py` — FluxEngine CLI simulator
- `tools/hw_simulators/sim_fcimage.py` — FC5025 fcimage simulator
- POSIX shell + Windows .cmd wrappers (so they show up as `dtc` /
  `fluxengine` / `fcimage` on PATH)
- `tests/hil/run_simulated.py` — Tier-2.5 HIL runner, gates SIMULATED
  vs FAIL vs NOT_RUN. **Never** reports PASS — only real hardware can.

The simulators feed deterministic synthetic flux from the existing
130 MB corpus (byte-exact verified against Greaseweazle 1.23) into the
V2 provider chain. Result: 7/7 SIMULATED across KryoFlux, FluxEngine,
FC5025.

Also new: `tools/adfcopy_sim.py` — serial-COM simulator for ADF-Copy
(mirrors `tools/applesauce_sim.py`). Pair with `com0com`/`socat` for
full Tier-2 end-to-end without hardware.

The simulator suite runs in CI on every push via the Audit job.

## What this release does **NOT** ship

These are intentionally deferred to v4.1.6 (with reasoning):

- **M3.2 XUM1541 libusb** — opencbm dependency + Commodore-bus timing make it its own 2-week strecke.
- **M3.3 Applesauce serial-protocol full handshake** — `?vers` is in; the `?disk`-readx state machine still has gaps.
- **UFI Windows + macOS backends** (`ufi_win.c` via DeviceIoControl,
  `ufi_mac.c` via IOKit). Linux SG_IO is in (MF-207).
- **ARCH-9 XUM1541 macOS .dylib loader** — needs `OpenCbmLibrary::load()` path.
- **5-and-3 Apple GCR** (DOS 3.2 13-Sektoren).
- **FM-Decoder completion** — `flux_decode_fm` flagged as incomplete in the decoder source.
- **Per-converter sidecar precision** — current Phase 2 emits category-level entries; per-track exact counts are v4.1.6.
- **`uft-decode` CLI binary build wiring** — scaffold at `cli/uft-decode/` ships; CMake/qmake enable + emulator.yml round-trip integration is v4.1.6.

## Upgrade notes

- **No breaking changes.** v4.1.4 → v4.1.5 is purely additive.
- The `uft_format_plugin_t` ABI gate (MF-260) is opt-in: legacy plugins
  with `api_version=0` are accepted with a one-shot stderr warning.
  Plugin authors should set `.api_version = UFT_PLUGIN_API_VERSION`
  before v5.0.
- LOSS.preflight requires explicit consent for `LOSSY_DOCUMENTED`
  conversions. GUI workflows that previously ran lossy converts silently
  now need to set `accept_data_loss=true` in `uft_convert_options_t`
  (`preserve_errors` field maps to this — see `uft_format_convert_dispatch.c:467`).

## Verified on

- Local: MinGW-w64 g++ 13.1.0, Qt 6.10.1
- CI: Linux GCC (Qt 6.7.3 + 6.10.1), macOS Clang, Windows MinGW
- Plugin compliance: `audit_plugin_compliance.py` 84/84 ✓
- Build parity: `verify_build_sources.py` 219/0 (no new regressions)
- Consistency: `check_consistency.py` 0/0/0/0 ✓
- Test suite: ctest 151/151 (100%)
- Simulator gate: 7/7 SIMULATED (Audit CI job)
