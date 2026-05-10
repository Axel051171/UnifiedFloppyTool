# Refactor Tasks — Type-Driven HAL

**Branch:** `refactor/type-driven-hal`
**Source-of-truth:** `docs/REFACTOR_BRIEF.md`
**Status legend:** ✅ done · 🟡 in progress · ⬜ pending

Order is binding. Each task is atomic; build + tests must be green at
the end of each task before the next begins.

---

## P0 — Foundation (commit, no parallel architecture yet)

| ID    | Task                                                                | Status | Commit  |
|-------|---------------------------------------------------------------------|--------|---------|
| P0.1  | Branch `refactor/type-driven-hal` from main HEAD                    | ✅     | (env)   |
| P0.2  | Bump C++17 → C++20 in `UnifiedFloppyTool.pro` + `CMakeLists.txt`    | ✅     | MF-150  |
| P0.3  | `include/uft/hal/outcomes.h` — Sum-Types                            | ✅     | MF-150  |
| P0.4  | `include/uft/hal/concepts.h` — Capability concepts                  | ✅     | MF-150  |
| P0.5  | `include/uft/hal/mixins.h` — Capability mixin templates             | ✅     | MF-150  |
| P0.6  | `tests/test_hal_foundation.cpp` + REFACTOR_BRIEF + REFACTOR_TASKS + HARDWARE_TRUTH_TESTS + CLAUDE.md addendum | ✅ | MF-150 |

---

## P1 — V2 providers, codegen, conformance (long-running)

Each P1 task ends in its own commit. Foundation must remain stable —
P0 headers must not be edited during P1.

| ID    | Task                                                                       | Depends on | Status |
|-------|----------------------------------------------------------------------------|------------|--------|
| P1.0  | `tools/wiring_codegen.py` skeleton (parse YAML, emit stub Cpp)             | P0         | ✅ MF-152 |
| P1.1  | `GreaseweazleProviderV2` (mixin composition, `uft_gw_*` as backend)        | P0         | ✅ MF-154 |
| P1.2  | `forms/tab_hardware.actions.yaml` (declarative wiring, GW-only at first)   | P0         | ✅ MF-156 |
| P1.3  | `include/uft/gui/wiring_runtime.h` — `wire_action<Cap>()` template          | P0         | ✅ MF-155 |
| P1.4  | Wire `HardwareTab` motor/seek/read/RPM/calibrate via codegen              | P1.1–1.3   | ✅ MF-157 |
| P1.5  | Conformance test framework `tests/hal_conformance.cpp`                    | P0         | ✅ MF-158 |
| P1.6  | Mock backends: `usb_loopback`, `subprocess_mock`, `serial_mock` (skeletons) | P0       | ✅ MF-159 |
| P1.7  | `MockProviderV2` — implements all concepts for conformance loop            | P1.5, P1.6 | ✅ MF-160 |
| P1.8  | `SCPProviderV2` (Read+WriteRawFlux+Detect; no motor/seek)                 | P1.1, P1.6 | ✅ MF-161 |
| P1.9  | `KryoFluxProviderV2` (ReadRawFlux+Detect via DTC subprocess; read-only)    | P1.6       | ✅ MF-162 |
| P1.10 | `FluxEngineProviderV2` (Read+Write via fluxengine-CLI subprocess)         | P1.6       | ✅ MF-163 |
| P1.11 | `FC5025ProviderV2` (read-only — only Read*+Detect mixins)                  | P1.6       | ✅ MF-164 |
| P1.12 | `XUM1541ProviderV2` (Read+Write sector via opencbm)                        | P1.6       | ✅ MF-165 |
| P1.13 | `ApplesauceProviderV2` (Read+Write flux via QSerialPort)                   | P1.6       | ✅ MF-166 |
| P1.14 | `ADFCopyProviderV2` (Read+Write flux via QSerialPort)                      | P1.6       | ⬜     |
| P1.15 | `USBFloppyProviderV2` (Read+Write sector via UFI HAL)                      | P1.6       | ⬜     |
| P1.16 | DTO migration: `if (r.success)` → `std::visit` in jobs + tabs              | P1.1–1.15  | ⬜     |
| P1.17 | Drop V1 base class + 10 V1 provider files                                  | P1.16      | ⬜     |
| P1.18 | Internalize `uft_gw_*` — out of `hardwaretab.cpp`, only in GW provider     | P1.4, P1.17| ⬜     |
| P1.19 | Remove X1541-family entries from controller combo                          | P1.4       | ⬜     |

