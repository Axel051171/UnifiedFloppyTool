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

## Audit 2026-06-10 — Closing Note (16 Findings)

Vollständiger 6-Spezialisten-Audit (must-fix-hunter, consistency-auditor,
forensic-integrity, abi-bomb-detector, stub-eliminator, single-source-
enforcer) auf Basis der Fable-5-Agent-Suite. Verifiziert via
`check_consistency.py` 0/0/0/0, `verify_build_sources.py` 0 Regressionen,
`audit_plugin_compliance.py` 84/84.

| ID | Severity | Status | Auflösung |
|---|---|---|---|
| **A01** | P0 | ✓ DONE | Shared `uftc_preflight_gate()` Helper; beide Entry-Points durchlaufen ihn (§1.1) |
| **A02** | P0 | ✓ DONE | KryoFlux→D64 ehrlicher `UFT_ERR_NOT_IMPLEMENTED` statt empty-image fabrication |
| **A03** | P0 | ✓ DONE (Helper + Field) | `UftFormatConverterWizard::promptLossyConsent()` + `accept_data_loss`-Feld. Wizard-Mockup→Worker-Wiring ist separates Follow-up |
| **A04** | P1 | ✓ DONE | Bounded `uftc_add_warning()` Helper; 60 unbounded snprintf-Sites mechanisch migriert |
| **A05** | P1 | ✓ DONE | `accept_data_loss` als eigenes Feld an `uft_convert_options_t` + `_ext_t` angehängt |
| **A06** | P1 | ✓ DONE | `uft_provenance.c` + `apridisk.c` Tool-Version-Stamp aus `UFT_VERSION_FULL` |
| **A07** | P1 | ✓ DONE | PLL-Standalone-Impl in `src/core/uft_pll.c` (neu); Duplikat-Struct aus `uft_core_stubs.c` entfernt |
| **A08** | P1 | ✓ DONE | Sektor→Sektor Raw-Byte-Copy mit `success=true` → `UFT_ERR_NOT_IMPLEMENTED`; IMG↔IMA via Same-Format-Pfad |
| **A09** | P2 | ✓ DONE | Root `CLAUDE.md`: 6 Metriken-Drifts korrigiert (plugins 80→84, paths 44→45, agents 25→22, tests, SCP-HAL-Status, dir-listing) |
| **A10** | P2 | ✓ DONE | `.claude/CLAUDE.md` Refactor-Banner: AKTIVER → ABGESCHLOSSEN mit MF-150/169/174/176 |
| **A11** | P2 | ✓ DONE | `check_consistency.py`: Pattern-Scan für hartkodierte Versions-Literale in `src/`+`include/`; Negativ-Test verifiziert |
| **A12** | P2 | ✓ DONE | KNOWN_ISSUES.md Doppel-§1.1 konsolidiert; Duplikat unter Prinzip 6 entfernt |
| **A13** | P2 | OFFEN | Commit-Strategie für die uncommitteten Bundles (Audit-Folge selbst) |
| **A14** | P2 | ✓ DONE | Pre-Commit-Hook Foundation-Gate toolchain-tolerant (`scripts/git-hooks/pre-commit`) |
| **A15** | P3 | ✓ VERIFIED | Stub-Inventar in dokumentiertem Zustand (KNOWN_ISSUES §7.3, M.0); keine Code-Änderung nötig |
| **A16** | P3 | ✓ DONE | ADF Write-Side TODOs (`add_file`, `delete`) → neuer KNOWN_ISSUES §7.4 Eintrag + Source-TODOs verlinken zurück |

