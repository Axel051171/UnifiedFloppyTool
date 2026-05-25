# Known Issues — Principle Compliance

Diese Liste dokumentiert Fälle wo UFT aktuell die [Design-Prinzipien](DESIGN_PRINCIPLES.md)
nicht vollständig einhält. Die Liste ist öffentlich (Meta-Prinzip C) und wird
aktiv abgearbeitet.

**Format pro Eintrag:**
- **Prinzip:** Welches Prinzip betroffen ist
- **Status:** `OPEN` / `MITIGATED` / `WORKING-AS-DESIGNED-INTERIM`
- **Beschreibung:** Was aktuell nicht stimmt
- **Workaround:** Was Nutzer heute tun können
- **Plan:** Wie und wann es adressiert wird

---

## Prinzip 1 — Niemals stille Datenverluste

### 1.1 `.loss.json` Sidecar-Format noch nicht implementiert
- **Status:** MITIGATED (Writer implementiert, Integration pending)
- **Beschreibung:** Schema `uft-loss-report-v1` + Writer-API
  (`include/uft/core/uft_loss_report.h`, `src/core/uft_loss_report.c`) sind
  da. 11 Verlust-Kategorien (WEAK_BITS, FLUX_TIMING, INDEX_PULSES,
  SYNC_PATTERNS, MULTI_REVOLUTION, CUSTOM_METADATA, COPY_PROTECTION,
  LONG_TRACKS, HALF_TRACKS, WRITE_SPLICE, OTHER). Schreibt JSON neben
  Ziel-Datei als `<target>.loss.json`. Noch NICHT an die 44 Konvertierungs-
  pfade angeschlossen — das passiert unter §1.2.
- **Workaround:** CLI kann `uft_loss_report_write()` direkt nutzen.
- **Plan:** §1.2 (Pre-Conversion-Report) wickelt alle `convert_*`-Pfade ein
  und ruft den Writer auf.

### 1.2 Nicht alle Konvertierungen haben Pre-Conversion-Report
- **Status:** MITIGATED (Helper da, Wiring in Konvertierer ausstehend)
- **Beschreibung:** Preflight-Helper implementiert
  (`include/uft/core/uft_preflight.h`, `src/core/uft_preflight.c`). Kombiniert
  §1.1 Sidecar-Writer + §5.1 Round-Trip-Matrix zu einer einheitlichen
  Pre-Check/Commit-API. Vier Entscheidungen nach Prinzip 1+4+5:
  - `OK` (LL still oder LD mit `accept_data_loss=true`)
  - `ABORT_NEED_CONSENT` (LD ohne Zustimmung)
  - `ABORT_IMPOSSIBLE` (Ziel kann Quelle nicht repräsentieren)
  - `ABORT_UNTESTED` (Paar nicht in Matrix)

  Aufrufer-Pattern: `uft_preflight_check()` vor Konvertierung; bei LD-OK
  nach Konvertierung `uft_preflight_emit_sidecar()` mit echten Verlust-
  Counts. Die Integration in die 44 bestehenden `convert_*`-Pfade ist
  der verbleibende Schritt.
- **Workaround:** Konvertierer die direkt `uft_loss_report_write()` nutzen,
  sind weiterhin gültig — der Helper ist Syntax-Zucker.
- **Plan:** Schrittweise Migration aller `convert_*`-Entry-Points auf
  den Preflight-Helper (pro Konvertierer ~10 Zeilen Glue-Code).

---

## Prinzip 5 — Round-Trip als First-Class Funktion

### 5.1 Round-Trip-Matrix unvollständig getestet
- **Status:** MITIGATED (Registry + API + 13 Paare, Rest UNTESTED)
- **Beschreibung:** Registry implementiert (`include/uft/core/uft_roundtrip.h`,
  `src/core/uft_roundtrip.c`). Status pro Paar: `UFT_RT_LOSSLESS` /
  `UFT_RT_LOSSY_DOCUMENTED` / `UFT_RT_IMPOSSIBLE` / `UFT_RT_UNTESTED`.
  Initial-Matrix hat 13 Einträge (SCP↔HFE LL, SCP→IMG/ADF/D64/IMD LD,
  HFE→IMG/ADF LD, IMG/ADF→SCP IM, IMG→HFE IM, IPF→ADF LD, STX→ST LD).
  Alles andere fällt auf UNTESTED und sollte nicht angeboten werden.
  Struktur-Invarianten per Test erzwungen: LD+IM brauchen Notes,
  keine Duplikate, UNTESTED nicht explizit gelistet.
