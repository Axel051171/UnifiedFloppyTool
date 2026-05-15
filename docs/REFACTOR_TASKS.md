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
| P1.14 | `ADFCopyProviderV2` (Read+Write flux via QSerialPort)                      | P1.6       | ✅ MF-167 |
| P1.15 | `USBFloppyProviderV2` (Read+Write sector via UFI HAL)                      | P1.6       | ✅ MF-168 |
| P1.16 | DTO migration: `if (r.success)` → `std::visit` in jobs + tabs              | P1.1–1.15  | ✅ obsolete — audit (pre-MF-169) found only 2 consumer sites (`unified_hal_bridge` + `onUnifiedCapture`), both deleted with V1 in P1.17. No separate migration needed. |
| P1.17 | Drop V1 base class + 10 V1 provider files                                  | P1.16      | ✅ MF-169 |
| P1.18 | Internalize `uft_gw_*` — out of `hardwaretab.cpp`, only in GW provider     | P1.4, P1.17| ✅ MF-171 |
| P1.19 | Remove X1541-family entries from controller combo                          | P1.4       | ✅ MF-170 |
| P1.20 | Migrate `FluxCaptureJob` to V2 outcome surface (drop uft_gw_* direct path) | P1.18      | ✅ **MF-199/MF-200** — drive-unit selection auf `GreaseweazleProviderV2` (MF-199, MF-197-Proposal) + `FluxCaptureJob` auf die V2-Outcome-Surface migriert (MF-200): `run()` nutzt `set_motor`/`seek`/`read_raw_flux` + `std::visit`, Per-Revolution-Split über `FluxCaptured.index_times_ns`. **Build-verifiziert** (voller qmake-Build grün). Runtime-Capture auf echter HW = Axels HIL-Check. |
| P1.21 | Migrate `FluxWriteJob`   to V2 outcome surface (drop uft_gw_* direct path) | P1.18      | ✅ **MF-201** — `FluxWriteJob` auf die V2-Outcome-Surface migriert (spiegelbildlich zu P1.20): `run()` nutzt `set_motor`/`seek`/`write_raw_flux` + `std::visit`, `FluxStream.transitions_ns` in ns (do_write_raw_flux konvertiert ns→ticks), `WriteFluxParams.verify` = `m_verify`. **Build-verifiziert**. |
| P1.22 | Remove `GreaseweazleProviderV2::raw_handle()` + `HardwareTab::gwDevice()`  | P1.20, P1.21 | ✅ **MF-202** — beide Accessoren entfernt: `raw_handle()` aus `greaseweazle_provider_v2.h`, `gwDevice()` aus `hardwaretab.{h,cpp}`. Nach P1.20/21 hatten sie null Consumer; `git grep` bestätigt: kein Code-Treffer mehr, nur Removal-Kommentare. **Build-verifiziert.** Damit ist der V1-C-API-Escape-Hatch vollständig zu — jeder `uft_gw_*`-Aufruf liegt jetzt hinter den `do_*`-Methoden von `GreaseweazleProviderV2`. |
| P1.23 | Route non-GW controllers through their V2 providers (variant dispatch)    | P1.17, P1.24 | ✅ — **Commit 1 (MF-205):** `ProviderV2Variant` (`std::variant` über `monostate` + 9 `unique_ptr<…ProviderV2>`), `currentProviderV2()` → `const ProviderV2Variant&`, Codegen `std::visit` + `if constexpr`-Gating, GW-Pfad via `get_if`. **Commit 2 (MF-206):** alle 8 non-GW Controller (`scp`/`kryoflux`/`fluxengine`/`fc5025`/`xum1541`/`applesauce`/`adfcopy`/`usb_floppy`) werden in `onConnect()` Honest-Stub konstruiert (null-Runner/null-Handle → jedes `do_*` gibt ehrlichen `ProviderError`), in `m_providerV2` abgelegt, via `rewireV2()` GUI-gegated; der „routing pending (P1.18)"-Messagebox entfällt. Audit ARCH-1/ARCH-4 geschlossen — 9/9 Provider GUI-erreichbar. **Build-verifiziert.** Produktions-Transporte (QProcess/QSerialPort/OpenCBM/libusb) bleiben M3.x-Folgearbeit. |
| P1.24 | **P0-on-integration** — KryoFlux+FluxEngine: echter Flux-Decode statt undekodierte Backend-Container-Bytes in `FluxCaptured::transitions_ns` | P1.17 | ✅ **Schritt 1 (MF-203):** Integritäts-Verstoß gestoppt — `do_read_raw_flux` gibt ehrlichen `ProviderError` statt fehletikettierter Bytes. **Schritt 2 — KryoFlux ✅ (MF-208):** echter Stream-Decoder `src/flux/uft_kryoflux_stream.c` (`uft_kf_decode()`, dokumentierte KryoFlux-Stream-Spec: Flux1/2/3 + Nop1/2/3 + Ovl16 + OOB); `KryoFluxProviderV2::do_read_raw_flux` → `FluxCaptured` mit ns-Intervallen + gemessenen `index_times_ns`. Tests: `test_kryoflux_stream.c` (11 synthetische Spec-Vektoren). **Schritt 2 — FluxEngine ✅ (MF-209):** SCP-Pivot — FluxEngine-`.flux` ist eine SQLite-DB (neue Dependency = verboten), daher fragt `do_read_raw_flux` jetzt SCP-Output an (`-o …scp`) und dekodiert es mit dem vetteten `uft_scp_parser`. Neu: `uft_scp_read_track_memory()` (FILE\*-freier Companion zu `uft_scp_open_memory`, das ein Halb-Feature war). Bug gefixt: `resolution` wurde aus Byte 9 statt 11 gelesen. `sample_ns` kommt jetzt aus dem SCP-Periodenwert (löst Audit FE-D1-2). Tests: `test_scp_read_memory.c` (echtes `CHECK`, 6 Gruppen inkl. `resolution≠0`) + `test_fluxengine_provider_v2` SCP-Decode-Smoke. **Alle Decoder fabrizieren nie einen Flux-Wert; truncated/malformed → Status/`FluxMarginal`/`ProviderError`, nie ein Müll-`FluxCaptured`.** **Hinweis:** synthetische Vektoren beweisen „Decoder == Spec-Lesart", nicht „== echte Hardware" — HIL-Capture-Vektoren bleiben Folgearbeit. |
| P1.25 | USB-Floppy UFI-Backend implementieren (`ufi_linux.c` zuerst) + `uft_ufi_backend_init()` registriert es | P1.17 | ✅ **MF-207** — `src/hal/ufi_linux.c`: Linux-SG_IO-Backend (`uft_ufi_ops_t` = `open`/`close`/`exec_cdb` über `ioctl(SG_IO)`), ganze TU unter `#ifdef __linux__`. `open()` verifiziert `SG_GET_VERSION_NUM ≥ 3.0` bevor der fd zurückgegeben wird; `exec_cdb` dekodiert Fixed-Format-Sense (sense_key/ASC/ASCQ) bei CHECK CONDITION und meldet Short-Transfer (`resid == requested`) als ehrlichen `UFT_ERR_IO` — kein stilles Schlucken, keine halb-gefüllten Buffer. `src/hal/ufi_backend.c`: echtes `uft_ufi_backend_init()` mit Plattform-`#ifdef`-Dispatch (Linux registriert, Windows/macOS noch `-1`); ersetzt den `return -1`-Stub aus `uft_core_stubs.c`. `ufi.c`: `struct uft_ufi_device` jetzt incomplete (Backend besitzt die Definition). Audit ARCH-3 geschlossen. **Windows-Build-verifiziert** (`ufi_linux.c` leere TU, `ufi_backend.c` `#else`-Pfad); Linux-SG_IO-Pfad kompiliert erst in der Linux-CI — keine HIL-Hardware lokal. |