**Nicht-Ziel:** Diese Tabelle wird nicht wieder aktualisiert; sie ist
historische Audit-Spur. Neue Findings landen in den thematischen
Abschnitten unten. Audit-Nachfolger entstehen ggf. mit eigener Closing-
Note (z.B. „Audit 2026-09-XX").

**Während des Audits entstandene Follow-ups:**
- Wizard-Progress-Page-Simulation → realer Worker-Wiring (UI-Job,
  blockiert A03 Endnutzung in der GUI)
- Batch-Wizard `uft_convert_options_ext_t` → `_t` Type-Confusion
  (pre-existing, in `src/gui/uft_batch_wizard.cpp:1064-1081` als
  Kommentar markiert)
- v4.2 ADF Write-Side echtes Implementieren (§7.2)
- Phase 2 Per-Converter loss-entry-Aggregation (§1.1)

---

## Prinzip 1 — Niemals stille Datenverluste

### 1.1 LossReport `.loss.json` Sidecar (Preflight gate)
- **Status:** ✓ Phase 1 CLOSED (MF-263 + UFT-A01 follow-up,
  V415-PLAN LOSS.preflight) — Per-converter Phase 2 (per-loss sidecar
  emit) bleibt offen.
- **Demo-Impact:** keiner — Phase 2 (category-level) ist live; per-track
  exakte Counts (v4.1.6) sind nicht im Demo-Scope.
- **Phase 1 (MF-263 + UFT-A01):** der gemeinsame Helper
  `uftc_preflight_gate()` in `src/formats/uft_format_convert_dispatch.c`
  ist der single chokepoint für alle 45 Konversionspfade — wird sowohl
  von `uft_convert_file()` als auch von `uft_convert_memory()` gerufen.
  Ruft `uft_preflight_check()` mit der (src,dst)-Format-ID, klassifiziert
  via Round-Trip-Matrix in LOSSLESS / LOSSY_DOCUMENTED / IMPOSSIBLE /
  UNTESTED, abbricht IMPOSSIBLE / UNTESTED / NEED_CONSENT mit Diagnose.
  `accept_data_loss` default false ⇒ GUI/CLI muss explizit User-Consent
  einholen bevor LOSSY läuft.
  Memory-Mode (`uft_convert_memory()`) durchläuft denselben Gate, kann
  aber kein `.loss.json` Sidecar emittieren (kein on-disk dst_path) —
  das Gate selbst gilt trotzdem.
- **Sidecar-Schema:** `uft-loss-report-v1` (`include/uft/core/uft_loss_report.h`,
  `src/core/uft_loss_report.c`), 11 Verlust-Kategorien (WEAK_BITS, FLUX_TIMING,
  INDEX_PULSES, SYNC_PATTERNS, MULTI_REVOLUTION, CUSTOM_METADATA,
  COPY_PROTECTION, LONG_TRACKS, HALF_TRACKS, WRITE_SPLICE, OTHER). JSON
  als `<target>.loss.json` neben Ziel-Datei.
- **UFT-A01 follow-up (2026-06-10):** vor diesem Commit umging
  `uft_convert_memory()` den Gate komplett (Audit-Findung A01). Helper
  herausgezogen, beide Entry-Points darauf umgestellt; „single chokepoint"-
  Garantie ist jetzt strukturell statt nur als Kommentar.
- **Phase 2 (v4.1.6):** Per-Converter loss-entry-Aggregation, dann
  `uft_preflight_emit_sidecar(&plan, losses, count)` nach erfolgreichem
  Convert. Pro Konverter ~5-10 LOC mechanische Arbeit.

### 1.2 Nicht alle Konvertierungen haben Pre-Conversion-Report
- **Status:** MITIGATED (Helper da, Wiring in Konvertierer ausstehend)
- **Demo-Impact:** keiner — der gemeinsame Helper `uftc_preflight_gate()`
  in `uft_format_convert_dispatch.c` ist seit MF-263 + UFT-A01 der
  single chokepoint für alle 45 Pfade; beide Entry-Points
  (`uft_convert_file()`, `uft_convert_memory()`) durchlaufen ihn. Demo
  zeigt das als Sicherheits-Feature.
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
- **Demo-Impact:** Workaround — Demo nutzt eines der 13 verifizierten
  Paare (Empfehlung: SCP→D64 oder SCP→IMG). Andere Paare bleiben außen vor.
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
- **Demo-Impact:** keiner — Wizard ist die Demo-Fläche. Andere GUI-Flächen
  werden im Demo-Skript nicht angefasst.
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

<!-- UFT-A12 fix (2026-06-10): the misplaced "1.1 LossReport" entry that
     used to live here was a content-duplicate of §1.1 under Prinzip 1,
     not an Emulator-Kompatibilität item. Consolidated into the canonical
     §1.1 above; this section now contains only §6.x items. -->


### 6.1 Keine CI-Pipeline mit Emulator-Verifikation
- **Status:** OPEN
- **Demo-Impact:** keiner — Emulator-CI ist Backend-Hygiene, im Demo nicht sichtbar.
- **Beschreibung:** Prinzip 6 verlangt CI die Exports durch Emulatoren
  schickt. Aktuell ist das für kein Format automatisiert. Manuelle Tests
  existieren ad-hoc.
- **Workaround:** Keine — Nutzer müssen Emulator-Tests selbst durchführen.
- **Plan:** Initial ADF/WinUAE, D64/VICE in 4.3.

### 6.2 Kompatibilitäts-Matrizen pro Format fehlen größtenteils
- **Status:** MITIGATED (Infrastruktur da, Populierung 1/80)
- **Demo-Impact:** keiner — Demo nutzt ADF (das populierte Plugin als Exemplar).
  Andere Formate zeigen leere Compat-Matrix, sind aber sonst funktional.
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

### 7.3 „287 Stub-Parser sind als registriert sichtbar"
- **Status:** ✓ CLOSED (MF-300, Plugin-is_stub-Triage 2026-07-02)
- **Auflösung:** Die These war doppelt überholt. (1) Die „287 Stubs"
  waren der Zensus von VOR dem MF-011-Cleanup — die Pattern-A-Dateien
  wurden gelöscht bzw. migriert (Rest: 4 REVIEW-Dateien, nicht
  registriert). (2) Die Triage aller **84 registrierten Plugins**
  (read_track-Body-Analyse + Test-Existenz, Script-gestützt) ergab:
  **83/84 haben echte Read-Implementierungen.** Der einzige Treffer —
  `g71_plugin_read_track` gab UFT_OK mit LEEREM Track zurück
  (fabrizierter Erfolg, A02-Muster) — wurde in MF-300 auf ehrliches
  `UFT_ERR_NOT_IMPLEMENTED` + Feature-Matrix `Read: PARTIAL` mit Note
  umgestellt.
- **Konsequenz:** Kein `is_stub=true`-Sweep nötig — es gibt keine
  registrierten Stub-Plugins. Das Feld bleibt für künftige Plugins als
  Ehrlichkeits-Mechanismus bestehen (`uft formats --real-only` nutzt es).
- **Erkannter Detector-Gap (dokumentiert, nicht kritisch):** semantisch
  faule Bodies (trivial-init + return UFT_OK) entgehen dem
  Pattern-1-Check des Lazy-Stub-Detectors; die Triage-Methodik dieses
  Eintrags (Body-LOC + Fehler-Return-Analyse) ist das Werkzeug dafür.

### 7.4 ADF Write-Side honest-stubs (`add_file`, `delete`)
- **Status:** OPEN (honest-stub, return -1)
- **Demo-Impact:** keiner — Read-Side voll funktional, Write-Side wird
  in der GUI nicht angeboten.
- **Beschreibung:** `uft_adf_add_file()` (`src/formats/uft_adf.c:897-901`)
  und `uft_adf_delete()` (`src/formats/uft_adf.c:907-911`) sind
  honest-stubs die `-1` zurückgeben. Ein dritter abgeleiteter Stub
  (Bitmap-Alloc / Directory-Hash / Checksum-Upkeep, ab Zeile 1013)
  fällt auf dieselben zwei zurück und gibt ebenfalls -1.
- **Hinweis:** TODOs in-source verweisen darauf zurück, sind aber bislang
  nirgendwo getrackt — eingetragen via UFT-A16 Audit-Follow-up.
- **Plan:** v4.2 — AmigaDOS bitmap alloc + directory hash insertion +
  block-checksum upkeep als ein zusammenhängender Patch
  (~300-500 LOC). Bis dahin: dokumentiert hier statt als „lazy stub".
  Übergreifend gesteuert via `docs/STUB_ELIMINATION_PLAN.md` Phase 5.

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
- **Demo-Impact:** keiner — `PLANNED FEATURE`-Banner schützen Consumer.
  Demo nutzt keine dieser Header-Funktionen.
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
- **Demo-Impact:** keiner — Test-Coverage ist intern. Demo zeigt Audit-Ergebnis
  (`audit/MASTER_REPORT.md`) als Status, nicht den Coverage-Stand der Tests.
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
| SCOPE.switch_decision | ✓ RESOLVED 2026-05-25 → C (Delete) | User-bestätigt: Option C = delete `src/switch/` + `src/cart7/` + GUI-Tab. Ausführung POST v4.1.5-tag (MF-271), NICHT im RC1-Window. `src/whdload/` bleibt. Pre-delete `archive/pre-mf271` Tag als Backup. |
| EMUCI.real (CLI uft-decode) | ⏳ scaffold-done MF-263 | `cli/uft-decode/main.c` + Integration-Checklist |
| TAG.v415 | ⬜ composite-blocked | `scripts/release/release_v415_checklist.md` — 8/11 gates ✓ |

**Status summary:** **10/13 V415-PLAN sub-goals geschlossen** (CLOSED oder
scaffold-done). Verbleibende 3:
- HIL.GW + P2.4 — Hardware/Kalender (Axel-machine + RC1-Window 2026-05-29)
- SCOPE.switch_decision — User wählt A/B/C in `docs/SCOPE_DECISION_NON_FLOPPY.md`
- TAG.v415 — Composite; 8/11 Gates ✓, wartet auf HIL+P2.4 + LOSS Phase 2 (v4.1.6)

### M.4 XUM1541 HAL-vs-OpenCBM Protokoll-Deltas (Emulator-Probe 2026-07-02)

- **Status:** ✓ RESOLVED IN CODE (MF-301, OpenCBM-Quell-Audit) —
  Tier-3 HW-Bench-Verifikation weiterhin ausstehend (wired-but-unbenched)
- **Audit-Quelle:** OpenCBM master, verbatim gelesen:
  `xum1541/xum1541_types.h`, `opencbm/lib/plugin/xum1541/xum1541.c`,
  `opencbm/lib/plugin/xum1541/archlib.c`
- **Verdikt 1 (Status-Read):** OpenCBM hat recht — `XUM_STATUSBUF_SIZE=3`,
  `[status, val_lo, val_hi]` LE. HAL-Fix: `xum_wait_status()` liest 3
  Bytes, BUSY-Loop wie der OpenCBM-Host, Extended-Value wird genutzt
  (WRITE: tatsächliche Byte-Anzahl → Short-Write ist jetzt ehrlicher
  Fehler).
- **Verdikt 2 (Header-Layout):** OpenCBM hat recht —
  `[opcode, proto|flags, size_lo, size_hi]`. Protokoll-Byte: obere
  Nibble Protokoll (`XUM1541_CBM=(1<<4)` …), untere Nibble Flags
  (`WRITE_TALK=1`, `WRITE_ATN=2`). HAL-Header + Impl umgestellt.
- **Verdikt 3 (NEU, HIGH — vom Audit entdeckt):** Die gesamte
  UFT-Bulk-Opcode-Tabelle war **fiktional** (WRITE_DATA=0, TALK=1,
  LISTEN=2, …, OPEN=8, CLOSE=9). Real existieren nur READ=8 und
  WRITE=9 — unser OPEN/CLOSE kollidierte mit den echten READ/WRITE!
  IEC-Adressierung ist KEINE eigene Opcode-Familie, sondern WRITE mit
  ATN-Flag und den rohen IEC-ATN-Bytes als Payload (LISTEN=0x20|dev,
  TALK=0x40|dev, Secondary=0x60|sec, UNLISTEN=0x3F, UNTALK=0x5F).
  HAL komplett auf diese Semantik umgeschrieben.
- **Verdikt 4 (IOCTL-Transport):** Bulk-Commands `[cmd, arg1, arg2, 0]`
  + 3-Byte-Status, NICHT Control-Transfer. Header-Kommentar korrigiert;
  IOCTL-Konstanten 23-31 verifiziert (waren korrekt). Neu:
  `uft_xum_iec_poll()` (IOCTL 27) als erster echter IOCTL-Wrapper.
- **Verdikt 5 (EOI-Länge):** `uft_xum_iec_read()` hat jetzt den
  `bytes_read`-Out-Parameter — EOI-verkürzte Transfers sind vom
  vollen Read unterscheidbar (forensische Längen-Erhaltung).
- **Restrisiko:** Alles gegen OpenCBM-QUELLE verifiziert, nicht gegen
  Silizium. Tier-3-Bench (UFT-008-Pattern) bleibt Gate für
  „production". Emulator-Anpassung an die verifizierte Wahrheit:
  siehe `tests/emulators/xum1541/DIVERGENCES.md` (MF-301 Folge-Lauf).

### CI-1 — CI-Test-Lauf durch `|| true` maskiert → ✓ RESOLVED (2026-07-04)

> **✓ RESOLVED.** Das Gate ist scharf: `|| true` ist aus beiden „Run
> tests"-Schritten entfernt, alle drei Test-Jobs (Linux 6.7.3, Linux
> 6.10.1, Windows) melden **163/163 grün** und ein failender Test rötet
> jetzt CI. Vorgehen (Commits MF-311..): (1) `-- -k` keep-going im Build,
> damit ein Breaker nicht ~150 Tests als „Not Run" mitreißt; das zeigte,
> dass real nur **7** Tests failten, nicht die halbe Suite. (2) 4 Build-
> Breaker gefixt: Linux `uft_ufi_linux_ops` (ufi_linux.c zur Test-Surface),
> Windows libusb (self-contained Mock-Stub `tests/usb_mock/libusb-1.0/
> libusb.h`). (3) 3 Runtime-Bugs gefixt: fnmatch NULL-Segfault auf POSIX
> (Header-Guard + `<stddef.h>`), test_roundtrip `/tmp`-Hardcode →
> TMPDIR/TMP/TEMP, test_wiring_runtime headless-Qt → offscreen. (4) `||
> true` entfernt, pro Plattform 163/163 verifiziert. Die ursprüngliche
> Fehldiagnose (headless-Qt/working-dir für ~95 Tests) war falsch: die
> Masse war „Not Run" durch den Build-Abbruch, nicht Runtime.