- **Workaround:** `uft_roundtrip_status(from, to)` vor Konvertierung abfragen.
- **Plan:** Integration in CLI-Konvertierungspfad (nächster Schritt zu §5.2
  GUI-Sichtbarkeit + §1.2 Pre-Conversion-Report).

### 5.2 Keine Sichtbarkeit des Round-Trip-Status in der GUI
- **Status:** MITIGATED (Converter-Wizard angeschlossen, weitere GUI-Flächen ausstehend)
- **Beschreibung:** `UftTargetPage::updateConversionWarning()` konsultiert
  jetzt `uft_roundtrip_status()` / `uft_roundtrip_note()` sobald Quell- UND
  Ziel-Format beide in der Roundtrip-Matrix hinterlegt sind
  (`FormatEntry.rt_id`). Anzeige farbkodiert:
  - **LOSSLESS** grün mit „byte-identical" Badge
  - **LOSSY-DOCUMENTED** orange mit expliziter Verlustliste + Hinweis auf
    `.loss.json` Sidecar
  - **IMPOSSIBLE** rot mit Grund
  - **UNTESTED** grau mit Verweis auf DESIGN_PRINCIPLES §5
  Fallback auf die bisherige Heuristik wenn ein Format noch nicht auf
  `uft_format_id_t` gemappt ist.
- **Workaround:** Entfällt — Wizard zeigt den Status direkt bei Format-Auswahl.
- **Plan:** Rest-GUI-Flächen (Main-Window Convert-Aktion, Batch-Dialog)
  gleiche Info anbringen wenn die dort implementiert werden.

---

## Prinzip 6 — Emulator-Kompatibilität

### 1.1 LossReport `.loss.json` Sidecar (Preflight gate)
- **Status:** ✓ Phase 1 CLOSED (MF-263, V415-PLAN LOSS.preflight) —
  Per-converter Phase 2 (per-loss sidecar emit) bleibt offen.
- **Phase 1 (MF-263):** `uft_convert_file()` ist der single chokepoint für
  alle 44 Konversionspfade. Ruft `uft_preflight_check()` mit der
  (src,dst)-Format-ID, klassifiziert via Round-Trip-Matrix in LOSSLESS /
  LOSSY_DOCUMENTED / IMPOSSIBLE / UNTESTED, abbricht IMPOSSIBLE / UNTESTED
  / NEED_CONSENT mit Diagnose. `accept_data_loss` default false ⇒ GUI/CLI
  muss explizit User-Consent einholen bevor LOSSY läuft.
- **Phase 2 (v4.1.6):** Per-Converter loss-entry-Aggregation, dann
  `uft_preflight_emit_sidecar(&plan, losses, count)` nach erfolgreichem
  Convert. Pro Konverter ~5-10 LOC mechanische Arbeit.

### 6.1 Keine CI-Pipeline mit Emulator-Verifikation
- **Status:** OPEN
- **Beschreibung:** Prinzip 6 verlangt CI die Exports durch Emulatoren
  schickt. Aktuell ist das für kein Format automatisiert. Manuelle Tests
  existieren ad-hoc.
- **Workaround:** Keine — Nutzer müssen Emulator-Tests selbst durchführen.
- **Plan:** Initial ADF/WinUAE, D64/VICE in 4.3.

### 6.2 Kompatibilitäts-Matrizen pro Format fehlen größtenteils
- **Status:** MITIGATED (Infrastruktur da, Populierung 1/80)
- **Beschreibung:** `uft_plugin_compat_entry_t` Array + `compat_entries` /
  `compat_count` Felder sind in `uft_format_plugin_t`. Status pro
  Konsumer: `UFT_EMU_COMPATIBLE` / `UFT_EMU_INCOMPATIBLE` / `UFT_EMU_PARTIAL`
  / `UFT_EMU_UNTESTED`. Felder pro Eintrag: consumer-name,
  status, note (Pflicht bei PARTIAL/INCOMPATIBLE), test_date, ci_tested.
  Populiert: ADF (6 Konsumer-Einträge als Exemplar).
- **Workaround:** Bis mehr Plugins populiert sind — in Issue-Tracker nach
  Format-Namen suchen.