> **Hardware-Provider-Audit (2026-05-14):** vollständiger forensischer
> Audit aller 9 V2-Provider über 5 Dimensionen unter
> [`audit/MASTER_REPORT.md`](../audit/MASTER_REPORT.md). Ergebnis: alle
> 9 PARTIAL — die Typ-Schicht ist solide (kein Fake-Erfolg, ARCH-0), die
> Integrations-Schicht unfertig (ARCH-1 → P1.23). P1.24/P1.25 sind neue
> Audit-Findings. Klasse-A-Quick-Fixes (ARCH-3-ABI, ARCH-5, ARCH-6,
> GW-D5-2) landeten als MF-186; VID/PID-Inkonsistenzen (ARCH-7) sind ein
> offener Verifikations-Task.

---

## P2 — Release (after P1 fully green)

| ID    | Task                                                                       | Status |
|-------|----------------------------------------------------------------------------|--------|
| P2.1  | `VERSION.txt` → `4.1.4` + RELEASE_NOTES + CHANGELOG + flatpak metainfo    | ✅ MF-173 |
| P2.2  | KNOWN_ISSUES cleanup: M.-2 (H-9) → CLOSED; H-1/H-2 noted as structural    | ✅ MF-174 |
| P2.3  | Tag `v4.1.4-rc1`; 14-day pre-release window                                | 🔄 RE-CUT 2026-05-15 auf `refactor/type-driven-hal` HEAD — der ursprüngliche Tag (2026-05-14, 5294563) war 54 Commits / MF-181…231 stale (gesamte P2.x-Audit- + P3-Tester-Phase + Decoder-Bugfixes landeten danach). Tag deckt jetzt was P2.4 tatsächlich merged. Fenster neu → 2026-05-29 |
| P2.4  | After 14 days clean: squash-merge to `main`, tag `v4.1.4`, delete branch   | ⬜     |