**Severity: HIGH (historisch — jetzt behoben).** Der „Run tests"-Schritt in `.github/workflows/ci.yml`
(Linux + macOS + Windows) wrappt `ctest` in `|| true`. Ein Versuch, das
Gate zu härten (MF-311: `--no-tests=ignore` **ohne** `|| true`), hat
aufgedeckt, dass die CI-**Umgebung** einen Großteil der Tests zur
Laufzeit failt, während dieselbe Suite lokal 100 % grün ist:

| Umgebung | ctest-Ergebnis mit echtem Gate | lokal (MinGW) |
|---|---|---|
| Linux 6.7.3 / 6.10.1 | ~95 / 162 FAILED (40 % pass) | — |
| Windows 2022 | ~133 / 162 FAILED (18 % pass), `test_roundtrip`: „fopen write failed" | — |
| lokal Win/MinGW | — | **163 / 163 PASS** |

Die failenden Tests sind nicht eine Klasse (GUI/Qt), sondern praktisch
die **gesamte Suite in Reihenfolge** (`test_2mg_plugin`, `test_3ds`,
`test_a2r_plugin`, …). Symptome: „no Qt platform plugin could be
initialized" (Linux, headless — jeder Test linkt via globalem
`CMAKE_AUTOMOC` Qt), Datei-Schreib-Fehler / Working-Dir-Rechte (Windows).