---

## P2 — Release (after P1 fully green)

| ID    | Task                                                                       | Status |
|-------|----------------------------------------------------------------------------|--------|
| P2.1  | `VERSION.txt` → `5.0.0-rc1` + RELEASE_NOTES + CHANGELOG                    | ⬜     |
| P2.2  | KNOWN_ISSUES cleanup: H-1 / H-2 / H-9 entries removed                      | ⬜     |
| P2.3  | Tag `v5.0.0-rc1`; 14-day pre-release window                                | ⬜     |
| P2.4  | After 14 days clean: squash-merge to `main`, tag `v5.0.0`, delete branch   | ⬜     |

---

## P3 — Tester-Strategie (parallel zu P1 möglich, Pflicht vor v5.0.0)

Spec: [`docs/TESTER_STRATEGY.md`](TESTER_STRATEGY.md). Doppelziel
"GW-kompatibel + beweisbar besser" über 7-Schichten-Testsystem. P3.1
ist die einzige Welle-1-Aufgabe ohne Abhängigkeit zu P1.

| ID    | Task                                                                       | Depends on | Status |
|-------|----------------------------------------------------------------------------|------------|--------|
| P3.1  | `gw_corpus/` Skeleton + `tools/uft_diff_test/` + 1 SCP-Fixture + DIV-001  | —          | ⬜     |
| P3.2  | Differential-Conformance-Tests pro GW-Command (~50 Tests)                  | P3.1, P1   | ⬜     |
| P3.3  | Improvement-Tests: forensic, multi_device, gui (~40 Tests)                 | P3.1, P1   | ⬜     |
| P3.4  | HIL-Skript + Golden-Reference-Katalog + erster v5.0.0-rc1 HIL-Report      | P3.2, P3.3 | ⬜     |

Neue Subagenten (landen mit P3.1):
- `differential-test-author` (Sonnet) — pro GW-Command/Format
- `improvement-test-author` (Sonnet) — pro DESIGN_PRINCIPLES-Eigenschaft

---

## STOP conditions

The agent (human or AI) MUST halt and ask before continuing if any of
these become true:

1. A test that was green at the start of the task fails for an
   unrelated reason.
2. A C-API symbol referenced in the brief or a header does not exist
   in the implementation source (or vice versa).
3. A mock backend produces data inconsistent with the protocol spec.
4. More than 3 build attempts on a single task fail — the architectural
   assumption was wrong; do not paper over.
5. The task would touch a file the brief lists as protected
   (`uft_gw_*` signatures, `tests/golden/`, `docs/DESIGN_PRINCIPLES.md`).
6. A single commit would touch more than 50 files.

In any of these cases:
- commit current state with `WIP:` prefix and a one-line marker
  describing the stop reason,
- record details in `docs/REFACTOR_NOTES.md`,
- wait for human input.

---

## Per-task contract

Every task ends with all of the following true:

- [ ] `cmake --build build` (or `qmake && mingw32-make`) green
- [ ] `ctest --output-on-failure` green; conformance test count ≥ baseline
- [ ] `python scripts/check_consistency.py` 0/0/0/0
- [ ] `python scripts/verify_build_sources.py` no new regressions
- [ ] commit message follows Conventional Commits + carries an MF-NNN
- [ ] this task's row in this table flipped to ✅

If any of those is not satisfied, the task is **not done**, regardless
of how complete the code looks.