> **Version-bump call (2026-05):** PATCH `v4.1.3 → v4.1.4` not MAJOR
> `v4.1.4`. UFT is an end-user app; no external SemVer contract is in
> play. The refactor delivers structural / forensic improvements with
> no working-user-workflow regression — see REFACTOR_BRIEF.md §9 for
> the full rationale.

---

## P3 — Tester-Strategie (parallel zu P1 möglich, Pflicht vor v4.1.4)

Spec: [`docs/TESTER_STRATEGY.md`](TESTER_STRATEGY.md). Doppelziel
"GW-kompatibel + beweisbar besser" über 7-Schichten-Testsystem. P3.1
ist die einzige Welle-1-Aufgabe ohne Abhängigkeit zu P1.

| ID    | Task                                                                       | Depends on | Status |
|-------|----------------------------------------------------------------------------|------------|--------|
| P3.1  | `gw_corpus/` Skeleton + `tools/uft_diff_test/` + 1 SCP-Fixture + DIV-001  | —          | ✅ MF-182 |
| P3.2  | Differential-Conformance-Tests pro GW-Command (~50 Tests)                  | P3.1, P1   | ✅ MF-217..225: tests/differential/ — 6/6 Disk-Klassen byte-exakt gegen gw 1.23 (IBM-DD/HD, AtariST, C64-GCR, Apple2-GCR, Amiga). Deckte 2 echte Decoder-Bugs auf (MF-218 A1-Sync-Run, MF-224 Apple-6-and-2) + Amiga-MFM-Decoder neu (MF-225) |
| P3.3  | Improvement-Tests: forensic, multi_device, gui (~40 Tests)                 | P3.1, P1   | ✅ MF-214..228: alle 6 Kategorien geliefert — forensic (MF-214/215/216), gui (MF-220), multi_device (MF-221), concurrency (MF-226), copy_protection (MF-227, +2 uft_longtrack Bugs), format_extension (MF-228) |
| P3.4  | HIL-Skript + Golden-Reference-Katalog + erster v4.1.4-rc1 HIL-Report      | P3.2, P3.3 | 🔄 MF-185/MF-230: tests/hil/ vollständig — run_hil.py + Scaffold-Tests grün, golden_reference.yaml jetzt ZWEISTUFIG (Software-Tier = #109-Differential-Corpus, CI-aktiv 6/6; Hardware-Tier = 9 Controller-Templates). RC1-Report zeigt beide Tiers. Offen: echter Hardware-Lauf braucht Rig + befüllten Katalog (Axel-Maschine) |

Neue Subagenten (gelandet mit P3.1, MF-182):
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