**Root cause: Umgebung, nicht die Tests.** `|| true` maskiert das seit
dem Schreiben des Workflows — die CI hat den Test-Lauf **nie** wirklich
grün gehabt, nur den Build.

**Konsequenz:** Alle 151 Unit-Tests + 439 Emulator-Assertions sind auf
CI effektiv **ungated**. Eine echte Regression würde `|| true` schlucken.

**Warum noch offen (Scope):** Das Gate zu härten ist erst sicher, wenn
die CI-Umgebung grün ist. Das ist eine eigene, mehrschrittige Aufgabe,
die CI-Iteration braucht (headless-Qt `QT_QPA_PLATFORM=offscreen`,
Windows-Working-Dir-Fix, ggf. Qt-Runtime-Pfad) und **lokal nicht
reproduzierbar** ist (lokal 163/163). `|| true` bleibt bis dahin bewusst
drin (mit Verweis auf diesen Eintrag im Workflow-Kommentar), um `main`
nicht dauerhaft rot auf einem vorbestehenden Umgebungs-Defekt zu halten.

**Nächster Schritt:** eigener CI-Env-Fix-Task —
(1) `QT_QPA_PLATFORM: offscreen` + nötige Qt-Runtime-Env im Test-Step,
(2) Windows-Working-Dir/Temp-Schreibrechte für `test_roundtrip` & Co
(`add_test(... WORKING_DIRECTORY ...)` oder Temp-Pfad in den Tests),
(3) danach `|| true` entfernen und pro Plattform grün verifizieren.