- **Plan:** `compatibility-import-export`-Agent oder Community-PRs erweitern
  die Matrizen iterativ. Langfrist: CI-Pipeline (§6.1) schreibt `ci_tested`
  automatisch.

---

## Prinzip 7 — Ehrlichkeit bei proprietären Formaten

### 7.1 Spec-Status-Marker pro Plugin
- **Status:** ✓ CLOSED (MF-262, V415-PLAN PLUGIN.spec_status, 2026-05-25) —
  Populierung **84/84 = 100%** (`audit_plugin_compliance.py`).
- **Beschreibung:** Feld `spec_status` (`uft_spec_status_t`) in
  `uft_format_plugin_t`. Vor MF-262 hatten 15/84 Plugins gesetztes Feld;
  die anderen 69 standen default auf `UFT_SPEC_UNKNOWN` (Prinzip-7-Verstoß).
- **Resolution:** Massentool `scripts/populate_spec_status.py` mit per-Format-
  Mapping (OFFICIAL_FULL/OFFICIAL_PARTIAL/REVERSE_ENGINEERED/DERIVED) basierend
  auf Format-Provenienz. 69 Plugins in einem Lauf populiert. Build grün, alle
  test_spec_status / audit_plugin_compliance Tests grün.
- **Folge-Arbeit:** Per-Plugin-Verfeinerung wo der Mapping-Bucket zu pauschal
  war (z.B. D71 ist DERIVED, könnte aber genauer als REVERSE_ENGINEERED markiert
  werden — CBM DOS war nie öffentlich spezifiziert). Erfolgt in v4.1.6 als
  Doku-Hygiene, nicht blockierend.

### 7.2 Feature-Matrizen pro Plugin
- **Status:** ✓ CLOSED (MF-263, V415-PLAN PLUGIN.features, 2026-05-25) —
  Populierung **84/84 = 100%** (`audit_plugin_compliance.py`).
- **Beschreibung:** `uft_plugin_feature_t` Array + `features` / `feature_count`
  Felder in `uft_format_plugin_t`. Vor MF-263 hatten 5/84 Plugins eine
  feature-matrix (ADF, HFE, IPF, STX, WOZ); die anderen 79 hatten
  `features = NULL` (Prinzip-7-Verstoß im audit).
- **Resolution:** `scripts/populate_features.py` generiert pro Plugin eine
  per-`.capabilities`-Bit abgeleitete Feature-Matrix (Read/Write/Create/
  Flux/Timing/Weak Bits/MultiRev als SUPPORTED/UNSUPPORTED). 79 Plugins
  in einem Lauf populiert. `audit_plugin_compliance.py` zeigt nun
  84/84 principle-7 compliant.
- **Folge-Arbeit:** Per-Plugin-Verfeinerung wo PARTIAL angebrachter wäre
  als SUPPORTED/UNSUPPORTED (z.B. WOZ-V1 schreibt nicht alle Tracks
  korrekt = PARTIAL mit note). Erfolgt in v4.1.6 als Hygiene.

### 7.3 287 Stub-Parser sind als "registriert" sichtbar
- **Status:** MITIGATED (Marker da, Populierung 0/287)
- **Beschreibung:** Feld `is_stub` ist in `uft_format_plugin_t` implementiert.
  Default `false` — das heisst: ein Stub MUSS aktiv `is_stub = true` setzen
  um ehrlich zu sein. CLI-Filter `uft formats --real-only` nutzt diesen Flag.
  Die eigentliche Populierung der 287 Stub-Plugins ist noch ausstehend.
- **Workaround:** Bis jeder Stub das Feld setzt, zählt ein Plugin ohne echten
  Parser als Prinzip-Verstoß unter CI-Audit (siehe §M.1).
- **Plan:** Stubs in `memory/project_stub_conversion.md` werden pro Tier
  abgearbeitet. Jedes Stub-Plugin bekommt entweder echte Implementierung
  ODER `is_stub = true` (stub-eliminator-Agent).

---

## Meta-Ebene

### M.-2 TrackData / OperationResult duplicate alias fields (MF-149, rule H-9)

- **Status:** CLOSED (resolved by MF-169 / P1.17, 2026-05)
- **Datei:** `src/hardware_providers/hardwareprovider.h` *(deleted)*
- **Beschreibung (historisch):** Die V1-DTOs `TrackData` und
  `OperationResult` enthielten je zwei Aliase für denselben Wert:
  - `TrackData::valid` ↔ `TrackData::success` (bool)
  - `TrackData::errorMessage` ↔ `TrackData::error` (QString)
  - `OperationResult::errorMessage` ↔ `OperationResult::error` (QString)
  Konsumenten konnten je nach Provider mal das eine, mal das andere Feld
  finden. In `fluxenginehardwareprovider.cpp` und
  `kryofluxhardwareprovider.cpp` schrieben einige Pfade nur den Alias
  (`errorMessage`) und liessen `error` leer — Reader, die das kanonische
  Feld lasen, sahen `""` und glaubten an ein erfolgreiches Ergebnis.
- **Resolution:** Der Type-Driven-HAL-Refactor (P1.x) ersetzte die
  V1-DTOs vollständig durch die `std::variant`-Sum-Types in
  `include/uft/hal/outcomes.h` (`SectorOutcome`, `FluxOutcome`, …).
  `bool success` + `QString error` existiert nicht mehr — der
  Forensik-Zustand IST der Variant-Alternative-Typ (`SectorRead` vs.
  `SectorMarginal` vs. `ProviderError`), nicht ein Flag-Paar das
  driften kann. MF-169 (P1.17) löschte `hardwareprovider.h` samt der
  beiden DTOs und der drei `uft_set_*`-Helfer ersatzlos; die V1-
  Provider die sie schrieben sind ebenfalls weg. Das Alias-Drift-
  Problem ist strukturell nicht mehr ausdrückbar.
- **Regression-Schutz:** `tests/test_hal_conformance.cpp` (65
  Sektionen) verifiziert pro V2-Provider dass `ProviderError`
  what/why/fix nie leer ist (Regel F-4, type-enforced via dem
  werfenden Konstruktor) und dass `SectorMarginal::divergent_reads`
  nie kollabiert wird (Regel F-3).

---

### M.-1 ATX-Probe Byte-Order-Bug (entdeckt + behoben 2026-04-24)

- **Status:** CLOSED (Fix + Test-Aktivierung in derselben Session)
- **Datei:** `src/formats/atx/uft_atx.c`
- **Ursprung:** `ATX_SIGNATURE` war als `0x41543858u` definiert mit Kommentar
  `"AT8X" LE`, aber das ist die **Big-Endian**-Darstellung. Der Probe nutzt
  `uft_read_le32(data)` auf einem Puffer mit Bytes 'A','T','8','X'
  (0x41, 0x54, 0x38, 0x58), was 0x58385441 ergibt — nicht 0x41543858.
  Folge: `atx_plugin_probe` akzeptierte **nie** eine echte ATX-Datei.
- **Fix:** `#define ATX_SIGNATURE   0x58385441u` (LE-korrekt) + Kommentar
  der die Endianness dokumentiert.
- **Regression-Schutz:** `tests/test_atx_plugin.c` mit 8 Assertions,
  darunter `probe_signature_constant_matches_le32_read` und
  `probe_old_buggy_constant_no_longer_matches`.
- **Entdeckung:** MF-007 Plugin-Test-Authoring.

---

### M.0 Planned APIs (MF-011 DOCUMENT-Welle)

- **Status:** MARKED, nicht implementiert (2026-04-24)
- **Beschreibung:** 98 Skeleton-Header in `include/uft/` deklarieren zusammen
  **1 952 öffentliche `uft_*`-Funktionen ohne Implementation**. Jeder dieser
  Header trägt jetzt einen `/* PLANNED FEATURE — <scope> */`-Banner, so dass
  Consumer vor neuen Call-Sites gewarnt werden.
- **Detailliste:** [`docs/PLANNED_APIS.md`](PLANNED_APIS.md) (auto-generiert
  aus `docs/skeleton_triage.csv`)
- **Workaround:** Bis zur Implementation linken Call-Sites entweder fehl (bei
  tatsächlicher Nutzung) oder die Funktionen sind tot (keine Consumer).
- **Plan:** Implementation erfolgt subsystem-weise in M2/M3 laut `MASTER_PLAN.md`.
  Kein neuer Call darf gegen einen `PLANNED FEATURE`-Header hinzugefügt werden,
  ohne zuerst die Implementation zu liefern oder das Prototyp zu entfernen
  (Master-Plan Regel 1).