### FMT-1 — D67 (CBM 2040) size gate rejects valid images → ✓ RESOLVED (2026-07-04, MF-314)

> **✓ RESOLVED.** `d67.c` leitet die erwartete Bildgröße jetzt aus der
> eigenen spt-Tabelle ab (`sum(spt)*256` = 690 Blöcke = 176640 B, VICE-
> Referenz bestätigt), wie `d80.c`/`d82.c` es schon taten. Kein Magic-
> Literal mehr. `test_d67_plugin` deckt es ab: akzeptiert das gültige
> 690-Block-Image, weist das alte 670-Block-Größe ausdrücklich ab und
> liest den letzten Track (der unter dem 670-Gate `UFT_EBOUNDS` war).

**Severity: HIGH (Prinzip 1 — „kein Bit verloren") — behoben.** `src/formats/
commodore/d67.c` gate-t die Bildgröße auf `szl != 670*256` (171520 B) und
gibt sonst `UFT_EINVAL`. Die eigene Per-Track-Sektor-Tabelle im selben
File summiert aber zu **690** Sektoren (17×21 + 7×20 + 6×18 + 5×17 = 690).
Ein reales, standardkonformes .D67 (CBM 2040 DOS 1.0 = **690 Blöcke =
176640 B**) wird damit von `uft_cbm_d67_open()` **abgelehnt** — das Tool
kann eine gültige Diskette nicht lesen. Umgekehrt würde ein akzeptiertes
670-Block-File beim Lesen der hinteren Tracks `UFT_EBOUNDS` liefern
(Datei kleiner als die spt-Tabelle impliziert).

Entdeckt beim Schreiben der Plugin-Tests (Package #2). Kein Test gebaut,
weil ein Test das kaputte Gate nur zementieren würde. **Fix-Kandidat:**
`670` → `690` (`sum(spt)`), plus ein test_d67_plugin analog zu
test_d80/test_d82. Vor dem Fix bestätigen, dass kein 670-Block-Variant
im Umlauf ist, den das Gate absichtlich matcht (unwahrscheinlich — 690
ist die dokumentierte Standardgröße). Nicht unilateral geändert:
forensische Correctness-Entscheidung, siehe DESIGN_PRINCIPLES Prinzip 1.

### FMT-2 — D90/D91 phantom formats removed, D90 catalog corrected (2026-07-04, MF-315)

**Severity: HIGH (Prinzip „Keine erfundenen Daten").** Two Commodore
format modules were fabricated and/or mis-implemented on every layer,
found during the Package #2 format audit and web-verified against VICE:

- **D91 (`.d91`) — fabricated, removed.** No such Commodore disk image
  format exists. The D9060 hard drive uses `.d60`, the D9090 uses `.d90`;
  `.d91` is invented. The impl (`d91.c`) was a copy of the d67 2040-floppy
  reader (35 tracks, 256 B) with the same 670-block size bug, its header
  claimed 154 tracks, and the registry row called it "CMD D9090 HD" — four
  mutually contradictory identities. Deleted impl + header + registry row +
  `.pro` ref.

- **D90 (`.d90`) — real format, wrong impl removed, catalog corrected.**
  `.d90` IS real: the **Commodore D9090 hard disk** (918 tracks × 32
  sectors × 256, ~7.5 MB flat block dump; VICE-supported). But `d90.c` was
  another mislabeled 35-track floppy reader (called itself "4040/2031",
  gate 174848 = 683 blocks, yet its spt table summed to 690 — internally
  inconsistent too) and was never dispatched (dead code). The registry
  called it "CMD D9060 HD" — wrong twice (it is **Commodore**, not CMD, and
  `.d90` = D9090, not D9060). Removed the broken floppy impl + header + a
  `.pro` ref; **corrected the registry catalog row** to the real identity
  ("Commodore D9090 HD (918x32, block dump)"). The extension is still
  recognised; a correct HD block-dump reader is a future task (verified
  geometry above — implement like a flat-LBA sector reader, not a floppy
  zone table).

No format-ID enum entry existed for either (the "138 IDs" SSOT is
untouched); the registry catalog dropped from 163 to 162 rows. Local suite
166/166 after removal; registry_v2.c compiles without the deleted headers.
Not fixed-in-place (a wrong impl of a real HD format would only cement the
fabrication); a genuine `.d90`/`.d60` reader is tracked as a future format.

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