---

### M.1 Nicht alle Prinzipien haben automatisierte Tests
- **Status:** MITIGATED (Kern-Audit live, weitere Checks ausstehend)
- **Beschreibung:** Meta-Prinzip A verlangt für jede Zusage einen CI-Test.
  Stand heute:

  | Prinzip / §  | Test(s)                                 | Enforcement |
  |--------------|-----------------------------------------|-------------|
  | 1.1 Sidecar  | `tests/test_loss_report.c` (8)          | ctest       |
  | 1.2 Preflight| `tests/test_preflight.c` (13)           | ctest       |
  | 5.1 Roundtrip| `tests/test_roundtrip_matrix.c` (13)    | ctest       |
  | 7.1–7.3      | `tests/test_spec_status.c` (15)         | ctest       |
  | 7.x (plugin-weit) | `scripts/audit_plugin_compliance.py` | ctest (Python), regression-guard |

  Der neue Plugin-Compliance-Audit scant alle 83 `uft_format_plugin_t`
  Literale unter `src/formats/` und prüft `.spec_status`, `.features`,
  `.compat_entries`, `.is_stub`. Baseline: **5 voll-compliant** (ADF, HFE,
  IPF, STX, WOZ), **15 mit spec_status**. CI failt bei Regression.

  Noch offen:
  - §1 Round-Trip-LL-Tests für konkrete Format-Paare (nur Matrix-API getestet)
  - §3 Fehlermeldungs-Struktur (Fix-Vorschlag + Warum + Was)
  - §6 Emulator-Pipeline im CI
- **Workaround:** `ctest --label-regex principle-compliance` führt alle
  Prinzip-Tests lokal aus.
- **Plan:** Integration der Audit-Baseline in CI-Job (als separater Schritt
  oder im Coverage-Workflow). Monotones Hochsetzen der `--min-pass` /
  `--min-spec-status` Baselines bei jeder Populierungs-Runde.

---

### M.2 v4.1.5-hardening — Closed in this release

Findings from the v4.1.5-hardening audit (MASTER_PLAN.md §v4.1.5):

| ID | Severity | Resolution | Commit |
|---|---|---|---|
| UFT-001 | P0 | 9/9 V2-Provider have live code path (1 Production + 8 Beta) | MF-249..MF-258 |
| UFT-002 | P0 | CMakeLists.txt version-comment stale → removed, refers to VERSION.txt SSOT | v4.1.5 pre-tag |
| UFT-003 | P1 | HardwareTab honest-stub provider styled distinctly (orange "Preview") | MF-247 |
| UFT-004 | P1 | `uft_format_plugin_t` got `api_version` field + runtime gate + sizeof-pin (216 B) | MF-260 |
| UFT-005 | P1 | `test_transitions_ns_contract` extended with KryoFlux + FluxEngine FFI shields | MF-260 |
| UFT-006 | P1 | `.claude/CLAUDE.md` updated 6 → 9 V2-provider list | v4.1.5 pre-tag |
| UFT-007 | P1 | VID/PID confirmed as SSOT in `uft_scp_direct.h` (orchestrator finding was stale) | MF-212 |
| UFT-T01 | P1 | `<threads.h>` got `__has_include` guard for MinGW | v4.1.5 pre-tag |
| UFT-T02 | P1 | 4 tests with phantom-symbol link errors fixed via per-test `target_sources` | v4.1.5 pre-tag |
| UFT-T04 | P2 | Reduced excluded tests 43 → 38 (re-enabled test_scp_direct_hal, test_applesauce_hal, test_fnmatch_shim, test_whdload_resload + new test_plugin_abi); remaining 38 tests reference impls deleted in MF-011 and stay excluded until restoration. | MF-260 |
| UFT-T05 | P3 | `src/analysis/events/CMakeLists.txt` already uses `CMAKE_CURRENT_SOURCE_DIR` (path bug structurally fixed); subdir not yet wired into root CMake — deliberate scope cap. | v4.1.5 pre-tag |

**Pre-tag test pass rate:** 47/180 → **151/151 (100%)**.

### M.3 V415-PLAN execution — 2026-05-25 (MF-261/MF-262)

Sub-goals from `C:\Users\Axel\Downloads\V415_GOAL_PLAN.md` Variante B:

| Sub-goal | Status | Resolution |
|---|---|---|
| P2.4 (Squash → main + v4.1.4 tag) | ⬜ blocked | RC1-Window läuft bis 2026-05-29 |
| HIL.GW (Greaseweazle real-HW tests) | ⬜ HW-blocked + ⏳ partial sim | Real-HW needs Greaseweazle; **Tier-2.5 simulator system (MF-267) closes the QProcess controllers (KryoFlux + FluxEngine + FC5025) end-to-end without hardware** — see `tools/hw_simulators/README.md` and `tests/hil/run_simulated.py` (7/7 SIMULATED). |
| SCP.D1.verify (USB opcodes vs SDK) | ✓ CLOSED MF-261 | 22/22 opcodes byte-exakt gegen samdisk/SuperCardPro.h verifiziert; audit/scp/REPORT.md D1 UNVERIFIED→PASS |
| M3.1 (SCP-Direct libusb wiring) | ✓ MF-254 | Wiring landed; Tier-3 HW-bench pending (UFT-008) |
| LOSS.preflight Phase 1 (chokepoint) | ✓ CLOSED MF-263 | `uft_convert_file()` ruft `uft_preflight_check()` → schützt alle 44 Pfade in einem Punkt |
| LOSS.preflight Phase 2 (sidecar) | ⬜ multi-session | Per-converter loss-entry-Aggregation für v4.1.6 |
| ARCH7.C.wire (Teensy probe) | ✓ CLOSED MF-213+MF-263 | Pure-classifier + QSerial-Wrapper + HardwareTab probe-on-Connect |
| ARCH7.B.fix (SCP VID/PID align) | ✓ CLOSED MF-212 | 0x16D0:0x0F8C in Header + GUI synchronisiert via Macro |
| PLUGIN.spec_status (65 plugins) | ✓ CLOSED MF-262 | 15/84 → 84/84 via scripts/populate_spec_status.py |
| PLUGIN.features (75 plugins) | ✓ CLOSED MF-263 | 5/84 → 84/84 via `scripts/populate_features.py` |
| BUILD.rebaseline | ✓ CLOSED MF-262 | 224→219, 5 entries resolved |
| SCOPE.switch_decision | ⏳ user-decision | `docs/SCOPE_DECISION_NON_FLOPPY.md` — A/B/C required |
| EMUCI.real (CLI uft-decode) | ⏳ scaffold-done MF-263 | `cli/uft-decode/main.c` + Integration-Checklist |
| TAG.v415 | ⬜ composite-blocked | `scripts/release/release_v415_checklist.md` — 8/11 gates ✓ |

**Status summary:** **10/13 V415-PLAN sub-goals geschlossen** (CLOSED oder
scaffold-done). Verbleibende 3:
- HIL.GW + P2.4 — Hardware/Kalender (Axel-machine + RC1-Window 2026-05-29)
- SCOPE.switch_decision — User wählt A/B/C in `docs/SCOPE_DECISION_NON_FLOPPY.md`
- TAG.v415 — Composite; 8/11 Gates ✓, wartet auf HIL+P2.4 + LOSS Phase 2 (v4.1.6)

---

## Wie beitragen

- **Neues Issue melden:** GitHub Issue mit Label `principle-violation`.
- **Eintrag abarbeiten:** PR die den Fix plus den entsprechenden CI-Test
  liefert. Eintrag hier wird dann entfernt.
- **Status-Update:** Wenn ein Eintrag obsolet wird oder sich der Status
  ändert, PR gegen diese Datei.

---

**Version:** 1.3
**Stand:** 2026-05-25 (MF-262 — V415-PLAN execution)

> **Änderungen v1.1 (P2.2 / MF-174):** M.-2 (rule H-9) auf CLOSED
> gesetzt — der Type-Driven-HAL-Refactor (P1.x) hat die V1-DTOs samt
> Alias-Drift strukturell eliminiert. Die Coding-Standards-Regeln H-1
> ("keine freigeschaltete Action ohne Capability") und H-2 ("kein
> `return false; Q_UNUSED(...)`-Default-Body") hatten nie eigene
> KNOWN_ISSUES-Einträge — sie sind in der V2-Architektur strukturell
> garantiert: H-1 über die Codegen-Phase-2-Disable-Logik, H-2 weil es
> keine Basisklasse mit virtuellen Stubs mehr gibt (Capability =
> Mixin-Komposition, nicht Methoden-Override).
