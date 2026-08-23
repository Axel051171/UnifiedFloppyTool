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

### FMT-3 — CMD FD (.d1m/.d2m/.d4m): three conflicting impls, all wrong sizes (2026-07-04, MF-316 partial)

**Severity: HIGH (Prinzip 1 + „Keine erfundenen Daten").** The CMD FD-2000/
FD-4000 formats are real (VICE + OpenCBM libcbmimage support them), but UFT
carried **three parallel implementations, none with the correct geometry**:

| Impl | .d1m size it accepts | reality |
|---|---|---|
| `src/formats/misc/d1m.c` (+d2m/d4m) | multiples of 533248 ("8050 Mega Image") | **REMOVED** (MF-316) — dead, never dispatched, absurd |
| `src/formats/c64/uft_cmd.c` | `D1M_SIZE` = 204800 (D2M 207360, D4M 414720) | wrong; `test_cmd.c` enshrines these |
| `src/formats/cmd_fd/uft_cmd_fd.c` | 737280 (treats it as a 720K **PC floppy**, 512 B sectors) | wrong geometry + wrong sector size |

**Verified correct native sizes** (VICE test images / OpenCBM, 256-byte
blocks): `.d1m` = **829440 B** (3008×256 data), `.d2m` = **1658880 B**
(6336×256), `.d4m` = **3317760 B** (12736×256). The native format is a
256-byte-block LBA image with a CMD partition/DNP-like directory structure
(spec: unusedino.de/ec64/technical/formats/d2m-dnp.html), NOT a
CHS PC floppy and NOT stacked 8050 images. A real `.d1m` (829440 B) is
rejected by all three UFT impls.

**Done (MF-316):** removed the dead `misc/d1m|d2m|d4m.c` 8050-mega
fabrication (impl + headers + `.pro`).
**Open (needs careful, verified reimplementation — not a hasty constant
swap):** consolidate to ONE correct CMD FD reader at the native sizes
above; reconcile `c64/uft_cmd.c` vs `cmd_fd/uft_cmd_fd.c` (pick one, delete
the other); fix `test_cmd.c` (it currently asserts the wrong 204800-class
sizes); verify registry descriptions ("...720KB/1.44MB/2.88MB" reflect the
wrong PC-floppy assumption). Exact byte sizes are load-bearing — do this
against the d2m-dnp spec, not from memory.

### FMT-4 — Format version × read × write coverage (audit 2026-07-04)

Verified audit of the **live** plugin architecture (the registered
`uft_*_plugin.c` set; the dead FloppyDevice duplicates are ignored). "Should
write?" reflects preservation value — capture/proprietary formats are
legitimately read-only.

| Format | Read versions | Write | Should write? | Verdict |
|---|---|---|---|---|
| **WOZ** | v1, v2 (2.1 = v2 sig) | **✓ module-level** (`woz_save` / `woz_save_to_memory`, MF-317, round-trip byte-identity tested) — plugin `.write` wiring + META/WRIT passthrough pending | **yes** (Apple II preservation std) | **partially closed** |
| **SCP** | yes | **✓ (writer existed, now FIXED + tested)** — `scp_writer_*` wrote "Track Length" in bytes not bitcell-count → every written SCP was unreadable (reader hit EOF); fixed MF-318, `test_scp_writer_roundtrip`. Plugin `.write_track` wiring still pending. | **yes** (Greaseweazle/SuperCard Pro) | **fixed at module level** |
| **MOOF** | info_version ≥ 1 | **✓ `uft_moof_save` (MF-319, save→read round-trip tested)** — plugin wiring + META/FLUX passthrough pending | **yes** (Apple II flux) | **closed at module level** |
| **HFE** | v1 + v3 signature (`HXCPICFE`/`HXCHFEV3`); **v3 opcodes NOT decoded** (reader only flags `is_v3`, reads track data v1-style) | **blank-formatter only** — the "writer" emits an empty HFE (zero track data) from a geometry; it does NOT serialize real MFM track data | data-preserving v1 write + v3 | **larger feature (blocked)** |
| A2R | v2 (`A2R2`) + v3 (`A2R3`) | ✗ | no — raw capture format | OK |
| IPF | partial (CAPS payload not decoded) | ✗ | no — proprietary CAPS/SPS, read-only | OK (read is partial) |
| ADF | v2 + v3 parsers | ✓ | — | OK |
| D88 | v1 + v2 | ✓ | — | OK |
| **Sector/container plugins** (IMD, ADF, D64-family, 86f, PRO, FDI, NFD, SAP, DMK, DC42, DSK, DMS, FDS, JVC, MSA, SCL, SSD, VDK, …) | container versions | **✓ — SYSTEMIC write no-op FIXED (MF-320/321)** | — | ~20 plugin `write_track` paths gate on `uft_sector_t.data_len`, but `uft_format_add_sector()` set only the legacy `.data_size` → every write silently did nothing for read-produced tracks. Root fix: `uft_format_add_sector` now sets `.data_len` too (`include/uft/uft_format_common.h`). Proven by `test_imd_write_roundtrip`. |

**Real write gaps (flux formats):** WOZ, SCP, MOOF — **all now closed at
module level with round-trip tests** (MF-317/318/319; the SCP work also
fixed a bug that made every written SCP unreadable). Remaining: HFE
data-preserving write + v3 (a larger feature — the current HFE writer only
formats a blank disk and the reader does not decode v3 opcodes). Sector/
container formats read+write fine.

**Deliberately NOT rushed:** a flux-format writer that emits subtly wrong
timing/bit-cells silently corrupts a forensic image — worse than no writer
(violates „Keine erfundenen Daten"). Each writer is a proper, spec-exact,
round-trip-tested feature: WOZ2 (INFO/TMAP/TRKS chunks + CRC32, spec
applesaucefdc.com), SCP (flux-timing header + track table), MOOF
(WOZ-derived), HFE v3 (extend the existing v1 writer with the variable-
bitrate track block). Priority order by preservation value: WOZ > SCP >
MOOF > HFE-v3. Note format code is split across `src/formats/` AND
`src/parsers/` (e.g. A2R).

**Concrete WOZ2-writer constraint (analysed 2026-07-04, must guide the
implementation).** WOZ2 is **512-byte-block-offset addressed**: each TRK
table entry holds an *absolute* file `starting_block`, and the BITS data
begins at block 3 (byte 1536). A naïve serializer that just concatenates
header(12) + INFO(8+60) + TMAP(8+160) + TRKS places the TRKS/BITS at byte
248, while the unchanged `starting_block` values still point at block 3 →
**structurally corrupt image**. The reader (`src/formats/apple/uft_woz.c`,
`woz_load_from_memory` → `woz_image_t`) preserves `info`(60) + `tmap`(160)
+ raw `track_data`, but NOT META/WRIT/FLUX/unknown chunks and NOT the
absolute block geometry. So a correct writer must (a) rebuild the WOZ2
block layout so BITS land on their referenced blocks, and (b) either
preserve or honestly drop META/WRIT — and be gated by a read→write→read
**byte-identity** test (which correctly rejects the naïve approach). This
is why it is a feature, not a one-function add.

### FMT-5 — uft_sector_t CRC-status alias mismatch → ✓ RESOLVED (2026-07-04, MF-322)

> **✓ RESOLVED.** Added a single-statement helper `uft_sector_set_crc(
> uft_sector_t*, bool)` in `uft_types.h` that sets all three fields
> (crc_ok + crc_valid + data_crc_ok) at once. `uft_format_add_sector()` now
> calls it with `true` (good sector); the ~9 reader error sites (d77, d88,
> dsk_cpc, imd, jv3, nfd, nib, stx, td0) call it with `false`. Being one
> statement, it is safe under the brace-less `if`s those sites use (which is
> exactly what broke the first attempt). Proven by `test_imd_write_roundtrip`
> asserting crc_ok/crc_valid/data_crc_ok on a good read sector; full suite
> 170/170, no regression from the widely-included `uft_types.h` change.

**Severity: MEDIUM (behoben).** Same bug class as the data_len fix (FMT-4 / MF-321):
`uft_sector_t` exposes the "CRC ok" state under **three** names — `crc_ok`
plus the legacy aliases `crc_valid` and `data_crc_ok`. `uft_format_add_sector()`
(the shared helper every read_track uses) sets only `id.crc_ok` + `status`,
leaving the top-level `crc_ok` / `crc_valid` / `data_crc_ok` at 0. Consumers
that check the aliases (`crc_valid`: ~12 sites incl. sap/moof; `data_crc_ok`:
uft_format_convert_flux.c) therefore see **good sectors as CRC-failed**.

Symmetrically, ~9 reader error paths set only `.crc_ok = false` on a bad
sector (d77, d88, dsk_cpc, imd, jv3, nfd, nib, stx, td0) without the aliases.

**Why not fixed yet:** a first attempt (set all three in add_sector + a
batch edit of the error sites) was reverted — the error sites use
**brace-less `if`s** (`if (err) x.crc_ok = false;`), so a naive line-insert
of the two alias assignments made them run unconditionally. The safe fix is
a single-statement helper, e.g. `static inline void uft_sector_set_crc(
uft_sector_t*, bool)` in `uft_types.h`, used in add_sector (`true`) and at
each error site (`false`) — one statement, safe under brace-less ifs — plus
a plugin-read test asserting `crc_valid`/`data_crc_ok` on a good sector.
Deferred to do it carefully rather than ship a half-consistent state.

### FMT-6 — DC42 write left the data checksum stale → ✓ RESOLVED (2026-07-04, MF-324)

**Severity: HIGH (Prinzip: keine stille Veränderung).** `dc42_write_track`
(DiskCopy 4.2) wrote modified sector data but never recomputed the image's
**data checksum** (BE32 at header offset 0x48, a checksum over the whole data
fork). A written DC42 therefore carried a checksum that no longer matched its
data — spec-conformant consumers (real DiskCopy, Mac emulators) flag such an
image as **corrupt**. UFT's own DC42 reader does not validate the checksum, so
the corruption was silent within the tool.

**Fix (MF-324):** added `dc42_data_checksum()` (the DiskCopy algorithm — add
each big-endian 16-bit word to a 32-bit accumulator, then rotate right 1;
web-verified vs DiscFerret / Mini vMac) and `dc42_update_data_checksum()`,
called at the end of `write_track` to recompute over the whole data fork and
rewrite the header. `test_dc42_checksum_roundtrip` builds a DC42 with a wrong
(zero) checksum, modifies a sector, writes, then independently recomputes the
checksum over the final data fork and asserts the header now matches. Full
suite 172/172.

### FMT-7 — Probe robustness audit: all format probes bounds-check input ✓ (2026-07-05)

**Result: clean (no fix needed).** A static audit of every `*_probe()`
function under `src/formats/` (the format-detection entry points, called on
untrusted/possibly-truncated file data) confirmed that **all** of them guard
on `size` / `file_size` before the first byte access (`memcmp(data,…)`,
`data[N]`, `read_le16(data)` …) — e.g. `if (!data || size < HEADER_SIZE)
return false;` or `if (size != IMAGE_SIZE) return 0;`. So a truncated or
malformed file cannot make a probe read past the buffer (no crash / info
leak at the detection stage). 0 unguarded probes across the ~84 registered
plugins. This is the read-path counterpart to the write-path memory-safety
fix MF-325 (DSK over-read). Recorded so the property is not silently
regressed; a new plugin's probe must keep its size guard.

**Full read-path memory-safety sweep (2026-07-05), result:** four systematic
scans across `src/formats/` — (1) probe input bounds (this entry, clean);
(2) reader chunk-offset/length validation (e.g. WOZ `if (pos+chunk_size >
size) break;` — clean); (3) `malloc(file_derived * N)` + fixed-offset writes
(found + fixed the RDB heap overflow, FMT-8; apridisk/atari_sparta/msx
verified bounded + NULL-checked); (4) `fread(fixed_stack_buf, 1,
file_derived_len, …)` stack-overflow class (16 sites, all verified safe: read
length is a `sizeof(struct)` that fits the buffer, a compile-time constant, an
explicit clamp to `sizeof(buf)`, or a sector size from a static geometry table
/ hard-coded value ≤ the buffer). Net across the format layer: 2 real
memory-safety bugs fixed (FMT-8 heap overflow, MF-325 DSK write over-read),
everything else verified clean.

### FMT-8 — RDB parser heap buffer overflow on tiny SummedLongs → ✓ RESOLVED (2026-07-05, MF-326)

**Severity: HIGH (memory safety, attacker-triggerable).** `uft_hdf_parse_rdb`
(Amiga RigidDiskBlock, in `src/formats/uft_hdf_parser.c`) reads the file-
controlled `size` (SummedLongs) field, does `malloc(size*4)`, then zeroes the
checksum field at `temp[8..11]`. For `size < 3` that write lands past the
allocation — a heap buffer overflow reachable from a malformed HDF/HDMA image
with a valid "RDSK" magic and `size` 0/1/2. (The other fixed-offset reads are
already covered by the `len < 256` guard at entry.)

**Fix (MF-326):** `if (size < 3 || size > len/4) return -1;` — reject a
SummedLongs too small to hold the checksum field before allocating/writing.
`test_hdf_rdb_bounds` feeds size 0/1/2 (must be rejected), a valid 64, and an
oversized/short buffer. Confirmed pre-fix ran the overflowing path (rc=0),
post-fix rejects it (rc=-1); full suite 173/173.

Found by the read-path memory-safety sweep (FMT-7 probes clean; this is a
deeper parse over-write) — scanning `malloc(file_derived * N)` sites.

---

### FMT-9 — Phase-4 copy-protection / disk-error coverage (in progress, 2026-07-05)

Goal: classify all formats into 3 representation classes and stage
copy-protection / disk-error awareness per class. See
[`FORMAT-CLASSIFICATION.md`](FORMAT-CLASSIFICATION.md).

**Landed:**
- **Classification** (MF-classify, `docs/FORMAT-CLASSIFICATION.md`): all 161
  registered format IDs mapped to Klasse 1/2/3 from the SSOT `data_layer`
  field, both axes (representation class + orthogonal disk-error marking).
  Scope refinements: HFEv3 is Klasse 2 (bitstream + partial timing), not 1;
  TAP is a tape-pulse format at the edge of Klasse 2; EDD kept in Klasse 1.
- **Klasse-1 flux pass-through** (MF-327): SCP multi-revolution weak-bit
  round-trip guard (`test_scp_weakbit_multirev`) — write→read preserves every
  revolution verbatim within one 25ns tick, weak windows stay mutually
  distinct (no majority-vote/collapse). Tolerance band documented.
- **Klasse-3 content warning** (MF-328): `uft_protection_probe_scp` scans the
  actual bytes so the lossy-export sidecar reports real weak-bit / multi-rev
  counts instead of a generic count=0 placeholder — and stays silent about
  weak bits when there are none (`test_protection_probe`, 5/5).
  **Precision limit (reported honestly):** the weak-track heuristic compares
  revolutions positionally, so on real disks it OVER-reports (motor-speed
  jitter changes the transition count between revolutions) — it flags most
  real multi-revolution captures, not only genuinely weak ones. This is the
  SAFE direction: it never marks a weak capture as clean, so the export
  warning is never wrongly suppressed (dispatch drops the WEAK_BITS entry only
  when weak_track_count == 0). It is NOT a precise weak-bit count; sub-track
  precision needs bit-cell decode + inter-revolution alignment (DeepRead
  multi-rev fusion). Validated on aligned synthetic flux; real-disk precision
  requires a ground-truth corpus. The over-report contract is pinned by
  `jitter_overreports_weak_safely`.
- **Disk-error marking, IMD** (MF-329): `test_imd_error_marks` proves the IMD
  plugin reads per-sector deleted-address-mark (dtype 3/4) and data-CRC-error
  (dtype 5/6) status, represents them on `uft_sector_t` (deleted / crc_ok +
  aliases), and preserves them through read→write→read (the in-place writer
  keeps the dtype byte). Documented limit: the writer preserves the existing
  dtype, it does not re-encode a caller-flipped status into a new dtype.
- **Variant research** (`docs/FORMAT-VARIANTS.md`): per-family documented
  sub-versions cross-checked with the code; prioritised gap list produced.
- **KryoFlux content probe** (MF-331): `uft_protection_probe_kryoflux` reports
  the robust signals (track / revolution count / flux-timing / transitions)
  for KryoFlux→sector exports. It sets `weak_detection_reliable = false` (a raw
  index-delimited stream isn't aligned), and the dispatch only suppresses the
  WEAK_BITS warning when a scan reliably ruled weak bits out — so KryoFlux
  keeps the conservative warning, never hiding a possible loss.
- **Disk-error marking, EDSK** (MF-332): `test_edsk_error_marks` proves the
  Amstrad/Spectrum DSK plugin reads the uPD765 ST1/ST2 result bytes per sector
  (ST1/ST2 bit5 = data CRC error, ST2 bit6 = deleted/control mark), represents
  them on uft_sector_t, and preserves them through read->write->read (the
  writer overwrites only sector data, leaving the Track-Info block untouched).
  Same documented limit as IMD. WP5 now covers IMD + EDSK.
- **Disk-error marking, TD0** (MF-334): `test_td0_error_marks` covers the
  read+represent half (TD0 is read-only) — per-sector flag byte bit0 = CRC
  error, bit2 = deleted. While testing, found + fixed a real off-by-one:
  `td0_open`'s geometry scan skipped `len-1` bytes of each data record while
  `read_track` consumes `len`, drifting one byte per data sector and
  mis-scanning multi-sector geometry. Fixed open to match the authoritative
  decoder. WP5 now covers IMD + EDSK + D64(read) + TD0(read).
- **Disk-error marking, STX** (MF-335): read+represent half (STX is read-only).
  Web-verified against the Pasti spec that the sector descriptor holds R at
  0x0A and the FDC status at 0x0E — the plugin read sec_id from 0x08 (ID track)
  and the FDC status from 0x0C (ID CRC), so sector IDs collapsed and CRC-error
  detection read the wrong byte; the deleted mark was never surfaced. Fixed the
  offsets and mapped FDC bit3->CRC error, bit5->deleted. `test_stx_error_marks`
  2/2. WP5 now covers IMD + EDSK + D64(read) + TD0(read) + STX(read).
  **Three real bugs surfaced by writing the disk-error tests** (D64 drop MF-333,
  TD0 off-by-one MF-334, STX offsets MF-335) — building the test IS the audit.
- **Disk-error audit, ATX + DMK (2026-07-05):**
  - **ATX** (`src/formats/atx/uft_atx.c`): already complete + spec-correct —
    sector header sh[0]=number, sh[1]=FDC status, sh[2:3]=position, sh[4:7]=data
    offset (verified against the VAPI/a8rawconv layout), FDC bit3→CRC error,
    bit5→deleted, plus weak-bit masks and data_mark (0xF8/0xFB). No bug; no
    change. (Test deferred — the chunked track structure is heavier to
    synthesise than the descriptor formats and the code is verified correct.)
  - **DMK** (`src/formats/dmk/uft_dmk.c`): the deleted-address-mark (DAM 0xF8)
    is mapped to `.deleted` correctly. DMK has NO explicit CRC-error flag — it
    stores the raw track bytes including the ID/data CRC words, so a CRC error
    can only be *computed* (MFM/FM CRC over the exact address-mark→data range).
    That is a structural enhancement (not a format-provided flag) with real
    byte-range/verification risk, so it's deferred, not faked. Deleted: done.
  Error-aware WP5 status: IMD, EDSK, D88 (r+w preserve) · D64, TD0, STX
  (r, fixed) · ATX (verified complete) · DMK (deleted done, CRC-compute
  deferred). Not yet audited: NFD.
- **FDI disk-error — format-identity finding (2026-07-05, do NOT fabricate):**
  two real FDI formats exist — Anex86 PC-98 FDI (signature "Formatted Disk
  Image file", raw C/H/S dump, no per-sector marks — correctly handled by
  `src/formats/fdi_pc98/uft_fdi_pc98.c` as a structural-limit Klasse-3 format)
  and Joguin FDI v2.0 (disk2fdi, oldskool.org FDISPEC.pdf, a completely
  different header). The registered generic `src/formats/fdi/uft_fdi_plugin.c`
  uses a 3-byte ASCII "FDI" magic with a custom 14-byte header and a per-sector
  `C+H+R+N+flags` layout that matches NEITHER real spec. Its read_track reads
  the per-sector `flags` byte and discards it (`(void)flags`). Because the
  format cannot be identified against a published spec, the `flags` semantics
  (CRC error? deleted?) cannot be verified — mapping it would be fabrication.
  Deferred pending format identification; likely related to the FMT-1..4
  copy-paste-fabrication cluster. Reported, not guessed.
- **NFD R0 reader structurally wrong vs spec (2026-07-05, do NOT blind-fix):**
  verified against the authoritative pc98.org/project/doc/nfdr0.html. The real
  NFD R0 sector-ID table at 0x120 holds **163×26 = 4238 per-SECTOR entries**,
  each 16 bytes: C@+0 H@+1 R@+2 N@+3 MFM@+4 DDAM@+5 Status@+6 ST0@+7 ST1@+8
  ST2@+9 PDA@+10 reserved@+11..15; the sector DATA follows sequentially from
  dwHeadSize (@0x110) — there is no per-entry data offset. The registered
  `src/formats/nfd/uft_nfd_plugin.c` instead models it as **164 per-TRACK index
  entries** and reads cyl@+1, head@+2, nsec@+3, ddam@+6, status@+7, st1@+9,
  st2@+10, and an invented data_off (LE32)@+12. So the entry count, the
  per-track-vs-per-sector model, the data location, AND the disk-error offsets
  (DDAM is +5 not +6, Status +6 not +7, ST1 +8 not +9, ST2 +9 not +10) are all
  wrong. Fixing only the mark offsets is incoherent — the whole reader model is
  wrong. This needs a full rewrite to the per-sector table + sequential data,
  and byte-exact correctness needs a real NFD corpus, so it is NOT attempted
  blind (FMT-1..4 fabrication cluster). NFD R1 (dwTrackHead arrays) uses the
  same wrong index model and is likely affected too. Reported, not guessed.
- **Disk-error marking, D88** (MF-336): read+represent+preserve. Fourth real
  bug of the audit — the plugin read the sector status from offset +0D (a
  reserved/RPM byte) instead of the DDAM flag at +07 and the FDC status at +08
  (verified vs pc98.org / MAME d88_dsk). Fixed; `test_d88_error_marks` 2/2.
- **Pattern (4 bugs, systemic):** the disk-error audit found four real bugs —
  D64 error-block dropped (MF-333), TD0 open-scan off-by-one (MF-334), and
  **two wrong sector-header status offsets** (STX MF-335, D88 MF-336). The
  format layer's descriptor/offset parsers need spec-offset web-verification
  before being trusted (consistent with FMT-1..4 fabrication findings). Test
  construction is the audit that surfaces them.

**Open (next steps, concrete):**
- **Phase-0 zoned/non-uniform geometry type** (found 2026-07-05): `uft_geometry_t`
  is uniform (single `sectors`/`sector_size`); it reports the MAX as a summary for
  GCR-zoned (D64/G64: 21/19/18/17) and per-track-variable (IMD/EDSK/TD0/D88)
  formats. Reads are correct via plugin tables (`test_d64_geometry_zones` proves
  the D64 zones). Next step: add an optional per-track sector-count/size vector to
  the geometry model — public-struct change, ABI-relevant, so additive (new field,
  not layout change) + abi-bomb-detector review. Effort L.
- **Header-CRC vs data-CRC separation** (new dimension, MF-338, mostly DONE):
  added an ABI-additive `id_crc_ok` field to uft_sector_t + `uft_sector_set_id_crc`
  helper; `uft_format_add_sector` defaults it true. Wired:
  - **D88 DONE** — 0xA0 -> id_crc_ok false, 0xB0 -> crc_ok false (was collapsed).
  - **EDSK DONE** — uPD765 ST2 DD -> data crc, ST1 DE alone -> ID crc.
  Both tested (`test_d88_error_marks`, `test_edsk_error_marks`, 4 sectors each).
  - **STX = structural limit (not a gap):** the WD1772 read-sector status has a
    single CRC-Error bit (0x08 = data-field CRC); an ID-field CRC failure surfaces
    as Record-Not-Found (0x10), which has several causes, so it cannot be mapped to
    id_crc_ok without over-interpreting. Data CRC (0x08) is already mapped.
    Documented, not fabricated.
  - **NFD deferred** — reader is structurally wrong (see NFD finding); the ST1/ST2
    separation lands with the full NFD rewrite (corpus-blocked).
- Extend the content probe to the remaining flux sources (IPF, A2R) and to
  bitstream sources (G64/HFE weak-marker snapshot) — each keeps the class-based
  fallback until its probe lands. SCP (MF-328) + KryoFlux (MF-331) done.
- Klasse-2 snapshot: represent weak-bit regions where the spec allows
  (G64 speed-zones, HFE variable bitrate); document limits in code + docs.
  **HFE v3 weak-bit opcode — DETECTION resolved (MF-354), full decode still
  open:** the opcode set was under-specified in the HxC PDF but is fully
  determined by the HxC *source* (`hfev3_loader.c`): 0xF0 NOP / 0xF1 SETINDEX /
  0xF2 SETBITRATE (2B) / 0xF3 SKIPBITS (2B) / 0xF4 **RAND = weak/fuzzy-bit
  region** (1B), all in the bit-reversed logical domain. UFT now **detects** the
  weak regions (`uft_hfe_v3_count_weak_opcodes`, unit-test `test_hfe_v3_weak`
  8/8) and reports the count via `read_metadata("weak_regions")`; the feature
  matrix lists weak-bits as `PARTIAL`. **Still open (deliberate):** the full
  opcode→bitstream decode with a per-bit `weak_mask`. The v3 `raw_data` still
  carries the opcode stream undecoded; a `weak_mask` with uncertain bit
  alignment would violate "Keine erfundenen Daten", and the HxC author states
  HFE only *approximates* the disk (not bit-exact). Byte-exact full decode needs
  a ground-truth v3 reference image. Not fabricated.
- Surface the content-probe result in the GUI converter warning (currently
  only written to the `.loss.json` sidecar).
- Per-format disk-error marking audit (read/preserve/write CRC-error,
  deleted-address-mark, bad-sector-flag) — error-aware set already flagged in
  the classification table. IMD done (MF-329); next: EDSK uPD765 status,
  STX fuzzy-mask, ATX.
- **D64 error-map (found 2026-07-05; read half RESOLVED MF-333, write half
  open):** the D64 *plugin* (`src/formats/d64/uft_d64_plugin.c`) used to read
  only the 256-byte sector data and drop the trailing error-info block
  (683/768 bytes) on both read and write, silently losing the 1541 controller
  error codes. **Read + represent now fixed (MF-333):** `open` detects the
  error block from the file size, `read_track` reads each sector's error code
  and marks the sector CRC-bad for any non-OK code (0x02+), so consumers see
  the original media defects (`test_d64_errormap`, 2/2). **Write-preserve still
  open:** the error map is a whole-file trailer that a per-track `write_track`
  cannot place — needs an open-time load + a close/flush that rewrites the
  trailer, or a whole-image save path. Effort L (reported, not half-baked).
- `FORMAT-VARIANTS.md`: per-family web research of sub-versions not yet
  read/written (WOZ1 vs 2, HFE v1/v3, A2R v2/v3, D88 revisions, …).
- **ATR 512-byte sectors (SpartaDOS X)** — ✓ RESOLVED (MF-340): `uft_atr.c` now
  accepts 512 in the sector-size check (offset + count math already handled any
  size generically with the boot-sector 128-B exception). `test_atr_512` builds
  a 3×128 + 5×512 ATR and asserts sector_size 512, 8 sectors, correct data
  offset. 1/1.
- **SCP extension-footer read/write — ✓ RESOLVED (MF-351):** the reader was
  spec-broken (found 2026-07-05); now fixed + emit added.
  Previously: the cbmstuff SCP spec puts
  a 4-byte "FPCS" signature as the FINAL bytes of the file; the codebase struct
  `uft_scp_footer_t` (52 B, 13×uint32, no FPCS field) and reader
  (`scp_parse_extension_footer`, reads the last 52 bytes) don't model that and
  likely use the wrong timestamp field sizes (spec: 8-byte timestamps). So the
  footer READER is probably already spec-incompatible, and footer-EMIT must not
  be built on it. Next step: verify the footer layout byte-for-byte against the
  spec + a real SCP-with-footer reference, correct the reader, THEN add emit.
  Deferred (needs a reference SCP), not guessed. (ADZ/gzip-ADF likewise deferred:
  zlib is not reliably linked here — imz uses a stored-only fallback — so ADZ
  would need a new zlib build dependency, a separate decision.)
- **D64 42-track variant** — ✓ RESOLVED (MF-350): the D64 plugin now accepts the
  extended 41/42-track sizes (200960/201745 and 205312/206114, VICE/Schepers-
  verified) additively — d64_spt extended to 42 (tracks 41/42 = 17 sectors),
  probe + open size-thresholds, D64_MAX_TRACK=42. `test_d64_42track` (4/4)
  covers probe, 42-track geometry (802 sectors), extended-track round-trip, and
  35-track no-regression; all 8 existing D64 tests still green.
- Byte-exact read correctness vs. real copy-protected disks is NOT verifiable
  without a ground-truth corpus — remains an explicit open point, not
  silently marked done.

**Tooling note (verify_build_sources parser):** a `SOURCES += \` block whose
first continuation line is a `#`-comment hides the ENTIRE block from
`scripts/verify_build_sources.py` (it joins continuations, then strips from
the first `#` to EOL). qmake compiles those sources fine; the verifier can't
see them (they sit in the accepted baseline). New sources must go in their own
`SOURCES +=` statement without a leading comment line, or they will be flagged
as a new A-divergence (as `uft_protection_probe.c` was until relocated).

### FMT-10 — IBM SaveDskF reader fabricated against wrong spec → uncompressed RESOLVED, LZW pending (2026-07-06, MF-356)

**Severity: HIGH (forensic-integrity).** Same class as FMT-2/FMT-3: the
SaveDskF/LoadDskF module (`src/formats/pc/uft_savedskf.c`, standalone API, not
in the plugin registry) was written against a spec that does not exist. Every
identifying field was wrong, so it would never read a real SaveDskF file:

- **Magic:** used `0x5A4B` ("KZ", little-endian). The real signature is a
  **big-endian** u16 at +0: `0xAA58` (old, uncompressed), `0xAA59` (new,
  uncompressed), `0xAA5A` (new, compressed). `0x5A4B` matches nothing.
- **Header geometry:** read sector-size/spt/heads/cyls at offsets 2/4/6/8. The
  real (FAT-BPB-style) header has bytes/sector @4, cylinders @24, heads @26,
  sectors/track @28, sectors-in-image @34, header-size @38.
- **Compression:** enum `{NONE, RLE, LZSS}`. Real SaveDskF has **no RLE and no
  LZSS**; its only compression is **IBM-LZW** (the OS/2 PACK scheme, `dskdcmps`
  modname `"ibmlzw"`). The fabricated RLE decompressor was removed.

Authoritative layout verified from Deark (`jsummers/deark` `modules/fat.c`
`de_run_loaddskf` + `loaddskf_read_header`) and the public-domain
`foreign/dskdcmps.h` LZW decompressor; archiveteam/justsolve corroborates the
magic bytes.

**Fixed (MF-356):** correct BE-magic classification, correct header offsets,
uncompressed extraction (old fmt data @0x200, new fmt @header-size; unused
sectors zero-filled = the format's defined "used sectors only" meaning, not
fabricated data). Test `test_savedskf_read` 5/5 (old + new fmt exact recovery,
empty-sector reconstruction, LZW deferral, bad-magic rejection).

**Deliberately open:** the IBM-LZW path (`0xAA5A`) returns `NOT_IMPLEMENTED`.
A faithful ~200-line `dskdcmps` LZW port needs a ground-truth compressed
reference file for byte-exact verification — none is in Deark's public repo.
Porting an unverified codec would risk silent corruption, which the forensic
mandate forbids. Unblock: obtain a real compressed SaveDskF (`SAVEDSKF /C`
output or an OS/2 fixpak `.dsk`), port `dskdcmps.h`, verify byte-exact.

### FMT-11 — NFD r0 (T98-Next PC-98) reader fabricated → RESOLVED (2026-07-06, MF-358)

**Severity: HIGH (forensic-integrity).** The NFD r0 reader modelled a wrong
structure end-to-end and would mis-read every real file:

- It read a **164 per-TRACK** entry table with a per-entry **data offset at
  +12**. The real r0 layout is a fixed **163×26 per-SECTOR** table of 16-byte
  entries at 0x120, and there is **no offset field** — sector data is stored
  contiguously from `dwHeadSize` (@0x110) in the order the valid (C≠0xFF) entries
  appear. Bytes 11-15 of an entry are padding, not an offset.
- Consequently the field offsets were all wrong: it read N (sector-size) as a
  per-track sector count, flMFM as the size code, byStatus as DDAM, and ST1/ST2
  from +9/+10 instead of +8/+9.

Authoritative layout verified from pc98.org/project/doc/nfdr0.html and a working
reference decoder (tomari/d88split `nfd2mhlt.pl`).

**Fixed (MF-358):** correct header parse (dwHeadSize/byHead), full 163×26 table
walk, sequential data-offset accumulation from dwHeadSize, correct entry fields
(C/H/R/N/flMFM/flDDAM/byStatus/ST0/ST1/ST2/byPDA), DDAM→deleted, ST1/ST2 bit5→
CRC-bad, real record number R preserved as the sector id, and in-place write that
actually persists to disk (the prior write only touched an in-memory buffer — a
silent no-op). Test `test_nfd_r0` 4/4 (geometry + per-(C,H) recovery, DDAM/CRC
flags, write persistence, r1 refusal).

**r1 (MF-360): now implemented.** The r1 variant shares the 0x120 header but
replaces the fixed table with a 164-entry LE32 track-pointer table at 0x120;
each non-zero pointer addresses an NFD_TRACK_ID1 (wSector, wDiag) + wSector
16-byte sector entries + wDiag 16-byte diagnostic entries. r1 sector data is
sequential from dwHeadSize but interleaved with per-sector retry copies
(byRetry@+10) and per-track diagnostic blocks ((1+byRetry@+9) × dwDataLen@+10),
which the parser skips to locate later sectors. Layout verified against
pc98.org/project/doc/nfdr1.html and tomari/d88split read_trkinfo_nfdr1, then
proven with a synthetic image whose data section interleaves a retry copy and a
diagnostic block (`test_nfd_r0::r1_recovery_with_retry_and_diag`). An unknown
revision (not '0'/'1') is still refused. NOTE: no real r1 corpus was available;
the reference perl decoder is itself marked experimental, so the retry/diagnostic
skip accounting is verified only against the (cross-checked) spec, not a
ground-truth file — a real r1 image should be run through it before relying on
r1 for forensic work.

### FMT-12 — FDI reader: right header, fabricated track/sector model → RESOLVED (2026-07-06, MF-359)

**Severity: HIGH (forensic-integrity).** The registered `fdi` plugin is the **ZX
Spectrum FDI** ("Full Disk Image", Alex Makeev) — not Anex86 PC-98 .fdi (that is
the separate, correct `fdi_pc98` plugin; the two are disjoint by content since
PC-98 FDI has no "FDI" magic). Its 14-byte header was correct, but the
track/sector model was invented: it read a flat table of 4-byte track offsets
and then 5-byte inline sector descriptors immediately followed by the sector
data.

The real Spectrum FDI stores **variable-length track headers** beginning at
`14 + extra-length` and walked sequentially: `FDI_TRACK` = track-data offset
(LE32, rel. to the data offset) + 2 reserved + sector count; then that many
`FDI_SECTOR` = C, H, R, N, flags, sector-data offset (LE16). Sector **data** is
held separately at `data_off + track_off + sector_off`, size `128<<(N&3)`. The
prior model mislocated every sector.

Authoritative layout verified against SAMdisk's `ReadFDI`
(`src/samdisk/fdi.cpp`) and the World-of-Spectrum format reference.

**Fixed (MF-359):** correct header-relative track-header walk, per-sector data
offsets, flags decoding (bit7 → deleted, bit6 → no-data, bit(N&3) → data-CRC-OK),
R preserved as the sector id, geometry from the actual per-track sector counts,
and an in-place write that persists to disk (the prior write was an in-memory
no-op). Test `test_fdi_spectrum` 3/3 (geometry + offset-based recovery,
deleted/CRC flags, write persistence). The probe-only `test_fdi_plugin` stays
green.

---

## Voll-Audit 2026-08-16 (MF-369) — Befunde jenseits der Löschwellen

Drei Lösch-Wellen sind erledigt (206 Dateien: 55 tote Sources, 90 verwaiste
Header, 61 tote CMakeLists; Abnahme je Welle: qmake-Vollbuild + ctest 209/209
+ 9 Gates). Die folgenden Befunde sind **Verbesserungs-Kandidaten**, keine
Löschungen — jeweils mit Klasse und konkretem nächsten Schritt.

### AUD-1 — Switch/hactool-Feature ist fachfremd und unverifiziert (Architecture)
`CONFIG+=switch_support` baut einen Nintendo-Switch-Cartridge-Dumper
(`src/switch/` + vendored hactool + vendored mbedtls) in das forensische
*Floppy*-Tool ein. Scope-fremd; ob das Feature überhaupt linkt (mbedtls-
Library-Sources im .pro-Block?) wurde nie verifiziert — kein Test, kein
CI-Lauf mit dem Flag. Nächster Schritt: extrahieren in eigenes Repo ODER
bewusst behalten und dokumentieren (Entscheidung Axel); bis dahin nicht
bewerben.

### AUD-2 — `uft_error_strings.c` wird generiert + verifiziert, aber nie kompiliert (Architecture)
`scripts/generators/gen_errors_strings.py` erzeugt die Datei aus
`data/errors.tsv`, `ssot_errors_compliance` prüft Generator-Output ==
eingecheckte Datei — aber kein Build kompiliert sie. (Welle 1 hätte sie fast
gelöscht — nur der SSOT-Test hat sie gerettet; Skript-Referenzen sind seitdem
Teil der Lösch-Beweispipeline.)
**Disposition (analysiert, MF-369):** Die API (`uft_strerror` + Aliase) ist in
`uft_error_ext.h` deklariert, konfliktfrei (einzige Definition) und hat
**null Aufrufer** im gesamten Baum. Verdrahten ohne Aufrufer wäre
Phantom-Aktivierung — die Datei bleibt bewusst unkompiliert als generierte,
SSOT-verifizierte Fehlertext-Tabelle. In `.pro` aufnehmen, **sobald der erste
echte Aufrufer entsteht** (z. B. GUI-Fehlermeldungen auf `uft_strerror`
umstellen — das wäre die eigentliche Verbesserung).

### AUD-3 — Enum-Duplikat-Drift cross-TU (Correctness) → Track-Status GELÖST (MF-371), GCR-Encoding offen
**Track-Status (gelöst):** `uft_track.h` trug einen Kompat-`#define`-Block mit
**anderen Werten** als die kanonische enum in `uft_types.h` (UNFORMATTED
`1<<0` vs. `1<<2`; Wert 1 hieß je nach TU „unformatiert" oder „READ_ERROR").
Da `#ifndef` enum-Konstanten nicht sieht, überschattete der Block die enum in
jeder TU, die den Header zog — Setter und Leser konnten denselben Wert
verschieden deuten. Aufruf-Site-Analyse: 4 Setter, 1 Leser (nur `== OK`,
kollisionsfrei), Makro-only-Namen (CRC_ERRORS/MISSING_DATA/LONG/SHORT/HALF/
QUARTER) 0 Nutzer. Fix: Block ersatzlos entfernt — alle Namen lösen zur enum
auf; der zuvor unmögliche `status == UFT_TRACK_UNFORMATTED`-Assert in
`test_g71_read` ist reaktiviert und grün. Verifiziert: qmake-Vollbuild +
ctest 209/209.
**GCR-Encoding (offen):** `UFT_ENC_GCR_CBM`(=3, uft_types) vs.
`UFT_ENC_GCR_C64`(=9, core/uft_track_base) sind zwei GETRENNTE Enums für
dasselbe Konzept mit vielen Nutzern — Vereinheitlichung ist eine eigene
API-Aufgabe (Aufruf-Site-Migration), nicht nebenbei lösbar.

### AUD-4 — 60+ unpushed Commits = CI-Blindflug (Process)
Die gesamte Sanierungs-Serie (MF-352…369) existiert nur lokal; CI hat seit
Wochen keinen dieser Stände gebaut (macOS/Windows-Pfade, Qt-Versionen).
Nächster Schritt: pushen — als **ein** Push (ein Matrix-Lauf statt 60;
macOS zählt 10× Minuten, siehe Skill `github-actions-sparen`).

### AUD-5 — `verify_build_sources.py` sieht konditionale .pro-Blöcke nicht (Tooling)
Der Parser führt Dateien aus `switch_support {}`-Blöcken als „absent from
.pro", obwohl qmake sie konditional baut — die Baseline nennt sie A-Einträge.
Dokumentierte Limitation; bei Bedarf Parser um Scope-Tracking erweitern.

### AUD-6 — In-Source-Build-Artefakte vergiften Shadow-Builds (Tooling)
`<repo>/release/` + `debug/` (von einem historischen In-Source-Build,
gitignored) ließen **jeden** Shadow-qmake-Build mit `fatal: xcopytab.moc`
scheitern (qmakes Depend-Scan fand die stalen mocs). Entfernt; Prävention-
Kandidat: `preflight-check` warnt künftig, wenn `<repo>/release|debug`
existiert.

### PROT-1 — CopyLock ST: Series-2 verifiziert, Series-1 feuert auf realen Daten nie (2026-08-16, MF-377) → ✓ RESOLVED (MF-380: 16/16)

**Kontext:** Erster Kopierschutz-Test gegen **echten** geschützten Code
(`test_corpus_protection_copylock`, Korpus: dec0de-Samples, lokal-only,
Provenienz im Manifest). Vorher galt laut `docs/SUBSYSTEM_MATURITY.md`:
„kein Scheme je gegen eine echte geschützte Original-Disk".

**Ergebnis (Correctness, unabhängig in Python gepinnt vor jedem UFT-Lauf):**

| Detektor-Pfad | Reale Samples | Befund |
|---|---|---|
| Series 2 (1989), init1/init2 + Trampolin | 14/14 dec0de-Samples | ✅ erkannt; `magic32` + `start_off` exakt reproduziert (Rainbow Islands 0x6B1D1929 @2214, Warlock 0x0 @1960) |
| Series 1 (1988), BRA.S + Keydisk-Pattern | 0/16 dec0de-Samples | ❌ **nie erkannt** (Stand MF-377; MF-379 → 6/16, MF-380 → 16/16, s.u.) |

**Ursache Series 1:** `uft_copylock_st_detect()` verlangt zusätzlich zum
BRA.S-Prefix das Keydisk-Pattern `50F9 0000 043E` (`ST $43E.L`) im **Klartext**.
In realen Series-1-Loadern liegt dieses Pattern innerhalb der TVD-verschlüsselten
Region — der Scan über alle 16 realen Series-1-Samples fand 0 Treffer für
Keydisk und 0 für das Serial-Pattern `2140 001C`, während der BRA.S-Prefix
überall vorhanden ist (6–1497 Treffer, also allein wertlos als Signal).
Der Detektor kann Series 1 auf Rohdaten strukturell nicht erkennen.

**Ursachen-Korrektur + Teil-Fix (MF-379).** Die obige Diagnose war richtig im
Ergebnis, aber ungenau im Mechanismus. Der Abgleich mit der Referenz-
Implementierung (dec0de `get_pattern_offset_robn88`, autoritative Quelle) zeigt:
das Muster wird **nicht** über einen vorab entschlüsselten Block gesucht,
sondern *durch* die Verschlüsselung hindurch —

- Schrittweite **2 Byte** (nicht 4),
- an jedem Offset wird **genau ein** 32-Bit-Wort entschlüsselt,
- dessen High-/Low-Wort gegen die ersten beiden Musterworte geprüft
  (erstes Wort `0x0000` wirkt als Wildcard),
- **weitere** Musterworte werden gegen den **rohen** Puffer verglichen, weil
  sie zum nächsten Kettenglied gehören.

Zwei eigene Hypothesen (Kette über rohe bzw. über entschlüsselte Vorgänger,
jeweils volle Blockentschlüsselung) ergaben 0/15 — erst die Referenz-Semantik
traf. Umgesetzt als `uft_copylock88_find_pattern_decoded()`; der wertlose
BRA.S-Vorfilter wurde entfernt.

**Messergebnis nach dem Fix:** **6 von 16** realen Series-1-Loadern erkannt,
bei **0 Falsch-Positiven** über 13 Series-2- und 15 Fremdschutz-Samples.
Alle Treffer sind rohe Protection-Extrakte (`.BIN`); alle 6 GEMDOS-`.PRG`
scheitern weiterhin, weil dort die Protection erst nach Entpacken/Relokation
im Text-Segment liegt — dec0de hat dafür eine komplette Programm-Pipeline
(`prog_t`, Anker auf `patterns[4]`), UFT nicht.

**Vollständig gelöst (MF-380) → ✓ RESOLVED.** Die verbleibende Lücke war kein
Pipeline-Problem, sondern erneut die falsche Signatur. Die Referenz erkennt
Series 1 gar nicht am verschlüsselten Körper, sondern am **unverschlüsselten
Prolog**, der den TVD-Exception-Handler installiert — und dieser Handler *ist*
die Entschlüsselungsroutine (`not.l d0 / swap d0 / eor.l d0,(a0)` ist genau das
~SWAP32-XOR-Schema in 68000-Code). Jeder Series-1-Loader muss ihn im Klartext
mitbringen, unabhängig davon, ob er als `.PRG` oder als Rohextrakt vorliegt.

Umgesetzt als `uft_copylock88_find_prolog()` mit den beiden TVD-Handler-Mustern
(dec0de `PATTERN_TVD1/TVD2_ROBN88`, zwei Wildcard-Bytes für die `lea`-Distanz).
Die Varianten a–e werden aus der Kombination Supervisor-Switch-Prolog × TVD-
Handler abgeleitet, exakt nach den Musterketten `prot_robn88a..e`. Dass die
Ableitung stimmt und nicht geraten ist, bestätigt die Referenz selbst: sie nennt
die TVD-Konstante der Variante d `PROT_TVD_FSHARK_ROBN88`, und Flying Shark ist
das einzige Sample im Korpus, das `tvd2` trägt.

**Endstand, gemessen mit der kompilierten UFT-Implementierung über alle 45
dec0de-Samples:**

| | erkannt | Falsch-Positive |
|---|---|---|
| Series 1 (1988) | **16/16** (Varianten a,b,c,d,e alle vertreten) | — |
| Series 2 (1989) | **14/14** | — |
| 15 Fremdschutzsysteme (Anti-bitos, Cooper, Zippy, CID, R.AL, Sly, Toxic) | — | **0** |

Der Weg dahin in drei Stufen, jede durch Messung widerlegt statt durch Meinung:
Roh-Bytesuche 0/16 → Suche durch die Entschlüsselung 6/16 → Prolog 16/16.
Rick Dangerous, in MF-377/379 noch ehrlicher Negativ-Assert, ist jetzt als
Variante a mit Prolog @420 erkannt — der Assert wurde umgedreht, wie im Test
angekündigt. Der Fall trägt tatsächlich **keine** Keydisk-Instruktion; das ist
kein Erkennungsproblem, sondern eine Eigenschaft dieses Loaders.

### PROT-2 — C64-Scheme-Erkennung ist headless, Pipeline-Test testet nur Mocks (2026-08-16, MF-377) → ✓ Datenlieferant vorhanden (MF-381), Restlücken benannt

Zwei Befunde aus derselben Untersuchung, beide **Architecture**:

1. **`ufm_c64_prot_analyze()` bekam unbrauchbare Metriken → Kern gelöst (MF-381).**

   > **KORREKTUR (MF-384).** Die ursprüngliche Fassung dieses Eintrags war in
   > zwei Punkten falsch, und beide gehen auf ungeprüfte Annahmen von mir
   > zurück: `ProtectionAnalysisWidget` wurde **nicht** mit MF-369 gelöscht —
   > es existiert, wird gebaut (`UnifiedFloppyTool.pro:249`) und **ruft**
   > `ufm_c64_prot_analyze()` auf (`ProtectionAnalysisWidget.cpp:195`). Es
   > „zeigt die Metriken nur an" ist ebenfalls falsch: es besitzt **zwei**
   > eigene Extraktoren, einen für SCP-Flux und einen für G64.
   >
   > Der Befund „C64-Schemes unerreichbar" stimmt trotzdem — aber aus einem
   > anderen Grund: die GUI-Extraktoren füllen `is_half_track`, `bitlen_*`,
   > `weak_region_*` und `illegal_gcr_events`, während der Klassifikator
   > `has_half_track`, `track_length_ratio`, `has_custom_sync`,
   > `sector_count`, `duplicate_ids` und `bad_gcr_count` liest. Die beiden
   > Seiten reden über dieselbe Struktur aneinander vorbei, deshalb feuert
   > kein einziger Check. Gleiches Ergebnis, andere Ursache.

   Damit war jeder C64-Scheme unerreichbar, unabhängig von der Qualität der
   Klassifikation.

   Neu: `ufm_c64_metrics_from_gcr()`
   (`src/protection/ufm_c64_metrics.c`) leitet die Metriken direkt aus einem
   rohen GCR-Bitstrom ab, wie ihn das G64-Plugin in `uft_track_t::raw_data`
   liefert. Damit läuft die Kette erstmals durchgehend:
   **G64-Plugin → GCR → Metriken → `ufm_c64_prot_analyze()`.**

   Verifiziert gegen ein **reales** VICE-3.10-c1541-G64
   (`tests/corpus_free/vice_c1541_35trk.g64`, rechtefrei, getrackt;
   nebenbei hebt das `g64` auf **T1b**). Ground Truth vorab per Python
   gepinnt, dann in C bestätigt: über alle 35 Spuren 21/19/18/17 Sektoren,
   `sync_count` = 2× Sektorzahl, längster Sync-Run 41 Bit, 0 illegale
   GCR-Codes, 0 Duplikat-IDs, Längenverhältnis 1,000. Die Zonen-Nennwerte
   (7692/7143/6667/6250 Byte) sind an dem belegt, was VICE tatsächlich
   schreibt, nicht angenommen. Negativkontrolle: auf der unprotected Disk
   meldet `ufm_c64_prot_analyze()` 0 Hits / `UFM_PROT_NONE`.

   **Bewusst NICHT gesetzt** (statt geraten): `has_custom_sync`,
   `density_deviation`, `jitter_rms`, `weak_region_*`, `revolutions`,
   `bitlen_*`. Die brauchen entweder Multi-Revolution-Flux (aus einem
   einzelnen G64-Track-Image nicht ableitbar) oder eine Definition, die
   dieses Projekt noch nicht gegen eine autoritative Quelle belegen kann.
   Sie bleiben null/false, damit ein Aufrufer keine Schätzung für eine
   Messung hält — dieselbe Regel, deren Verletzung PROT-3 war.

   **`has_custom_sync` nachgeliefert (MF-382), belegt statt geschätzt.**
   Quelle: nibtools (`rittwage/nibtools` @`0abdc11`), das kanonische
   C64-Preservation-Werkzeug. Dort trennt `check_sync_flags()` zwei Zustände
   (`BM_NO_SYNC` = gar keine Sync-Marke, `BM_FF_TRACK` = Killer-Track, fast
   die ganze Spur Sync), und `find_track_cycle_headers()` fällt auf
   `find_track_cycle_syncs()` zurück für den dritten Fall: „track
   w/non-standard headers". Genau dieser dritte Fall ist die Definition:

   > `has_custom_sync` = Sync-Marken vorhanden, aber **keine** davon leitet
   > einen Standard-1541-Header ein — und die Spur ist **kein** Killer-Track.

   Killer-Tracks bleiben bewusst ausgeschlossen, weil nibtools sie ebenfalls
   getrennt führt und weil sonst jeder Killer-Track stromabwärts wie V-MAX!
   aussähe. Die 3-Byte-Toleranz für Killer stammt aus nibtools
   (`syncs >= length - 3`). Verifiziert: auf allen 35 Spuren des realen G64
   **nicht** gesetzt; gesetzt dagegen auf einem realen headerlosen
   GCR-Ausschnitt (Bytes [26,326) von Spur 1: ein Sync, ein Datenblock 0x07,
   kein Header) — beide Fälle vorab per Python gepinnt.

   **Positivkontrolle nachgeliefert (MF-383) → Punkt 1 vollständig.**
   Ein Clean-Disk-Test allein beweist nichts: ein Detektor, der *immer*
   „sauber" meldet, besteht ihn ebenfalls. Deshalb zwei reale Dumps aus der
   *C64 Preservation Project 10th Anniversary Collection* (von Pete Rittwage
   ausdrücklich als „no strings attached"-Download freigegeben; Spielecode
   urheberrechtlich geschützt → lokal-only, sha256 + exakter Pfad im Manifest):

   | Disk | Rolle | Befund |
   |---|---|---|
   | Bounty Bob Strikes Back [Big Five, 1985] | **positiv** | 71 belegte Slots, davon **35 echte Halbspuren**; Ganzspuren 1–10, 13–16 und 37 tragen Sync **ohne** Standard-Header; Spur 1 zusätzlich kurz (6250 statt 7692 Byte, Ratio 0,813) |
   | Alien Syndrome [Sega, 1987] | **negativ** | echte Produktionsdisk, 40 Spuren, durchgehend Standard-Header, **0 Auffälligkeiten** |

   Ergebnis: `has_custom_sync` feuert auf der geschützten Disk **genau** auf
   den 15 vorab gepinnten Ganzspuren und auf keiner weiteren, auf der echten
   kommerziellen Disk auf **keiner**. Der Klassifikator liefert dort 15
   `CUSTOM_SYNC`-Hits, hier 0 Hits / `UFM_PROT_NONE`. Damit ist die
   Diskriminierung an echten Daten belegt, nicht nur die Abwesenheit von
   Fehlalarmen. Nebeneffekt: `g64` steigt von T1b auf **T1** (erstes Format
   mit realem Original **und** Cross-Tool-Beleg).

   Was die Positivkontrolle **nicht** zeigt: dass die *Scheme-Namen* stimmen
   (siehe PROT-5) — und RapidLok bleibt unauslösbar, aber aus einem anderen
   Grund als gedacht (siehe PROT-6).

### ABI-1 — `uft_crc_correct` war in zwei Headern mit unvereinbaren Signaturen deklariert (2026-08-18, MF-397) → ✓ RESOLVED

**Correctness / ABI.** Gefunden beim Umbau von `test_recovery` auf echten Code.
Derselbe Symbolname war zweimal öffentlich deklariert:

| Header | Signatur | Definition vorhanden? |
|---|---|---|
| `include/uft/uft_god_mode.h:216` | `bool uft_crc_correct(uint8_t*, size_t, int, uft_crc_correction_t*)` | **ja** (`uft_god_mode_api.c:288`) |
| `include/uft/uft_crc.h:55` | `int uft_crc_correct(uft_crc_type_t, uint8_t*, size_t, int, uft_crc_result_t*)` | **nein** |

In C ist das lautlos. Eine Übersetzungseinheit, die `uft_crc.h` einbindet und
die Fünf-Parameter-Form aufruft, linkt gegen die Vier-Parameter-Definition und
übergibt die Argumente in den falschen Registern — **ohne Compiler- und ohne
Linker-Diagnose**. Nur wenn jemand beide Header zusammen einbindet, fällt der
Widerspruch auf; das tut derzeit niemand.

**Warum es noch nicht geknallt hat:** genau eine Datei bindet `uft_crc.h` ein
(`src/formats/misc/pc_img.c`) und ruft die Funktion nicht auf. Geladene Waffe,
kein abgefeuerter Schuss.

**Fix:** Die Phantom-Deklaration entfernt, zusammen mit `uft_crc_result_t` und
`UFT_CRC_MAX_ERRORS`, die ausschließlich ihr dienten (repo-weit auf weitere
Nutzer geprüft: keine). `uft_crc_type_t` bleibt — `uft_crc_polys.h` nutzt ihn.
An der Stelle steht jetzt ein Kommentar, der die Rekonstruktion verbietet.

**Randbefund (Hygiene, kein Korrektheitsproblem):** `crc16_ccitt` existiert
**viermal** im Baum — einmal öffentlich (`uft_crc_correction_v2.c:24`) und
dreimal `static` (`uft_god_mode_api.c:277`, `uft_disk_quickscan.c:68`,
`uft_mfm_encoder.c:104` als Update-Variante). Alle geprüften Fassungen sind
algorithmisch identisch (CCITT-FALSE, init 0xFFFF, Polynom 0x1021), es ist also
Redundanz und keine Divergenz — anders als bei FMT-14. Zusammenführen lohnt,
eilt aber nicht.

### GUI-1 — Port-Pflicht für Controller, die keinen Port benutzen (Issue #34, MF-412) → ✓ RESOLVED

**Correctness / Prinzip 4 (keine irreführenden Vorgaben).** Aus einem realen
Nutzerbericht (GitHub #34, 2026-08-11): ein FC5025 ließ sich nicht verbinden.
Das Gerät hängt unter Windows korrekt am `libusb-win32`-Treiber und erscheint
nie als COM-Port; UFT verlangte trotzdem einen.

`HardwareTab::onConnect()` prüfte **unbedingt**:

```c
if (port.isEmpty()) { warn("Please select a valid port."); return; }
```

Fünf der neun Controller lesen das Portfeld überhaupt nicht. Belegt Aufrufstelle
für Aufrufstelle in `src/hardwaretab.cpp`:

| Controller | liest den Port? | wie adressiert |
|---|---|---|
| greaseweazle | ja — `gwp->open(port)` | USB-CDC, erscheint als COM |
| applesauce, adfcopy | ja — `QSerialPort…::open(port)` | seriell |
| usb_floppy | ja — als Gerätepfad | UFI |
| scp, xum1541 | **nein** | libusb, über VID/PID gefunden |
| kryoflux, fluxengine, **fc5025** | **nein** | externes CLI-Werkzeug |

Zwei sichtbare Folgen: ohne COM-Port stand „(No ports found)" in der Liste und
der Connect-Knopf war **abgeschaltet**; mit einem fremden COM1 meldete die
Statuszeile „Connecting to FC5025 on COM1" — eine Aussage über etwas, das
dieser Pfad nie anfasst.

**Behoben (MF-412).** Die Entscheidung liegt jetzt in
`include/uft/hal/uft_controller_transport.h` — reines C, ohne Qt, damit
prüfbar; `src/gui/` hat keine Testabdeckung, und genau daran ist PROT-8
jahrelang unbemerkt geblieben. `tests/test_controller_transport.c` hält sieben
Fälle fest, darunter der gemeldete und die Gegenrichtung: die drei seriellen
Controller **müssen** weiterhin einen Port verlangen. Ein zehnter Controller,
der der Tabelle nicht hinzugefügt wird, lässt den Test scheitern statt still in
UNKNOWN zu fallen.

Im Widget: `onConnect()` verlangt den Port nur noch, wo er gelesen wird; die
Statuszeile nennt keinen Port mehr, wenn keiner benutzt wird; die Portliste
zeigt für die betroffenen Controller einen erklärenden, deaktivierten Eintrag
statt fremder COM-Ports, und der Connect-Knopf bleibt bedienbar. Ein leerer
oder unbekannter Controller-Schlüssel behält bewusst das alte Verhalten —
`detectSerialPorts()` läuft einmal aus dem Konstruktor **vor**
`populateControllerList()`, und der Start darf sich nicht ändern.

*Nicht Teil dieses Fehlers:* nach dem Verbinden braucht der FC5025-Pfad weiterhin
`fcimage` auf dem PATH. Fehlt es, meldet der Provider das ausdrücklich
(`fc5025_provider_v2.cpp:332`) und nennt beide Wege — das war schon vorher
ehrlich und bleibt es.

*Verifikation:* 197/197 ctest grün; `src/hardwaretab.cpp` gegen Qt 6.10.2 mit
`uic`-generiertem UI-Header syntaxgeprüft (rc=0). Ein manueller GUI-Smoke-Test
war nicht möglich — kein Gerät und keine Anzeige in dieser Umgebung.

### PRINC-1 — Die einzige Kompatibilitätsmatrix war aus dem Doku-Beispiel kopiert (2026-08-18, MF-414) → ✓ RESOLVED

**Correctness / Prinzip 6.** Aufgefallen beim Angehen von Sprint-3 S3-2
(„Kompatibilitätsmatrix nur 1 von 88 Plugins"). Das Problem war nicht die
fehlende Matrix bei 87 Plugins — das ist ein ehrliches Nichts. Das Problem war
die **eine, die es gab**.

`uft_adf_plugin.c` führte sechs Einträge mit konkreten Urteilen:

| Konsument | Urteil | Zusatz |
|---|---|---|
| WinUAE 5.3 / 4.x, FS-UAE 3.1 | `COMPATIBLE` | — |
| FS-UAE <3.0 | `INCOMPATIBLE` | „rejects timing track annotations" |
| Amiga Explorer | `COMPATIBLE` | Testdatum 2026-03 |
| real Amiga hw | `PARTIAL` | **„85% of test disks round-trip cleanly"** |

**Nichts davon wurde je gemessen.** Die Tabelle ist wörtlich aus dem
*Illustrationsbeispiel* in `docs/DESIGN_PRINCIPLES.md` §6 (Zeilen 282–287)
übernommen worden; der Einführungs-Commit sagt es selbst: „ADF exemplarisch
populiert … **1:1 aus dem DESIGN_PRINCIPLES.md §6 Beispiel**". Die Kopie
widerspricht ihrer Vorlage sogar im einzigen prüfbaren Feld — das Beispiel
markiert drei Zeilen „CI-getestet", der Code lieferte sie mit
`ci_tested = false` aus. Es gibt keinen CI-Job, der ADF-Ausgabe an einen
Emulator gibt, und das Projekt besitzt keine Amiga-Hardware (UFT-008, HIL-Tier
NOT_RUN) — die 85 % können keinen Ursprung haben.

Das ist dieselbe Klasse wie PROT-3 und FMT-2/3/10/11/12, nur eine Ebene höher:
nicht ein Parser gegen eine erfundene Spec, sondern eine *Illustration*, die zur
*Behauptung* befördert und im Binary ausgeliefert wurde. Und das Audit zählte
sie als das eine vorbildliche Plugin.

**Behoben (MF-414).** Alle sechs Einträge stehen auf `UFT_EMU_UNTESTED`, jeder
mit der Notiz, warum. Die Konsumentennamen bleiben: *welche* Ziele für ADF
zählen, ist echte Information — die Urteile waren es nicht. Aus einer Sammlung
von Behauptungen wird damit eine Aufgabenliste.

**Wächter ergänzt.** `audit_plugin_compliance.py` konnte nur Vorhandensein
prüfen (`compat_entries != NULL`) und hätte kopierten Beispieltext nie von
Messwerten unterschieden. `check_consistency.py` hat jetzt die Kategorie
„unbacked compat claims": jedes Urteil außer `UNTESTED` muss einen Beleg
**benennen** — `ci_tested = true` oder ein `test_date`; `PARTIAL` und
`INCOMPATIBLE` zusätzlich eine Notiz. Gegenprobe mit der Originaltabelle
gemacht: vier Verstöße namentlich gemeldet.

*Grenze der Prüfung, ausdrücklich:* sie stellt fest, ob ein Beleg genannt ist,
nicht ob er stimmt. „Amiga Explorer" mit erfundenem Datum 2026-03 wäre
durchgegangen. Eine mechanische Prüfung kann Ehrlichkeit nicht ersetzen, nur
die stillschweigende Variante ausschließen.

*Nicht getan und warum:* die übrigen 87 Plugins **nicht** mit
UNTESTED-Matrizen befüllt. Das würde die Audit-Zahl von 1/88 auf 88/88 heben,
ohne eine einzige Information hinzuzufügen — `compat_entries == NULL` und eine
Liste aus lauter UNTESTED sagen dasselbe. Eine Matrix gehört dorthin, wo jemand
etwas geprüft hat oder wo die relevanten Ziele nicht offensichtlich sind.

### PERF-1 — Zeitmessung in UFT funktionierte still nicht; die Decoder-Hotpaths sind nicht der Engpass (2026-08-20, MF-435) → ✓ BEHOBEN + BASELINE

**Correctness + Messung.** Der Baum hatte keinen einzigen Benchmark, nur eine
`uft-benchmark`-Skill, die nie benutzt wurde. Der Grund dafür stellte sich als
strukturell heraus, nicht als Nachlässigkeit.

`include/uft/uft_platform.h` trug:

```c
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#include <time.h>
static inline int uft_clock_gettime(int clk, struct timespec *ts) {
    time_t t = time(NULL);
    ts->tv_sec = t;
    ts->tv_nsec = 0;          /* Ein-SEKUNDEN-Auflösung */
    return 0;
}
#define clock_gettime uft_clock_gettime
#endif
```

Drei Fehler in einem Block:

1. **Er ersetzte eine Funktion, die funktioniert.** MinGW-w64 liefert ein
   echtes `clock_gettime` mit 100 ns Granularität — gemessen, nicht vermutet.
2. **Ob er greift, hing an der Include-Reihenfolge.** Der Guard prüft
   `CLOCK_MONOTONIC`, das `<time.h>` definiert. Wer `<time.h>` zuerst
   einband, behielt die echte Uhr; wer über einen UFT-Header kam, bekam den
   Shim. Zwei Dateien im Baum, zwei verschiedene Uhren.
3. **Der Ersatz meldete ganze Sekunden.** Jede Messung unter einer Sekunde
   las sich als exakt 0.

Betroffen war `src/core/uft_capture.c::now_seconds()`, das
`uft_capture_result_t::elapsed_seconds` füllt — dieses Feld war unter Windows
auf ganze Sekunden gerastert. (`uft_fat32_mbr.c` erreicht `uft_platform.h`
nicht und war nicht betroffen.)

Eine Uhr, die 0 vergangene Zeit meldet, ist das zeitliche Gegenstück zu
erfundenen Daten. Ersatzlos entfernt: jede Toolchain, mit der dieses Projekt
baut, hat `clock_gettime`. Eine, die es nicht hat, bricht jetzt laut beim
Linken statt still zu lügen.

*Pikant:* `include/uft/compat/uft_platform.h` enthält seit jeher eine
**korrekte** QPC-basierte Implementierung mit hoher Auflösung. Wegen der
geteilten Include-Guards aus ARCH-1 ist sie nie erreichbar. Die gute Version
war tot, die kaputte war live.

**Baseline (`tests/benchmarks/bench_decode_hotpath`).** GCC 13.1.0 MinGW-w64,
x86-64, `-O3 -DNDEBUG`, Netzbetrieb, Median aus 11 Messungen nach 3 Warmups,
drei unabhängige Läufe:

| | Median | Durchsatz |
|---|---|---|
| `uft_pll_process_flux_mfm` | 1,320–1,380 ms | 72,5–75,7 M Übergänge/s |
| `flux_find_sync` (Vollscan) | 0,317–0,330 ms | 1514–1578 M Bits/s |

**Ergebnis: beide sind nicht optimierungswürdig.** Hochgerechnet auf eine
DD-Diskette (80 Zylinder × 2 Köpfe × 3 Umdrehungen = 480 Umdrehungen) sind das
zusammen rund **1,2 s CPU** gegen **~96 s reine Drehzeit** plus Spurwechsel.
Die Decode-Arithmetik ist in der Größenordnung von **einem Prozent** der
Wanduhr eines echten Einlesevorgangs.

Genau dafür misst man vorher. Beide Funktionen *sehen* nach Kandidaten aus —
die PLL rechnet `double` pro Übergang, `flux_find_sync` läuft Bit für Bit und
lädt dasselbe Byte achtmal. Beide umzuschreiben wäre verlockend gewesen und
hätte unter einer Sekunde je Diskette gebracht, bei Risiko in Code, wo ein
falsches Bit ein falsches Archiv ist.

**Nicht gemessen:** Ende-zu-Ende-Konvertierung, Datei-I/O, die
OTDR/DeepRead-Pipeline (12 Stufen), die GUI. Wenn etwas real langsam ist,
liegt es dort — und gehört gemessen, bevor es angefasst wird.

### ARCH-7 — 24 Kopien der CBM-Zonentabelle in drei Indexkonventionen (2026-08-20, MF-434) → ◐ SSOT STEHT, 20 Kopien jetzt GEPRÜFT (MF-459), Migration weiterhin offen

**Architecture.** Sektoren pro Spur, Geschwindigkeitszone, Gap und Kapazität
sind Eigenschaften eines **Laufwerks**, nicht eines Dateiformats. Ein 1541 legt
21 Sektoren auf Spur 1, egal ob das Ergebnis als D64, G64 oder NIB gespeichert
wird.

Gemessen über den ganzen Baum: **24 Kopien** dieser Tabelle in 23 Dateien, in
**drei unverträglichen Indexkonventionen**:

| Form | Vorkommen |
|---|---|
| 1-basiert, führende 0, 43 Einträge (Spuren 1-42) | 8 |
| 1-basiert, führende 0, 41 Einträge (Spuren 1-40) | 6 |
| **0-basiert**, 40 Einträge | 3 |
| D71-Varianten mit 70, 71, 35, 36 Einträgen | 5 |
| D67 (20 statt 19 in Zone 2) und Lisa Twiggy | 2 |

**Wichtig und ausdrücklich: die Werte stimmten überall überein.** Das ist kein
Fehler, der behoben wird, sondern ein Fakt, der ein Zuhause bekommt, bevor die
nächste Off-by-one daraus entsteht, dass jemand eine 0-basierte Tabelle
1-basiert liest. Die beiden echten Abweichungen sind korrekt: D67 mit 20
Sektoren in Zone 2 (das *ist* der Unterschied zwischen 2040 und 1541, 690
gegen 683 Blöcke) und Lisa Twiggy als völlig anderes Laufwerk.

**Umgesetzt:** `include/uft/formats/cbm/uft_cbm_geometry.h` beschreibt vier
Familien (1541, 2040, 1571, 1581) als Zonengrenzen statt als Arrays — die
Zonen sind der eigentliche Fakt, die Arrays waren immer nur derselbe Fakt 35-,
40-, 42- oder 70-mal ausgeschrieben. Spurnummern sind durchgehend **1-basiert**
wie in jedem Laufwerkshandbuch, und es gibt bewusst **kein Array zum
Indizieren**: einen Akzessor kann man nicht mit der falschen Konvention lesen.

`test_cbm_geometry` hält die neuen Akzessoren gegen wörtliche Kopien aller drei
Konventionen **und** gegen die Referenzabbilder: die Blockarithmetik trifft die
Dateigrößen von `vice_c1541_35trk.d64`, `_2040.d67`, `_70trk.d71` und
`_80trk.d81` exakt. Das bindet die Zahlen an Datenträger, die c1541 erzeugt
hat, nicht an das, was UFT ohnehin schon glaubte.

Migriert sind die drei Dateien auf der ARCH-6-Naht: der GCR-Encoder
(`uft_d64_g64.c`, fünf Tabellen), das D64-Plugin und das D67-Plugin.
`test_convert_via_plugin` belegt, dass die G64-Ausgabe dabei **bitidentisch**
bleibt — die SSOT reproduziert die alten Tabellen also nicht nur im Test,
sondern produktiv.

**Zwei Ehrlichkeiten:**

1. Der **2040-Gap ist unbekannt** und wird als 0 gemeldet, nicht geraten. Eine
   Zone-2-Spur mit 20 Sektoren hat weniger Platz je Sektor als eine mit 19, der
   1541-Wert von 19 kann also nicht einfach übernommen werden, und eine
   belastbare Quelle habe ich nicht gefunden.
2. Der **1581 meldet −1 für die Geschwindigkeitszone**, nicht 0. Er ist MFM;
   GCR-Zonen gibt es dort nicht, und 0 läse sich als „Zone 0".

**Offen: 22 weitere Kopien.** Bewusst nicht in einem Zug migriert — mehrere
davon stehen in genau den überzähligen Lesern, die ARCH-6 löschen will
(`d64_parser_v2`, `d64_parser_v3`, `commodore/d64.c`, `d64_file`, …). Erst die
Leser zusammenführen, dann übernehmen die Überlebenden die Geometrie. Andersherum
wäre es Arbeit an Code, der verschwinden soll.

---

## Nachtrag MF-459 (2026-08-22): geprüft statt migriert

Die Migration bleibt aus dem oben genannten Grund liegen. Was dazwischen fehlte,
ist jetzt da: **niemand prüfte, ob die verbliebenen Kopien mit der SSOT
übereinstimmen.** Der Eintrag oben sagt „die Werte stimmten überall überein" —
das war eine Messung von 2026-08-20, kein Zustand, der sich hält.

**20. Gate** (`scripts/cbm_zone_gate.py`): findet jedes Array-Literal im Baum,
das nach Zonentabelle aussieht, und rechnet es gegen die SSOT. Aus
„24 Kopien, die zufällig übereinstimmen" wird „20 Kopien, deren
Übereinstimmung nachgewiesen ist" — und eine spätere Migration ist damit
verifizierbar statt riskant.

Die SSOT-Werte werden aus `src/formats/cbm/uft_cbm_geometry.c` **gelesen**,
nicht im Wächter wiederholt. Ein Wächter mit eigener Kopie der Wahrheit wäre
die 25.

| | |
|---|---|
| Zonentabellen im Baum | 20 |
| durch die SSOT erklärt | **20** |
| Konventionen, die aufgehen | 1-basiert mit führender 0, 1-basiert ohne, zwei Seiten hintereinander (D71) |

Verifiziert: eine einzige geänderte Zahl (`21` → `20` in
`uft_nib_format.c`) wird gemeldet.

**Bewusst ausgenommen:** `src/formats/lisa/` — die Apple Lisa „Twiggy" ist
ebenfalls zonenweise aufgebaut und beginnt bei 22 Sektoren, hat mit Commodore
aber nichts zu tun (46 Zylinder, 512-Byte-Sektoren). Der Eintrag oben nennt sie
als eine der beiden echten Abweichungen. Sie zu melden wäre ein Falschbefund,
sie stillschweigend mitzuerklären wäre schlimmer.

### Ein latenter Namenskonflikt, dabei gefunden und behoben

`c64_sectors_per_track` existierte in **zwei Headern mit verschiedenen
Elementtypen**:

| Header | Deklaration |
|---|---|
| `include/uft/protection/uft_c64_protection.h:241` | `static const int c64_sectors_per_track[]` |
| `include/uft/uft_cbm_gcr.h:231` | `static const uint8_t c64_sectors_per_track[41]` |

Geprüft (`gcc -MM` über alle Nutzer beider Header): **derzeit zieht keine
Übersetzungseinheit beide** — die Kollision war latent, wie `UFT_BIG_ENDIAN`
vor MF-455 und wie `UFT_SCP_SIGNATURE` in ARCH-2, das viermal existierte,
einmal mit anderem Typ. Wer beide eingebunden hätte, bekäme einen
Redefinitionsfehler oder, je nach Reihenfolge, einen anderen Typ unter
demselben Namen.

Der Eintrag in `uft_cbm_gcr.h` heißt jetzt `cbm_gcr_sectors_per_track`.
**Regel B** des Gates hält das zu: derselbe Bezeichner mit verschiedenen Typen,
sobald mindestens eine Definition in einem Header steht.

> Auch hier zeigte die Rot-Probe erst, was die Regel taugt: die erste Fassung
> meldete **drei** Fälle. Zwei davon waren Fehlalarm — `static const` in einer
> `.c` ist dateilokal, zwei `.c`-Dateien dürfen denselben Namen tragen. Die
> Regel gilt jetzt nur, wenn eine Definition in einem Header steht.

---

### ARCH-18 — `uft_xdf_api_impl.c`: eine zweite Format-Schicht, die niemand ruft (2026-08-22, MF-459) → ⚠ OFFEN

Beim Zählen der Zonentabellen aufgefallen. Der Dateikopf verspricht:

> Complete implementation of: Format handlers (ADF, D64, G64, IMG, ST, TRD),
> Batch processing, Comparison, Hardware integration stubs, JSON export

Gemessen:

- **sechs** `static` Funktionen (`import_adf`, `import_d64`, `import_g64`,
  `import_img`, `import_st`, …) tragen `__attribute__((unused))` — sie sind per
  Deklaration als aufruferlos markiert
- die öffentliche API (`xdf_api_batch_create`, `xdf_api_batch_process`,
  `xdf_api_compare`) hat **null** Aufrufer in `src/` und `tests/`
- **elf** `(void)`- bzw. „reserved for"-Marker, darunter
  `(void)sectors_per_track; /* reserved for full impl */` in `import_adf`

`import_d64` liest D64 mit **eigener Zonentabelle** — eine der 20 oben, und
damit eine parallele Format-Schicht neben dem D64-Plugin (ARCH-6).

`src/formats/xdf/DEFERRED.md` erklärt, warum die Datei einmal *nicht*
restauriert wurde: sie brauchte `<fnmatch.h>`, das MinGW nicht hat. Der
Blocker ist längst weg (`src/compat/uft_fnmatch.c` existiert), die Datei liegt
im Baum und wird gebaut — nur ruft sie niemand. Das Dokument ist damit
veraltet.

**Nicht in MF-459 entschieden.** Sie einzubinden hieße, eine zweite
Format-Schicht scharf zu schalten, die ARCH-6 gerade abbaut; sie zu löschen ist
ein eigener Schritt mit der MF-369-Beweispipeline. Beides gehört zu ARCH-6,
nicht zu ARCH-7.

### PRINC-2 — `uft_smart_open()` liefert eine erfundene Qualitätsbewertung und druckt sie als forensischen Bericht (2026-08-21, MF-443) → ✓ BEHOBEN (MF-444), Nachweis in `tests/test_smart_open_quality.c`

> **Stand MF-444:** Rumpf neu. `analyze_quality()` liest jede Spur über das
> Plugin und zählt; alle Felder starten auf `UFT_QUALITY_NOT_DETERMINED`
> (−1) und der Bericht schreibt „not determined", wo nichts gemessen wurde.
> Erkennung läuft über `uft_probe_buffer_format()` (Registry) statt über die
> 16er-Tabelle; die Extension-Rückfalllinie („Name endet auf .d64, also 50 %
> sicher") ist ersatzlos weg. Die v3-Kette ist opt-in
> (`uft_smart_register_v3_handler()`), nicht mehr fest verdrahtet.
> `enable_god_mode`/`god_mode_used` und `uft_calculate_metrics()` entfernt —
> letztere gab für jede Eingabe vier Konstanten zurück und überschrieb damit
> die gezählten Werte. Test: 683 / 690 / 3200 Sektoren aus D64 / D67 / D81,
> exakt, gegen VICE-Images. Zwei Folgefunde daraus: ARCH-9, ARCH-10.

**Prinzip 1 („Keine erfundenen Daten"), schwerster Verstoß dieser Reihe.**
Gefunden bei der Frage, ob die v3-Kette als API etwas taugt.

`uft_smart_open()` verspricht laut Header: Formaterkennung, Kopierschutz-
Erkennung, **Qualitätsbewertung** und einen Bericht — in einem Aufruf. Die
Implementierung zerfällt in drei sehr unterschiedliche Teile:

| Teil | Zustand |
|---|---|
| **Erkennung** | **echt** — ruft die tatsächlichen Plugin-Probes und meldet deren Konfidenz. Aber über eine handgepflegte Tabelle von **16** Formaten statt der Registry mit 88. Duplikat, keine Erfindung. |
| **Kopierschutz** | teilweise — ruft die v3-Parser, Konfidenz ist eine handvergebene Konstante je Schema, funktioniert nur für D64/G64/SCP |
| **Qualität** | **erfunden** |

`analyze_quality()` (`src/core/uft_smart_open.c:268`) mit Standardoptionen:

```c
quality->level            = UFT_QUALITY_GOOD;
quality->readable_sectors = 100;
quality->total_sectors    = 100;
```

Hartkodiert. Alles Übrige bleibt bei 0 aus dem `memset` — `crc_corrected`,
`weak_bits_found`, `weak_bits_resolved`. Nur wenn `enable_god_mode` gesetzt ist
(Standard: **false**), werden `crc_errors` und `bit_error_rate` tatsächlich
berechnet; die Sektorzahlen bleiben auch dann bei 100/100.

**Und `uft_smart_report()` gießt genau das in einen Bericht:**

```
QUALITY ASSESSMENT
  Level:       Good
  Sectors:     100 / 100 readable
  CRC Errors:  0 (corrected: 0)
  Weak Bits:   0 (resolved: 0)
  God-Mode:    Not needed
```

Für jede Diskette, ohne dass ein Sektor gelesen wurde.

Das ist gefährlicher als jeder andere Fund dieser Reihe, und zwar nicht wegen
Speichersicherheit. Ein Speicherfehler stürzt ab oder verfälscht Daten, die
jemand später prüfen kann. **Dieser Text sieht aus wie eine Messung und wandert
in ein Erhaltungsprotokoll.** Ein Archiv, das „100 / 100 readable, Quality
Good" zu einer beschädigten Diskette ablegt, hat kein Werkzeugproblem mehr,
sondern einen falschen Nachweis im Bestand.

**Entschärfend:** `uft_smart_open()` hat heute **keinen Aufrufer**, der Bericht
wird also nirgends erzeugt. Der Defekt ist geladen, nicht abgefeuert — wie
mehrere Funde dieser Reihe.

**`uft_advanced_mode()`** hat denselben Charakter, milder: eine **eigene**
Formaterkennung mit **8** Formaten und fest verdrahteten Konfidenzen (`95`),
parallel zur Registry mit 88 und parallel zu smart_opens eigener Tabelle mit
16. Drei Formaterkennungen, drei Reichweiten.

**Bewertung, ehrlich in beide Richtungen.** Die *API-Form* ist gut und fehlt
dem Werkzeug tatsächlich: ein Aufruf, der eine Diskette öffnet und
zurückgibt, was sie ist, ob sie geschützt ist, wie gut sie gelesen wurde, samt
Bericht. Genau das will ein Archivar, und nichts sonst im Baum bietet es an
einer Stelle. Der *Rumpf* ist das Gegenteil dessen, wofür das Projekt auf
seiner wichtigsten Achse steht.

**Drei Wege, Entscheidung steht aus:**

1. **Rumpf neu bauen, Form behalten** — Erkennung über
   `uft_probe_buffer_format()` (88 statt 16), Qualität aus echtem
   Spur-für-Spur-Lesen über die Plugin-Schnittstelle plus OTDR für den Level,
   Weak Bits aus der Multiread-Maschinerie, Kopierschutz über die 35+ Schemata
   in `src/protection/` statt der v3-Heuristik. Das ist ein Feature, kein
   Aufräumen.
2. **Entfernen** wie `src/switch/` — die Kette hat keinen Einstiegspunkt, und
   aus ihr ist nichts zu übernehmen (siehe die Prüfung unter ARCH-8).
3. **Als Minimum sofort:** die erfundenen Werte durch ein ehrliches „nicht
   ermittelt" ersetzen, damit der Bericht nicht behaupten kann, was er nicht
   weiß — unabhängig davon, welcher der beiden Wege gewählt wird.

### ARCH-11 — `uft_register_all_formats()` lief zum ersten Mal — und fand drei Fehler (2026-08-21, MF-446) → ✓ BEHOBEN, Wächter erweitert

Direkte Folge von ARCH-9/ARCH-10: die Registry funktioniert jetzt, also lässt
sie sich zum ersten Mal ausführen. Das war noch nie passiert — die Funktion
hatte im gesamten Projektleben keinen Aufrufer.

**Erst beim Ausführen sichtbar, alle drei unabhängig voneinander:**

**1. Pufferüberlauf um eins.** `all_plugins[]` war über eine handgeschriebene
Summe von Gruppen-Literalen dimensioniert, mit dem Gruppennamen im Kommentar.
Eine Zeile war abgedriftet: `OTHER + 47` gegen ein `g_other_plugins[]` mit 48
Einträgen. Das Array fasste damit exakt so viele Zeiger wie es Plugins gibt —
kein Platz für den Terminator:

```c
all_plugins[idx] = NULL;      /* idx == 130, Array hat 130 Elemente */
```

Behoben, indem die Größe aus denselben Arrays abgeleitet wird, auf die
`g_groups` zeigt (`ALL_PLUGIN_SLOTS` über `ARRAY_COUNT`). Ein Plugin in eine
Gruppe einzutragen vergrößert die Tabelle jetzt automatisch; es gibt keine
zweite Stelle mehr zu pflegen.

**2. Sieben Plugins waren definiert und in keiner Gruppe gelistet** — also für
`uft_register_all_formats()` unerreichbar: **SCP** (der Flux-Container, den der
gesamte DeepRead-Pfad liest), **G64**, **IMG**, ExtADF, KorgDSS1, AkaiS900,
LisaTwiggy. Nachgetragen.

**3. Die echte Plugin-Zahl ist 137, nicht 88.** 88 sind ausgeschrieben, 49
entstehen aus dem `DSK_PLUGIN()`-Makro in `src/formats/dsk_generic/`.
`MAX_FORMAT_PLUGINS` stand nach MF-445 auf 128 — neun Plugins wären mit
`UFT_ERROR_BUFFER_TOO_SMALL` abgewiesen worden. Jetzt 192.

**Der eigene Wächter war das Problem an Punkt 3.** `plugin_registry_gate.py`
aus MF-445 zählte nur ausgeschriebene Definitionen, meldete deshalb 88 und gab
für eine Kapazität von 128 grünes Licht. Ein Zähler, der zu niedrig zählt, ist
schlechter als keiner — ihm wird geglaubt. Der Wächter zählt jetzt
Makro-Instanzen mit und prüft zusätzlich, dass **jedes** definierte Plugin in
einer Gruppe gelistet ist (Punkt 2 wäre damit sofort aufgefallen).

Nachweis: `tests/test_register_all_formats.c` — 137 definiert, 137 registriert,
SCP/G64/IMG namentlich, doppelter Aufruf ändert nichts, kein doppelter `.name`.
Die Zahlen sind absichtlich exakt: „über hundert" wäre auch bei 128 durchgegangen
(verifiziert — mit `MAX_FORMAT_PLUGINS 128` fallen vier der sechs Tests).

**Weiterhin offen:** `uft_register_all_formats()` hat immer noch keinen Aufrufer
in der Anwendung. Der Mechanismus funktioniert jetzt, die GUI ruft ihn nicht auf
— das ist der nächste Schritt und eine eigene Aufgabe (ARCH-12).

---

### ARCH-13 — Bei Punktgleichstand in der Probe entscheidet die Registrierungsreihenfolge (2026-08-21, MF-447) → ✓ SICHTBAR GEMACHT (MF-448)

Gemessen beim Aufräumen von ARCH-12, mit allen 137 Plugins registriert:

```
atrcopy_dos2sd.xfd    XFD=40  JVC=40  DSK_SV=40  DSK_VEC=40  V9T9=40  JV1=35  XDM86=35
```

Fünf Plugins antworten mit exakt 40. `uft_probe_buffer_format()` vergleicht mit
`conf > best_conf`, also gewinnt der zuerst Registrierte — hier XFD, weil die
ATARI-Gruppe in `g_groups[]` vor OTHER steht. Das ist richtig aus Versehen.

Der Fall ist nicht pathologisch: XFD *ist* ein Rohformat ohne Header, und ein
1-zu-1-Sektorabbild derselben Größe ist von einem anderen Rohformat derselben
Größe nicht unterscheidbar. Das Problem ist nicht, dass die Probe unsicher ist,
sondern dass das Ergebnis so aussieht wie ein sicheres.

**Gelöst in MF-448 — als Sichtbarkeit, nicht als Auflösung.** Der Fall ist
nicht auflösbar: XFD *ist* ein Rohformat ohne Header, und ein Sektorabbild
derselben Größe ist von einem anderen Rohformat derselben Größe nicht zu
unterscheiden. Das Problem war nie die Unsicherheit, sondern dass das Ergebnis
wie Sicherheit aussah.

Neu: `uft_probe_ranking_t` + `uft_probe_buffer_ranked()` /
`uft_probe_file_ranked()`. Der Gewinner ist unverändert und weiterhin
deterministisch (strenges `>`, also behält der zuerst Registrierte einen
Gleichstand) — kein Aufrufer sieht ein anderes Format als vorher. Neu ist, dass
`tied`, `claimants`, `runner_up` und dessen Konfidenz erhalten bleiben, statt in
der Schleife verworfen zu werden. `uft_probe_buffer_format()` ist jetzt ein
Wrapper darum.

`uft_smart_open()` reicht das in `detection.equally_ranked` weiter und schreibt
bei `> 1` in die Warnungen: „Format nicht eindeutig: N Plugins beanspruchen die
Datei mit derselben Konfidenz. 'X' gewinnt durch Registrierungsreihenfolge,
nicht durch Evidenz."

Der Name ist bewusst nicht `uft_probe_result_t` — den führt bereits
`include/uft/uft_format_probe.h` (siehe ARCH-14).

---

### ARCH-14 — `uft_probe_format()` liefert eine Container-ID, und sein Mehrdeutigkeits-Feld ist ein Phantom (2026-08-21, MF-448) → ✓ BEHOBEN (MF-450), Wurzel war eine andere als vermutet

Gefunden über eine Namenskollision. `include/uft/uft_format_probe.h` definiert
seit jeher ein `uft_probe_result_t` mit genau dem Feld, das ARCH-13 gebraucht
hätte:

```c
int          alternative_count;
uft_format_t alternatives[4];
int          alt_confidence[4];
```

Die einzige Implementierung (`src/core/uft_probe_format_impl.c`) füllt es nie —
sie `memset`t die Struktur und setzt genau ein Feld:

```c
result->format = (uft_format_t)plugin->format;
```

Zwei Probleme in einem:

1. **`alternative_count` ist permanent 0.** Ein Feld, das Mehrdeutigkeit melden
   soll und es nie tut, ist schlimmer als keines — es liest sich als „geprüft,
   keine Alternativen".
2. **Der Rückgabewert ist `plugin->format`,** also für 131 von 137 Plugins
   `UFT_FORMAT_DSK`. Der einzige Aufrufer,
   `src/formats/uft_format_convert_dispatch.c:490`, wählt daraus den
   Konvertierungspfad. Dieselbe Klasse wie ARCH-10, eine Ebene höher.

Außerdem sah diese Datei `uft_probe_result_t` nur über einen transitiven
Include; ein expliziter `#include "uft/uft_format_probe.h"` kollidiert dort mit
`UFT_FCLASS_*`/`UFT_CLASS_*` aus einem anderen Header — ein eigener
Header-Konflikt, der mit ARCH-1 zusammenhängt.

**Behoben in MF-450 — aber die Wurzel lag woanders, als ARCH-10 und ARCH-14
beide angenommen hatten.**

Beide Einträge sagten sinngemäß: „`uft_format_t` benennt eine Container-Klasse,
kein Format." Das war falsch. Das Enum hat **55 Werte**, darunter
`UFT_FORMAT_D64`, `UFT_FORMAT_ADF`, `UFT_FORMAT_ATR`, `UFT_FORMAT_IMD` — und
**30 Plugins deklarierten trotzdem `UFT_FORMAT_DSK`, obwohl das Enum ihren
exakten Namen führt.** Nicht das Enum war das Problem, sondern die
Deklarationen.

| | vorher | nachher |
|---|---|---|
| verschiedene `.format`-Werte | 7 | **37** |
| IDs, die genau ein Plugin meinen | 6 | **36** |
| Plugins auf `UFT_FORMAT_DSK` | 131 | 101 |

Die verbleibenden 101 sind korrekt: 49 `DSK_PLUGIN()`-Varianten plus Plugins,
deren Namen das Enum keinen Wert gibt (D67, KorgDSS1, …). Für die *ist*
„generischer Sektorcontainer" die richtige Aussage.

**Was das konkret repariert hat.** `uft_convert_file()` wählt seinen
Konvertierungspfad über `uft_convert_get_path(src_format, dst_format)`, und die
Tabelle ist auf echte Formate verschlüsselt — `D64→G64`, `SCP→HFE`, `ADF→SCP`.
Eine Quelle, die als `UFT_FORMAT_DSK` erkannt wurde, traf **keinen** dieser
Einträge. Die Antwort auf nahezu jede Dateikonvertierung war „No conversion
path from … to …".

Und der gefährlichere Zweig:

```c
if (src_format == dst_format) {
    /* Same format: direct copy */
    err = uftc_write_output_file(dst_path, src_data, src_size);
    result->success = true;
```

Mit allem auf einer ID war das einen schlecht gewählten Zielwert davon entfernt,
eine D81 unverändert in eine `.d64` zu schreiben und Erfolg zu melden.

**Das Phantom-Feld.** `uft_probe_result_t` führt seit jeher
`alternative_count`, `alternatives[4]`, `alt_confidence[4]` und `warnings[256]`
— die einzige Implementierung setzte genau `result->format` und `memset`te den
Rest. Permanent 0 liest sich nicht als „nicht ausgefüllt", sondern als „geprüft,
nichts anderes passte". Jetzt gefüllt aus `uft_probe_buffer_ranked()`; dafür
merkt sich das Ranking bis zu vier Gleichplatzierte (`tied_with[]`), während
`tied` die wahre Anzahl bleibt.

**Und der Dispatcher lehnt Mehrdeutigkeit ab.** Konvertieren heißt, den Decoder
des Gewinners über die Bytes laufen zu lassen; wenn ein anderes Plugin sie
genauso stark beansprucht hat, war die Wahl Registrierungsreihenfolge, und das
Ergebnis wäre eine aus einer Vermutung abgeleitete Datei, gemeldet als
Konvertierung.

**Fünf vorher grüne Zusicherungen mussten geändert werden** — alle in
`test_plugin_identity.c`, `test_smart_open_quality.c` und
`test_register_all_formats.c`, und alle hielten den kaputten Zustand fest
(„4 Plugins auf `UFT_FORMAT_DSK`", „`dsk > 120`", „`uft_get_format_plugin` liefert
für eine D81 das D64-Plugin"). Der Mehrdeutigkeits-Fall lebt jetzt dort weiter,
wo er echt ist: `DSK_FM7` und `DSK_MSX` teilen sich `UFT_FORMAT_DSK` zu Recht.

**Gate E** in `scripts/plugin_registry_gate.py`: ein Plugin, dessen `.name` einem
`UFT_FORMAT_<NAME>` entspricht, muss diesen Wert deklarieren. Verifiziert — mit
`.format = UFT_FORMAT_DSK` im D64-Plugin meldet es die Stelle.

**Nachweis:** `tests/test_convert_file_detection.c`, der erste Test für
`uft_convert_file()` überhaupt. Prüft, dass die Quelle als `UFT_FORMAT_D64`
erkannt wird, dass `D64→G64`, `D64→SCP` und `ADF→SCP` in der Pfadtabelle wieder
auffindbar sind, dass D64 und D81 nicht mehr dasselbe Format sind, dass
`alternative_count` bei einer eindeutigen Datei 0 und bei der fünffach
beanspruchten XFD größer 0 ist, und dass eine mehrdeutige Quelle abgelehnt wird,
ohne eine Datei zu schreiben. Mit dem alten `.format = UFT_FORMAT_DSK` fallen
zwei der fünf Tests (verifiziert).

**Offen bleibt:** `uft_probe_format()` sieht `uft_probe_result_t` in
`uft_format_convert_dispatch.c` weiterhin nur über einen transitiven Include; ein
expliziter `#include "uft/uft_format_probe.h"` kollidiert dort mit
`UFT_FCLASS_*`/`UFT_CLASS_*` aus einem anderen Header. Das gehört zu ARCH-1
(Header-Aufteilung).

---

### ARCH-15 — Die Formaterkennung hängt davon ab, wie viel der Aufrufer gelesen hat (2026-08-21, MF-448) → ✓ BEHOBEN (MF-449), 17. Gate steht

Gemessen beim Testen von ARCH-13, und gravierender als der Gleichstand daneben.

`uft_probe_file_format()` liest **4096** Bytes. `uft_smart_open()` liest bis zu
**65536**. Dieselbe Datei, zwei Einstiegspunkte, zwei verschiedene Formate:

| Puffer | Ergebnis |
|---|---|
| 4096 Bytes | **XFD** = 40, gleichauf mit JVC, DSK_SV, DSK_VEC, V9T9 |
| 65536 Bytes | **JV3** = 70, allein |

Die Identifikation ist damit keine Eigenschaft der Datei, sondern der
Puffergröße, die der Aufrufer zufällig gewählt hat. Und der größere Puffer
liefert die **schlechtere** Antwort: JV3 ist TRS-80, die Datei ist Atari.

Beide Zahlen sind in `tests/test_register_all_formats.c` exakt festgenagelt
(`the_probe_answer_depends_on_how_much_the_caller_read`), damit jede
Vereinheitlichung sichtbar wird.

**Behoben in MF-449.** Die Frage „welche Antwort ist richtiger" hatte eine
Antwort, die kein zusätzliches Referenzmaterial brauchte: **keine von beiden**,
und zwar aus drei getrennten Gründen.

**Es waren nicht zwei Einstiegspunkte, sondern drei.**
`uft_find_format_plugin_for_file()` war eine dritte Kopie der Probe-Schleife —
eigener 4096-Byte-Stackpuffer, eigene Best-Confidence-Schleife, und danach ein
Rückfall auf die Datei-Extension, genau die Rateheuristik, die
`uft_smart_open()` in MF-444 verloren hat. Sie hatte im gesamten Baum keinen
Aufrufer. Entfernt statt nachgezogen: drei Kopien einer Entscheidung sind der
Grund, warum die Puffergrößen überhaupt auseinanderlaufen konnten.

**4096 war zu klein für den Baum.** `jv3_probe()` beginnt mit

```c
if (size < JV3_HEADER_SIZE || ...) return false;    /* 8960 */
```

Durch `uft_probe_file_format()` — und damit durch `uft_disk_open()` — konnte
JV3 also **nie** anschlagen. Ein Format, das das Werkzeug zu lesen behauptet und
nicht identifizieren konnte. Es ist das einzige Probe im Baum über 4096 (das
zweitgrößte ist D88/D77 mit 688).

**65536 war zu groß, solange `jv3_probe()` so aussah.** Sein Beweis war

```c
if (trk < 80 && (flags & 0x03) < 4) valid++;
```

Die zweite Hälfte kann bei einem Zwei-Bit-Feld nicht falsch werden, also blieb
„das Byte im Abstand 3 ist kleiner als 80" — für etwa ein Drittel beliebiger
Daten wahr. Fünf Treffer in hundert Einträgen sind damit für jede Eingabe so
gut wie sicher, und die Antwort war Konfidenz 70. Deshalb gewann JV3 (TRS-80)
über eine Atari-Datei, sobald der Puffer groß genug war, dass das Probe
überhaupt lief.

Neu geschrieben nach demselben Muster wie D88/DMK in MF-447: geprüft wird, was
`jv3_open()` selbst prüft — der Lauf bis zum `0xFF 0xFF`-Terminator, die Summe
aus `jv3_sizes[flags & 3]`, und dass diese Summe plus Header in die Datei passt.
Ein Verzeichnis aus Rauschen summiert sich nicht auf die Datei, in der es steht.

**Eine Konstante:** `UFT_PROBE_BUFFER_SIZE = 65536` in
`include/uft/uft_format_plugin.h`, benutzt von `uft_probe_file_format()`,
`uft_probe_file_ranked()` und `uft_smart_open()`.

**17. Gate** (`scripts/probe_buffer_gate.py`): vergleicht jede `size < X`-Schranke
am Kopf eines Probes gegen die Konstante und verbietet Zahlen-Literale als
Puffergröße. Verifiziert: mit `UFT_PROBE_BUFFER_SIZE 4096` meldet es
`jv3_probe()`. Grenze ehrlich benannt — ein Probe, das seinen Bedarf zur
Laufzeit errechnet, wird nicht erfasst.

Nachweis: `tests/test_register_all_formats.c` —
`both_probe_entry_points_give_the_same_answer` prüft alle elf Referenz-Images
plus die XFD durch beide Türen, `jv3_no_longer_matches_arbitrary_bytes` prüft
das Probe gegen ein Verzeichnis aus plausibel aussehendem Rauschen.

---

### PORT-1 — `format(printf, …)` prüft auf MinGW gegen den falschen Dialekt (2026-08-21, MF-448) → ✓ BEHOBEN

Nebenbefund aus MF-448, aufgetaucht beim Einschalten von `-Wformat`.

Der CMake-Build hatte **überhaupt keine Warn-Flags**. Deshalb kam in MF-444
durch, dass `uft_smart_report()` einen Format-String mit einem `%s` mehr hatte
als Argumente — die Zeile `"God-Mode: %s"` überlebte eine Änderung, die ihr
Argument entfernte. `snprintf` las über das Ende der Varargs hinaus; die
Testsuite prüfte nur die Mitte des Berichts und blieb grün. Der Compiler hätte
es umsonst gesagt.

Beim Einschalten von `-Werror=format` meldete der Baum dann „unknown conversion
type character 'z'" quer durch die Konvertierungs-Schicht. **Meine erste
Diagnose war falsch** — ich nahm an, `%zu` funktioniere auf MinGW nicht, und
wollte `__USE_MINGW_ANSI_STDIO` setzen. Nachgemessen (kompiliert und
ausgeführt): `%zu` funktioniert zur Laufzeit einwandfrei. Der Fehler lag im
Attribut: `__attribute__((format(printf, …)))` wählt auf MinGW den
MSVCRT-Dialekt, der C99 nicht kennt. gcc prüfte also gegen eine Formatsprache,
die dieser Code nicht benutzt.

`UFT_PRINTF_FMT` in `include/uft/uft_compiler.h` verlangt auf MinGW jetzt
`gnu_printf`; die einzige handgeschriebene Stelle
(`uftc_add_warning`) benutzt das Makro. Damit baut der gesamte Baum mit
`-Wformat -Werror=format -Werror=format-extra-args` sauber durch, und die Klasse
ist ab jetzt ein Build-Fehler statt einer Warnung unter vielen.

---



---

### ARCH-12 — Die Anwendung registriert keine Plugins (2026-08-21, MF-446) → ✓ BEHOBEN (MF-447) — und der Startup-Aufruf legte zwei Probe-Fehler frei

`uft_register_all_formats()` ist ab MF-446 nachweislich korrekt, wird aber
nirgends aufgerufen — nicht in `main()`, nicht in `UftMainWindow`. In der
laufenden Anwendung ist die Registry damit leer, und jeder Pfad über
`uft_disk_open()`, `uft_probe_file_format()`, `uft_smart_open()` oder die
`uft_disk_*`-API bekommt „kein Plugin gefunden".

Es sind sogar **zwei** tote Registrierungswege, nicht einer. Das Makro
`UFT_REGISTER_FORMAT_PLUGIN(name)` in `include/uft/uft_format_common.h`
**definiert** lediglich eine Funktion:

```c
uft_error_t uft_register_##name(void) {
    return uft_register_format_plugin(&uft_format_plugin_##name);
}
```

84 Plugins nutzen es. **Keine** dieser 84 Funktionen wird irgendwo aufgerufen.
`scripts/gen_format_list.py` bezeichnete sie als „auto-registered" — das war nie
zutreffend, das Makro registriert nichts, es schreibt einen Registrar. Wording
korrigiert (MF-446).

Dass das nie auffiel, hat denselben Grund wie ARCH-9: die GUI benutzt für ihre
Formatarbeit die Qt-Provider und direkt gelinkte Plugin-Symbole, nicht die
Registry. Zwei parallele Wege zum selben Zweck — siehe ARCH-6 (zwei
Format-Layer, vier Nahtstellen).

**Behoben in MF-447.** `main()` registriert vor dem ersten Fenster; eine
unvollständige Registrierung wird als Dialog gezeigt, nicht geloggt — fehlende
Formate sind in jedem Öffnen-Dialog still abwesend, und „dieses Tool kann die
Diskette nicht lesen" ist genau das Falsche zum Verschweigen. Registrierung ist
reine Buchhaltung: alle 137 Plugins haben `.init == NULL`.

`uft_register_all_formats()` und `uft_get_format_count()` hatten **in keinem
Header eine Deklaration** — Aufrufer mussten sich ein eigenes `extern`
schreiben, genau die Klasse, auf die MF-442 einen Wächter gesetzt hat. Das ist
auch der Grund, warum niemand die Funktion aufrief: ein Symbol ohne Header hat
keinen naheliegenden Ort, von dem aus es gerufen wird. Jetzt deklariert.

**Was der Startup-Aufruf freilegte — und was passiert wäre, hätte ich ihn ohne
Prüfung eingebaut:**

Mit 137 registrierten Plugins läuft die Probe-Schleife zum ersten Mal über mehr
als einen Eintrag. Ergebnis auf den zwölf Referenz-Images in
`tests/corpus_free`:

| Datei | vorher gewonnen von | eigene Konfidenz |
|---|---|---|
| `vice_c1541_35trk.d64` | **D88 = 90** | D64 = 75 |
| `vice_c1541_2040.d67` | **D88 = 90** | D67 = 75 |
| `vice_c1541_70trk.d71` | **D88 = 90** | D71 = 70 |
| `vice_c1541_8050.d80` | **D88 = 90** | D80 = 75 |
| `vice_c1541_8250.d82` | **D88 = 90** | D82 = 75 |
| `vice_c1541_80trk.d81` | **D88 = 90** | D81 = 80 |
| `xdftool_dd_ofs.adf` | **D88 = 90** | ADF = 95 |
| `atrcopy_dos2sd.atr` | **D88 = 90** | ATR = 95 |

`d88_probe()` beanspruchte **elf von zwölf** Dateien mit Konfidenz 90 und
gewann in acht Fällen. Die gesamte Beweislage waren zwei Bytes:

```c
uint32_t dsz = uft_read_le32(data + 0x1C);
uint8_t media = data[0x1B];
if (dsz <= file_size && (media == 0 || media == 0x10 || media == 0x20)) {
    *confidence = 90; return true;
}
```

In einer D64 ist Offset 0x1B gewöhnliche Sektordaten, die zufällig 0x00 sind,
und die LE32 bei 0x1C gewöhnliche Sektordaten, die zufällig kleiner sind als die
Datei. Die Registrierung einzuschalten, ohne das zu prüfen, hätte das Werkzeug
**schlechter** gemacht als im nicht-funktionierenden Zustand: jede Commodore-
Diskette wäre als japanisches PC-88-Image geöffnet worden.

`dmk_probe()` war derselbe Fall in schwächerer Form (Konfidenz 85 für G64, G71,
ATR aus zwei Wertebereich-Prüfungen).

**Beide korrigiert** — als Bugfix an Bestehendem, ohne neue Spec-Behauptung:
geprüft wird ausschließlich, was der jeweilige Reader in derselben Datei
ohnehin dereferenziert. Bei D88 die Track-Offset-Tabelle bei 0x20, auf die
`d88_read_track()` direkt seekt; bei DMK die Größengleichung, die
`dmk_read_track()` selbst rechnet (`DMK_HDR + tracks × sides × track_len`).
Zusätzlich skaliert die Konfidenz jetzt mit der Beweislage, statt eine Zahl zu
verkünden.

**Ehrliche Grenze:** `tests/corpus_free` enthält weder ein echtes D88 noch ein
echtes DMK. Der Positiv-Pfad ist gegen synthetische Header verifiziert, die aus
genau den Feldern gebaut sind, die die jeweiligen Reader dereferenzieren — das
beweist, dass die Probe akzeptiert, was ihr Reader lesen kann, **nicht**, dass
sie jede real existierende Datei akzeptiert. Der Negativ-Pfad ist gegen zwölf
echte Images anderer Formate verifiziert.

**Ein vorher grüner Test wurde geändert:**
`tests/test_plugin_probe_real.c::d88_checks_media_byte_and_declared_size`
prüfte einen 0x400-Byte-Nullpuffer mit media 0x00 und Größe 0x300 — also ein
Image, dessen 164 Track-Offsets alle null sind, in dem kein einziger Track
steht. Genau diese Nachsicht war der Bug. Fixture ist jetzt ein Header, dem der
Reader folgen kann; Test umbenannt auf
`d88_checks_media_byte_declared_size_and_track_table`.

Nachweis: `tests/test_register_all_formats.c` — elf Referenz-Images, jedes vom
eigenen Plugin gewonnen; D88/DMK beanspruchen keins davon mehr; synthetische
D88-/DMK-Header werden weiterhin akzeptiert. Mit dem alten `d88_probe()` fallen
vier der elf Tests (verifiziert).

---

### ARCH-9 — Die Plugin-Registry konnte nie mehr als 7 der 88 Plugins aufnehmen (2026-08-21, MF-444) → ✓ BEHOBEN, Nachweis im Test

**Gefunden, weil `uft_smart_open()` nach dem Umbau über die Registry erkennt und
der erste Test damit der erste Aufrufer der Registry überhaupt war.**

`uft_register_format_plugin()` wies Duplikate ab — verglichen wurde
`plugin->format`:

```c
for (size_t i = 0; i < g_format_plugin_count; i++)
    if (g_format_plugins[i]->format == plugin->format)
        return UFT_ERROR_PLUGIN_LOAD;
```

**82 der 88 Plugins tragen `.format = UFT_FORMAT_DSK`.** Das Enum benennt eine
Container-Klasse („ein Sektorimage"), kein Format. Die Registry nahm also das
erste DSK-Plugin und wies die anderen 81 ab — still, mit demselben Fehlercode
wie für ein kaputtes Plugin.

| | |
|---|---|
| Plugins im Baum | 88 |
| eindeutige `.format`-Werte | **7** |
| davon `UFT_FORMAT_DSK` | **82** |
| eindeutige `.name`-Werte | **88** |

Daraus folgt der zweite Teil des Befunds: **`uft_register_all_formats()` hatte
im gesamten Baum keinen einzigen Aufrufer** — auch nicht in der GUI. Jeder
Konsument ist um die Registry herumgewachsen und linkt Plugin-Symbole direkt
(`extern const uft_format_plugin_t uft_format_plugin_d64;`). Ein Mechanismus,
der nie funktioniert hat, wurde nicht repariert, sondern umgangen.

**Dritte Schicht, gefunden in MF-445:** `MAX_FORMAT_PLUGINS` war **32**. Selbst
mit korrigierter Duplikatsprüfung hätte die Registry 32 Plugins genommen und die
restlichen 56 mit `UFT_ERROR_BUFFER_TOO_SMALL` abgewiesen — still. Drei
unabhängige Deckel auf demselben Mechanismus, keiner davon sichtbar, weil eine
leere Registry überall `NULL` antwortet. Jetzt 128, Überlauf schreibt nach
stderr, und `scripts/plugin_registry_gate.py` vergleicht die Konstante gegen die
tatsächliche Plugin-Zahl (+16 Reserve).

**Fix (MF-444):** Duplikatsprüfung auf `.name` (über alle 88 eindeutig).
`uft_register_all_formats()` ist idempotent — bereits registriert ist der
gewünschte Endzustand, kein Fehler — und bricht nicht mehr beim ersten
abgewiesenen Plugin ab, sondern versucht alle und meldet den ersten echten
Fehler danach. Neu: `uft_registered_format_plugin_count()`,
`uft_get_format_plugin_by_name()`, `uft_count_format_plugins_for()`.

Nachweis: `tests/test_smart_open_quality.c::registering_twice_is_not_an_error`
— vier Plugins auf derselben Container-ID registrieren, doppelte Anmeldung
abgewiesen, Bestand unverändert.

---

### ARCH-10 — `uft_get_format_plugin(format)` ist für 82 von 88 Plugins ein Zufallstreffer (2026-08-21, MF-444) → ✓ BEHOBEN (MF-445), Wächter steht

Folgefund aus ARCH-9. Die Funktion liefert das **erste** registrierte Plugin mit
der gesuchten `uft_format_t`. Da 82 Plugins `UFT_FORMAT_DSK` führen, hängt die
Antwort von der Registrierungsreihenfolge ab und ist für 81 davon falsch, sobald
der Aufrufer ein bestimmtes Format meinte.

Betroffen (alle übergeben eine `uft_format_t` und können nicht mehr ausdrücken):

- `src/core/uft_disk_convert.c:58,90,120,121`
- `src/core/uft_disk_verify.c:49,157`
- `src/core/uft_disk_stream.c:18`
- `src/core/uft_disk_transaction.c:180`
- `src/core/uft_format_verify.c:23,75,153`
- `src/core/uft_recovery_fusion.c:91,107`
- `src/core/uft_core_stubs.c:155`

Bisher fiel das nicht auf, weil die Registry leer war (ARCH-9) und alle diese
Pfade schlicht `NULL` bekamen. Mit funktionierender Registrierung werden sie
scharf — und liefern dann still das falsche Plugin.

**Behoben in MF-445.** Die Annahme aus MF-444 („`uft_disk_t` trägt bereits das
geöffnete Plugin") war falsch — der Struct trug nur `format`. Ein Kommentar in
`src/core/uft_format_verify.c:22` sagte es sogar: *„Lookup plugin via registry
(disk->plugin nicht im Struct)"*. Jemand hatte das Problem gesehen und
umschifft.

> **Nachtrag MF-450:** die Begründung unten („das Enum benennt eine
> Container-Klasse") war falsch. `uft_format_t` hat 55 Werte, darunter
> `UFT_FORMAT_D64` — 30 Plugins deklarierten `UFT_FORMAT_DSK`, obwohl das Enum
> ihren exakten Namen führte. Der Fix hier (Diskette merkt sich ihr Plugin)
> bleibt richtig und nötig, weil 101 Plugins die Container-ID zu Recht teilen;
> aber die Mehrdeutigkeit war zu 30/131 hausgemacht. Siehe ARCH-14.

Der Fix ist die Diagnose ernst genommen: **die Diskette merkt sich ihr Plugin.**
`struct uft_disk` bekommt `const struct uft_format_plugin *plugin`
(**angehängt**, nicht eingefügt — der Struct ist public, das Layout ist ABI).
`uft_disk_open()` hatte den Zeiger die ganze Zeit in der Hand und warf ihn weg;
jetzt behält es ihn.

Der schwerste Einzelfall war `uft_disk_close()`: es suchte das Plugin über
`disk->format` und rief dessen `close()` auf `disk->plugin_data` auf — Speicher,
den ein **anderes** Plugin alloziert hatte. Ein Free auf fremdem Speicher, der
nur deshalb nie aufschlug, weil die Registry leer war (ARCH-9).

| Gruppe | Stellen | Lösung |
|---|---|---|
| Diskette in der Hand | 11 | `uft_disk_plugin(disk)` |
| nur eine Format-ID | 3 | `uft_resolve_format_plugin(id, pfad_hinweis, &kandidaten)` |
| Aufrufer weiß es | neu | `uft_get_format_plugin_by_name()`, `uft_disk_convert_as()`, `uft_disk_convert_check_by_name()` |

`uft_disk_plugin()` rät nicht: für eine handgebaute Diskette ohne Plugin-Eintrag
wird die Format-ID **nur** verwendet, wenn genau ein Plugin sie führt. Sonst
NULL — der Rückgabewert bekommt gleich `plugin_data` in die Hand.

`uft_resolve_format_plugin()` ist eine Leiter, die auf jeder Sprosse ablehnen
kann: eindeutige ID → Extension des Zielpfads unter den Plugins dieser ID →
nichts. Nie ein First Match. Dass hier eine Extension entscheidet und
`uft_smart_open()` genau diese Rückfalllinie in MF-444 verloren hat, ist kein
Widerspruch: dort wurde der Name einer **existierenden** Datei zur Aussage über
ihren Inhalt gemacht; hier existiert die Datei noch nicht und der Aufrufer
**wählt** — „out.d81" ist eine Absichtserklärung, und genau das ist ein
Zielformat.

Nachweis: `tests/test_plugin_identity.c`. Registriert D64 **vor** D81 (damit ein
First Match garantiert falsch liegt), öffnet ein D81 und prüft, dass die
Diskette ihr eigenes Plugin kennt — inklusive der Assertion
`uft_disk_plugin(disk) != uft_get_format_plugin(disk->format)`, dem Bug als
Vergleich geschrieben. Ohne den Fix rot (verifiziert), mit ihm grün.

---

### PRINC-3 — `uft_advanced_read_track()` buchte jeden beschädigten Sektor als „recovered", ohne etwas zu tun (2026-08-21, MF-444) → ✓ BEHOBEN

Dritter Fund derselben Klasse, im Schwestermodul von `uft_smart_open()`.

`src/core/uft_advanced_mode.c:385-425`: bei aktivem God-Mode wurde ein
Kalman-Filter konfiguriert und initialisiert — in zwei Stack-Variablen, die
danach verworfen wurden, unter dem Kommentar „... process flux data through
Kalman ...". Der CRC-Korrektur-Zweig war ein leeres Kommentar-Paar. Danach:

```c
q.god_mode_used = true;
...
if (use_god_mode && q.has_errors) {
    handle->recovered_sector_count += q.error_count;
    q.recovered_bits = q.error_count * 8;   /* Estimate */
}
```

Gelesen wurde die Spur unverändert über den normalen Handler. `error_count`
zählt die **defekten** Sektoren — die wurden eins zu eins als wiederhergestellt
verbucht und von `uft_advanced_get_stats()` als `recovered_sectors` und
`crc_corrections` veröffentlicht. Je kaputter die Diskette, desto größer der
behauptete Rettungserfolg.

**Fix:** Die Anforderung wird protokolliert (`UFT_WARN`, ausdrücklich: „no
recovery algorithm is implemented on this path"), die Spur normal gelesen,
nichts gezählt. `recovered_sectors` meldet jetzt 0 — und 0 ist wahr.

---

### ARCH-8 — Handgeschriebene `extern`-Deklarationen: drei falsche in einer Datei, eine mit Schreibzugriff auf eine beliebige Adresse (2026-08-21, MF-442) → ✓ KLASSE LEER, Wächter steht

**Correctness, Speichersicherheit.** Gefunden beim Untersuchen der
`parser_v3`-Schiene. `src/formats/uft_v3_bridge.c` deklarierte:

```c
extern bool scp_detect_protection(const struct scp_disk*, char*, size_t);
```

Die Definition in `uft_scp_parser_v3.c` nimmt **vier** Parameter — der vierte
ist ein `float *confidence`, den die Funktion direkt nach dem Null-Check
dereferenziert:

```c
bool scp_detect_protection(const scp_disk_t* disk, char* protection_name,
                           size_t name_size, float* confidence) {
    if (!disk) return false;
    *confidence = 0.0f;        /* <- schreibt durch das vierte Argument */
```

Bei einem Aufruf mit drei Argumenten steht dort, was zufällig im vierten
Argumentregister liegt. **Ein Schreibzugriff auf eine beliebige Adresse.**

Dasselbe galt für `d64_detect_protection` und `g64_detect_protection`, und für
`g64_export_d64`: zwei Parameter deklariert, drei definiert — der dritte ein
`bool include_errors`, auf den die Funktion verzweigt. Das exportierte D64
bekam eine Fehlerkarte oder nicht, je nach Registerinhalt.

**Warum das niemand sah.** Die v3-Parser haben **keine Header**; ihre Typen
liegen in den `.c`-Dateien. Die Brücke konnte also nichts einbinden und schrieb
die Prototypen von Hand. Eine lokale `extern`-Deklaration ist ein Versprechen,
das der Compiler glaubt — er hat keine zweite Quelle, gegen die er prüfen
könnte.

Der Kontrast im selben Baum ist deutlich: `uft_smart_open.c` bindet
`uft_v3_bridge.h` ein und wurde geprüft; `uft_advanced_mode.c` deklarierte
dieselben drei Funktionen ebenfalls von Hand und wurde es nicht. Beim Korrigieren
der Header-Signatur schlug prompt nur die zweite Datei fehl — die
Hand-Deklarationen dort sind jetzt durch den `#include` ersetzt.

**Reichweite, ehrlich:** weder `uft_smart_open()` noch `uft_advanced_open()`
hat heute einen Aufrufer. Es war eine geladene Falle, keine laufende
Fehlfunktion — dieselbe Lage wie bei mehreren Funden dieser Reihe.

**Mitgenommen, mit Einschränkung.** Die v3-Parser melden eine Konfidenz je
Schema (0,85 für C64-Weak-Bits, 0,80 für Amiga-Long-Tracks, 0,75 generisch).
Die Brücke verwarf sie wegen der falschen Signatur, und `uft_smart_open.c:246`
setzte stattdessen `prot->confidence = 80`. Sie wird jetzt durchgereicht.

*Korrektur an der ersten Fassung dieses Eintrags:* dort stand, der Parser
„berechne" die Konfidenz. Das stimmt nicht — es sind **handvergebene
Konstanten je Zweig** (`*confidence = 0.85f;` direkt im `if`). Eine Konstante
je Schema ist besser als eine globale Konstante für alle, aber gemessen ist
auch sie nicht. Der Unterschied ist in diesem Projekt keine Wortklauberei.

**Wächter.** `scripts/extern_decl_conflicts.py`, verdrahtet als 15. Kategorie
in `check_consistency.py`. Er vergleicht **Parameterzahlen**, nicht
Typschreibweisen — `size_t` gegen `unsigned long` oder ein Typedef gegen die
zugrundeliegende Struktur sind legitime Unterschiede, eine abweichende
Parameterzahl ist keiner. Geprüft werden nur `extern`-Deklarationen in
`.c`-Dateien (dort wohnen die handgeschriebenen Versprechen) und nur Namen mit
**genau einer** Definition im Baum.

Nach den drei Korrekturen: **0 Treffer.** Die Baseline ist leer — die Klasse
ist nicht verwaltet, sondern geschlossen.

*Offene Scope-Frage, nicht entschieden:* die gesamte v3-Kette hat keinen
lebenden Einstiegspunkt. `uft_smart_open.c` (0 Aufrufer),
`uft_advanced_mode.c` (0 Aufrufer), `uft_v3_bridge.c` (nur von diesen beiden
benutzt) und die drei `*_parser_v3.c` (nur von der Brücke). Zusammen mehrere
tausend Zeilen. Ob das eine noch nicht angeschlossene öffentliche API ist oder
Sediment, ist eine Produktentscheidung wie seinerzeit `src/switch/` — deshalb
hier notiert und nicht einseitig ausgeführt.

#### Was aus der v3-Kette übernehmenswert wäre: nichts (Prüfung 2026-08-21)

Vor jeder Entscheidung über die Kette die Frage, ob sie etwas kann, das der
produktive Baum nicht kann. Fähigkeit für Fähigkeit geprüft:

| v3-Fähigkeit | produktives Gegenstück | Urteil |
|---|---|---|
| Diagnose-Codes, ~20 (`SCP_DIAG_PLL_UNLOCK`, `_WEAK_BITS`, `_LONG_TRACK`, …) | `otdr_event_type_t` in `analysis/floppy_otdr.h`: 17 Ereignistypen **plus** 5 Schweregrade **plus** `otdr_sample_t` je Flusswechsel (Abweichung, Jitter-RMS, `quality_db`, `is_stable`) | OTDR ist echte Obermenge |
| Score-Zerlegung in 5 Teilwerte (flux/timing/consistency/decode/structure) | OTDR liefert `quality_db` **pro Sample** und Severity **pro Ereignis** | feiner aufgelöst |
| Multi-Revolution: „beste Umdrehung" wählen | `recovery/uft_multiread_pipeline.c`: Pro-**Byte**-Voting mit Confidence-Map und Weak-Flag | siehe unten |
| `d64_verify(original, written, differences)` | `uft_disk_verify()` über `uft_disk_t`, also **formatunabhängig**, plus `uft_sector_compare` mit GUI-Dialog | allgemeiner |
| `scp_detect_weak_bits` | Weak-Bit-Maschinerie aus PROT-12 | vorhanden |
| Konfidenz je Schutzschema | handvergebene Konstanten (s. o.) | kein Vorteil |

**Der Punkt, der über „haben wir auch" hinausgeht:** v3 wählt die *beste*
Umdrehung und verwirft die übrigen. Der produktive Weg stimmt **pro Byte** über
alle Umdrehungen ab und leitet daraus zusätzlich eine Confidence-Map und
Weak-Bit-Erkennung ab. Eine Umdrehung auszuwählen wirft genau die Information
weg, aus der sich Weak Bits überhaupt erst erkennen lassen — bei einem
forensischen Werkzeug ist das kein Geschmacksunterschied, sondern der
Unterschied zwischen „Diskette hat instabile Bits" und „Lesefehler".

`d64_verify` ist zusätzlich D64-spezifisch, während `uft_disk_verify()` über
die Plugin-Schnittstelle arbeitet und damit für jedes der 88 Formate gilt.

**Ergebnis:** aus der v3-Kette ist nichts zu übernehmen. Sie ist keine
Fundgrube, sondern eine ältere, gröbere Parallelentwicklung derselben Ideen —
dieselbe Form wie die sechs D64- und fünf SCP-Leser, nur eine Abstraktionsebene
höher. Das entkoppelt die Scope-Frage von der Substanzfrage: was auch immer mit
der Kette geschieht, es geht dabei keine Fähigkeit verloren.

### ARCH-6 — Zwei parallele Formatlayer: der Konverter kennt die Plugin-Registry nicht (2026-08-20, MF-433) → ◐ ERSTE NAHT GESCHLAGEN

**Architecture.** Gemessen, nicht vermutet:

```
grep "uft_get_format_plugin|->read_track" src/formats/uft_format_convert*.c
→ nichts
```

`dispatch_conversion()` (`src/formats/uft_format_convert_dispatch.c:107`) ist
eine handgeschriebene Paar-Kette — `if (src == X && dst == Y) return
convert_x_to_y(...)` — und jede dieser Funktionen benutzt einen **eigenen**
Lader (`d64_load_buffer`), nicht `uft_format_plugin_d64`. Es gibt also zwei
Formatlayer nebeneinander: 88 Plugins für Probe und GUI, rund zwanzig
handverdrahtete Paare für die Konvertierung, ohne gemeinsamen Code.

Was das kostet, in Zahlen:

| | |
|---|---|
| D64-Leser im Build | **6** + Plugin |
| SCP-Leser | **5+** |
| IMD-Leser | 3 — einer las das Spurformat falsch (FMT-15) |
| 1541-Zonentabelle hartkodiert in | **8 Dateien** |
| Tests, die `uft_convert_file()` aufrufen | **0** |

Und es erklärt Ältereres: warum ein in einem Leser behobener Fehler in den
anderen fünf stehen bleibt, und warum ein T1b-Tier wenig aussagt — das Tier
zertifiziert den **Plugin**-Pfad, die Konvertierung läuft über einen anderen
Leser.

**Erste Naht (MF-433).** `uft_cbm_g64_encode_via_plugin()` benutzt denselben
GCR-Encoder wie `d64_to_g64()`, bezieht seine Sektoren aber aus
`plugin->read_track()`. Belegt durch `test_convert_via_plugin`:

- beide Wege liefern **bitidentisches** G64 für `vice_c1541_35trk.d64`
  (35 Spuren, 683 Sektoren, jede Spur byteweise verglichen),
- die Disk-ID kommt nachweislich aus der BAM und nicht aus dem Default —
  sie geht in jede Sektorkopf-Prüfsumme ein, ein falscher Wert verdirbt das
  ganze Abbild strukturell unauffällig,
- **D67 geht durch denselben Encoder**, obwohl es nie eine Paar-Funktion
  dafür gab: 35 Spuren, alle **690** Sektoren, inklusive der Zonen mit 20
  statt 19 Sektoren, die D67 gerade von D64 unterscheiden.

`uftc_convert_d64_to_g64()` läuft jetzt produktiv über den Plugin-Pfad, wenn
ein Quellpfad vorliegt.

**Verhaltensänderung, ausdrücklich:** der Plugin-Pfad erkennt 41- und
42-Spur-Abbilder, der Blob-Lader deckelt bei 40 und lässt den Rest still
fallen. Solche Abbilder konvertieren jetzt vollständig. Es gibt kein
41/42-Spur-Referenzabbild im Korpus — dieser Pfad ist begründet, nicht
getestet.

#### Zweite Naht: G64 → D64, die Dekodierrichtung (2026-08-20, MF-436)

Dieselbe Umstellung rückwärts, mit einem Unterschied im Vorgehen: die
Spur-Extraktion aus `g64_to_d64()` wurde **herausfaktorisiert**
(`gcr_track_to_sectors()`), nicht kopiert. Beide Wege benutzen jetzt denselben
GCR-Dekoder — eine zweite Kopie wäre genau die Krankheit, deretwegen dieser
Umbau stattfindet.

Belegt durch `test_convert_via_plugin`:

- Blob- und Plugin-Pfad liefern **bitidentische** Sektordaten für
  `vice_c1541_35trk.g64` (683 Blöcke verglichen, Disk-ID `42` aus der
  GCR-BAM-Spur auf beiden Wegen),
- und der **vollständige Roundtrip D64 → G64 → D64 gibt die Ausgangsdiskette
  zurück**: 683 Sektoren, 0 Prüfsummenfehler, **0 abweichende Bytes** von
  174 848. Das ist die stärkere Aussage als jede Hälfte für sich — Enkodieren
  und Zurücklesen prüft Sektorköpfe, Prüfsummen, Syncmarken, Gap-Behandlung
  und die Zonentabelle in einem Durchgang. „Kein Bit verloren" als Assertion.

**Fund dabei: `geometry.cylinders` bedeutet nicht überall dasselbe.**

| Container | `geometry.cylinders` | bedeutet |
|---|---|---|
| D64 (feste Belegung) | 35 | die **Ausdehnung** — Größe *ist* Inhalt |
| G64 (Kapazitätskopf) | 42 | die **adressierbare Reichweite** |

Das Referenz-G64 deklariert 84 Slots, belegt sind 35. Der erste Entwurf des
Plugin-Dekoders vertraute `cylinders` und erzeugte daraus ein 40-Spur-D64 aus
einer 35-Spur-Diskette — **85 Blöcke Füllmaterial, ausgegeben als
wiederhergestellte Sektoren**. Genau die erfundenen Daten, die Prinzip 1
verbietet. Der alte Blob-Pfad hatte das richtig gemacht, indem er auf Inhalt
prüfte; der Plugin-Pfad tut das jetzt auch. Ein eigener Testfall nagelt die
Unterscheidung fest, damit sie nicht ein drittes Mal jemanden erwischt.

#### Dritte Naht: HFE → ADF — hier war der Konverter nicht doppelt, sondern falsch (2026-08-20, MF-437) → ✓ BEHOBEN

**Correctness, Fabrikationsklasse.** Die ersten beiden Nähte fanden
Duplikate. Diese fand etwas anderes.

`uftc_convert_hfe_to_sectors()` setzte für `dst_format == UFT_FORMAT_ADF`
lediglich `sectors = 11` und dekodierte dann weiter mit **IBM-System-34-
Struktur**: drei aufeinanderfolgende `0x4489`-Syncs, IDAM `0xFE` mit C/H/R/N,
DAM `0xFB` vor den Daten. AmigaDOS hat davon nichts — zwei Syncs je Sektor,
danach ein odd/even-getrenntes Info-Langwort, OS-Label, Kopf- und
Datenprüfsumme, 512 Byte. Kein IDAM, kein DAM, **nie drei Syncs am Stück**.

Gemessen an `tests/corpus_free/gw_amigados.hfe`, Spur 0 Seite 0:

| | |
|---|---|
| `0x4489`-Syncs | **22** = 11 Sektoren × 2, die AmigaDOS-Signatur |
| längster Sync-Lauf | **1** — die Schleife verlangt ≥ 3 |
| IDAM (`0xFE`) | **0** |
| DAM (`0xFB`) | **0** |
| Bytes nach dem 1. Sync | `44 89 55 2A AA A5` — zweiter Sync, dann odd/even-Info |

Die Extraktionsschleife konnte also **kein einziges Mal auslösen** — geprüft
über alle 160 Spurseiten, nicht nur über eine. Der Ausgabepuffer ist
`calloc`'d, `result->tracks_converted++` lief **unbedingt** je Kopf, und
`result->success = true`, sobald die Datei geschrieben war.

**Ergebnis: eine 880-KB-Datei aus Nullen, gemeldet als 160 konvertierte
Spuren.** Das ist keine fehlgeschlagene Konvertierung, das ist eine
erfundene — genau das, was Prinzip 1 verbietet.

*Nicht end-to-end nachgestellt:* ein direkter Aufruf von
`uftc_convert_hfe_to_sectors()` scheiterte an der Link-Fläche des
Konverter-Clusters (dieselbe Ursache wie Punkt 2 unten). Der Beweis steht
über die Messung am echten Abbild plus den Code: die Bedingung `sync_count
>= 3` ist auf keiner der 160 Spurseiten erfüllbar.

**Der richtige Dekoder war die ganze Zeit da.** `decode_amiga_sector()` in
`src/flux/uft_flux_decoder.c` behandelt das Odd/Even-Schema korrekt, mit
einem sorgfältigen Kommentar darüber, dass hier **kein** Clock-Strip-Schritt
angewendet werden darf. Der Konverter kannte ihn nicht — er hatte seinen
eigenen.

*Behoben:* die Bitstrom-Hälfte ist als `flux_decode_amiga_bits()`
herausfaktorisiert — faktorisiert, nicht kopiert, es bleibt genau ein
AmigaDOS-Sektordekoder. Ein HFE speichert *rückgewonnene Zellen*, keinen
Flux, also braucht dieser Weg keine PLL. Gelesen wird über
`uft_format_plugin_hfe`, das De-Interleave und Bit-Reverse ohnehin schon
macht — beides hatte der Konverter danebenstehend nachgebaut.

**Beleg:** `gw_amigados.hfe` ist greaseweazles MFM-Kodierung von
`xdftool_dd_ofs.adf` (beide getrackt, T1b). Der korrigierte Weg liefert
**alle 1760 Sektoren, null Prüfsummenfehler, 0 abweichende Bytes von
901 120** — die Ausgangs-ADF, Byte für Byte.

Nebenbei ehrlich gemacht: `tracks_converted` zählt jetzt nur noch Spuren, die
tatsächlich Sektoren geliefert haben, und `sectors_converted` wird auf diesem
Pfad überhaupt erst gesetzt.

#### Vierte Naht: SCP → ADF — und ein SCP-Plugin, das die halbe Umdrehung lieferte (2026-08-20, MF-438)

Diese Naht schließt die Kette: das Korpus-SCP ist greaseweazles **Flux**-Aufnahme
derselben `xdftool_dd_ofs.adf`, aus der auch das HFE stammt. Flux, Zellen und
Sektoren — drei Darstellungen einer Diskette, gegeneinander prüfbar.

**Derselbe Amiga-als-IBM-Defekt wie MF-437, eine Ebene tiefer.**
`uftc_convert_scp_to_mfm_sectors()` legt eine PLL über den Flux und reicht den
Bitstrom an `uft_mfm_decode_track()` — laut eigenem Kommentar „the canonical
MFM IDAM/DAM parser". Auch für ADF. Belegt: derselbe Track liefert dem
IBM-Dekoder **0** Sektoren und dem Amiga-Dekoder **11**.

Eine Zeile daneben sagt die Verwirrung laut:

```c
double data_rate = (dst_format == UFT_FORMAT_ADF) ? 500000.0 : 500000.0;
```

Ein Ternär mit identischen Zweigen.

**Der schwerere Fund: das SCP-Plugin gab jede Spur als halbe Umdrehung zurück.**

`scp_read_revolution_flux()` behandelte das `length`-Feld des
Revolution-Eintrags als Byte-Zahl und leitete `length / 2` Flusswerte daraus
ab. `length` zählt **16-Bit-Flusswerte**. Gemessen an
`tests/corpus/gw_amigados.scp`, Spur 0:

| | vorher | nachher |
|---|---|---|
| Flusswerte | 25 263 | **50 526** |
| Summe der Intervalle | 100,05 ms | **200,00 ms** |
| eigene Dauerangabe der Spur | 200,00 ms bei 300 rpm | unveraendert |
| dekodierte AmigaDOS-Sektoren | 5 von 11 | **11 von 11** |

Die Dateistruktur bestätigt es unabhängig: rev0-Daten beginnen bei `0x1C`,
rev1 bei `0x18AD8` — rev0 umfasst also 101 052 Bytes = 50 526 × 2.

Das ist die gefährlichste Sorte Fehler in diesem Werkzeug: **still**. Es kam
kein Fehler, keine Warnung, es kamen Sektoren — nur eben die Hälfte. Wer eine
Diskette mit 5 von 11 lesbaren Sektoren sichert, hält eine beschädigte
Diskette in der Hand, nicht einen Lesefehler im Werkzeug.

`src/flux/uft_scp_parser.c:376` hatte es die ganze Zeit richtig
(`flux_count = rev_info.track_length`). Wieder zwei Leser, einer korrekt, und
welcher läuft, hängt am Aufrufweg.

**Beleg nach der Korrektur:** alle 1760 Sektoren, null Prüfsummenfehler,
**0 abweichende Bytes von 901 120** — die Ausgangs-ADF, Byte für Byte, aus
echtem Flux mit echter Laufwerks-Jitter durch eine echte PLL.

`test_convert_scp_adf` nagelt die Vollständigkeit ohne externe Referenz fest:
die Summe der Intervalle muss der Umdrehungsdauer entsprechen, die die Spur
selbst mitbringt. Der Test **überspringt** (Exit 77) ohne das 32-MB-Abbild,
das gitignored ist — CI kann ihn nicht fahren, lokal ist er reproduzierbar.

**Drittes Auftreten der Semantik-Falle:** `uft_track_t::flux` sind ns-**Intervalle**,
`flux_raw_data_t::transitions` sind kumulative **Zeiten**. Die Umrechnung stand
handgeschrieben in `tests/differential/uft_flux_decode.c` (mit erklärendem
Kommentar). Sie hat jetzt eine Stelle: `flux_raw_from_ns_intervals()`.

#### Die halbe-Umdrehung-Klasse zu Ende geprüft: einer von fünf (2026-08-20, MF-439) → ✓ GESCHLOSSEN

Nach MF-438 stand die Frage, wie viele der **fünf** SCP-Leser denselben Fehler
machen. Alle fünf gelesen:

| Leser | Deutung von `length` | |
|---|---|---|
| `src/flux/uft_scp_parser.c:376,518` | Flusswerte (beide Pfade) | ✓ |
| `src/formats/scp/uft_scp_multirev.c:670` | Flusswerte | ✓ |
| `src/formats/scp/uft_scp_parser_v3.c:1205` | `length * 2` Bytes | ✓ |
| `src/formats/scp/uft_scp_reader_v2.c:806` | `length * 2` Bytes | ✓ |
| `src/formats/scp/uft_scp_plugin.c:116` | `length / 2` | ✗ (MF-438) |

Einer von fünf. Aber Lesen ist kein Beweis, und fünf Leser eines Formats sind
eine stehende Einladung an den nächsten, abzudriften. `test_scp_readers_agree`
macht aus dem Lesen eine Behauptung:

1. **Plugin und Parser liefern identischen Flux** — nicht „beide plausibel",
   sondern Wert für Wert gleich, über sechs Spuren quer über die Diskette
   (0, 1, 40, 79, 158, 159). Zwei Leser, die beide bei der halben Umdrehung
   abschneiden, würden ein Plausibilitätskriterium bestehen; Gleichheit nicht.
2. **Der Anker, den kein Leser fälschen kann:** die Summe der Intervalle muss
   der Umdrehungsdauer entsprechen, die die Spur selbst mitbringt. Pro Spur
   geprüft, nicht einmal — eine längenabhängige Kürzung würde sonst auf der
   kürzesten Spur durchrutschen.
3. **Datei- gegen Speicherpfad** in `uft_scp_parser.c`: zwei unabhängige
   Implementierungen desselben Lesevorgangs, eine Antwort.

**Gegenprobe gemacht.** Gegen den Stand vor MF-438 schlägt der Test fehl, mit
genau der Diagnose, die man sich wünscht:

```
plugin_and_parser_return_identical_flux   FAIL: pt.flux_count == rev->flux_count
the_flux_spans_a_whole_revolution         track 0: flux spans 100.05 ms of 200.00 ms (50 %)
```

*Nebenbefund, nicht behoben:* `uft_scp_parser_v3.c` und `uft_scp_reader_v2.c`
enthalten je einen Selbsttest-Block hinter `#ifdef SCP_V3_TEST` bzw.
`#ifdef SCP_READER_TEST` — zusammen **238 Zeilen mit `main()` und `assert()`**,
die **kein Build je definiert** (geprüft über `.pro`, CMake und die
Workflows). Assertions, die nie laufen, sind keine Tests; sie sehen nur so
aus. Gehört zur Aufräumfrage der fünf Leser, nicht hierher.

#### Von fünf SCP-Lesern auf drei (2026-08-20, MF-440)

Nachdem `test_scp_readers_agree` bewiesen hatte, dass die Leser dasselbe
liefern, war die Zusammenführung risikoarm — das war der Sinn der Reihenfolge.
Die Konsumenten-Messung ergab dann etwas Einfacheres als erwartet: **zwei der
fünf hatten überhaupt keine Aufrufer.**

| Leser | Zeilen | Befund |
|---|---|---|
| `src/flux/uft_scp_parser.c` | 675 | kanonisch — 15 Konsumentendateien, GUI, HAL-Provider, Tests, ein Python-Werkzeug |
| `src/formats/scp/uft_scp_parser_v3.c` | 1765 | lebt: `uft_v3_bridge.c` → `uft_smart_open.c` / `uft_advanced_mode.c`, Kopierschutz-Erkennung |
| `src/formats/scp/uft_scp_plugin.c` | 478 | lebt über die Plugin-Registry |
| `src/formats/scp/uft_scp_multirev.c` | 859 | **null Aufrufer** → entfernt |
| `src/formats/scp/uft_scp_reader_v2.c` | 991 | **null Aufrufer** → entfernt |

**Warum die erste Messung falsch war.** Ein naiver Symbolabgleich meldete für
`uft_scp_reader_v2.c` Aufrufer in `uft_scp_plugin.c` und `uft_hal_unified.c`.
Beide definieren aber **eigene statische** `scp_open()`/`scp_close()` mit
völlig anderen Signaturen; ebenso ist `scp_disk_type_name` in
`uft_scp_parser_v3.c` dessen eigene statische Funktion. Drei Fehltreffer, alle
aus derselben Ursache: generische Namen in einem Baum mit fünf Implementierungen
eines Formats. Genau die „Symbol-Rausch"-Stufe der Löschbeweispipeline aus
MF-369 — ohne sie hätte ich hier nichts gelöscht und die falsche Begründung
notiert.

**Beweispipeline vollständig durchlaufen:** keine Header (also kein
`#include`-Konsument möglich), unbedingt in der `.pro` gelistet, keine
Skript-/Werkzeug-Referenz, keine Selbstregistrierung per Makro oder
Konstruktor, alle Symboltreffer als Kollisionen widerlegt, Abnahme-Build grün.

**1850 Zeilen entfernt.** Rückholbar über den Tag
`archive/pre-mf440-scp-readers`.

*Nicht gelöscht und warum:* `uft_scp_parser_v3.c` sieht mit 1765 Zeilen nach
dem größten Brocken aus, hängt aber an der Kopierschutz-Erkennung, die
`uft_smart_open.c` beim automatischen Formaterkennen aufruft. Das ist ein
lebender Pfad; ihn zusammenzuführen ist eigene Arbeit, kein Aufräumen
nebenbei.

#### Von sechs D64-Lesern auf fünf, davon drei mit klarer Rolle (2026-08-20, MF-441)

Dieselbe Methode wie MF-440, bessere Beweislage: das D64-Korpusabbild ist
getrackt, also könnte ein Übereinstimmungstest hier in CI laufen.

| Leser | Zeilen | Befund |
|---|---|---|
| `src/formats/c64/uft_d64_g64.c` | 1470 | kanonisch für Konvertierung — 14 Konsumenten, 27/30 Funktionen benutzt |
| `src/formats/d64/uft_d64_parser_v3.c` | 1682 | lebt über `v3_bridge` (Schutzerkennung) |
| `src/formats/c64/uft_d64_file.c` | 849 | Dateisystem-Ebene, nicht Container — GUI-Explorer + Tests |
| `src/formats/uft_d64_writer.c` | 435 | **3 von 18** Funktionen benutzt |
| `src/formats/d64/uft_d64_plugin.c` | 184 | lebt über die Plugin-Registry |
| `src/formats/commodore/d64.c` | 264 | **null Aufrufer** → entfernt |
| `src/formats/commodore/uft_d64_view.c` | 197 | **null Aufrufer** → entfernt |
| `src/formats/d64/uft_d64_parser_v2.c` | 788 | **alles `static`**, `main()` hinter nie definiertem `#ifdef` → entfernt |

**Der interessanteste Fall ist `uft_d64_parser_v2.c`.** 788 Zeilen, 21
Funktionen — *alle* `static`, dazu ein `main()` hinter `#ifdef
D64_PARSER_TEST`, das kein Build je definiert. Die Übersetzungseinheit trägt
also **nichts** zur Binärdatei bei: kein linkbares Symbol, keine Registrierung,
kein Konstruktor. Sie wurde bei jedem Build kompiliert und wieder weggeworfen.

**Eine Falle unterwegs.** `include/uft/formats/d64.h` wurde von
`src/formats/uft_format_registry_v2.c` eingebunden — auf den ersten Blick ein
lebender Konsument der Registry-Tabelle, also genau die Sorte Referenz, die
kein Aufrufsite ist (MF-423). Nachgesehen: der Header deklariert **nur**
`D64_SECTOR_SIZE` und `D64_MAX_TRACKS`, beide auch in
`formats/c64/uft_d64_g64.h`, und `registry_v2.c` benutzt keine von beiden.
Der Registry-Eintrag ist ein reiner String (`{"D64", "d64", …}`), keine
Funktionszeiger. Include und Header entfernt.

Ebenso geprüft und widerlegt: alle fünf `uft_cbm_d64_*`-Funktionen aus
`commodore/d64.c` haben null Referenzen im ganzen Baum — die einzigen Treffer
auf das Präfix sind `uft_cbm_d64_decode_via_plugin` aus MF-436, ein anderer
Name.

**1249 Zeilen entfernt.** Rückholbar über `archive/pre-mf441-d64-readers`.

*Offen:* `uft_d64_writer.c` benutzt 3 von 18 exportierten Funktionen. Eine
teil-tote Datei ist kein Löschkandidat, aber die 15 ungenutzten gehören
angesehen. Und `uft_d64_parser_v3.c` hängt wie sein SCP-Gegenstück an der
Schutzerkennung von `uft_smart_open.c` — derselbe lebende Pfad, dieselbe
eigene Aufgabe.

**Offen, mit Grund:**

1. **Plugins können nicht aus dem Speicher öffnen.** `uft_format_plugin_t`
   hat nur `open(disk, path, ro)`. `uft_convert_memory()` hat keinen Pfad und
   bleibt deshalb auf dem Blob-Lader. Sauber wird das erst mit einem
   additiven `open_memory()` hinter dem vorhandenen `api_version`-Gate.
2. **`uft_convert_file()` hat keinen einzigen Test.** Die öffentliche
   Konvertier-API mit 45 dokumentierten Pfaden ist end-to-end ungeprüft; die
   Link-Fläche dafür ist heute der ganze Konverter-Cluster. Sie schrumpft mit
   jedem Paar, das auf die Plugins wandert — deshalb erst die Naht, dann der
   Test.
3. Die übrigen ~19 Paar-Funktionen und die fünf überzähligen D64-Leser. Vor
   dem Löschen die Beweispipeline aus MF-369, nicht Augenmaß.

#### Zwei Speicherfehler, die die Naht ans Licht gebracht hat (MF-433)

Beide fielen an, weil der neue Pfad `uft_track_free()` benutzte — also das
tat, was die API nahelegt.

**`uft_track_free()` gibt die Struktur selbst frei.** Die Funktion endet mit
`free(track)`. **22 von 29 Aufrufstellen übergaben eine Stack-Adresse**, davon
**18 in `src/core/uft_format_verify.c`** — dem generischen `verify_track`,
das Plugins in ihrer Struct verdrahten. `free()` auf einen Stack-Zeiger; im
Test sofort als `0xC0000374` (Heap-Korruption) sichtbar.

Dass es bisher nicht knallte, liegt an einem zweiten Defekt: `uft_track_init()`
nullt alles, also ist `owns_data` **false**, und der interessante Teil von
`uft_track_free()` wurde übersprungen — jede Plugin-Spur leckte ihre Sektoren.
Genau deshalb geben 52 Dateien die Innereien von Hand frei; zwei
Roundtrip-Tests tragen sogar Kommentare, die die Falle erklären statt sie zu
beheben.

*Behoben:* `uft_track_release()` gibt den Inhalt frei und lässt die Struktur
stehen, `uft_track_free()` ist beides plus `free(track)` und gilt nur noch für
Heap-Spuren. `uft_format_add_sector()` setzt `owns_data`, damit eine Spur
besitzt, was sie kopiert hat. 21 Stack-Aufrufstellen umgestellt. Die drei
Heap-Stellen benutzen `uft_track_alloc()` und bleiben korrekt.

**`uft_track_alloc()` war mit einer Signatur deklariert, die es nie gab.**
`uft_track.h` sagte `(uint32_t layers, size_t bit_count)`, die einzige
Definition (`uft_unified_types.c:224`) nimmt `(size_t max_sectors, size_t
max_raw_bits)`, und alle vierzehn Aufrufer übergeben eine Sektorzahl. Wer
`uft_track.h` einband und die Funktion aufrief, hätte Layer-Flags an eine
Sektorkapazität übergeben. Die falsche Deklaration überlebte, weil keine
Übersetzungseinheit beides tat — die erste, die es tat, brach den Build, und
so wurde sie gefunden. Dieselbe Form wie ARCH-5, eine Ebene höher: ein Fakt,
drei Header, einer davon falsch.

### ARCH-5 — Namen, die Enum-Konstante *und* Makro sind: die Klasse hinter MF-427 (2026-08-19, MF-431) → ✓ LIVE-FÄLLE BEHOBEN, Wächter steht

**Correctness, systemisch.** MF-427 war kein Einzelfall, sondern eine Form.
Die Frage danach lautete nicht „ist er behoben", sondern „wie viele weitere
Konstanten haben dasselbe Muster" — und die ließ sich beantworten, weil der
Mechanismus mechanisch suchbar ist: jedes `#ifndef`/`#ifdef`, dessen Symbol in
einem `enum` steht. Der Präprozessor sieht Enum-Konstanten nicht, also greift
so ein Guard nie, und das Makro gewinnt lautlos.

**Gemessen** (`scripts/enum_macro_conflicts.py`): 1843 Dateien, 6163
Enum-Konstanten, 10682 Makros, 4695 Guards. Davon **33 Guards auf einem
Enum-Symbol** und **51 Namen, die als Enum und als Makro mit verschiedenen
Werten existieren**.

Entscheidend ist nicht die Existenz, sondern ob eine Übersetzungseinheit beide
Seiten erreicht. Die Include-Hüllen aller 980 TUs, gegen den Stand **vor**
MF-427 gegengeprüft, damit das Werkzeug erst den bekannten Fall findet, bevor
ich seinen Zahlen glaube:

| | vor MF-427 | nach MF-427/428 | jetzt |
|---|---|---|---|
| LIVE | **9** | 5 | **0** |
| LATENT | 47 | 46 | 46 |

Die neun Live-Fälle vor MF-427 schlossen `UFT_ENC_GCR_CBM` mit **49**
Übersetzungseinheiten ein — das ist der Fund, der zufällig über einen
G71-Track auffiel.

**Die fünf, die nach MF-427 noch standen, und was sie waren:**

1. **`UFT_ENCODING_FM` / `_MFM` / `_RAW`** (6 TUs) — genau der Fall, den die
   Rückfrage vorhergesagt hat. `uft_flux_pll.h` deklarierte den Encoding-Enum
   lokal und schrieb zwölf Zeilen darunter

   ```c
   #ifndef UFT_ENCODING_FM
   #define UFT_ENCODING_FM   UFT_ENC_FM
   #endif
   ```

   — geprüft gegen die Enum-Konstante **in derselben Datei**. Der Guard konnte
   nicht greifen, das Makro überschattete sofort den Enum, den es gerade
   eingeführt hatte (Enum FM = 0, Makro `UFT_ENC_FM` = 1). Dazu
   `uft_god_mode.h` mit denselben Namen und einer dritten Nummerierung
   (MFM = 0, was in `uft_types.h` `UFT_ENC_UNKNOWN` ist).

   *Warum es trotzdem nichts kaputt machte:* `uft_pll_init()` ist `static
   inline`, Argument und Vergleich (`encoding == UFT_ENCODING_MFM`) werden in
   derselben Übersetzungseinheit expandiert, und alle sechs Aufrufstellen
   übergeben das Makro als Literal statt eines gespeicherten Werts. Selbst-
   konsistent — bis das erste `track->encoding` aus dem Formatlayer in diesen
   Vergleich läuft. Genau die Grenze, die latent existiert und beim nächsten
   `#include` scharf wird.

   *Behoben:* `uft_flux_pll.h` bindet `uft_types.h` ein und deklariert nichts
   Eigenes mehr; die God-Mode-Konstanten heißen `UFT_GODMODE_ENC_*` und sagen
   damit, dass sie der Parameterraum **einer API** sind und nicht die
   Encoding-Nummerierung des Formatlayers.

2. **`UFT_PLATFORM_UNKNOWN`** (3 TUs) — zwei Begriffe unter einem Namen: im
   Schutzlayer „die Zielplattform der **Diskette** ist unbekannt" (Enum, 0),
   in `uft_platform.h` „das **Build-Host-OS** ist keines der bekannten"
   (Makro, 1). Das Makro entsteht nur im `#else`-Zweig, also auf keiner
   Plattform, auf der wir bauen — auf einem unbekannten Host hätte jedes
   `= UFT_PLATFORM_UNKNOWN` im Schutzlayer still die Ordinalzahl 1 bedeutet.
   Das OS-Flag hatte **null Nutzer** (`grep`: kein einziges
   `defined(UFT_PLATFORM_UNKNOWN)`) und ist entfernt; `UFT_PLATFORM_NAME`
   trägt dieselbe Information.

3. **`UFT_IO_ERR_EOF`** (1 TU) — `uft_safe_io.h` deklariert es als Wert 5 des
   kleinen `uft_io_error_t`, die generierte Alias-Tabelle bildete denselben
   Namen auf den globalen Fehlercode −14 ab. Der `#ifndef` im Generat konnte
   den Enum nicht sehen, also gab `uft_safe_io.h:419` `return UFT_IO_ERR_EOF;`
   aus einer Funktion mit Rückgabetyp `uft_io_error_t` die −14 zurück. Zwei
   Fehlerräume, ein Name, null Nutzer. Zeile aus
   `data/errors_legacy_aliases.tsv` entfernt und neu generiert
   (`verify_errors_ssot: OK`).

**Wächter.** `scripts/enum_macro_conflicts.py`, verdrahtet als 14. Kategorie in
`check_consistency.py`. Ein **LIVE**-Fall ist nicht baselinebar und schlägt
immer fehl — dort entscheidet die Include-Reihenfolge über die Bedeutung eines
Namens, das ist nie „akzeptiert". Die 46 latenten stehen in
`scripts/enum_macro_baseline.json`; ein neuer Name schlägt fehl, ein
aufgelöster ebenfalls.

**Was der Wächter nicht kann, ausdrücklich:** Include-Hüllen folgen jedem
`#include`, auch in nicht genommenen `#if`-Zweigen. Das überschätzt, wer was
sieht — die sichere Richtung, denn es kann einen Live-Fall nicht verstecken.
Und Werte werden nur aufgelöst, soweit reines C es ohne Compiler zulässt;
alles andere zählt als unaufgelöst und wird nicht gemeldet, weil eine
Schätzung hier schlimmer wäre als eine Lücke.

**Die 46 latenten, kurz:** 25 stammen aus `uft_error_compat_gen.h` gegen den
Enum in `src/core/uft_error_codes.h` — einer **zweiten** Datei dieses Namens
neben `include/uft/core/uft_error_codes.h`, also ARCH-4-Material. Weitere sind
`FMT_*` (zwei Format-ID-Nummerierungen), `HFE_IF_*`, `UFT_IMD_SECTOR_*`. Alle
sind heute unerreichbar füreinander; keiner ist damit richtig.

### ARCH-2 Nachtrag — der Zähler übersah funktionsartige Makros (2026-08-19, MF-431)

`check_macro_conflicts()` verglich `#define` gegen `#define`, überging aber
alles mit Parameterliste als vermutetes Rauschen. In genau dieser Lücke saß
`UFT_PREFETCH` mit zwei verschiedenen Rümpfen, **und der Compiler meldete es
bei jedem Build**. Ein Prüfwerkzeug, das grün meldet, während der Compiler rot
schreibt, ist dieselbe Krankheit wie alles andere in dieser Reihe.

Der Zähler sieht jetzt beide Formen. Rümpfe werden mit normalisierten
Parameternamen verglichen — `#define F(x) ((x)+1)` und `#define F(a) ((a)+1)`
sind dasselbe Makro, eine Umbenennung ist kein Befund. Zwei neue Treffer:

- `UFT_ALIGNED` — fünffach parallel in `uft_common.h`, `uft_compiler.h`,
  `uft_config.h`, `uft_platform.h`, `uft_simd.h`. `platform-boilerplate`,
  hängt an ARCH-1, in die Baseline aufgenommen.
- `UFT_SECTOR_SIZE` — `(128U << (id)->size_code)` in
  `core/uft_unified_types.h` gegen schlichte `512` in
  `formats/uft_fat32_mbr.h`. Ein Akzessor und eine Konstante unter einem
  Namen; kein TU sieht beide, aber der erste, der es täte, bekäme entweder
  einen Syntaxfehler oder still die 512. Direkt umbenannt zu
  `FAT32_SECTOR_SIZE` (13 Stellen) statt aufgenommen.

### FMT-15 — Der IMD-Adapter las das Spurformat falsch und lief dabei über den Puffer (2026-08-19, MF-430) → ✓ BEHOBEN

**Correctness + Memory-Safety.** Gefunden beim Abarbeiten der ARCH-2-Gruppe
`format-constant`: `UFT_IMD_HEAD_MASK` stand mit 0x01 und 0x0F im Baum, was
zur Frage führte, was das Kopf-Byte eines IMD-Spurkopfes eigentlich bedeutet.

Zwei unabhängige Implementierungen sagen dasselbe:

| Quelle | Aussage |
|---|---|
| MAME `src/lib/formats/imd_dsk.cpp` | `offs += 5 + sector_num` — die Sektor-Nummernkarte wird **bedingungslos** übersprungen; danach `if(header[2] & 0x80)` Zylinderkarte, `if(header[2] & 0x40)` Kopfkarte |
| hharte/libimd `src/libimd.h` + `.c` | `IMD_HFLAG_CMAP_PRES 0x80`, `IMD_HFLAG_HMAP_PRES 0x40`; `smap` wird gelesen, **bevor** ein Flag geprüft wird |

`include/uft/formats/uft_imd_adapter.h` hatte es dreifach falsch:

```c
#define UFT_IMD_HAS_SECTOR_MAP  0x80   /* Sector numbering map follows */
#define UFT_IMD_HAS_CYLINDER_MAP 0x40  /* Cylinder map follows */
#define UFT_IMD_HAS_HEAD_MAP    0x20   /* Head map follows */
```

0x80 ist die *Zylinder*karte, 0x40 die *Kopf*karte, und ein 0x20-Flag gibt es
nicht — die Nummernkarte ist **Pflicht**, kein Optionsfeld. Für eine gewöhnliche
Spur (Kopf-Byte 0x00) wurde sie deshalb nie verbraucht, und jeder folgende
Offset im Spursatz war um `sector_count` Bytes zu klein.

Dazu ein **dateigesteuerter Pufferüberlauf**: `sector_count` ist ein Byte aus
der Datei, also bis 255; die Zielarrays fassten 64 Einträge, und vor dem
`memcpy` wurde nur die *Quell*grenze geprüft, nie die Zielgrenze. Drei
`memcpy` pro Spur, bis zu 191 Bytes über das Ende.

**Reichweite — und warum das kein P0 ist:** der registrierte Plugin-Pfad
(`src/formats/imd/uft_imd_plugin.c`) macht es richtig; er liest die
Nummernkarte bedingungslos und testet 0x80/0x40. Der Adapter hat **null
Aufrufer** außerhalb seiner selbst, wird aber mitkompiliert. Es war also
totliegender Code mit einem falschen Parser darin, nicht der Lesepfad des
Werkzeugs. Behoben statt gelöscht: ein falscher Parser ist schlimmer als
keiner, und „unreferenziert = löschbar" ist genau die Annahme, die MF-423
widerlegt hat.

**Beweis, dass der Test vorher rot war:** `test_imd_track_record` gegen den
alten Adapter endet nicht mit einem Fehlschlag, sondern mit **Exit 139
(SIGSEGV)** — die 255-Sektoren-Vektorprobe schreibt über den Rand, bevor
irgendeine Ausgabe erscheint. Nach dem Fix 5/5 grün.

*Offen geblieben:* `include/uft/profiles/uft_imd_format.h` hatte die beiden
Flags **vertauscht** (0x40 Zylinder, 0x80 Kopf) — korrigiert, aber dieser
Header hat null Includer, und die ganze `profiles/`-Kette hängt an einem
`uft_format_registry.h`, das ebenfalls niemand einbindet. Ob die Kette weg
kann, gehört zu ARCH-4, nicht hierher. `UFT_IMD_MAX_TRACKS` (160/255) und
`UFT_IMD_MAX_COMMENT` (4096/8192) bleiben unterschiedlich: das sind
Implementierungsgrenzen, keine Formateigenschaften — IMD nennt weder eine
Spur- noch eine Kommentarobergrenze.

`imd` steht damit in `docs/spec_verification.json` und rückt auf **T2**.

### ARCH-4 — Header-Duplikate: 15 aufgelöst, 7 brauchen echte Zusammenführung (2026-08-18, MF-424/425)

**Architecture.** Umsetzung von „eine Wahrheit, ein Ort" auf der Header-Ebene —
der strukturellen Wurzel von FMT-14, PROT-12, ARCH-1 und ARCH-2.

**Bestandsaufnahme:** 520 Header, davon **30 mit mehrfach vergebenem
Dateinamen**. Sechs davon waren bereits korrekte Weiterleitungen
(`/* Forward-include: canonical header is … */`) — das Muster existierte im
Projekt also schon und musste nur konsequent angewandt werden.

**Umgesetzt (MF-424/425): 15 Inhaltsduplikate zu Shims**, zusammen **4156
Zeilen** doppelter Deklarationen. Kein Aufrufer betroffen: die
nicht-kanonischen Kopien hatten null Includes, und der Pfad bleibt gültig für
alles, was ich übersehen haben könnte. Vor jedem Eingriff geprüft, dass kein
Werkzeug die Datei als **Daten** liest — die Lehre aus MF-423.

**Verbleibend, mit Grund:**

| Fall | Warum kein Shim |
|---|---|
| `uft_platform.h` | ARCH-1 — die Zusammenführung bricht gemessen 187 Tests, weil `compat/` `mkdir`/`close`/`read` als Makros umdefiniert |
| `uft_disk.h` | **Zwei verschiedene Konzepte** unter einem Namen: „Disk Handle Definition" (54 Z.) gegen „Unified Disk Structure" (173 Z.). Beide ungenutzt. Zusammenlegen wäre falsch, hier fehlt eine Umbenennung |
| `uft_core.h`, `uft_crc.h`, `uft_endian.h`, `uft_fat12.h`, `uft_format_detect.h`, `uft_imd.h`, `uft_kryoflux.h` | **beide Kopien haben Konsumenten** und abweichenden Inhalt. Ein Shim würde stillschweigend ändern, welche Deklarationen ein Aufrufer sieht |

Für die letzte Gruppe ist das Rezept: Deklarationen in den kanonischen Header
vereinigen (auf Konflikte prüfen), die Konsumenten der anderen Kopie umhängen,
**dann** shimmen. Das ist Arbeit pro Fall, keine mechanische Umstellung —
`uft_fat12.h` existiert dreifach mit 303/410/862 Zeilen.

**Nebenbefund, festgehalten:** `uft_fdi.h` meint „FAT Disk Image"; UFT hat
daneben ein **unverwandtes** FDI-Format-Plugin (`src/formats/fdi/`) für den
ZX-Spectrum-Container mit eigenem Korpus-Eintrag. Zwei verschiedene Dinge unter
derselben Abkürzung — steht jetzt im Shim-Kommentar, damit sie niemand
zusammenzieht.

### ARCH-3 — Der Skelett-Audit sieht nur `uft_*`; 22 Banner-Header sind wirklich unfertig (2026-08-18, MF-420)

**Correctness / Prozess.** Beim Durchsehen der offenen TODO-Listen aufgefallen —
und es korrigiert eine Aussage, die **ich selbst** am selben Tag in den
MASTER_PLAN geschrieben hatte.

`scripts/audit_skeleton_headers.py` meldete:

```
Skeleton headers (>= 10 decls, >= 80% missing): 0
Total unimplemented declarations: 0
```

Ich habe das in MF-409 als Beleg genommen, MF-011 („175 Skeleton-Header, 3355
Phantom-Funktionen") sei geschlossen. Das Skript zählt jedoch laut seinem
eigenen Kopfkommentar **ausschließlich `uft_*`-präfixierte Prototypen**. Die
Null gilt für diese Teilmenge; die Ausgabezeile sagte das nicht, und ich habe
sie als Aussage über alle Header gelesen.

**Gegenprobe.** Ein Durchlauf über die 34 Header, die noch einen
Skeleton-Banner tragen:

| | Anzahl |
|---|---|
| Banner-Header gesamt | 34 |
| davon **ohne** fehlende Definition — Banner veraltet | **12** |
| davon mit tatsächlich fehlenden Definitionen | **22** |

Die größte Lücke ist `include/uft/formats/uft_woz.h`: **15 von 15**
Prototypen ohne Definition — `woz_metadata_init`, `woz_metadata_parse`,
`woz_read_track`, `uft_woz_detect_version` und weitere. Einzeln nachgeprüft:
keine davon existiert in `src/`. Sämtlich **ohne** `uft_`-Präfix und damit für
den Audit unsichtbar. Weitere echte Lücken: `fs/uft_fat_boot.h` (12/12),
`fs/uft_fat_atari.h` (12/12), `uft_process.h` (9/9),
`uft_format_verify.h` (9/9), `fs/uft_fat_badblock.h` (9/9).

Die andere Richtung ist ebenfalls ein Befund: **12 Banner beschreiben erledigte
Arbeit**. `uft_platform.h` etwa trägt „declares 32 public functions; 27 are NOT
implemented" — heute ist es genau **einer** (`uft_file_size`), der Rest sind 12
im Header selbst definierte `static inline`. Ein Banner, der nachweislich
Falsches sagt, ist so schädlich wie ein fehlender.

**Umgesetzt (MF-420):**
- Die Ausgabe des Audits nennt jetzt ihren Geltungsbereich („uft_*-prefixed
  decls only") und weist ausdrücklich darauf hin, dass unpräfixierte Prototypen
  außerhalb liegen. Kein Verhaltenswechsel — es ist eine Ehrlichkeitskorrektur
  an der Stelle, an der die Fehlinterpretation entstand.
- Die MF-011-Zeile im MASTER_PLAN steht wieder auf **teilweise** statt CLOSED,
  mit dem Grund daneben.

**Abgearbeitet (MF-421/MF-422).** Zuerst die veralteten Banner entfernt (10 von
den 12 Kandidaten; zwei blieben, weil ihr Banner Implementierungs*tiefe*
beschreibt, nicht fehlende Definitionen). Dann die MF-011-Triage auf die
verbliebenen 21 echten Skelette:

| | |
|---|---|
| Prototypen ohne Definition **und ohne Aufrufer** | 77 |
| Prototypen ohne Definition **mit** Aufrufern | 37 |
| Header davon **komplett unreferenziert** | **10** |

Die 10 unreferenzierten sind Phantom-Zwillinge lebender Subsysteme und wurden
gelöscht — **2784 Zeilen**:

`decoder/uft_pll.h`, `flux/uft_pll_pi.h`, `flux/pll/uft_pll_pi.h`,
`formats/uft_woz.h`, `fs/uft_atari_dos.h`, `uft_applesauce.h`,
`uft_format_verify.h`, `uft_process.h`, `uft_tool_adapter.h`

**Ein zehnter wurde zurückgeholt: `hal/uft_fc5025.h`.** Meine
Erreichbarkeitsprüfung deckte `src/`, `include/` und `tests/` ab — **nicht
`audit/`**. Der CI-Audit-Workflow schlug fehl, weil
`audit/fc5025/extract_uft.py:28` genau diese Datei liest, um UFTs USB-IDs und
das Format-Enum gegen die Referenz zu stellen. Sie ist kein Phantom, sondern
ein Konstanten-Header mit einem toten Prototyp obendrauf. Wieder eingesetzt,
alle neun `audit/*/diff.py` laufen wieder durch.

*Lehre für die nächste Runde:* Konsumenten eines Headers sind nicht nur
`#include`-Zeilen. Die Audit-Werkzeugkette liest Header als **Daten**. Eine
Erreichbarkeitsprüfung, die nur den Übersetzungsgraphen ansieht, übersieht
das.

Jeder Einzelfall vorher geprüft: WOZ existiert **fünffach** im Include-Baum, das
echte Plugin nimmt `formats/apple/uft_woz.h`; FC5025 läuft ausschließlich über
`hardware_providers/fc5025_provider_v2.h` (ein `src/hal/uft_fc5025.c` gibt es
nicht); `uft_atari_dos.c` bindet den Root-Header ein, nicht den unter `fs/`.

**Nebeneffekt, den der Wächter aus MF-419 sofort meldete:** das Entfernen löste
**drei** der 29 Makro-Konflikte auf — `UFT_WOZ1_MAGIC`, `UFT_WOZ2_MAGIC`,
`WOZ_MAGIC`, allesamt aus der gefährlichen `magic-type-split`-Gruppe, die damit
von 5 auf 2 schrumpft. Die konkurrierenden String-Definitionen standen in genau
den gelöschten Phantomen. Baseline 29 → 26.

**Ein wichtiger Fehler auf dem Weg dahin, dokumentiert damit er nicht wiederkehrt:**
die erste Fassung der Triage übersah `static inline`-Definitionen in Headern und
meldete deshalb `uft_pll_init()` als fehlend — eine Funktion, die in
`uft_flux_pll.h:317` definiert ist und im Konvertierungspfad viermal aufgerufen
wird. Hätte ich danach gelöscht, wäre der Build zerbrochen. Wer diese Analyse
wiederholt, muss Header **mitscannen**.

*Verbleibend:* 11 Skelett-Header, die referenziert werden und deshalb
Einzelfallprüfung brauchen. Der größte zusammenhängende Block ist die
FAT-Erweiterungsfamilie (`fs/uft_fat_atari.h`, `_boot.h`, `_badblock.h`,
`uft_fat32.h`, `fs/uft_bbc_dfs.h`) — ein in sich geschlossenes, nicht
implementiertes Subsystem, dessen einziger Aufrufer der ausgeschlossene Test
`test_fat_extensions` ist. Das ist eine Entscheidung IMPLEMENT gegen DELETE für
das ganze Subsystem, keine Einzelprototypen-Frage.

### ARCH-2 — `UFT_SCP_SIGNATURE` existiert viermal, einmal mit anderem Typ (2026-08-18, MF-418)

**Correctness.** Beim Verdrahten der SCP-Adapter (S3-1) stürzte der erste Lauf
in `memcmp` ab. Ursache:

| Ort | Definition |
|---|---|
| `include/uft/uft_format_parsers.h:115` | **`0x504353`** — ein Integer |
| `include/uft/flux/uft_scp_parser.h:26` | `"SCP"` (hinter `#ifndef`, verliert also) |
| `include/uft/profiles/uft_scp_format.h:39` | `"SCP"` |
| `include/uft/uft_scp_format.h:36` | `"SCP"` |

`uft_core_stubs.c` bindet `uft_format_parsers.h` zuerst ein, `memcmp` bekam
also die **Adresse** `0x504353` und griff ins Leere. Welche Definition eine
Übersetzungseinheit sieht, hängt allein von der Include-Reihenfolge ab; zwei
der vier stehen ungeschützt, zwei hinter `#ifndef`.

Dieselbe Krankheit wie FMT-14 (drei Sync-Definitionen), PROT-12 (vier
`weak_mask`-Granularitäten), MF-411 (24 geteilte Include-Guards) und ARCH-1
(zwei `uft_platform.h`) — hier aber mit **unterschiedlichem Typ**, was sie von
einer stillen Fehlfunktion zu einem Absturz macht. Insofern der harmloseste der
fünf Fälle: er wurde sofort sichtbar.

*Umgangen (MF-418):* der Adapter schreibt `memcmp(data, "SCP", 3)` aus, mit der
Liste aller vier Fundstellen daneben. Der Name bleibt unbenutzbar, bis die vier
zusammengeführt sind.

**Größere Messung dazu, nicht behoben:** eine Zählung über alle Header ergab
**93 Makros mit mehr als einem Wert**. Ein Teil ist harmlos (`880` gegen
`880u`, Groß-/Kleinschreibung in Hex-Escapes, `O_BINARY` als `0` gegen
`_O_BINARY` je Plattform). Ein Teil ist es nicht:

```
DIR_ENTRY_SIZE      16      gegen 32
TAP_BLOCK_HEADER    0x00    gegen 0x01
TAP_BLOCK_DATA      0x02    gegen 0xFF
UFT_86F_FLAG_HOLE   0x0002  gegen 0x0004
```

**Triage durchgeführt (MF-419).** Von den 93 Rohtreffern bleiben nach
Normalisierung **29** echte Fälle. Weggefallen sind: gleicher Wert anders
geschrieben (`880`/`880u`, Hex-Groß-/Kleinschreibung, `16`/`0x10`),
Definitionen in sich ausschließenden `#if`-Zweigen derselben Datei, und
`#ifndef`-Ketten, bei denen der erste Include gewinnt und alle denselben Wert
tragen.

**Gemessen: keine Übersetzungseinheit sieht zwei Varianten desselben Makros.**
Geprüft über `gcc -H` auf 32 Kandidatenquellen. Alle 29 sind damit **latent,
nicht aktiv** — dieselbe Lage wie bei den Include-Guards vor MF-411.

Die 29 in vier Gruppen:

| Gruppe | Anzahl | Charakter |
|---|---|---|
| `platform-boilerplate` | 12 | `UFT_API`, `UFT_INLINE`, `UFT_PACKED*`, `UFT_RESTRICT`, `UFT_ARCH_NAME` … fünffach parallel in `uft_compiler.h`, `uft_config.h`, `uft_platform.h`, `uft_common.h`, `uft_packed.h`, `compat/`. Werte stimmen je Plattform — der Defekt sind fünf parallele Header, nicht die Werte. Ausnahme `UFT_PACKED`: Attribut gegen Pragma ist ein echter Nutzungsunterschied (ARCH-1). |
| `format-constant` | 9 | Gleiches Format, widersprüchliche Werte: 86F-Flags (**komplett verschiedene Belegung**), IMD dreifach (`MAX_SECTORS` 64/255/256, `MAX_TRACKS` 160/255, `HEAD_MASK` 0x01/0x0F), `UFT_MAX_SECTOR_SIZE` 8192/16384. Höchstens eine kann stimmen; 86F und IMD sind **T3**, also ohne Quelle kein Sieger. |
| **`magic-type-split`** | **5** | **Die gefährliche Gruppe.** `uft_format_parsers.h` definiert Format-Signaturen als **Integer**, alle anderen Header dieselben Namen als **String**: `UFT_SCP_SIGNATURE` `0x504353` gegen `"SCP"`, `UFT_TD0_SIGNATURE_NORMAL` `0x4454` gegen `"TD"`, `UFT_IPF_SIGNATURE` `0x53504143` gegen `"CAPS"`, `UFT_WOZ1_MAGIC`/`WOZ_MAGIC` analog. Genau das ließ MF-418 in `memcmp` abstürzen. Bei `WOZ_MAGIC` sind es nicht einmal dieselben Bytes: `0x0A0D0AFF` ist die Folge **nach** der Signatur. |
| `name-too-generic` | 3 | Zwei **verschiedene** Formate unter demselben Namen: `DIR_ENTRY_SIZE` 16 (Atari DOS) gegen 32 (C64 BAM), `TAP_BLOCK_HEADER`/`_DATA` (C64-TAP gegen Sinclair-TAP — zwei unterschiedliche „TAP"-Formate). Kein Wert ist falsch, die Namen sind es. Beheben durch Präfix. |

**Wächter mit Baseline angelegt.** `check_consistency.py` hat die Kategorie
„macro value conflicts"; `scripts/macro_conflict_baseline.json` führt die 29
mit Kategorie und Begründung. Ein **neuer** Name schlägt fehl, und ein
Baseline-Eintrag, der nicht mehr kollidiert, ebenfalls — sonst verrottet die
Liste still. Alle drei Richtungen gegengeprobt.

*Nicht behoben, bewusst:* die 29 selbst. Das ist ein Durchgang durch den
Format-Layer, der eigene Aufmerksamkeit braucht; die Reihenfolge nach Nutzen
ist `magic-type-split` (absturzfähig) → `name-too-generic` (mechanisch,
risikolos) → `format-constant` (braucht Quellen) → `platform-boilerplate`
(hängt an ARCH-1).

#### Nachtrag (2026-08-19, MF-427): der erste Fall, der **aktiv** falsch war

Die Aussage oben — „keine Übersetzungseinheit sieht zwei Varianten desselben
Makros" — galt für Makro-gegen-Makro. Sie deckt einen Fall nicht ab, den die
Zählung gar nicht sehen konnte: **Makro gegen Enum-Konstante.**

`include/uft/uft_track.h` definierte die Encoding-Konstanten selbst, hinter

```c
/* Define local constants only if uft_types.h not included */
#ifndef UFT_ENC_UNKNOWN
```

`UFT_ENC_UNKNOWN` ist in `uft_types.h` aber ein **Enum-Wert**, kein Makro. Der
Präprozessor sieht ihn nie, der Zweig war also **immer** aktiv — obwohl
`uft_track.h` `uft_types.h` zwei Zeilen weiter oben bereits einbindet. Die
Zahlen widersprachen sich:

| Übersetzungseinheit | `UFT_ENC_GCR_CBM` |
|---|---|
| nur `uft_types.h` (alle Format-Plugins) | **9** |
| zusätzlich `uft_track.h` (43 Tests, GUI-Konsumenten) | **3** |

Ein Plugin schrieb 9 in `track->encoding`, jeder Vergleich auf der anderen
Seite prüfte gegen 3 und traf nie. Kein Absturz, kein Compiler-Wort — der
Vergleich war schlicht immer falsch.

Gefunden nicht durch Lesen, sondern weil `test_corpus_cbm_vice` einen echten
G71-Track einlas und dessen Encoding nicht benennen konnte. Der Makro-Block ist
ersatzlos entfernt (MF-427); an seiner Stelle steht der Grund, damit ihn
niemand „zur Sicherheit" wieder einsetzt. Suite danach 200/200.

**Folge für den Wächter:** `check_macro_conflicts()` vergleicht `#define` gegen
`#define`. Ein Name, der einmal als Makro und einmal als Enum-Konstante
existiert, fällt durch — und das ist genau die Kombination, die keine Warnung
erzeugt und trotzdem zwei Werte hat. Die Lücke ist bekannt und nicht
geschlossen; sie zu schließen heißt, Enum-Bezeichner mitzuindizieren.

#### `magic-type-split` ist abgeräumt (2026-08-19, MF-428)

Die Gruppe von zwei verbliebenen Namen (`UFT_IPF_SIGNATURE`,
`UFT_TD0_SIGNATURE_NORMAL`, dazu `UFT_SCP_SIGNATURE` außerhalb der Liste) ist
erledigt. Der Befund beim Nachzählen: **die Integer-Zwillinge in
`uft_format_parsers.h` hatten null Nutzer.** Jede reale Verwendung im Baum ist
`memcmp`/`memcpy` gegen den String:

| Name | Nutzer | Form |
|---|---|---|
| `UFT_SCP_SIGNATURE` | `src/flux/uft_scp_parser.c` (3×) | `memcmp(…, "SCP", 3)` |
| `UFT_TD0_SIGNATURE_NORMAL` | `profiles/uft_td0_format.h` (3×) | `memcmp`/`memcpy` |
| `UFT_IPF_SIGNATURE` | keine | — |

Die Integer-Definitionen sind gelöscht, an ihrer Stelle steht der Grund plus
der Verweis auf den kanonischen Header. `UFT_IPF_CHUNK_CAPS` behält die
numerische Form — dort ist ein 32-Bit-Tag auch das Richtige.

Damit ist die Umgehung aus MF-418 zurückgebaut: `uft_core_stubs.c` benutzt
wieder `UFT_SCP_SIGNATURE` statt des ausgeschriebenen `"SCP"`.

#### Zwei Kollisionen waren **live**, nicht latent (2026-08-19, MF-428)

Beim Bauen fielen zwei Redefinitions-Warnungen auf, die es nach der Triage
nicht geben dürfte:

```
uft_compiler.h:61: warning: "UFT_COMPILER_VERSION" redefined
uft_config.h:118:  warning: "UFT_PREFETCH" redefined
```

Beide sind der ARCH-1-Familie zuzurechnen (parallele Plattform-Header), beide
hatten **null funktionale Nutzer**, beide sind behoben: `uft_compiler.h` ist
jetzt alleiniger Eigentümer der Compiler-Identität (mit der besseren Formel
inklusive Patchlevel, die vorher in `uft_platform.h` stand) und der
Prefetch-Makros; `uft_platform.h` bindet ihn ein, `uft_config.h` definiert
nichts Eigenes mehr.

Zwei Lehren daraus, beide unbequem:

1. Die Aussage „keine Übersetzungseinheit sieht zwei Varianten" galt für die
   29 gemessenen Namen — `UFT_PREFETCH` war nie darunter, weil der Zähler
   **funktionsartige Makros bewusst überspringt**. Genau dort saß eine
   Live-Kollision. Zweite bekannte Lücke des Wächters, neben Makro-gegen-Enum.
2. Der Compiler meldete beide seit jeher bei jedem Build. Es hat niemand
   hingesehen. Ein Zähler in einem Skript ersetzt nicht das Lesen der
   Build-Ausgabe.

#### `name-too-generic` ist abgeräumt (2026-08-19, MF-429)

Die zweite Gruppe, mechanisch wie angekündigt. Kein Wert war falsch, die Namen
waren es:

| Name | Werte | neue Namen |
|---|---|---|
| `DIR_ENTRY_SIZE` | 16 (Atari DOS) · 32 (C64-BAM) · 24 (FLEX) | `ATARI_DOS_DIR_ENTRY_SIZE`, `C64_DIR_ENTRY_SIZE`, `FLEX_DIR_ENTRY_SIZE` |
| `TAP_BLOCK_HEADER` / `_DATA` | 0x01/0x02 (C64-TAP) · 0x00/0xFF (Sinclair-TAP) | `C64_TAP_BLOCK_*`, `ZX_TAP_BLOCK_*` |

Beide „TAP" sind verschiedene Formate mit demselben Kürzel, deshalb hat auch
keines den generischen Namen behalten — sonst hätte die nächste Kollision nur
einen anderen Aufhänger. Vier Aufrufstellen angepasst, ctest 200/200.

**Stand der vier ARCH-2-Gruppen:** `magic-type-split` leer (MF-428),
`name-too-generic` leer (MF-429), `format-constant` offen (braucht Quellen —
86F und IMD sind T3, ohne Beleg gibt es keinen Sieger), `platform-boilerplate`
offen (hängt an ARCH-1). Baseline von 29 über 25 auf **19**.

#### Offen und belegt: `UFT_ENCODING_MFM` hat zwei Werte (MF-428)

Nach demselben Muster, aber **nicht** behoben, weil die Auflösung eine
Entscheidung über die God-Mode-/PLL-Schnittstelle verlangt:

| Header | `UFT_ENCODING_MFM` | `UFT_ENCODING_FM` |
|---|---|---|
| `uft_god_mode.h:276` | **0** | 1 |
| `uft_types.h:252` (Alias auf `UFT_ENC_MFM`) | **3** | 1 |

`FM` stimmt zufällig überein, `MFM` nicht — und die 0 kollidiert zusätzlich mit
`UFT_ENC_UNKNOWN`. Gemessen, welchen Wert die Aufrufer tatsächlich sehen:

- `src/formats/uft_format_convert_flux.c` → 3, gibt ihn an
  `uft_pll_init(…, uft_encoding_t)` weiter. In sich stimmig.
- `src/core/uft_advanced_mode.c` → 2 für `UFT_ENCODING_GCR_C64`, gibt ihn an
  `uft_kalman_config_init()`, das in `uft_god_mode_api.c` mit derselben
  Nummerierung übersetzt wird. Ebenfalls stimmig.

**Gesucht und nicht gefunden:** eine Stelle, an der ein Wert die Grenze
überquert — also ein Plugin, das `track->encoding = UFT_ENCODING_MFM` (3)
schreibt und von God-Mode-Code (0/1/2/3) gelesen wird. Solange es die nicht
gibt, ist das keine falsche Ausgabe, sondern eine gestellte Falle. Wer sie
entschärft, muss zuerst festlegen, welche Nummerierung die God-Mode-API
eigentlich meint — `uft_pll_init` existiert nämlich in **zwei** Signaturen
(`uft_decoder_plugin.h:335` mit `adjust_pct`, `uft_flux_pll.h:317` mit
`uft_encoding_t`), was denselben Aufruf je nach Include-Satz anders bedeutet.

### FMT-18 — Zwei unbelegte Sync-Werte nachgerechnet, X-Copys Fehlerklassen übernommen (2026-08-21, MF-454) → ✓ TEILS GEKLÄRT

Aus einer zweiten Referenzliste zum X-Copy-Umfeld
(`keirf/disk-utilities` libdisk, X-Copy-Handbuch 1992 auf archive.org).

**1. Zitate korrigiert.** Die Zeilennummern aus MF-452/453 waren um zwei bis
drei Zeilen daneben. Nachgeprüft und in Code, Tests und Doku berichtigt:

| Zitat | war | ist |
|---|---|---|
| `cmp.w #INDEXCOPY,D1` | `xcop.s:2108` | **`xcop.s:2112`** |
| Sync-Register D2–D6 | `xcop.s:2113-2117` | **`xcop.s:2114-2118`** |
| Rotations-Suchschleife | `xcop.s:2120-2135` | **`xcop.s:2119-2138`** |
| `synctab`, `xio.s:210`, `xcop.s:1836` | — | unverändert richtig |

Der Upload enthält **zwei** Quellbäume, `xcopy_src/` und `xcopy_src(1)/`.
Geprüft: die Unterschiede liegen ausschließlich in der Bootblock-Prüfsumme
(`xcop.s:4411-4677`, `addq/neg` durch `not.l` ersetzt — arithmetisch identisch,
da −(x+1) = ~x) und bei `xio.s:2451`. **Keine** der zitierten Stellen ist
betroffen.

**2. `0x8914` ist keine eigene Sync-Konstante.** MF-453 hatte die beiden
unbelegten Werte aus `src/protection/uft_amiga_protection.c` stehen lassen.
Nachgerechnet:

```
0x448A = 0100 0100 1000 1010
0x8914 = 1000 1001 0001 0100      ← 0x448A um EIN Bit nach links rotiert
```

In einer bitweisen Sync-Suche ist ein Muster und seine Rotation **derselbe
Sync bei anderer Bitausrichtung**. `0x8914` als eigenen Schutz-Sync zu führen
heißt daher mit hoher Wahrscheinlichkeit, `0x448A` ein zweites Mal zu zählen —
plausibel abgelesen aus einem um ein Bit verschobenen Dump. Die Rotation ist
gerechnet; die Schlussfolgerung ist als solche benannt.

`0x8A91` (CopyLock) ist **Rotation von keinem** bekannten Muster — weder aus
X-Copy noch aus libdisk. Bleibt unerklärt.

Beide bleiben stehen, bis eine reale Referenzdiskette entscheidet, und wandern
nicht in die SSOT. Neu abgesichert: `tests/test_amiga_decoder_limits.c` prüft,
dass **keine zwei Einträge der SSOT Rotationen voneinander sind** — samt
Gegenprobe, damit der Test nicht trivial besteht.

**3. X-Copys Fehlerklasse 5 gibt es jetzt.** Die Klassifikation ist doppelt
belegt: 1–6 im Quelltext (`xcop.s:1163-1174`, Label `tofewsc`, `nosync`,
`no2sync`, `hecksum`, `headerr`, `blcksum`), 1–8 im Handbuch von 1992 (Siren
Software).

| | | | |
|---:|---|---:|---|
| 1 | ≠ 11 Sektoren | 5 | Fehler im Header-Longword |
| 2 | kein Sync | 6 | Datenblock-Prüfsumme |
| 3 | nach Gap kein Sync | 7 | Longtrack |
| 4 | Header-Prüfsumme | 8 | Verify-Fehler |

UFT hatte 4 und 6 als Zähler. **Klasse 5 fiel unter den Tisch:** ein Sektor,
dessen Info-Long nicht mit `0xFF` beginnt oder dessen Spur-/Sektornummer nicht
in die Geometrie passt, wurde als `FLUX_ERR_NO_SYNC` gemeldet — also als „kein
Sync gefunden". Das ist eine andere Diagnose: der Sync **war** da, der Kopf
dahinter passt nicht. Auf einer geschützten Diskette ist genau das der
interessante Fall.

Neu: `FLUX_ERR_BAD_HEADER` und `flux_decoded_track_t.bad_header_format`.
Klassen 1, 3, 7 und 8 fehlen absichtlich — sie sind Aussagen über die ganze
Spur bzw. über einen Schreibvorgang, nicht über einen Sektor, und werden nicht
mit einer Näherung gefüllt.

**Dabei mitgefunden:** nach einem gescheiterten Sektor sprang die Schleife
`sync_pos + 16` weiter und fand damit den **zweiten** Sync des Amiga-Paares,
lief auf denselben kaputten Kopf und zählte ein zweites Mal — gemessen
`bad_header_format == 2` für einen Sektor. Der Zähler hätte Sync-Kandidaten
gezählt statt Sektoren. Es wird jetzt über den ganzen Sync-Lauf gesprungen.

**Nicht übernommen:** libdisk führt weitere Amiga-Syncs (`0x4521` Z Out,
`0x4891` Turbo Outrun, `0x4A84` Future Tank und ein Dutzend Custom-Handler).
Sie sind gute Kandidaten, fallen aber unter die EINFRIER-REGEL: ohne reale
Referenzdiskette pro Format kein neuer Formatcode. Als Lead in
`docs/XCOPY_COMPARISON.md` notiert.

---

### FMT-17 — Die Amiga-Sync-Muster lagen dreimal im Baum, und der Decoder benutzte keines davon (2026-08-21, MF-453) → ✓ BEHOBEN

Fund 4 aus [`docs/XCOPY_COMPARISON.md`](XCOPY_COMPARISON.md). Der
Migrationsbericht von 2026-04-24 führt „Multi-Pattern-Sync" als zu bauende
Portierung — tatsächlich war sie **dreimal gebaut und nirgends angeschlossen**.

| Datei | Inhalt |
|---|---|
| `src/analysis/uft_track_analysis.h:41-44` | `0xA245` = **„Ocean/Imagine"** |
| `src/formats/amiga/uft_amiga_protection.h:38-41` | `0xA245` = **„Beyond the Ice Palace"** |
| `src/protection/uft_amiga_protection.c:23,52` | kennt keinen davon, dafür `0x8a91`, `0x8914` |
| `src/flux/uft_flux_decoder.c:1169` | benutzt keine davon — nur `MFM_SYNC_PATTERN` |

Eine Amiga-Diskette mit Arkanoid- oder Mercenary-Sync dekodierte damit zu
**null Sektoren**, während zwei andere Module wussten, dass es diese Syncs gibt.

**Der Namensstreit ist aus der Quelle entschieden.** `xcop.s:2347-2351` enthält
eine auskommentierte `synctab` neben der Suchschleife:

```asm
;synctab
;   DC.W  $9521,$A245,$A89A,$448A,$4489,$0000,...
;   DC.W  $9521     ; ARKANOID SYNC
;   DC.W  $A245     ; BEYOND THE ICE PALACE
;   DC.W  $A89A     ; MERCENERY/BACKLASH
```

`0xA245` ist Beyond the Ice Palace; `SYNC_AMIGA_OCEAN` war falsch benannt und
ist jetzt ein Alias auf `SYNC_AMIGA_BTIP`. Für `0x448A` nennt die Quelle keinen
Titel — der Eintrag trägt deshalb `name == NULL`, was eine Aussage ist und kein
fehlender Eintrag.

**SSOT:** `include/uft/formats/uft_amiga_syncs.h` +
`src/formats/amiga/uft_amiga_syncs.c`. Jeder Eintrag nennt seine Herkunft.

**Decoder:** `flux_decoder_options_t` bekommt `sync_patterns` / `sync_count`.
NULL/0 bedeutet weiterhin nur `0x4489` — **der Default ändert kein bestehendes
Ergebnis**. Wer geschützte Disketten lesen will, übergibt
`UFT_AMIGA_SYNC_PATTERNS`.

Neu: `flux_find_sync_any()` — ein Durchlauf, ein Schieberegister, alle Muster
pro Bitposition, so wie X-Copy es macht (`xcop.s:2119-2138`: `rol.l` plus sechs
Vergleiche). N Einzelaufrufe von `flux_find_sync()` wären nicht nur N
Durchläufe, sie lieferten auch den frühesten Treffer des **ersten** Musters
statt des frühesten überhaupt.

Dabei mitgefunden: `mfm_skip_sync_run()` verglich fest gegen
`MFM_SYNC_PATTERN`. Solange nur `0x4489` gesucht wurde, war das dasselbe —
sobald der Decoder `0xA245` findet, würde ein Sync-Lauf aus zwei Custom-Syncs
nicht übersprungen und der zweite Sync als Info-Long gelesen. Das Muster kommt
jetzt als Parameter; der IBM-Pfad ruft weiterhin mit `MFM_SYNC_PATTERN`.

**Nicht übernommen:** `0x8a91` (CopyLock) und `0x8914` (Psygnosis Type B) aus
`src/protection/uft_amiga_protection.c`. Sie stehen in keiner der anderen
Tabellen und nicht in der X-Copy-Quelle, gegen die die übrigen belegt sind —
also unbelegt. Sie bleiben stehen und sind dort als unbelegt markiert, statt
ungeprüft in die SSOT zu wandern.

Nachweis: `tests/test_amiga_decoder_limits.c` — ein Sektor mit `0xA245`-Sync
dekodiert mit der Liste und ohne sie nicht; die Tabelle hat fünf Einträge und
nennt je eine Quelle; `flux_find_sync_any()` liefert den frühesten Treffer über
alle Muster. Ohne den Fix fällt der erste davon (verifiziert).

---

### FMT-16 — Drei Fehler im Amiga-Flux-Pfad, gefunden durch den X-Copy-Quellenvergleich (2026-08-21, MF-452) → ✓ BEHOBEN

Vollständige Analyse: [`docs/XCOPY_COMPARISON.md`](XCOPY_COMPARISON.md).
Ergänzt [`XCOPY_ALGORITHM_MIGRATION.md`](XCOPY_ALGORITHM_MIGRATION.md) und
[`XCOPY_INTEGRATION_TODO.md`](XCOPY_INTEGRATION_TODO.md), die den
Algorithmus-Import abdecken; die drei Fehler unten stehen in keinem von beiden.

**1. `0xF8BC` wurde als Sync gesucht.** In X-Copy ist der Wert `INDEXCOPY`
(`xcopy_src/xcopy.i`) und dient als Modus-Sentinel:

```asm
    move.w  sync,D1
    cmp.w   #INDEXCOPY,D1
    beq.s   stdsync        ; -> NUR nach $4489 suchen, index-synchron
```

Er bedeutet dort ausdrücklich „kein Custom-Sync" und steht **nie auf einer
Diskette**. UFT führte ihn in `AMIGA_SYNCS[]`, in `all_syncs[]`, in
`KNOWN_SYNCS[]` und meldete bei Treffer `"Index Copy Protection"`. Ein
16-Bit-Muster trifft in einem MFM-Bitstream im Mittel alle 65536 Bit — auf einer
100.000-Bit-Spur ein- bis zweimal. Der Befund war Rauschen mit Namen. Aus allen
drei Tabellen entfernt; die Konstante bleibt als `AMIGA_MODE_INDEXCOPY` mit
Erklärung stehen, damit der nächste Leser der X-Copy-Quelle sie wiederfindet.

**2. Das 16-Byte-Sektor-Label wurde verworfen.** `uft_flux_decoder.c` las es,
rechnete es in die Header-Prüfsumme und warf es mit `(void)label;` weg —
`flux_decoded_sector_t` hatte kein Feld dafür. Stiller Datenverlust auf dem
Hauptpfad eines Werkzeugs mit dem Grundsatz „Kein Bit verloren". AmigaDOS legt
dort Wiederherstellungsdaten ab, mehrere Schutzverfahren benutzen es als Ablage.

Nebenbefund dabei: `id_crc` und `data_crc` waren `uint16_t`, die
Amiga-Prüfsummen sind **32-bittig**. Die Gültigkeitsflags stimmten (verglichen
wurde vor der Zuweisung mit voller Breite), der gespeicherte Wert war halbiert —
ein Bericht gab eine Zahl aus, die nicht auf der Diskette steht. Auf `uint32_t`
geweitet.

**3. Der Flux-Pfad konnte keine Amiga-HD und keine 82 Zylinder.**

```c
if (track > 159 || sec > 10) return FLUX_ERR_NO_SYNC;
```

- `sec > 10` verwarf HD (22 Sektoren, 0–21) — während
  `src/formats/adf/uft_adf_plugin.c:96` `"HD variant (1760 KB)"` als
  `UFT_FEATURE_SUPPORTED` führt und `uft_adf_parser_v2.c` `ADF_SIZE_HD` kennt.
  Der Sektorpfad konnte HD, der Fluxpfad nicht: SCP→ADF einer HD-Diskette
  lieferte nichts.
- `track > 159` verwarf die Zylinder 80 und 81. X-Copy erlaubt
  `endtrack DC.W 79 ; 0 - 81` (`xio.s:210`) und steppt in `track0` bis 83.
  Genau dort liegen Zusatzkapazität und Kopierschutz.

Grenzen jetzt `AMIGA_MAX_SECTOR 21` und `AMIGA_MAX_TRACK 167`, abgeleitet aus
dem, was Hardware tragen kann statt aus dem häufigsten Fall. Ein DD-Sektor mit
unplausibler Nummer fällt weiterhin durch — über die Header-Prüfsumme, was der
richtige Mechanismus dafür ist.

Nachweis: `tests/test_amiga_decoder_limits.c`, sieben Fälle gegen synthetische
Sektoren, die aus demselben Layout gebaut sind, das der Decoder liest. Ohne die
Fixes fallen fünf davon (verifiziert).

**Offen aus derselben Durchsicht:**
- Die Amiga-Sync-Muster liegen **dreimal** im Baum, mit widersprüchlichen Namen
  (`0xA245` heißt einmal „Ocean/Imagine", einmal „Beyond the Ice Palace"), und
  der Decoder benutzt keine davon — er sucht nur `0x4489`. Eine geschützte
  Amiga-Diskette dekodiert damit zu null Sektoren. Siehe XCOPY_COMPARISON §4.
- T1 aus `XCOPY_INTEGRATION_TODO.md` (Virus-Signaturen) ist **nicht
  durchführbar**: `xvslibrary` trägt „All Rights Reserved", das Archiv enthält
  keinen Lizenztext. Dieselbe Regel wie bei `src/switch/` (MF-441).

---

### ARCH-16 — Acht Header definierten das Packing-Vokabular, und `fat12_bpb_t` war deshalb 40 Bytes statt 36 (2026-08-21, MF-451) → ✓ BEHOBEN, 18. Gate steht

Gefunden beim Angehen von ARCH-1. Die dort beschriebene Doppelung zweier
Plattform-Header ist der kleinere Teil des Problems: **`UFT_PACKED`,
`UFT_PACK_BEGIN` und `UFT_PACK_END` waren in acht Headern definiert** — und sie
widersprachen sich.

| Header | `UFT_PACKED` auf GCC | `UFT_PACK_BEGIN` |
|---|---|---|
| `uft_packed.h` | `__attribute__((packed))` | `_Pragma("pack(push,1)")` |
| `uft_compiler.h` | leer | `_Pragma(...)` |
| `uft_platform.h` | `__attribute__((packed))` | **leer** |
| `uft_common.h` | `__attribute__((packed))` | `UFT_PACKED_BEGIN` **leer** |
| `uft_config.h` | leer | — |
| `compat/uft_platform.h` | **leer** | — |
| `floppy/uft_floppy_device.h` | — | `_Pragma(...)` |
| `formats/uft_fdi.h` | — | **leer**, Packing über `UFT_PACKED_ATTR` |

Welche Definition eine Übersetzungseinheit sah, entschied die
Include-Reihenfolge — für Makros, die das Speicherlayout von Strukturen
bestimmen, die **direkt auf Diskettenbytes gecastet werden**. Vier der acht
benutzten `#ifndef`, machten das Ergebnis also ausdrücklich von der Reihenfolge
abhängig; `uft_common.h` trug dazu den Kommentar „only define if not already
defined by uft_packed.h", was genau die Abhängigkeit beschreibt, die das
Problem ist.

**Der Schaden, konkret.** Vier Dateien hatten ein `UFT_PACK_END` **ohne**
`UFT_PACK_BEGIN` — ein `pack(pop)`, das den Pragma-Stack des Compilers unter
seine Basis schiebt. Drei kamen damit durch, weil ihre Strukturen zufällig
natürlich ausgerichtet sind (2IMG 64 Bytes, G64 12, STX 16). Die vierte nicht:

```c
typedef struct {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;   /* soll auf 0x0B liegen */
    ...
} fat12_bpb_t;
UFT_PACK_END                     /* ohne BEGIN */
```

und benutzt als

```c
const fat12_bpb_t *bpb = (const fat12_bpb_t*)image;
uint16_t bytes_per_sect = bpb->bytes_per_sector;
```

Ohne Packing schiebt der Compiler vor jedes 16-/32-Bit-Feld nach ungerader
Position ein Füllbyte: die Struktur wächst von **36 auf 40 Bytes**, und
`bytes_per_sector` wird von Offset **12 statt 11** gelesen. Jedes Feld ab dem
OEM-Namen kam aus der falschen Stelle — Sektorgröße, Cluster-Größe,
FAT-Anzahl, Sektorzahlen, Geometrie. Ein FAT12-Bootsektor, durch diese Struktur
gelesen, lieferte Zahlen, die nie im Image standen.

**Fix.**
- Eine Quelle: `include/uft/uft_packed.h`. Die anderen sieben binden sie ein,
  statt selbst zu definieren. Der Header beginnt mit `#undef` auf alle Namen,
  ist also gegen Reste immun.
- Die vier fehlenden `UFT_PACK_BEGIN` ergänzt.
- **Layout wird geprüft, nicht beschrieben:** `_Static_assert` auf `sizeof` für
  alle vier Strukturen und zusätzlich auf `offsetof` für jedes Feld des
  FAT12-BPB. „Es ist jetzt gepackt" ist genau die Art Behauptung, die hier
  unbemerkt aufgehört hat zu stimmen.
- Nebenwirkung: fünf Einträge der Makro-Konflikt-Baseline (`UFT_PACKED`,
  `_ATTR`, `_BEGIN`, `_END`, `_STRUCT`) haben sich damit erledigt, 17 → 12.

**18. Gate** (`scripts/packing_gate.py`): kein `#define UFT_PACK*` außerhalb von
`uft_packed.h`, und `UFT_PACK_BEGIN`/`UFT_PACK_END` müssen je Datei aufgehen.
Grenze ehrlich benannt — die Balance wird pro Datei gezählt, ein BEGIN im
Header und ein END in der einbindenden `.c` würde durchrutschen; dagegen helfen
die `_Static_assert`s, nicht der Zähler.

Verifiziert: ohne das `UFT_PACK_BEGIN` meldet der Compiler
`static assertion failed: "FAT12 BPB is 36 bytes on disk"` und zwölf
Offset-Zusicherungen dazu.

**Was von ARCH-1 offen bleibt:** die eigentliche Entflechtung der beiden
`uft_platform.h` (Guard-Kollision, invasive POSIX-Shims). Die Packing-Ebene war
der Teil, der bereits Schaden angerichtet hat; der Rest bleibt wie dort
beschrieben stehen.

---

### FLUX-4 — die gemessene Umdrehung erreichte den Decoder nie (2026-08-22, MF-475) → ✓ BEHOBEN

MF-471 hat die Zellendauer aus der **gemessenen** Umdrehung statt aus einer
Annahme bestimmt. Danach hatte sie keinen Aufrufer — und beim Nachmessen
zeigten sich zwei Bruchstellen, die beide dafür gesorgt hätten, dass ein
gesetztes Medienprofil **nichts** geändert hätte.

#### 1. Der Erzeuger lieferte keine Index-Impulse

`flux_raw_from_ns_intervals()` (`src/flux/uft_flux_decoder.c`) machte
`memset(out, 0, sizeof(*out))` und füllte `index_times` nie. Damit war
`index_count == 0`, die Messung in MF-471 schlug **immer** fehl, und die Wahl
fiel auf ihren Nennwert zurück.

Das ist der Grund, warum ich im Konvertierungspfad nicht einfach
`dopts.media = …` gesetzt habe: die Zeile hätte fachkundig ausgesehen und
wäre wirkungslos gewesen. Ein Profil ohne Messung ist keine Verbesserung,
sondern eine Behauptung.

Neu ist `flux_raw_from_ns_intervals_indexed()`, das die gemessene
Umdrehungsdauer mitnimmt. Die Information gab es längst: das SCP-Plugin
schreibt sie aus dem Umdrehungskopf nach `uft_track_t::metrics.index_time_ns`
(`src/formats/scp/uft_scp_plugin.c:401`) — sie wurde auf dem Weg zum Decoder
nur fallen gelassen.

> **Warum die Dauer geprüft wird, bevor sie übernommen wird.** Aus einer
> Umdrehungsdauer entstehen zwei Marken: 0 und `revolution_ns`. Das ist eine
> Aussage über die **Struktur** des Datenstroms — „dieser Strom ist genau eine
> Umdrehung". Deckt er drei, oder einen abgeschnittenen Rest, wäre die Aussage
> falsch, und der Decoder würde daraus eine falsche Zellendauer rechnen, die
> wie eine Messung aussieht. Die Toleranz ist ±50 % der kumulierten Flusszeit:
> weit genug für eine angeschnittene Umdrehung, eng genug, um zwei
> auszuschließen. Passt es nicht, bleibt die Spur ohne Marken und der Nennwert
> gilt — sichtbar schlechter statt unsichtbar falsch.

#### 2. Vier von fünf Decodern kannten die Wahl gar nicht

Die dreistufige Reihenfolge aus MF-471 stand in `flux_decode_mfm()`. Die
anderen vier trugen weiter ihre feste Zahl:

| Decoder | Zeile vor MF-475 |
|---|---|
| `flux_decode_fm` | `opts->bitcell_ns ? … : FLUX_FM_BITCELL_NS` |
| `flux_decode_gcr_c64` | `if (bitcell_ns == 0) bitcell_ns = 4000.0;` |
| `flux_decode_gcr_apple` | `… = APPLE_GCR_BITCELL_NS;` |
| `flux_decode_amiga` | `… = FLUX_MFM_DD_BITCELL_NS;` |

Darunter ausgerechnet der **FM-Pfad** — Atari 810/1050, 288 min⁻¹, also genau
der Fall, für den das Profil gebaut wurde.

Das ist zum wiederholten Mal dieselbe Diagnose: **wo ein Fakt an fünf Stellen
steht, ist die Fassung, die läuft, nicht die bessere, sondern die zufällig
aufgerufene.** Seit MF-475 gibt es `flux_pick_bitcell_ns()` und fünf Aufrufer,
jeder mit seinem eigenen Nennwert als letzter Stufe.

#### Die verdrahtete Kette

```
SCP-Umdrehungskopf (duration)
  → uft_scp_plugin.c        metrics.index_time_ns
  → uft_format_convert_flux.c  flux_raw_from_ns_intervals_indexed()
  → flux_raw_data_t         index_times = {0, rev_ns}
  → flux_pick_bitcell_ns()  Profil + Messung → Zellendauer
  → flux_decode_amiga()     PLL-Startperiode
```

Gesetzt wird `UFT_MEDIA_AMIGA_DD` im SCP→ADF-Pfad. Das Medium ist dort
tatsächlich bekannt und nicht geraten: das Ziel ist ADF, und ADF gibt es nur
für AmigaDOS-DD.

#### Rot-Proben

| Verfälschung | Ergebnis |
|---|---|
| Plausibilitätsprüfung entfernt | genau `builder_refuses_a_revolution_that_does_not_fit` fällt |
| FM-Pfad auf den Zustand vor MF-475 zurück | genau `fm_decoder_follows_the_measured_revolution` fällt |
| Rangfolge umgedreht (Messung vor explizitem `bitcell_ns`) | genau `an_explicit_bitcell_still_wins_over_the_measurement` fällt |

`ctest` 219/219, alle 21 Gate-Kategorien 0, `verify_build_sources.py` 0/0.

**Ehrlich zur Reichweite.**

- Verdrahtet ist **ein** Pfad: SCP→ADF. Die anderen Flux-Konvertierungen
  (SCP→D64, SCP→G64, SCP→HFE) bauen ihre Flux-Daten nicht über
  `flux_raw_from_ns_intervals()` und sind unberührt — dort ist die Verdrahtung
  offen und jeweils ein eigener Schritt.
- Der GUI-Pfad setzt kein Medienprofil. Das Feld existiert in
  `flux_decoder_options_t`, aber kein Dialog schreibt hinein; Punkt 5 der
  Gap-Analyse (Prozent-Nachstellung als Bedienelement) ist weiter offen.
- Geprüft ist, dass die gemessene Umdrehung die **Startperiode der PLL**
  bestimmt. Dass ein 288-min⁻¹-Abbild damit auch mehr Sektoren liefert als
  ohne, ist plausibel und **nicht gemessen** — dafür braucht es ein reales
  Abbild mit abweichender Drehzahl im Korpus.
- Zwei Module aus dieser Reihe bleiben ohne Aufrufer: die
  Multiread-Klassifikation (MF-473) und der ATX-Schreiber (MF-474). Beide
  sind getestet, beide brauchen ihren eigenen Verdrahtungsschritt.
  *(Nachtrag: die Klassifikation ist seit MF-478 verdrahtet — siehe REC-1.)*

---

### FLUX-8 — eine Umdrehung genügt nicht überall, und niemand sagte es (2026-08-23, MF-483) → ✓ BEHOBEN

Erste Umsetzung aus dem Entwurf „Universelle Capture/Decode-Settings":
`index_sync` im Medienprofil — das einzige Feld dieses Entwurfs, das eine
forensische **Aussage** erzeugt statt eines Stellknopfs.

**Das Problem.** Bei einem nicht indexsynchronen Format kann ein Sektor über
die Indexmarke hinauslaufen. Ein Ein-Umdrehungs-Abbild schneidet ihn dann
mitten durch — und zwar **unauffällig**: es fehlt einfach ein Sektor, genau
wie bei einem Lesefehler. Wer das nicht gesagt bekommt, hält eine
unvollständige Sicherung für eine beschädigte Diskette.

a8rawconv sagt es für Atari wörtlich (`rawdiskscp.cpp:120-124`):

> „Only one disk revolution found in image. Atari disks are not index aligned
> and require at least two revolutions."

Dort ist die Warnung **fest verdrahtet**, weil a8rawconv nur Atari kennt.
UFT kennt viele Formate — fest verdrahtet würde entweder bei jedem gewarnt
oder bei keinem. Deshalb gehört die Eigenschaft ins Profil.

#### Je Zeile ein eigener Beleg

| Profil | index_sync | Beleg |
|---|---|---|
| Atari FM / MFM | `NONE` | a8rawconv, wörtlich (s. o.) |
| AmigaDOS DD | `NONE` | **strukturell**, nicht zitiert: keine Index-Adressmarke, und das Info-Long führt ein Feld *sectors to gap* (`decode_amiga_sector`). Das braucht nur, wessen Sektorlage sich gegen die Lücke verschiebt — also wer track-at-once schreibt, wo der Kopf gerade steht |
| PC 360K/720K/1.2M/1.44M | `INDEXED` | IBM System 34 setzt eine Index-Adressmarke (0xFC) an den Spuranfang |
| Apple II GCR | `UNKNOWN` | Das Disk-II-Laufwerk hat **gar keinen Indexsensor**; die Frage ist dort anders gestellt. Ohne belastbare Quelle wird nichts behauptet |

`UNKNOWN` warnt nicht. Eine Warnung ohne Beleg wäre eine erfundene Aussage
über das Medium — dieselbe Sorte Fehler, gegen die die EINFRIER-REGEL steht.

#### Das Minimum wird gerechnet, nicht danebengeschrieben

Der Entwurf schlug `index_sync` **und** `revs_min` als zwei Felder vor.
Zwei Felder, die dasselbe sagen, laufen auseinander — und dann gilt, was
zufällig gelesen wird. Genau diese Klasse Fehler hat das Projekt in dieser
Woche dreimal getroffen: MF-475 (fünf Zellendauer-Stellen), MF-479 (zwei
Layout-Rechnungen), MF-481 (zwei Offset-Deutungen). Also gibt es ein Feld
und `uft_media_min_revolutions()` leitet daraus ab. Der Test prüft die
Ableitung für **jedes** Profil der Tabelle, nicht für eine Auswahl — sonst
schützt er einen neuen Eintrag nicht.

#### Rot-Proben

| Verfälschung | Ergebnis |
|---|---|
| AmigaDOS fälschlich `INDEXED` | Profiltest **und** Wandlungstest fallen |
| Ableitung durch feste `1` ersetzt | Ableitungstest **und** Wandlungstest fallen |

Dazu die Gegenprobe `enough_revolutions_says_nothing_about_index_sync`: bei
zwei Umdrehungen darf **keine** Warnung kommen. Ohne sie würde eine Warnung,
die immer erscheint, genauso grün leuchten.

`ctest` 224/224, alle 21 Gate-Kategorien 0, `verify_build_sources.py` 0/0,
qmake-Release-Build grün.

**Ehrlich zur Reichweite.**

- Die Warnung erscheint auf **einem** Pfad (SCP→ADF) — dem einzigen, der ein
  Medienprofil setzt.
- **Das Capture erzwingt das Minimum nicht.** Die Spinbox in
  `forms/tab_workflow.ui` lässt 1 zu und erklärt die Regel nur im Tooltip;
  ein Clamp gegen `uft_media_min_revolutions()` ist GUI-Arbeit und braucht
  zuerst, dass das Profil im Capture-Pfad überhaupt bekannt ist. Heute ist es
  das nicht.
- **AmigaDOS `NONE` ist strukturell begründet, nicht zitiert.** Die
  Begründung steht oben und ist nachprüfbar; eine Quelle wie a8rawconvs Satz
  für Atari wäre stärker. Wer eine findet, soll sie eintragen.
- Apple II bleibt `UNKNOWN`, und das ist Absicht.

---

### FLUX-7 — 83 Zylinder rein, 80 raus, kein Wort dazu (2026-08-23, MF-482) → ✓ BEHOBEN

Punkt 3.5 der a8rawconv-Gap-Analyse (Geometrie-Override). Beim Nachmessen
kam wieder zuerst ein Verlust heraus:

```c
if (cyls  > ADF_CYLS)  cyls  = ADF_CYLS;      // vorher: ohne ein Wort
```

Eine Amiga-Diskette mit 82 oder 83 Zylindern ist nichts Exotisches — genau
dort liegen Zusatzkapazität und Kopierschutz, und der AmigaDOS-Dekoder lässt
Spuren bis 167 seit MF-452 **ausdrücklich** zu. ADF fasst 80. Gekürzt werden
*muss* also; verschwiegen werden darf es nicht. „Kein Bit verloren" heißt
hier: **kein Bit still verloren.**

Gemessen: eine SCP mit 83 Zylindern → ADF, `warning_count` enthielt kein Wort
über die drei verlorenen Zylinder je Seite.

#### Der Bereich entsteht jetzt in drei Stufen

| Stufe | Quelle | vorher |
|---|---|---|
| 1 | was die Datei sagt (`disk.geometry`) | ✓ |
| 2 | was der Aufrufer vorgibt (`target_geometry`) | **fehlte** |
| 3 | was das Zielformat fasst | still |

Stufe 2 ist a8rawconvs `-g tracks,sides` (`a8rawconv.cpp:1354-1364`; beim
Lesen eines Abbilds als `forced_tracks`/`forced_sides`,
`rawdiskscp.cpp:126-127`). `uft_convert_options_t::target_geometry` gibt es
dafür seit jeher — mit dem Kommentar „(0 = auto)" — und **niemand las es je**.
Sechster Eintrag derselben Liste nach MF-471/473/474/479/480.

Damit ist zugleich die numerische Form dessen da, was FLUX-6 als
`scp-ss40`/`ds40`/`ss80`/`ds80` offen gelassen hatte: `-g 80,1` ist `ss80`.
Was weiterhin fehlt, ist ein Bedienelement.

#### Rot-Proben

| Verfälschung | Ergebnis |
|---|---|
| Zylinder-Vorgabe ignoriert | `an_explicit_range_limits_what_is_read` **und** `a_range_beyond_the_target_is_refused_out_loud` fallen |
| Kürzungsmeldung entfernt | `cylinders_the_target_cannot_hold_are_reported_not_dropped` fällt |

Zwei Verfälschungen, zwei verschiedene Fehlerbilder. `one_side_can_be_selected`
bleibt bei der ersten grün — richtig, die Seitenvorgabe ist ein eigener Zweig.

Dazu eine Gegenprobe, die eine Dauer-Meldung auffliegen ließe: bei 80
Zylindern darf **keine** Kürzungsmeldung kommen.

`ctest` 224/224, alle 21 Gate-Kategorien 0, `verify_build_sources.py` 0/0,
qmake-Release-Build grün.

**Ehrlich zur Reichweite.**

- **Overdump beim SICHERN fehlt weiter** — und das ist die Hälfte, die
  Punkt 3.5 eigentlich meint. `FluxCaptureJob::setGeometry()` existiert,
  aber die Zahlen kommen aus `WorkflowTab::setHardwareDevice()`, also aus
  dem erkannten Laufwerk; kein Bedienelement kann „lies 83 Zylinder" sagen.
  Das ist GUI-Arbeit mit manuellem Rauchtest, kein `ctest`-Fall.
- Verdrahtet ist **ein** Wandlungspfad (SCP→ADF). SCP→D64, SCP→IMG,
  SCP→HFE und SCP→G64 leiten ihre Geometrie weiterhin selbst ab.
- Die Vorgabe kann über das Aufgezeichnete hinausgehen (blindes Lesen); die
  fehlenden Spuren zählen dann als nicht gelesen. Sie kann **nicht** über das
  Zielformat hinausgehen — dort greift Stufe 3, jetzt hörbar.
- Der Test benutzt absichtlich **nicht dekodierbaren** Kurzflux: geprüft wird,
  was der Wandler über den *Bereich* sagt, nicht was er aus den Spuren holt.
  Eine vollständige AmigaDOS-Spur je Zylinder wären 16 MB Testdaten für eine
  Aussage, die davon nicht abhängt.

---

### FLUX-6 — jede SCP-Teilaufnahme las sich als leere Diskette (2026-08-23, MF-481) → ✓ BEHOBEN

Punkt 2.3 der a8rawconv-Gap-Analyse fragt nach einem Bedienelement für die
SCP-Ablage (`scp-ss40`/`ds40`/`ss80`/`ds80`). Beim Nachmessen kam zuerst
etwas anderes heraus, und es ist schwerer: **das SCP-Plugin verlor jede
Datei, deren Aufzeichnung nicht bei Spur 0 beginnt** — still, ohne Fehler
beim Öffnen.

Gemessen an einer selbst geschriebenen Datei mit den Zylindern 20–29
beidseitig: **20 Spuren geschrieben, 0 gelesen.**

#### Zweimal derselbe Fehler: relativ statt absolut

1. `scp_get_geometry()` rechnete die Zylinderzahl aus der **Anzahl**
   aufgezeichneter Spuren (`end - start + 1`), der Spurzugriff darunter
   indiziert aber **absolut** (`cylinder * 2 + head`) und weist alles
   unterhalb von `start_track` ab. Bei `start_track = 40` meldete das Plugin
   10 Zylinder, der Aufrufer las 0–9, also die Spuren 0–19 — die es in der
   Datei nicht gibt.
2. `scp_open()` las die Offset-Tabelle ab Dateianfang und legte sie **ab
   Index `start_track`** ab. Die Tabelle ist absolut indiziert. Gelesen
   wurden also die Einträge 0–19 (Nullen) und nach 40–59 geschrieben —
   jeder Offset 0.

Solange `start_track == 0` war, fielen beide zusammen. Nur deshalb ist es
nie aufgefallen.

#### Autorität

a8rawconv 0.95 (`src/a8rawconv/rawdiskscp.cpp`, GPL-2-or-later,
Referenz-Orakel, wird nicht gebaut):

| Stelle | Aussage |
|---|---|
| `:126` | `tracks_to_read = (mEndTrack + 1) / image_track_step` — **`end + 1`, nicht der Bereich** |
| `:144` | `mTrackOffsets[image_track]` — absolut indiziert |
| `:106` | „the start/end track range is in terms of tracks per disk and not tracks per side" |
| `:100-105` | Das 96-tpi-Flag beschreibt das **Laufwerk**, nicht die Ablage in der Datei |

**Gegengelesen im eigenen Baum, und das ist der eigentliche Befund:** der
UFT-SCP-Schreiber legt die Offsets unter `track_offsets[track_num]` ab
(`uft_scp_writer.c:220`) und schreibt die volle 168-Einträge-Tabelle; der
kanonische Parser liest sie absolut (`uft_scp_parser.c:247`). **Nur dieses
Plugin wich ab.** Wieder der Fall, in dem ein Fakt mehrfach implementiert ist
und die Fassung läuft, die zufällig aufgerufen wird — dieselbe Diagnose wie
bei MF-475 (fünf Zellendauer-Stellen) und MF-479 (zwei Layout-Rechnungen).

#### Was NICHT geändert wurde, und warum

- **Keine verschobene Nummerierung.** Zylinder 20 bleibt Zylinder 20. Die
  bequeme Lösung — den Bereich auf 0 schieben — wäre forensisch falsch: die
  Spurnummer steht auf dem Medium.
- **„Nicht aufgezeichnet" bleibt von „unformatiert" getrennt.** Ein Zylinder
  unterhalb der Startspur meldet `UFT_ERROR_TRACK_NOT_FOUND`; eine Lücke
  *innerhalb* des Bereichs meldet `UFT_TRACK_UNFORMATTED`. Unformatiert ist
  eine Aussage über die **Diskette**, nicht aufgezeichnet eine über die
  **Aufnahme** — beides zusammenzuwerfen hieße, eine nie gemessene
  Eigenschaft des Mediums zu behaupten. Beides wird geprüft.
- **Die Kopfzahl bleibt 2**, auch wenn der Kopf `heads` etwas anderes sagt.
  Das ist die Regel des Orakels: SCP reserviert Einträge für beide Seiten
  auch bei einseitigen Abbildern (`:111-113`), und a8rawconv liest `mSides`
  aus, benutzt es aber **nicht** für die Ablage.

#### Rot-Probe

Beide Hälften einzeln zurückgedreht — jede für sich lässt
`a_partial_capture_starting_above_zero_is_not_lost` fallen, die übrigen drei
bleiben grün. Zwei unabhängige Ursachen, ein Symptom.

`ctest` 223/223, alle 21 Gate-Kategorien 0, `verify_build_sources.py` 0/0,
qmake-Release-Build grün.

**Ehrlich zur Reichweite.**

- **Punkt 2.3 ist damit NICHT erledigt.** Was fehlt, ist die erzwungene
  Deutung (`ss40`/`ds40`/`ss80`/`ds80`) als Bedienelement. Der konkrete
  Blocker: `uft_format_plugin_t::open(disk, path, ro)` hat keinen Platz für
  eine Option — das braucht entweder einen additiven `open_ex()` hinter dem
  `api_version`-Gate (vgl. ARCH-6) oder einen Weg über `uft_disk_t`.
- Behoben ist der Lesepfad des **Plugins**. Der kanonische Parser war schon
  richtig; wer über ihn geht (seit MF-478 der Umdrehungspfad in SCP→ADF),
  war nie betroffen — wohl aber die Geometrie, die dieselbe Konvertierung
  vom Plugin bezieht. Eine SCP-Teilaufnahme konvertierte also bis hierher zu
  einer leeren ADF.
- **Erweiterter Modus (Flag 0x40) bleibt offen.** a8rawconv liest die Tabelle
  dann von 0x80 (`:94-98`); UFT tut das nicht. Der eigene Schreiber setzt das
  Flag nie, deshalb fällt es hier nicht auf — eine fremde Datei mit
  erweitertem Kopf wird weiterhin falsch gelesen. Eigener Schritt.
- Geprüft gegen **selbst geschriebene** SCP-Dateien, nicht gegen eine fremde
  Teilaufnahme. Der Schreiber ist dieselbe Codebasis; ein Abbild von
  SuperCard Pro selbst wäre die stärkere Probe.

---

### FLUX-5 — der Feineinsteller war da und niemand konnte ihn drehen (2026-08-23, MF-480) → ✓ BEHOBEN

Punkt 2.1 der a8rawconv-Gap-Analyse. `flux_decoder_options_t::media_adjust_pct`
existiert seit MF-471, ist getestet, wird von `flux_pick_bitcell_ns()`
ausgewertet — und **kein Aufrufer setzte ihn je**. Fünfter Eintrag derselben
Liste nach MF-471/473/474/479.

Verdrahtet über `uft_convert_options_t::decode_cell_adjust_pct` (angehängt,
ABI), den Dispatcher und den SCP→ADF-Pfad. Standard 100 = unverändert.

#### Was die Messung ergab — und was dadurch aus dem Plan fiel

Vor dem Test stand die Frage: wirkt der Wert überhaupt? Ein Sweep über
Zellendauern beantwortete sie:

| wahre Zelle | 100 % | 150 % |
|---|---|---|
| 2400 ns (120 %) | 22/22 | 22/22 |
| 2600 ns (130 %) | 22/22 | 22/22 |
| 3000 ns (150 %) | **0/22** | 22/22 |

**Bis etwa ±30 % fängt die PLL den Versatz selbst ab.** Ein Test bei 4 %
Abweichung — der naheliegende — hätte grün geleuchtet und über den
Einsteller nichts ausgesagt.

#### Der erste Testaufbau war unphysikalisch

3000 ns je Zelle bei 300 min⁻¹ heißt 66666 Zellen je Umdrehung. Elf
AmigaDOS-Sektoren brauchen 95392. **Eine solche Spur gibt es nicht** — der
Test lieferte folgerichtig 14 von 22 Sektoren, weil die Spuren abgeschnitten
waren, und ich hätte das um ein Haar als „Feineinsteller wirkt nicht ganz"
gedeutet.

Der stimmige Fall geht in die andere Richtung: eine Diskette, die **dichter**
beschrieben wurde als das gewählte Profil annimmt — 1000 ns je Zelle, 200000
Zellen je Umdrehung, elf Sektoren passen bequem, echte 200 ms. Die aus dem
DD-Profil abgeleitete Zellendauer liegt dann um den Faktor zwei daneben. Das
ist genau der Fall eines **falsch gewählten Medienprofils**, und 50 % holt
die Diskette vollständig zurück.

#### Ein Rettungswerkzeug, kein Verbesserer

Derselbe Wert macht eine gesunde Diskette unlesbar — das prüft
`a_wrong_nudge_makes_a_good_stream_unreadable`, und deshalb ist der Standard
neutral. Ein Wert außerhalb 50…200 wird vom Medienprofil abgelehnt und der
Decoder fiele **still** auf 100 zurück; der Wandler meldet das jetzt, statt
den Bediener an einem unverbundenen Knopf drehen zu lassen.

#### Rot-Probe

Zuweisung `dopts.media_adjust_pct = opts->decode_cell_adjust_pct` entfernt:
2 der 4 Tests fallen, und zwar beide Richtungen (Rettung und Schaden). Die
Ausgangslage und die Bereichsprüfung bleiben grün — richtig, sie hängen nicht
an dieser Zeile.

`ctest` 222/222, alle 21 Gate-Kategorien 0, `verify_build_sources.py` 0/0,
qmake-Release-Build grün.

**Ehrlich zur Reichweite.**

- **Nicht belegt ist der Nutzen an einer marginalen Diskette** — dem Fall aus
  a8rawconvs Handbuch, wo man in Ein-Prozent-Schritten sucht und die Good/Bad-
  Quote beobachtet. Der synthetische Flux ist jitterfrei. Belegt ist: der Wert
  erreicht den Decoder, wirkt in beide Richtungen, und außerhalb des Bereichs
  wird er hörbar abgelehnt.
- Wirksam nur auf Pfaden, die ein Medienprofil setzen — das ist weiterhin
  **einer** (SCP→ADF).
- Der Bediener sieht den Knopf nicht: kein Dialog schreibt das Feld. Die
  Good/Bad-Quote für den Suchlauf steht dagegen bereits im Ergebnis
  (`sectors_converted` / `sectors_failed`).
- Der AmigaDOS-Fluxgenerator ist nach `tests/flux_gen/amigados/` gewandert,
  weil ihn jetzt zwei Tests brauchen. Sein Selbsttest gegen den an der echten
  Aufnahme belegten Dekoder (MF-438) ist mitgewandert und bleibt die einzige
  Begründung, warum man ihm glauben darf.

---

### FMT-25 — ATX erfand ein Layout, das es auf keiner Diskette gibt (2026-08-23, MF-479) → ✓ BEHOBEN

`uft_atx_write()` muss auch Spuren schreiben können, die keine **gemessenen**
Winkelpositionen mitbringen — jede Spur, die nicht aus einem ATX stammt, ist
so eine. Bis hierher verteilte der Schreiber sie als `s / n`: gleiche
Abstände, erster Sektor bei 0.

**So liegt keine Atari-Diskette.** Das DOS schreibt SD und ED verschränkt
(etwa 9:1 bzw. 13:1), und jede Spur ist gegen die vorige um rund 8 % einer
Umdrehung versetzt, weil der Kopf Zeit zum Umsetzen braucht. Bei ATX ist die
Winkelposition kopierschutzrelevant — ein gleichmäßiges Layout ist damit
nicht nur ungenau, es ist eine **Form, die es auf dem Medium nicht gibt**.
Prinzip 1 gilt in beide Richtungen: nichts still verlieren, aber auch nichts
still erfinden.

#### Der Rechner lag seit Monaten daneben

`uft_compute_interleave()` (`src/core/uft_interleave.c`) ist die wortgleiche
Portierung von a8rawconvs `compute_interleave`, hat einen eigenen Test
(`tests/test_interleave.c`) — und hatte **keinen Aufrufer**. Ein Eintrag mehr
in derselben Liste wie MF-471, MF-473 und MF-474: geprüft und ohne Wirkung.

Der Index in die Verschränkungstabelle ist die **Listenposition**, nicht die
Sektornummer. Grund: ATX kennt doppelte Sektornummern (Phantom-Sektoren), und
die liegen gerade **nicht** an derselben Stelle — über die Nummer indiziert
würden sie zusammenfallen und der Schutz wäre weg.

#### Gerechnet bleibt gerechnet

Die `UFT_WARN`-Meldung fällt **nicht** weg. Sie benennt jetzt, was gerechnet
wurde:

```
ATX: mindestens eine Spur brachte keine Winkelpositionen mit - ersatzweise
wurde das Atari-Verschraenkungslayout gerechnet (9:1 SD / 13:1 ED, 8%
Spurversatz). Diese Positionen sind GERECHNET, nicht gemessen
```

Näher am Medium ist nicht dasselbe wie gemessen. Wer eine gemessene Position
mitbringt, bekommt seine zurück — die Rechnung fasst sie nicht an, und genau
das prüft `measured_positions_are_never_overwritten`.

#### Ein vorher grüner Test musste geändert werden

`test_atx_roundtrip::a_track_without_positions_still_writes_and_reads`
verlangte **aufsteigende** Positionen. Das war keine Eigenschaft des Formats,
sondern des alten Ersatzlayouts: Verschränkung heißt gerade, dass
aufeinanderfolgende Sektornummern nicht aufeinanderfolgend auf der Spur
liegen. Die Forderung hätte das richtige Layout ausgeschlossen.

Geprüft wird jetzt, was der Kommentar dieses Tests immer schon meinte —
paarweise verschieden und im gültigen Bereich. Das ist eine Korrektur einer
falschen Zusicherung, keine Absenkung: die Bedingung ist strenger geworden
(paarweise statt nur benachbart).

#### Rot-Probe

Ersatzzweig zurück auf `s / n`: 4 der 5 neuen Tests fallen. Der fünfte
(`phantom_sectors_keep_distinct_positions`) bleibt grün — richtig so, `s / n`
liefert ebenfalls verschiedene Positionen; dieser Test bewacht eine andere
Eigenschaft.

`ctest` 221/221, alle 21 Gate-Kategorien 0, `verify_build_sources.py` 0/0,
qmake-Release-Build grün ohne neue Warnungen.

**Ehrlich zur Reichweite.**

- `uft_atx_write()` selbst hat **weiter keinen Aufrufer**. Dieser Schritt macht
  den Schreiber richtig, er liefert keinen ATX-Export. Was dafür fehlt, steht
  unten.
- Die Verschränkungsrechnung ist auf **Atari** zugeschnitten (128/256-Byte-
  Sektoren, 8 % Versatz). Für eine Spur aus einer anderen Welt ohne gemessene
  Positionen wäre sie falsch — ATX ist ein Atari-Format, deshalb ist das hier
  richtig und anderswo nicht übertragbar.
- Belegt ist die Rechnung gegen `compute_interleave`, **nicht** gegen ein
  reales ATX-Abbild. Es liegt keines im Korpus; ATX bleibt deshalb T2.

> **Was ATX-Export wirklich blockiert (gemessen, nicht vermutet).** Ein
> Konvertierungspfad nach ATX bräuchte ein eigenes `UFT_FORMAT_ATX` im
> Format-Enum — das Plugin meldet `UFT_FORMAT_DSK`, also Format-SSOT-Fläche.
> Der Smart-Export-Dialog (`src/gui/uft_smart_export_dialog.cpp`) **schreibt
> nichts**, er wählt nur einen Namen, und hat selbst keinen Aufrufer. Vor
> allem aber: es gibt **keinen einzigen Atari-Flux-Pfad** im Baum
> (Flux→Sektor existiert für D64, ADF, IMG — nicht für ATR/ATX), und
> `flux_decode_fm` hat nie eine reale Diskette dekodiert; im Korpus liegt
> kein Atari-Flux. Solange das so ist, wäre ein SCP→ATX-Pfad genau das, was
> die EINFRIER-REGEL (MF-363) verhindern soll: neuer Format-Layer-Code gegen
> einen unbelegten Decoder ohne Referenzabbild. **Der nächste Schritt ist
> Korpus-Arbeit, nicht Code:** eine reale Atari-Flux-Aufnahme, gegen die
> `flux_decode_fm` belegt werden kann.

---

### REC-1 — vier von fünf Umdrehungen wurden weggeworfen (2026-08-23, MF-478) → ✓ BEHOBEN

Eine SCP-Aufnahme enthält bis zu **fünf** Umdrehungen derselben Spur. Der
SCP→ADF-Wandler benutzte davon die erste. Das war kein Versehen und stand
seit jeher im Code, der es tat:

```c
// Flux der ersten Revolution lesen
// (Für bessere Ergebnisse könnte man alle Revolutions kombinieren)
```
> `src/formats/scp/uft_scp_plugin.c:377-378`

Alles nach Umdrehung 0 war damit **gemessene, übertragene, gespeicherte
Information ohne jede Wirkung** — bei einem Werkzeug, dessen erster Grundsatz
„Kein Bit verloren" lautet, die teuerste Art von Datenverlust: er passiert
nach dem Sichern, im eigenen Haus, an Daten, die man bereits hat.

#### Was daran mehr ist als „mehr Lesungen"

Parallel dazu lag seit MF-473 `multiread_class_t`
(`src/recovery/uft_multiread_pipeline.c`) im Baum — mit Test, mit
Dokumentation, mit **null** Aufrufern in `src/`. Die beiden Lücken sind
dieselbe Lücke: mehrere Lesungen desselben Sektors auszuwerten heißt sagen zu
können, **wie sie sich zueinander verhalten**.

| Klasse | Bedeutung | Was der Pfad vorher meldete |
|---|---|---|
| `STABLE_GOOD` | ein Inhalt, CRC geprüft | (Sektor geschrieben) |
| `STABLE_BAD_CRC` | ein Inhalt, CRC **immer** falsch | `sectors_failed++` |
| `WEAK` | mehrere Inhalte bei falscher CRC | `sectors_failed++` |
| `AMBIGUOUS_GOOD` | mehrere Inhalte trotz guter CRC | `sectors_failed++` |

Drei verschiedene Befunde, eine Zahl. Und der interessanteste davon ist
`STABLE_BAD_CRC`: das ist **kein Schaden**, sondern ein absichtlich falsch
aufgezeichneter Kopierschutz. Ihn als beschädigt zu melden ist falsch, ihn
als weak zu melden auch — und nochmal lesen hilft nicht, weil er stabil ist.
Genau das sagt der Wandler jetzt:

```
1 Sektoren stabiler CRC-Fehler: jede Umdrehung liest dasselbe,
die Pruefsumme bleibt falsch — Kopierschutz, erneutes Lesen aendert das nicht
```

#### Was sich am Verhalten NICHT ändert

Geschrieben wird weiterhin nur, was `recovered` ist — eine Lesung ohne
gültige Prüfsumme überschreibt nie gute Daten (MF-466: *Übereinstimmung ist
keine Prüfung*). Neu ist ausschließlich, dass mehr Lesungen zur Abstimmung
antreten und dass das Ergebnis benannt wird.

`min_passes` steht in diesem Pfad auf **1**, nicht auf dem Vorgabewert 3: wie
viele Umdrehungen es gibt, bestimmt die Datei und keine Wiederhol-Strategie.
Eine einzige Umdrehung heißt „keine Abstimmung möglich", nicht „Fehler".

#### Nebenbefund: `use_multiple_revs` war auch tot

Das Feld steht in `uft_convert_options_t` **und** in
`uft_convert_options_ext_t`, `uft_convert_default_options()` setzt es auf
`true`, und der Dispatcher reicht es durch — gelesen hat es niemand. Jetzt
schaltet es diesen Pfad: `false` liest ausdrücklich nur die erste Umdrehung.
Das ist zugleich die eingebaute Gegenprobe des Tests.

#### Die verdrahtete Kette

```
SCP-Datei, N Umdrehungen je Spur
  → uft_scp_read_track()        alle Umdrehungen (statt Plugin = nur die erste)
  → flux_raw_from_ns_intervals_indexed()   je Umdrehung (MF-475)
  → flux_decode_amiga()         je Umdrehung
  → multiread_add_pass()        je Sektorposition eine Stimme je Umdrehung
  → multiread_execute()         Abstimmung + classify_passes()
  → ADF (nur recovered) + Klassen-Meldung im Wandlungsbericht
```

Das Plugin bleibt für Kopfprüfung und Geometrie zuständig, damit diese Logik
nicht ein zweites Mal existiert; öffnet der kanonische Parser die Datei nicht,
fällt der Pfad auf die eine Umdrehung des Plugins zurück — also exakt auf das
bisherige Verhalten, nicht auf weniger.

#### Rot-Proben

| Verfälschung | Ergebnis |
|---|---|
| Umdrehungsschleife auf `r < 1` gekürzt | `a_sector_broken_in_revolution_zero_is_recovered_from_the_others` **und** `…_reported_as_weak` fallen |
| `*cls_out = res.class_` durch `STABLE_GOOD` ersetzt | `a_stable_crc_error_is_reported_as_copy_protection` **und** `…_reported_as_weak` fallen |

Zwei verschiedene Verfälschungen, zwei verschiedene Fehlerbilder — die Tests
prüfen tatsächlich zwei getrennte Dinge.

> **Zum Testaufbau.** `tests/test_convert_scp_adf.c` deckt denselben Pfad gegen
> eine echte Greaseweazle-Aufnahme ab und **SKIPt in CI**, weil die Datei 32 MB
> groß und gitignored ist. Ein Test, der nur lokal läuft, schützt keine
> Verdrahtung. Also erzeugt `tests/test_convert_scp_adf_multirev.c` eine
> AmigaDOS-Spur selbst. Der Kodierer dort ist die exakte Umkehrung von
> `decode_amiga_sector()` — und **prüft sich selbst**: der erste Test verlangt,
> dass der (gegen die echte Aufnahme belegte, MF-438) Dekoder alle 11 Sektoren
> byteidentisch zurückgibt. Ein Kodierer, den der belegte Dekoder liest, ist
> kein zweites unbelegtes Stück Code.

`ctest` 220/220, alle 21 Gate-Kategorien 0, `verify_build_sources.py` 0/0,
qmake-Release-Build grün ohne neue Warnungen.

**Ehrlich zur Reichweite.**

- Verdrahtet ist **ein** Pfad: SCP→ADF. `uftc_convert_scp_to_d64()` sucht sich
  weiterhin die Umdrehung mit den meisten Flusswechseln und dekodiert nur
  diese; SCP→HFE und SCP→G64 ebenso. Das ist jeweils ein eigener Schritt.
- Von den 12 exportierten Symbolen des Moduls haben jetzt 6 einen Aufrufer.
  `multiread_track()` (Callback-getriebenes Mehrfachlesen echter Hardware),
  `multiread_vote_buffers()`, `multiread_get_stats()` und
  `multiread_generate_report()` bleiben ohne — der HAL-Lesepfad ist der
  natürliche Ort dafür und nicht Teil dieses Schrittes.
- `weak_offset` und `distinct_contents` werden gefüllt und **nicht
  ausgewertet**. Sie zeigen auf den ATX-Weak-Chunk (MF-467/474) — dort gehören
  sie hin, und das ist der nächste sinnvolle Schritt: ein SCP mit weak-Sektoren
  nach ATX schreiben, statt sie nur zu zählen.
- Der Zusammenhang „Kopierschutz" ist **benannt, nicht klassifiziert**: der
  Wandler sagt „stabiler CRC-Fehler", nicht welches Schutzverfahren. Die
  Verknüpfung mit `src/protection/` ist offen.
- `ADF_SPT = 11` heißt: der Pfad deckt AmigaDOS-DD ab. Eine HD-Diskette mit 22
  Sektoren dekodiert der Decoder seit MF-452, dieser Wandler schreibt sie
  nicht.

---

### FMT-24 — ATX kann jetzt auch geschrieben werden (2026-08-22, MF-474) → ✓ BEHOBEN

Punkt 3.2 der a8rawconv-Gap-Analyse. Bis hierher konnte UFT ATX lesen und
nicht schreiben — und die Begründung dafür stand seit MF-467 in der Datei:

> „write_track is deliberately omitted: ATX encodes per-sector timing
> anomalies, weak bits, duplicate sector IDs, and FDC-status quirks that
> cannot be synthesised from sector payload alone."

**Diese Begründung war richtig, ist es aber nicht mehr.** Seit MF-467 trägt
die gelesene Spur genau diese Dinge: Status je Sektor (Daten- und
Adressfeld-CRC, fehlendes Datenfeld, Deleted-Mark, langer Sektor, weak),
doppelte Sektornummern als Phantomsektoren, und die Weak-Maske je Byte. Ein
Rückweg ist damit Wiedergabe, nicht Erfindung.

#### Eine Lücke, die ich selbst eingebaut hatte

Ein Feld fehlte: die **Winkelposition**. Mein MF-467-Leser las den
`mTimingOffset` aus dem Sektorkopf **gar nicht mehr** — die alte Fassung
hatte ihn wenigstens gelesen und dann verworfen (`(void)timing_off`).

Bei ATX ist das keine Nebensache. Zwei Sektoren mit derselben Nummer
unterscheiden sich **nur** durch ihre Position, und genau das ist der
Phantomsektor-Kopierschutz. Ohne sie wären es zwei nicht unterscheidbare
Kopien.

Neu in `uft_sector_t`, am Struct-Ende angehängt wie `id_crc_ok` davor:
`angular_position` (0…1 vom Indexpuls, ATX zählt in 1/26042 Umdrehung —
`diskatx.cpp:170`) plus `has_angular_position`.

> **Warum ein eigenes Flag und keine „negativ = unbekannt"-Konvention.**
> Sektoren entstehen an vielen Stellen per `memset(&s, 0, sizeof s)`. Der
> Nullwert hieße dann „liegt genau am Indexpuls" — eine Aussage, die niemand
> getroffen hat. Ein Flag ist memset-sicher; eine Konvention hätte jeder
> Erzeuger einzeln einhalten müssen, und einer hätte sie übersehen.

#### Der Schreiber

`uft_atx_write()` schreibt die ganze Diskette auf einmal. **Kein
`write_track`** je Spur, und das ist keine Bequemlichkeit: ein ATX ist eine
Kette von Spur-Datensätzen ohne Offset-Tabelle — eine Spur nachträglich zu
ersetzen hieße, alles danach zu verschieben.

Feldsemantik ist die Umkehrung des Lesers, Beleg für Beleg gegen
`write_atx` (`diskatx.cpp:216-416`), inklusive der Feinheit, dass ein
Adressfeld-CRC-Fehler **beide** Bits setzt (`:346-349`) und der Abschluss der
Chunk-Liste acht Nullbytes sind (`:415-416`).

**Was der Schreiber nicht kann, sagt er.** Eine Spur aus einer anderen Quelle
hat keine Winkelpositionen — kein anderes Format liefert sie. Sie werden dann
gleichmäßig verteilt, und der Aufruf meldet:

```
ATX: mindestens eine Spur brachte keine Winkelpositionen mit — sie wurden
gleichmaessig verteilt. Diese Positionen sind GERECHNET, nicht gemessen
```

Ein gleichmäßiges Layout sieht plausibel aus und ist es nicht; bei einem
Format, dessen Positionen Kopierschutz tragen, wäre stilles Erfinden der
schlimmere Fehler als gar kein Export.

#### Rot-Proben

Das write-read-Muster hat in diesem Projekt schon zwei echte
Korruptionsfehler gefunden (SCP MF-318, IMD MF-320), deshalb steht es hier am
Anfang. Zwei gezielte Verfälschungen zeigen, dass der Test den Inhalt prüft
und nicht die Anwesenheit von Code:

| Verfälschung | Ergebnis |
|---|---|
| Leser verwirft die Position wieder | 3 von 4 Fällen fallen, darunter der Phantomsektor-Fall |
| Weak-Chunk mit falschem Typ geschrieben | genau ein Fall fällt: `back.sectors[3].weak == true` |

`ctest` 218/218, alle 21 Gate-Kategorien 0.

**Ehrlich zur Reichweite.**

- Geprüft ist **semantische** Identität der Spur über den Rundlauf, nicht
  Byte-Identität der Datei. Byte-Identität hinge an Dingen, die das Format
  offen lässt (Creator-Kennung, Reihenfolge gleichrangiger Chunks) — sie zu
  verlangen hieße, eine Konvention zu prüfen statt den Inhalt.
- Der Gegentest gegen **a8rawconv-Ausgabe desselben Dumps**, den die
  Gap-Analyse vorschlägt, steht aus: dafür braucht es a8rawconv als Binary
  im Testharness und ein reales Abbild. Beides ist der nächste Schritt und
  ein eigener.
- **Kein Aufrufer exportiert bisher ATX.** `uft_atx_write()` ist die Funktion
  dafür; der Smart-Export-Dialog kennt sie noch nicht.
- Lange Sektoren werden als `LOST_DATA|DRQ` zurückgeschrieben, aber ohne
  den zugehörigen Ext-Chunk — die physische Größe geht dabei verloren. ATX
  speichert ohnehin nur 128 Byte je Sektor (`write_atx:381`), der Ext-Chunk
  ist reine Beschreibung. Nachzuholen, wenn ein reales Abbild mit langen
  Sektoren vorliegt.

---

### FLUX-3 — stabil-mit-CRC-Fehler ist Kopierschutz, nicht Schaden (2026-08-22, MF-473) → ✓ BEHOBEN

Punkt 3.8 der a8rawconv-Gap-Analyse, und der Punkt, der die fünf Umdrehungen
aus FLUX-2 erst zu etwas nütze macht.

**Was schon da war.** MF-466 hat geklärt, dass Übereinstimmung keine Prüfung
ist: `recovered` verlangt seither mindestens eine CRC-geprüfte Lesung. Und
`vote_buffer()` hatte die CRC-Vorauswahl schon — gibt es eine geprüfte
Lesung, zählen die ungeprüften nicht mit.

**Was fehlte.** Die Aussage, was mehrere Lesungen *zueinander* sagen.
`has_weak_bits` beantwortet nur „irgendein Byte wich ab" — und genau das
trifft auf den interessantesten Fall **nicht** zu.

Der Algorithmus steht in `src/a8rawconv/disk.cpp:236-365` (`sift_sectors`,
Referenz-Orakel, wird nicht gebaut):

1. **CRC-Vorauswahl**, getrennt für Adress- und Datenfeld (`:240-254`).
2. Bleibt danach **genau ein** Inhalt → stabil.
3. Bleiben **mehrere** → der häufigste gewinnt; bei schlechter CRC ist der
   Sektor zusätzlich weak ab dem längsten gemeinsamen Präfix (`:331-345`).

Daraus vier Klassen, und die dritte Zeile ist die, um die es geht:

| Inhalte | CRC | Klasse | Bedeutung |
|---:|---|---|---|
| 1 | gut | `STABLE_GOOD` | gesunder Sektor |
| mehrere | schlecht | `WEAK` | weak ab `weak_offset` |
| **1** | **schlecht** | **`STABLE_BAD_CRC`** | **Kopierschutz** — absichtlich falsche CRC, jedes Mal dieselbe |
| mehrere | gut | `AMBIGUOUS_GOOD` | sehr ungewöhnlich; a8rawconv warnt und behält eine |

a8rawconv sagt an dieser Stelle wörtlich „Stable CRC error detected"
(`disk.cpp:364`). Der Unterschied ist praktisch, nicht akademisch:

- Als **weak** gemeldet wäre er falsch — er wackelt nicht.
- Als **beschädigt** gemeldet wäre er falsch — er ist genau so gewollt.
- **Nochmal lesen hilft nicht**, weil er stabil ist. Ein Werkzeug, das das
  nicht unterscheidet, schickt den Benutzer in eine Wiederholung, die
  nichts bringen kann.

**Der Weak-Offset ist das längste gemeinsame Präfix ALLER überlebenden
Lesungen**, nicht der Abstand zweier davon. Damit hat er dieselbe Bedeutung
wie der Weak-Chunk eines ATX-Abbilds (`src/formats/atx/uft_atx.c`, MF-467) —
der eine Wert lässt sich unverändert in das andere Format schreiben.

**Neu** in `multiread_sector_t`, angehängt statt eingefügt (ABI):
`class_`, `weak_offset`, `distinct_contents`, plus
`multiread_class_name()`. Die CRC-Vorauswahl steht jetzt als eigene
Funktion `pass_survives_crc_sift()` da — Klassifikation und Abstimmung
benutzen dieselbe Regel, statt sie zweimal zu schreiben.

**Rot-Probe, zweistufig.** Ohne die Implementierung linkt der Test nicht —
das beweist nur, dass Code fehlt. Deshalb die schärfere Variante: die
Unterscheidung selbst verfälscht (`STABLE_BAD_CRC` → `WEAK`). Es fällt
**genau ein** Test, `stable_bad_crc_is_protection_not_weakness`, alle
anderen zwölf bleiben grün. Der Test prüft also die Unterscheidung und
nicht die Anwesenheit von Code.

`ctest` 217/217, alle 21 Gate-Kategorien 0.

**Ehrlich zur Reichweite.** Die Pipeline hat weiterhin **keine Aufrufer in
`src/`** — sie ist getestet, aber nicht verdrahtet (steht so schon in
BUG-11). Was jetzt fehlt, ist der Weg von der Aufzeichnung dorthin: der
FluxCaptureJob sammelt seit FLUX-2 bis zu fünf Umdrehungen ein, gibt sie
aber unverändert an den SCP-Schreiber weiter, statt sie je Sektorposition
zu klassifizieren. Das ist der nächste Schritt und ein eigener.

Ebenfalls offen: die „salvage"-Hälfte von 3.8 — aus mehreren schlechten
Lesungen eine bessere zusammensetzen. Die Klassifikation sagt jetzt, wo das
überhaupt Sinn ergibt (`WEAK` ja, `STABLE_BAD_CRC` nein); das Zusammensetzen
selbst tut sie nicht.

---

### FLUX-2 — Umdrehungszahl war fest verdrahtet, Voreinstellung war die falsche (2026-08-22, MF-472) → ✓ BEHOBEN

Punkt 2.2 der a8rawconv-Gap-Analyse. Der Flux-Capture-Job bekam seine
Umdrehungszahl aus dem Quelltext:

```cpp
/* src/workflowtab.cpp:540, vorher */
m_captureJob->setRevolutions(2);
```

`FluxCaptureJob` kann seit jeher 1…5 (`setRevolutions()` klemmt selbst), und
der SCP-Schreiber reicht bis zu fünf Umdrehungen unverändert durch (MF-327).
Nur wählen konnte sie niemand.

**Die Zwei war dabei nicht bloß unflexibel, sondern die falsche Zahl.**
a8rawconv setzt `g_revs = 5` (`a8rawconv.cpp:52`) und begründet es im
Handbuch, Abschnitt „Imaging physical floppy disks":

> „Use the preservation modes of the imaging software when possible. This
> typically records five revolutions of all tracks on the disk. For older
> disk formats this often over-images the disk, but that's much better than
> finding out later you're missing part of it. Another reason to do this is
> that **if the physical disk is deteriorating, you only want to do one pass
> on the disk because you may not get another chance.**"

Genau das war der Fehler: bei einer zerfallenden Diskette gibt es keinen
zweiten Durchgang, und drei verworfene Umdrehungen sind drei verlorene
Chancen. Zu viel aufzuzeichnen kostet Plattenplatz; zu wenig kostet die
Diskette.

**Neu:** eine SpinBox 1…5 in der Operations-Zeile, Voreinstellung **5**.

**Und eine Warnung bei 1**, weil die Untergrenze belegt ist — dasselbe
Handbuch, wörtlich:

> „For Atari 8-bit disks, you must have at least two revolutions imaged per
> track … **An index-aligned, one-rev image will not work because sectors can
> cross the index mark.**"

Das gilt nicht nur für Atari: Amiga und Commodore sind ebenfalls nicht
indexsynchron. Die Warnung nennt das, sagt was 5 dafür leistet, und läuft
**vor** dem Anlaufen des Laufwerks — eine Zustimmung danach wäre wertlos.
Die Entscheidung bleibt beim Menschen: es gibt Fälle, in denen eine
Umdrehung alles ist, was die Diskette noch hergibt.

#### AUD-8 — dabei gefunden: 17 alte `ui_*.h` in der Repo-Wurzel

Der erste Build nach der UI-Änderung scheiterte an

```
error: 'class Ui::TabWorkflow' has no member named 'spinRevolutions'
```

— obwohl `forms/tab_workflow.ui` das Widget enthielt und `uic` es korrekt
erzeugte. Der Grund steht in einer einzigen Zeile des erzeugten Makefiles:

```
INCPATH = -IC:/Users/Axel/Github/UnifiedFloppyTool-4.1.0 -I. -I...
```

**Die Repo-Wurzel steht vor dem Shadow-Build-Verzeichnis.** Dort lagen 17
`ui_*.h` vom **18. April** (eines vom 14. Mai) — Reste eines alten
In-Source-Builds. Sie überschatteten alles, was `uic` frisch erzeugte; der
Compiler sah eine Oberfläche von vor vier Monaten. Nachgemessen: zu jeder
der 17 Dateien existiert ein `.ui` unter `forms/`, sie sind also wirklich
nur Reste und keine notwendige Zutat.

Das ist dieselbe Klasse wie AUD-6 (MF-369, stale `release/`- und
`debug/`-Verzeichnisse), nur eine Ebene tiefer — und mit derselben
Eigenschaft: **alle sind gitignored** (`.gitignore:22`), CI sieht sie nie,
nur der lokale Baum leidet, und zwar still. Der Fehler liest sich dabei wie
ein Fehler im eigenen Code, nicht wie ein Umgebungsproblem.

Die bestehende Kategorie „in-source build artifacts" deckt jetzt auch
`ui_*.h` und `moc_*.cpp` in der Wurzel ab. Rot-Probe: zwei der alten
Dateien zurückgelegt → Meldung mit Anzahl, Namen und Ursache; entfernt →
wieder grün.

**Ehrlich zur Verifikation:** der komplette qmake-Release-Build übersetzt
und linkt nach dem Aufräumen (0 Fehler), `ctest` 217/217, alle 21
Gate-Kategorien 0. Die GUI-Strecke — Wert einstellen, Aufzeichnung starten,
Warnung bei 1 sehen — wurde **nicht** durchgeklickt; ohne Flux-Hardware
lässt sich davon nur der Dialog selbst prüfen.

---

### FLUX-1 — die Drehzahl beim Lesen bestimmt das Laufwerk, nicht die Diskette (2026-08-22, MF-471) → ✓ GRUNDLAGE STEHT

Erster Punkt der a8rawconv-Gap-Analyse (3.1), und ihr wichtigster: ohne ihn
ist für Atari alles Weitere wertlos.

**Der Fall.** Eine Atari-Diskette wurde mit **288 min⁻¹** beschrieben. Beim
Sichern liegt sie in einem 300-min⁻¹-Laufwerk — dem Normalfall bei
Greaseweazle, SCP und KryoFlux. Die Drehzahl beim Lesen bestimmt das
Laufwerk, also laufen dieselben Bitzellen um 288/300 = **4 % schneller**
vorbei. Wer mit der nominalen Zellendauer dekodiert, liegt um genau diese
4 % daneben: 4000 ns Nennwert gegen 3840 ns tatsächlich.

**Was UFT stattdessen tat:**

```c
/* src/flux/uft_flux_decoder.c:507, vorher */
double bitcell_ns = opts->bitcell_ns;
if (bitcell_ns == 0) {
    bitcell_ns = FLUX_MFM_DD_BITCELL_NS;  /* Default to DD */
}
```

Keine Messung, keine Anpassung — bei fehlender Vorgabe die Annahme „MFM DD,
2 µs". Für eine Atari-FM-Diskette ist das nicht 4 % daneben, sondern der
Faktor 2.

**Der Rechenweg** stammt aus a8rawconv 0.95 (`src/a8rawconv/`,
Referenz-Orakel, wird nicht gebaut):

```cpp
// a8rawconv.cpp:133-136
// Atari disk timing produces 250,000 clocks per second at 288 RPM. We must
// compute the effective sample rate given the actual disk rate.
const double cells_per_rev = 250000.0 / (288.0 / 60.0) * (hd ? 2 : 1);
double scks_per_cell = rawTrack.mSamplesPerRev / cells_per_rev
                       * g_clockPeriodAdjust;
```

Der Kniff: **die Laufwerksdrehzahl muss man gar nicht kennen.** Sie steckt
bereits in der gemessenen Umdrehungsdauer des Abbilds. Was das Profil
beitragen muss, ist nur die nominale Zellenzahl je Umdrehung des *Mediums*.

```
Zellen je Umdrehung = Datenrate × 60 / Medien-Drehzahl     (Profil)
Zellendauer         = gemessene Umdrehungsdauer / Zellen je Umdrehung
```

`mSamplesPerRev` ist dort gemessen, nicht angenommen — aus den
Index-Impulsen (`rawdiskkf.cpp:203-206` für KryoFlux, `rawdiskscp.cpp:167-175`
für SCP), und zwar **über alle Zwischenräume gemittelt**: ein einzelner trägt
den vollen Motor-Jitter.

**Neu:** `include/uft/flux/uft_media_profile.h` + `src/flux/uft_media_profile.c`
— acht Medienprofile mit belegten Konstanten, die Umdrehungsmessung aus
Index-Impulsen, die Adaption, und der Prozent-Feineinsteller (Punkt 2.1 der
Analyse, a8rawconvs `-p`, Grenzen 50…200). Er steht dort, weil er in der
Quelle im selben Ausdruck steht; ihn wegzulassen hieße, die Formel halb zu
übernehmen.

| Medium | min⁻¹ | Zelle | Beleg |
|---|---:|---:|---|
| Atari FM | **288** | 4 µs | `a8rawconv.cpp:133`, `analyze.cpp:37`, `encode.cpp:4` |
| Atari MFM | **288** | 2 µs | `analyze.cpp:42`, `a8rawconv.cpp:1143` |
| PC 360K / 720K | 300 | 2 µs | `analyze.cpp:47-49` |
| PC 1.2M | 360 | 1 µs | 5,25"-HD dreht mit 360 |
| PC 1.44M | 300 | 1 µs | — |
| Amiga DD | 300 | 2 µs | — |
| Apple II GCR | 300 | 4 µs | `encode.cpp:5` |

**Zwei echte Aufrufer**, damit daraus kein Modul ohne Verbraucher wird
(ARCH-19/22 sind die Mahnung dazu):

1. **Laufwerksprofile.** `UFT_DRIVE_ATARI_810`, `_1050` und `_XF551` gab es
   nicht — die einzigen Laufwerke im Baum, die nicht mit 300 min⁻¹ drehen,
   fehlten ganz. Angehängt, nicht eingefügt: bestehende Enum-Werte dürfen
   sich nicht verschieben. Geometrien aus `diskatr.cpp:42-56` (SD 18×128 FM,
   ED 26×128 MFM, DD 18×256 MFM).
2. **Der Decoder.** `flux_decode_mfm()` wählt die Zellendauer jetzt in drei
   Stufen: ausdrücklich vorgegebenes `bitcell_ns` → Medienprofil plus
   gemessene Umdrehung → die alte Annahme. **Ohne gesetztes Profil ändert
   sich nichts** (`opts.media == UFT_MEDIA_UNKNOWN` ist der Default), und
   schlägt die Messung fehl, greift Stufe 3 — keine halb gerechnete Zahl,
   die wie eine Messung aussähe.

**Rot-Probe.** `tests/test_media_profile.c`, zehn Fälle. Der entscheidende:
dasselbe Abbild wird zweimal dekodiert, einmal mit Atari-MFM-Profil und
einmal mit PC-720K — **gleiche nominale Zellendauer, verschiedene
Medien-Drehzahl**. Das Verhältnis der Bitraten muss 300/288 sein. Ohne die
Verdrahtung sind beide Läufe identisch, und genau dieser Fall fällt um.

> Für diesen Test läuft die PLL-Regelung abgeschaltet. Mit Regelung zieht sie
> die Periode zum tatsächlichen Fluss hin, beide Läufe konvergieren, und der
> Startwert wird unsichtbar — also genau die Größe, um die es geht. Das ist
> kein Kunstgriff, um grün zu werden: die *Wahl* der Startperiode ist der
> Gegenstand, das Einregeln danach ist ein anderer.

**Ehrlich zur Reichweite.** Verifiziert ist die Rechnung gegen eine
funktionierende Implementierung, nicht an einer realen Atari-Diskette — es
liegt keine im Korpus. Was noch **nicht** getan ist:

- Kein Aufrufer setzt `opts.media` bisher automatisch. Der Weg
  „Abbild → Medium erkennen → Profil setzen" ist offen; wer das Profil kennt,
  kann es heute schon übergeben.
- Der FM-, GCR- und Amiga-Zweig des Decoders wählt seine Zellendauer
  weiterhin selbst — nur `flux_decode_mfm()` ist umgestellt.
- Zonen-Aufzeichnung (Commodore, Apple) gilt je Zone, nicht je Diskette. Das
  Modul rechnet je Diskette und sagt das im Kopf.

Damit sind die Punkte 3.1 und 2.1 der Gap-Analyse abgedeckt, soweit sie ohne
GUI-Arbeit gehen. Offen bleiben 2.2 (`-revs` als Einstellung), 2.3
(SCP-Layout), 3.2 (ATX-Writer), 3.3–3.9.

---

### BUILD-2 — der macOS-Build zerbrach an MF-468, und zwar zu Recht (2026-08-22, MF-470) → ✓ BEHOBEN

Mein Fehler aus BUILD-1, von der CI gefunden: `build-macos` rot.

```
../src/hal/uft_scp_direct.c:48:12: fatal error: 'libusb-1.0/libusb.h' file not found
```

**Warum es überhaupt erst jetzt auffiel.** Vor MF-468 setzte der qmake-Build
`UFT_HAS_LIBUSB` nirgends — dieser `#ifdef`-Zweig wurde auf macOS also **nie
übersetzt**. Der Schalter hat nicht den Fehler verursacht, er hat ihn
sichtbar gemacht.

**Warum Linux und Windows trotzdem grün waren.** `pkg-config --cflags
libusb-1.0` liefert den Pfad **bis einschließlich** des
`libusb-1.0`-Verzeichnisses, und dort heißt der Header schlicht `libusb.h`:

| Plattform | Include-Pfad | funktioniert |
|---|---|---|
| Linux (pkg-config) | `-I/usr/include/libusb-1.0` | beide Formen — weil `/usr/include` **ohnehin** im Standardsuchpfad liegt |
| Windows (MinGW-Präfix) | `-I<präfix>/include` | nur `<libusb-1.0/libusb.h>` |
| macOS (Homebrew) | `-I/opt/homebrew/Cellar/libusb/1.0.30/include/libusb-1.0` | nur `<libusb.h>` |

Der Linux-Erfolg war also Zufall: die lange Form geht dort nur auf, weil das
Elternverzeichnis zusätzlich im Standardpfad steht. Auf Homebrew steht das
Präfix nirgends — dort geht nur die kurze.

**Behoben an der Wurzel**, in `uft_scp_direct.c` und `uft_xum1541.c`:

```c
#if defined(__has_include)
#  if __has_include(<libusb.h>)
#    include <libusb.h>
#  else
#    include <libusb-1.0/libusb.h>
#  endif
#else
#  include <libusb-1.0/libusb.h>
#endif
```

**Bewusst NICHT im Build-System geflickt.** Der erste Versuch hängte in der
`.pro` zusätzlich `pkg-config --variable=includedir` an. Das hätte funktioniert
— und zwei Build-Systeme unterschiedlich geflickt zurückgelassen, also genau
die Asymmetrie erzeugt, gegen die BUILD-1 antritt. Zurückgenommen; CMakes
Kommentar, der dasselbe Falsche behauptete („can resolve
`<libusb-1.0/libusb.h>`"), ist korrigiert.

**Rot-Probe**, lokal mit dem Homebrew-Pfadlayout nachgestellt:

```
$ gcc -fsyntax-only -DUFT_HAS_LIBUSB=1 -I<präfix>/include/libusb-1.0 src/hal/uft_scp_direct.c
src/hal/uft_scp_direct.c:48:12: fatal error: libusb-1.0/libusb.h: No such file or directory
```

Nach dem Fix übersetzen beide Dateien unter **beiden** Pfadlayouts fehlerfrei.

**Lehre, die zu BUILD-1 gehört:** ich hatte MF-468 als „am kompletten
Release-Build verifiziert" gemeldet — auf **einer** Plattform. Ein Schalter,
der neu gesetzt wird, schaltet auf jeder Plattform Code scharf, der dort noch
nie übersetzt wurde. „Baut bei mir" ist bei einem Build-System-Eingriff kein
Nachweis, und drei Plattformen hat nur die CI.

---

### HAL-2 — FluxEngine: der Befund stimmt, die Beschreibung war zehn Monate alt (2026-08-22, MF-470) → ✓ KLARGESTELLT

Nachgeprüft, nicht angenommen. Die Analyse führte drei Flag-Stubs als offene
Verdrahtungslücke:

> `setMotor()`, `seekCylinder()`, `recalibrate()` setzen nur Member-Flags,
> rufen kein CLI auf. Entweder CLI-Anbindung nachziehen oder die drei
> Capabilities ehrlich aus dem Capability-Set nehmen, damit `rewireV2()` die
> Buttons deaktiviert.

**Die zweite Hälfte ist längst geschehen** — und die erste hat kein Ziel mehr:

| gemessen | Ergebnis |
|---|---|
| FluxEngine-Klassen im Baum | genau eine: `FluxEngineProviderV2` |
| `setMotor` / `seekCylinder` / `recalibrate` darin | kommen **nicht vor** |
| `grep -rn "::setMotor\|void setMotor"` über `src/` | **null Treffer** |
| Mixins `ControlsMotor` / `SeeksHead` / `Recalibrates` | nicht komponiert, mit negativen `static_assert` festgehalten |
| Knöpfe in der erzeugten Verdrahtung | `wire_action<cap::X>` nimmt bei fehlender Capability den `setEnabled(false)`-Zweig |
| Test dafür | `tests/test_wiring_runtime.cpp`, Klausel 2 + 2b (deaktiviert **und** nicht verbunden) |

Der V1-Provider existiert nicht mehr; die drei Stubs sind mit ihm
verschwunden. Was blieb, war der Dateikopf, der sie **im Präsens** aufzählte
(„`setMotor()` STUB — sets `m_motorOn` flag only") und damit den nächsten
Leser auf die Suche nach gelöschtem Code schickte — mich eingeschlossen.

Der Befund selbst bleibt stehen, weil er die Begründung für die ausgelassenen
Mixins trägt; er steht jetzt im Präteritum und nennt, wo die Abwesenheit heute
festgehalten und geprüft wird.

**Nicht geändert:** kein Code. Eine CLI-Anbindung wäre auch nicht möglich —
`fluxengine` hat weder ein `motor`- noch ein `seek`- noch ein
`recalibrate`-Unterkommando; das Werkzeug steuert beides implizit über `-c`
innerhalb jedes Lese-/Schreibaufrufs. Genau deshalb sind die Capabilities
ausgelassen, und das ist die richtige Antwort, nicht eine offene Lücke.

---

### HAL-1 — SCP: die GUI gab dem Provider `nullptr` (2026-08-22, MF-469) → ✓ VERDRAHTET, Bench offen

Zweiter Teil der Lücke aus BUILD-1. Nachdem der Schalter im Release-Build
gesetzt ist, fehlte noch der Aufruf:

```cpp
/* src/hardwaretab.cpp:810, vorher */
m_providerV2 = std::make_unique<::uft::hal::SCPProviderV2>(nullptr);
```

Jeder SCP-Klick landete damit im ehrlichen Stub — obwohl der C-HAL dahinter
seit MF-254 fertig ist.

**Kein Runner, ein Handle.** Die naheliegende Lösung wäre eine
Runner-Fabrik gewesen, wie sie für Applesauce, ADF-Copy, KryoFlux,
FluxEngine, FC5025 und USB-Floppy existiert (19 `make_*_runner`-Fabriken im
Baum). `SCPProviderV2` nimmt aber keinen Runner, sondern ein
`uft_scp_direct_ctx_t*` — und sein eigener Kopf sagt dazu:

> `scp_provider_v2.h:93` — „The handle must remain valid for the lifetime of
> this provider. **Ownership is NOT transferred** — call
> `uft_scp_direct_close()` externally."

Besitzt es also niemand, leckt jede Controller-Umschaltung das USB-Handle und
hält das Gerät für jeden anderen Prozess belegt. Deshalb hält
`HardwareTab` es jetzt in einem `unique_ptr` mit eigenem Deleter und gibt es
frei, bevor `rewireV2()` läuft — nach dem Zerstören des Providers, der einen
rohen Zeiger darauf hält.

**Wenn kein Gerät da ist**, bleibt das Handle null und der Provider meldet
weiter seinen ehrlichen Fehler. Der bedeutet jetzt aber etwas anderes: „kein
Gerät", nicht mehr „nicht verdrahtet". Damit der Unterschied sichtbar ist,
steht der Grund im Log, in drei unterscheidbaren Fällen:

| Fall | Meldung |
|---|---|
| geöffnet | `SuperCard Pro: device opened (libusb)` |
| Build ohne libusb | `this build was compiled without libusb - no production transport` |
| kein Gerät | `no device found (rc=…)`, mit dem Hinweis auf Zadig/WinUSB |

Der letzte Hinweis ist kein Beiwerk: unter Windows belegt der
FTDI-Standardtreiber das Gerät, ohne libusb-Endpunkte anzubieten — der
häufigste Grund, warum ein angeschlossener SCP nicht gefunden wird.

**Worauf die Verdrahtung ruht, und was davon geprüft ist.** Drei Verträge,
alle bereits durch Tests gedeckt:

| Vertrag | Test |
|---|---|
| `uft_scp_direct_open()` ohne Gerät → Fehler **und** `ctx == NULL` | `test_scp_direct_hal.c::open_without_hardware` |
| `uft_scp_direct_close(NULL)` ist zulässig | `test_scp_direct_hal.c::close_null_is_safe` |
| `SCPProviderV2(nullptr)` liefert einen ProviderError, keinen Absturz | `test_scp_provider_v2.cpp` (4 Fälle) |

**Ehrlich zur Verifikation:** der komplette qmake-Release-Build übersetzt und
linkt (`UnifiedFloppyTool.exe`, 0 Fehler), `ctest` 216/216. Die GUI-Strecke
selbst — Controller „scp" wählen, Verbinden drücken — wurde **nicht**
durchgeklickt, und ohne SCP-Hardware ließe sich dabei ohnehin nur der
„kein Gerät"-Zweig beobachten. Der Tier-3-Nachweis am echten Gerät bleibt
UFT-008 und ist extern delegiert.

`uft_scp_direct_write_flux()` bleibt unverändert NOT_IMPLEMENTED, bis der
Lesepfad an echter Hardware verifiziert ist — ein fehlerhafter Flux-Strom
kann forensische Medien physisch beschädigen. Das ist Projektregel, keine
Lücke.

---

### BUILD-1 — `UFT_HAS_LIBUSB` kannte nur CMake, der Release-Build baute SCP als Stub (2026-08-22, MF-468) → ✓ BEHOBEN + 21. WÄCHTER

Der samdisk-portierte SCP-Lesepfad in `src/hal/uft_scp_direct.c` ist seit
MF-254 fertig (474 Zeilen, Akkumulator-Reset je Umdrehung, 22/22 Opcodes
byte-verifiziert). Erreichbar war er trotzdem nicht.

`UFT_HAS_LIBUSB` wurde **ausschließlich** von `CMakeLists.txt:155` gesetzt —
also nur im Test-Build. Die `UnifiedFloppyTool.pro`, aus der die Releases
entstehen, kannte das Wort „libusb" nicht einmal. Drei Stellen fragen den
Schalter ab:

```
src/hal/uft_scp_direct.c    die ganze libusb-Implementierung
src/hal/uft_xum1541.c       dito
src/hardwaretab.cpp:998     `_has_production_transport` für scp/xum1541
```

Im Release fielen also alle drei in den Stub-Zweig — **einschließlich der
GUI-Anzeige**, die den Transport folgerichtig als nicht produktiv meldete. Die
Tests liefen dabei grün, weil CMake den Schalter setzt.

**Behoben:** die `.pro` erkennt libusb jetzt in derselben Reihenfolge wie
CMake — pkg-config zuerst, dann eine Suche über bekannte Präfixe (auf Windows
das MinGW-Präfix, das Qt mitliefert), mit `qmake LIBUSB_PREFIX=/pfad` als
Übersteuerung. Findet sie nichts, sagt sie das im Build-Log, statt still einen
anderen Build zu erzeugen.

Verifiziert am kompletten qmake-Release-Build:

```
-DUFT_HAS_LIBUSB=1 ... -IC:/Qt/Tools/mingw1310_64/include
g++ ... -o release/UnifiedFloppyTool.exe ... -LC:/Qt/Tools/mingw1310_64/lib -lusb-1.0
```

Beide HAL-Dateien übersetzen mit dem Define warnungsfrei, die Anwendung linkt.

#### Der 21. Wächter: `scripts/define_parity_gate.py`

Der eigentliche Befund ist nicht das fehlende Define, sondern **dass es
niemand bemerken konnte**. `verify_build_sources.py` vergleicht *Quelldateien*
zwischen den Build-Systemen — Defines nicht. Ein Schalter, der in einem Build
an und im anderen aus ist, lässt beide fehlerfrei durchlaufen und trotzdem
verschiedenen Code entstehen. Das ist dieselbe Klasse Divergenz, nur
unsichtbar.

Drei Regeln:

| | |
|---|---|
| **A** | ein `UFT_*`, das nur ein Build-System setzen kann |
| **B** | ein `UFT_HAS_*`/`UFT_ENABLE_*`, das der Code abfragt und **kein** Build setzt — ein Schalter, der nie an sein kann |
| **C** | ein `UFT_*`, das gesetzt wird und im Quellcode nirgends vorkommt — ein Schalter ohne Verbraucher |

Regel A wertet die **Fähigkeit** aus, nicht das Ergebnis: beide Seiten stehen
in Bedingungen (`packagesExist`, `if(LIBUSB_FOUND)`), die das Skript nicht
ausführt. Ob zwei Bedingungen dasselbe *meinen*, kann nur ein Mensch
entscheiden — deshalb hat die Baseline eine Begründungspflicht je Eintrag.

**Zwei Rot-Proben:**

1. Die `.pro`-Änderung zurückgenommen → `build define parity: 1`. Der Wächter
   erkennt genau den Fehler, für den er gebaut wurde.
2. Ein frei erfundener CMake-Schalter → von Regel A **und** C gemeldet.

**Drei Schwächen im Wächter selbst, vor dem Scharfschalten gefunden:**

- `#if UFT_HAS_ZLIB` ohne `defined()` galt nicht als Abfrage — damit hätte er
  jeden so geschriebenen Schalter für tot erklärt.
- Include-Guards (`#ifndef X` + `#define X`) zählten als Feature-Abfrage.
- Regel C prüfte nur Präprozessor-Bedingungen. Eine als **Wert** benutzte
  Konstante wie `UFT_VERSION_STRING` galt damit als verbraucherlos, obwohl vier
  Quelldateien sie benutzen.
- Und, zum vierten Mal in diesem Projekt: der Wächter las **Kommentare als
  Code**. Der Kommentar, der erklärt warum `UFT_HAS_SWITCH` entfernt wurde,
  steht innerhalb des `target_compile_definitions()`-Blocks und nennt den
  Namen — der Schalter galt danach weiter als gesetzt. Ein Wächter, der seine
  eigene Begründung für eine Anweisung hält, meldet Befunde, die er selbst
  erzeugt hat.

#### Was er sofort fand

**Zwei Leichen aus MF-441, entfernt statt baselined:** `UFT_HAS_SWITCH` und
`UFT_HAS_CART7` wurden von CMake gesetzt, obwohl `src/switch/` und
`src/cart7/` in MF-441 gelöscht wurden. Niemand las sie.

**Ein Versprechen ohne Einlösung:** `UFT_HAS_QT_CHARTS` wurde gesetzt und von
keiner Quelldatei gelesen — es gibt kein `QChart`, kein QtCharts-Include, kein
Chart-Widget. Die Meldung „chart widgets disabled" versprach etwas, das nie
geschrieben wurde. Define entfernt, Meldung sagt jetzt die Wahrheit.

Die übrigen elf Abweichungen stehen mit Begründung in
`scripts/define_parity_baseline.json` — vier davon warten auf eine
Entscheidung, siehe ARCH-23.

---

### ARCH-23 — vier Schalter ohne Verbraucher (2026-08-22, MF-468) → ⚠ OFFEN

Von `define_parity_gate.py` Regel C gefunden, baselined statt entfernt, weil
jede Entfernung eine kleine inhaltliche Entscheidung ist:

| Schalter | gesetzt in | Lage |
|---|---|---|
| `UFT_HAS_ZLIB` | `src/core/CMakeLists.txt:46` | zlib wird verlinkt, die Meldung verspricht „APD extras enabled" — den Codepfad gibt es nicht |
| `UFT_C64_PROTECTION_VERSION` | `src/protection/c64/CMakeLists.txt:17` | Versionszeichenkette, die niemand liest |
| `UFT_C64_KNOWN_TITLES_DB` | dito | Feature-Schalter ohne Feature |
| `UFT_TRACK_ALIGN_VERSION` | `src/protection/c64/CMakeLists.txt:35` | wie oben |

Zu entscheiden ist jeweils: verdrahten oder entfernen. Beides ist eine eigene
Aufgabe — im selben Commit wie eine Build-Korrektur wären es zwei Dinge unter
einem Beweis.

---

### FMT-23 — ATX lieferte für jede Datei eine leere Diskette (2026-08-22, MF-467) → ✓ BEHOBEN, T3 → T2

**a8rawconv ist jetzt das zweite Referenz-Orakel** des Projekts
(`src/a8rawconv/`, 44 Dateien, 9 194 Zeilen, GPL-2.0-or-later, wird von keinem
Build kompiliert — siehe `src/a8rawconv/README.md`). Arbeitsteilung:

| Orakel | deckt ab |
|---|---|
| `src/samdisk/` | PC, CPC, Sinclair, Atari ST, generische FDC-Formate |
| `src/a8rawconv/` | **Atari 8-bit (ATX/VAPI, FM), Interleave, Apple/Mac-GCR** |

Der erste Einsatz hat sofort etwas gefunden.

**Was verglichen wurde.** `diskatx.cpp` liefert beide Seiten: `read_atx()`
(:64-190) und `write_atx()` (:216-416). Der **Schreiber** ist dabei die
stärkere Quelle — er legt Feld für Feld fest, was der Leser konsumiert. Dazu
zwei `static_assert` auf 48 bzw. 32 Byte Kopfgröße.

**Was unser Leser tat.** Vier Fehler, zwei davon tödlich:

| Feld | belegt | `uft_atx.c` vorher |
|---|---|---|
| Spurkopf-Flags | 0x10 | 0x14 |
| Chunk-Offset im Spurkopf | **0x14** | **0x18** |
| Dichte im Dateikopf | 0x12 | 0x13 (Füllbyte) |
| Spuraufzählung | Aufzeichnungen **hintereinander**, der Reihe nach abgelaufen | erfundene Tabelle von LE32-Offsets, dazu eine „Spurzahl" bei 0x14 — das ist `mImageId`, das Format hat keine Spurzahl |

Die Folge des zweiten Eintrags: der Chunk-Offset kam aus Füllbytes, war also
**0**. Der Chunk-Scan begann damit auf dem Spurkopf selbst, fand keine
Sektorliste und gab eine leere Spur zurück — mit `UFT_OK`. **Jede ATX-Datei
las sich als leere Diskette, ohne eine einzige Fehlermeldung.**

Der MF-421-Statusabgleich führte TA3 („ATX-Plugin") als erledigt, weil dabei
ein Byte-Order-Fehler in der Signatur gefunden worden war, „wegen dem ATX nie
Sektordaten geliefert hatte". Die Signatur war danach richtig. Sektordaten kam
trotzdem keine. Ein Probe-Test allein kann das nicht bemerken — und mehr gab
es nicht.

**Rot-Probe** (`tests/test_atx_layout.c`, gegen ein bytegenau nach
`diskatx.cpp` gebautes Abbild):

```
  [TEST] track_records_are_walked_and_sectors_delivered ... FAIL @ 186: disk.geometry.cylinders == ATX_TRACKS
  [TEST] fdc_status_becomes_sector_status               ... FAIL @ 234
  [TEST] weak_chunk_marks_from_its_offset_to_the_end    ... FAIL @ 271
```

Der erste Fall fällt schon an der Spurzahl: die kam aus `mImageId`.

**Neu belegt übernommen.** Die FDC-Status-Semantik stand vorher halb im
Dateikopf und wurde halb ausgewertet:

| Bits | Bedeutung (write_atx:333-364, read_atx:170-186) | jetzt |
|---|---|---|
| 0x18 **zusammen** | CRC-Fehler im **Adressfeld** | `UFT_SECTOR_ID_CRC_ERROR` |
| 0x08 allein | CRC-Fehler im **Datenfeld** | `UFT_SECTOR_CRC_ERROR` |
| 0x10 | kein Datenfeld vorhanden | `UFT_SECTOR_MISSING`, Puffer ausdrücklich als Platzhalter |
| 0x20 | Deleted-Mark 0xF8 | `UFT_SECTOR_DELETED` |
| 0x04\|0x02 | langer Sektor | `UFT_SECTOR_EXTRA` + Meldung der physischen Größe |
| 0x40 | Weak-Sektor | `UFT_SECTOR_WEAK` |

Dazu zwei Dinge, die vorher fehlten:

- **Phantomsektoren.** Zwei Einträge mit derselben Sektornummer auf einer Spur
  sind einer der ältesten Atari-Kopierschutze — und der Grund, warum das
  Format eine *Liste* führt und kein Array (`write_atx:279-284`). Beide
  Vorkommen tragen jetzt `UFT_SECTOR_DUPLICATE`.
- **Weak-Chunk-Zuordnung.** Der Chunk nennt den Index in der **Sektorliste**.
  Vorher wurde er als Index in die erzeugten Sektoren gelesen — was
  auseinanderläuft, sobald ein Listeneintrag übersprungen wird. Die Zuordnung
  wird jetzt mitgeführt.

Die Sektornummer wird über `uft_format_add_sector_with_id()` (ARCH-20,
MF-465) durchgereicht statt über den `id - 1`-Umweg, der Sektor 0 auf 1
abgebildet hätte.

| Stufe | vorher | nachher |
|---|---:|---:|
| T2 | 16 | **17** |
| T3 | 58 | **57** |

**Ehrlich zur Grenze:** verifiziert ist das Layout gegen eine funktionierende
Implementierung, nicht das Verhalten an einem realen ATX-Abbild — es liegt
keines im Korpus. Deshalb T2. Zwei weitere ATX-Implementierungen liegen
außerdem noch im Baum (`src/formats/atari/uft_atx_parser_v2.c` 732 Zeilen,
`src/formats/atari/atx.c` 97 Zeilen) und sind **nicht** mitgeprüft — das ist
ARCH-6.

---

### Lizenzwiderspruch im a8rawconv-Fork behoben (2026-08-22, MF-467)

Der Fork `Axel051171/a8rawconv-0.95` trug eine `LICENSE`-Datei mit **CC0-1.0**,
während sein eigenes README und 17 seiner Quelldateien ausdrücklich
GPL-2-or-later nennen:

```
README.md:636  "a8rawconv is released under the GNU General Public License,
                version 2 or later. A copy of the license is included in the
                file COPYING"

*.cpp          "Copyright (C) 2014-2020 Avery Lee
                ... either version 2 of the License, or (at your option)
                any later version."
```

CC0 ist eine Rechteverzichtserklärung; für fremden GPL-Code lässt sie sich
nicht abgeben — ein Fork kann die Lizenz des Originals nicht ändern. Die im
README genannte `COPYING` fehlte zudem ganz.

Behoben in `Axel051171/a8rawconv-0.95` (Commit `5db54b4`): CC0 entfernt,
`COPYING` mit dem GPL-2.0-Text angelegt. GitHub meldet den Fork seither als
GPL-2.0. Für UFT ändert sich nichts — GPL-2 zu GPL-2 ist verträglich — aber
der Bestand in `src/a8rawconv/` steht damit unter einer Lizenz, die auch
draußen stimmt.

---

### BUG-11 — „Alle Lesungen einig" war als „wiederhergestellt" gebucht (2026-08-22, MF-466) → ✓ BEHOBEN

Aus dem a8rawconv-Vergleich (siehe `docs/A8RAWCONV_INTEGRATION_TODO.md`,
Abschnitt „Nachtrag 2026-08-22"). Dessen Trennung von *bad read* und *weak
read* zeigt auf eine Stelle in unserer Multiread-Pipeline:

```c
/* src/recovery/uft_multiread_pipeline.c:360, vorher */
result->recovered = (avg_conf >= ctx->config.min_confidence);
```

`avg_conf` misst, **wie weit die Lesungen untereinander übereinstimmen** — mit
der CRC hat es nichts zu tun. Ein Sektor, dessen Lesungen alle dasselbe sagen,
dessen CRC aber nie stimmte, erreicht damit Konfidenz 100 und wurde als
`recovered = true` gemeldet, bei `good_reads == 0`.

Das ist nicht der Ausnahmefall, sondern **genau das, was ein Kopierschutz
absichtlich erzeugt**: ein Sektor mit vorsätzlich falscher CRC liest sich
vollkommen stabil, jedes Mal gleich. Er ist weder weak noch beschädigt — und
schon gar nicht wiederhergestellt.

Die Pipeline trennte gute von schlechten Lesungen bereits **beim Abstimmen**
(nur CRC-geprüfte Durchgänge stimmen mit, sobald es welche gibt — die Regel
aus `vote_buffer`). Nur bei der **Aussage am Ende** fiel die Trennung wieder
weg.

Jetzt verlangt `recovered` beides: genug Übereinstimmung **und** mindestens
eine geprüfte Lesung. Die Daten werden weiterhin herausgegeben — verworfen
wird nichts — nur die Behauptung nicht mehr aufgestellt. Wer die Fälle
unterscheiden will, liest `good_reads == 0` bei hoher `confidence`: stabil,
aber von nichts bestätigt.

| Fall | vorher | jetzt |
|---|---|---|
| 3 gleiche Lesungen, alle CRC-fehlerhaft | recovered ✓ | recovered ✗, Daten da, `has_weak_bits` = false |
| 1 geprüfte + 2 ungeprüfte, alle gleich | recovered ✓ | recovered ✓ |
| Lesungen uneinig | wie gehabt | wie gehabt |

**Rot-Probe:** `stable_reads_without_a_verified_crc_are_not_recovered` fällt
auf der alten Fassung (`FAIL @ 158: res.recovered == false`); das
Gegenstück `one_verified_read_is_enough_to_claim_recovery` ist auf beiden
grün — es hält fest, dass die Verschärfung nicht zu weit geht.

**Ehrlich zur Reichweite:** die Pipeline hat **keine Aufrufer in `src/`** —
nur Tests. Es war also keine falsche Meldung im laufenden Betrieb, sondern
eine falsche Regel in einem Modul, das noch verdrahtet werden muss. Genau
deshalb war jetzt der billige Zeitpunkt.

---

### ARCH-20 (Auflösung) + BUG-10 — die Sektornummer ist jetzt die vom Medium (2026-08-22, MF-465) → ✓ BEHOBEN

Der Befund stand seit MF-463 offen: `uft_format_add_sector()` setzte
`id.sector = sector_num + 1`, und 71 Plugins benutzen den Helfer.

**Der Beweis brauchte keine Synthetik.** Im Korpus liegt dieselbe Diskette
zweimal, geschrieben vom selben kanonischen Fremdwerkzeug:

```
tests/corpus_free/vice_c1541_35trk.d64
tests/corpus_free/vice_c1541_35trk.g64
```

Die G64 ist ein GCR-Bitstrom — ihre Sektornummern sind die Bytes, die VICE in
die 1541-Headerblöcke geschrieben hat, und unser G64-Leser übernimmt sie
wörtlich (`uft_g64.c:621`). Die D64 ist dieselbe Diskette als Sektordump. Die
Rot-Probe druckt den Widerspruch in einer Zeile:

```
FAIL track 1: D64 says 1 2 3 ... 21, G64 says 0 1 2 ... 20
```

Zwei eigene Leser, eine Diskette, zwei Antworten. Einer erfindet.

**Was geändert wurde.** Neuer Helfer `uft_format_add_sector_with_id()` — nimmt
die Nummer, wie sie auf der Diskette steht. `uft_format_add_sector()` ist jetzt
ein Aufruf davon mit `n + 1` und damit unverändert im Verhalten; die
1-basierten Formate merken nichts.

Umgestellt wurden die dreizehn Plugins, deren Medium bei 0 zählt:

| Familie | Sektoren | Plugins |
|---|---|---|
| Apple II | 0..15 | `do`, `po`, `2img`, `nib` |
| Amiga | 0..10 | `adf`, `adf_ext` |
| Commodore | 0..N-1 | `d64`, `d67`, `d71`, `d81`, `d80`, `d82` |
| — | Pseudo-Sektor | `woz_plugin` (Kommentar sagte „sector 0", Code schrieb 1) |

Bei `nib` wog es schwerer als bei den anderen: dort ist `sec` **aus dem
Adressfeld dekodiert**, also echte forensische Information von der Diskette —
und wurde um eins erhöht, bevor sie beim Aufrufer ankam.

Mitgezogen: die beiden Stellen in `uft_d64_g64.c`, die den Versatz
kompensierten (`!= D64_BAM_SECTOR + 1` und `- 1` mit dem Kommentar „ID is
1-based"), und `tests/test_convert_via_plugin.c:179`, das
`bam.sectors[0].id.sector == 1` festschrieb — die BAM liegt auf Spur 18
**Sektor 0**.

Nicht angefasst: `fdi`, `imd`, `jv3`, `nfd`, `dmk` reichen schon seit jeher
`R - 1` an den Helfer, damit dessen `+1` die echte Nummer wiederherstellt. Die
Formate waren also richtig, der Weg dorthin nur umständlich. CP/M setzt seine
ID ohnehin selbst (`def->first_sector + s`).

#### BUG-10 — dabei gefunden: `img_write_track()` verlor Sektor 0 still

Der Schreibpfad suchte seine Sektoren als **1..N**
(`uft_track_find_sector(track, s)`) und füllte jeden Fehlgriff mit Nullen —
ohne Meldung. Das setzt voraus, dass die Quelle nummeriert wie eine
IBM-PC-Diskette.

Mit den korrigierten IDs wäre daraus sofort echter Datenverlust geworden: eine
Amiga-Spur (0..10) hätte Sektor 0 verloren und einen Null-Sektor dazubekommen.
Die Rot-Probe zeigt genau das:

```
FAIL: slot 0 holds 0xA1, expected 0xA0
```

Die Falle war schon vorher da, nur nicht erreichbar: jede Quelle mit Versatz
oder Lücke in der Nummerierung — eine IMD mit Sektor-Map etwa — verlor hier
Daten. `img_write_track()` schreibt die Sektoren jetzt in aufsteigender
Reihenfolge ihrer **Nummer auf der Diskette** und sagt es, wenn es auffüllen
muss.

**Warum das nicht nur Kosmetik war.** `g64_write_track()` kodiert `id.sector`
direkt in den GCR-Headerblock (`uft_g64.c:786`), und der generische
Plugin-zu-Plugin-Konverter (`src/core/uft_disk_convert.c`, erreichbar über
`uft_disk_batch.c`) reicht die Spur ungefiltert weiter. Eine D64, über diesen
Weg nach G64 konvertiert, hätte die Sektornummern **1..21** auf eine Diskette
geschrieben, die ein 1541 nur mit 0..20 lesen kann. Nur der eigens gebaute
`d64_to_g64()` kompensierte.

**Nicht geprüft:** die drei weiteren Schreibpfade mit ID-Suche —
`86f`, `pro` (Atari), `dcm` (Atari). Alle drei gehören zu 1-basierten
Formaten und werden im Dispatch nur von ihresgleichen beliefert; über den
generischen Konverter gilt für sie dieselbe Klasse wie für IMG.

`ctest` 215/215, `check_consistency` 20 Kategorien 0.

> Nebenbei hat der Enum-vs-Makro-Wächter (MF-427) den neuen Test selbst
> angehalten: `SPT` und `SS` als Enum-Konstanten kollidierten mit Makros
> gleichen Namens in `test_adf_write_roundtrip.c`. Umbenannt statt
> baselined — der Wächter hatte recht.

---

### BUG-9 — Die GUI schrieb beim D64-Dateiexport über den eigenen Stack (2026-08-22, MF-464) → ✓ BEHOBEN

Gefunden beim CP/M-Vergleich, über einen Umweg, der den Fund erst möglich
gemacht hat — siehe ARCH-21 darunter.

`src/explorertab.cpp` rief an zwei Stellen:

```cpp
uint8_t *outData = nullptr;
size_t   outSize = 0;
if (d64_extract_file(image, size, name, &outData, &outSize) == 0 && outData) {
    fileData = QByteArray((const char*)outData, (int)outSize);
```

Fünf Argumente — so stand es in `include/uft/uft_file_ops.h`. Die **einzige**
Definition, `src/formats/c64/uft_d64_file.c:169`, nimmt **vier**:

```c
int d64_extract_file(const uint8_t *d64_data, size_t d64_size,
                     const char *filename, d64_file_t *file)
```

und schreibt durch den vierten Parameter eine ganze `d64_file_t` (~48 Byte:
`filename[17]`, `file_type`, `block_count`, `data`, `data_size`).

Der vierte Parameter war hier `&outData` — ein 8 Byte großer Zeiger auf dem
Stack der GUI-Funktion. Der Callee legte also seine Struktur über `outData`
und über das, was daneben lag (`outSize`). Danach prüfte der Aufrufer
`outData` — inzwischen die ersten acht Zeichen des **Dateinamens** — fand es
„nicht NULL" und reichte es als Zeiger an `QByteArray` weiter. Ein Zeiger aus
ASCII-Buchstaben, mit einer Länge aus einem ebenfalls überschriebenen Slot.

Nichts hat gewarnt: C-Linkage, ein Name, zwei Formen. Der Compiler sah in der
GUI nur den Header, der Linker nur den Namen.

**Dass es nie funktioniert haben kann**, zeigt der Gegenbeweis im Baum:
`tests/test_d64_file.c:152` ruft dieselbe Funktion seit jeher **richtig** auf
(`d64_extract_file(data, size, "HELLO", &file)`) und ist grün — der Test
inkludiert den echten Header, die GUI den falschen.

Behoben: der Prototyp in `uft_file_ops.h` trägt jetzt die echte Signatur (der
Header zieht `uft_d64_file.h` dafür herein), beide Aufrufstellen benutzen eine
`d64_file_t`. Syntaxgeprüft mit `g++ -fsyntax-only` gegen Qt 6.10.2.

**Ehrlich zur Verifikation:** der C-Pfad ist durch `test_d64_file` gedeckt und
grün. Die GUI-Strecke selbst (Explorer-Tab → Datei aus D64 exportieren) wurde
**nicht** durchgeklickt. Der Schritt ersetzt einen Aufruf, der nachweislich
nicht funktionieren konnte, durch den, den der bestehende Test benutzt; ein
Smoke-Test bleibt trotzdem offen.

---

### ARCH-21 — Header-Prototypen prüft niemand, wenn niemand sie einbindet (2026-08-22, MF-464) → ◐ WÄCHTER STEHT, 20 Altfälle offen

Der Wächter aus MF-442 (`scripts/extern_decl_conflicts.py`) prüfte
ausdrücklich **nur** `extern`-Zeilen in `.c`-Dateien, mit der Begründung:
Header-Deklarationen seien „der normale Mechanismus und werden vom Compiler
geprüft, wo immer der Header eingebunden wird".

Diese Begründung hat ein Loch: **wo immer** heißt nicht **irgendwo**. Zwei
Header dürfen denselben Namen mit verschiedenen Signaturen deklarieren,
solange keine Übersetzungseinheit beide einbindet. Genau das war der Fall bei

```
include/uft/formats/uft_cpm_diskdef.h    uft_cpm_format(uft_disk_image_t*,  def)
include/uft/formats/uft_cpm_diskdefs.h   uft_cpm_format(uft_disk_image_t**, def, fill)
```

— zwei Parameter hier, drei dort, und nur die Drei-Parameter-Fassung
existiert (`src/formats/cpm/uft_cpm_diskdefs.c:1420`). Dieselbe latente Klasse
wie `c64_sectors_per_track` vor MF-459 und `UFT_SCP_SIGNATURE` in ARCH-2. Die
Zwei-Parameter-Deklaration ist entfernt; der Header sagt jetzt, warum.

**Regel B** vergleicht deshalb jetzt auch Header-Prototypen mit der Definition
— auf Parameteranzahl, wie Regel A, und nur für C-Header (`.h`, nicht `.hpp`)
und Namen, die keine `.cpp`/`.hpp` ebenfalls deklariert, weil C++-Overloads
unterschiedliche Stelligkeit legitim machen.

> **Nebenbefund in der eigenen Meldung.** Die Zeilennummern waren rund ein
> Dutzend Zeilen zu klein. Kommentare werden vor der Suche zu Leerzeichen
> ausgeweißt, und das führende `^\s*` des Musters verschluckt dann den
> Doc-Kommentar über dem Prototyp — der Treffer begann in dessen erster Zeile.
> Gemeldet wird jetzt die Zeile des **Namens**. Ein Wächter, der auf die
> falsche Zeile zeigt, kostet den Leser genau das Vertrauen, das ihn nützlich
> macht.

**Was Regel B sofort fand: 22 Fälle.** Einer davon war BUG-9 oben, einer die
CP/M-Kollision. Die übrigen **20** stehen in
`scripts/extern_decl_baseline.json` — ein Baseline-Eintrag ist ein
**bekannter**, kein akzeptierter Fehler:

| Name | Header sagt | Definition hat |
|---|---:|---:|
| `uft_format_registry_init` | 1 | 0 |
| `uft_format_detect` (3 verschiedene Header!) | 5 / 4 / 3 | 2 |
| `uft_chs_to_lba` | 4 | 3 |
| `uft_lba_to_chs` | 5 | 4 |
| `uft_detect_format` | 2 | 3 |
| `uft_crc16_ccitt` | 3 | 2 |
| `uft_format_can_convert` | 2 | 3 |
| `uft_verify_result_to_json` | 1 | 3 |
| `cpm_open` | 2 | 5 |
| `uft_imd_get_track` | 4 | 3 |
| `uft_mfm_write_track` | 5 | 4 |
| `uft_mfm_get_track_length` | 3 | 2 |
| `uft_scp_read` | 4 | 3 |
| `uft_fat32_validate` | 1 | 2 |
| `uft_fat32_format` | 3 | 4 |
| `uft_copylock_reconstruct` | 3 | 4 |
| `bam_validate` | 2 | 4 |
| `bam_sector_offset` | 3 | 2 |

`uft_format_detect` ist der schlimmste Einzelfall: **drei** Header
deklarieren ihn mit fünf, vier und drei Parametern, die Definition hat zwei.

Warum keiner davon bisher geknallt hat, ist bei jedem einzeln zu klären —
in den meisten Fällen steht der Aufrufer in derselben `.c` wie die Definition
und sieht damit die richtige Signatur, nicht den Header. BUG-9 war der Fall,
wo das nicht galt.

**Rot-Probe des Wächters:** eine absichtlich verfälschte Stelligkeit in
`uft_cpm_diskdefs.h` wird gemeldet, nach dem Zurücknehmen ist er wieder grün;
und beim Beheben von BUG-9 verlangte er von sich aus den Baseline-Eintrag
zurück („no longer mismatches").

### ARCH-22 — `uft_file_ops.h`: 12 von 19 Prototypen ohne Definition (2026-08-22, MF-464) → ⚠ OFFEN

Bei BUG-9 mitgemessen:

| | |
|---|---|
| Prototypen in `include/uft/uft_file_ops.h` | 19 |
| davon irgendwo definiert | 7 |
| **Phantom** | **12** |

`adf_extract_file`, `adf_inject_file`, `adf_list_files`, `atr_extract_file`,
`atr_inject_file`, `atr_list_files`, `d64_inject_file`, `d64_list_files`,
`d81_inject_file`, `ssd_inject_file`, `trd_inject_file`, `trd_list_files`.

Keiner hat einen Aufrufer — hätte er einen, würde die Anwendung nicht linken.
Das ist der **laute** Fall und damit der harmlosere; gefährlich war genau der
eine Name, der zu einer *anderen* Funktion linkte (BUG-9). Deshalb wurde
zuerst der behoben.

Dieselbe Klasse wie die in MF-366 entfernte Audit-Trail-/Forensic-Report-API.
Löschen ist der nächste Schritt und braucht seinen eigenen Beweis: `uft_*`-
Header sind öffentliche API-Fläche, die Entfernung gehört in die
RELEASE_NOTES. Die GUI benutzt für ADF ohnehin die reale `uft_adf_*`-API
(`src/explorertab.cpp`), nicht diese Namen.

---

### FMT-22 — Apple DO/PO gaben Füllbytes als gelesene Sektoren aus (2026-08-22, MF-463) → ✓ BEHOBEN, DO bleibt bewusst T3

Vierter Schritt des T3-Abbaus — und der erste, der **nicht** in einer Hebung
endet. Warum, steht unten; es ist der Punkt des Eintrags.

**Der Befund.** Beide Leser sahen so aus:

```c
if (fread(buf, 1, DO_SS, p->file) != DO_SS) { memset(buf, 0xE5, DO_SS); }
uft_format_add_sector(track, s, buf, DO_SS, cyl, 0);
```

Ein Sektor, der nicht in der Datei steht, kam als 256 Byte `0xE5` zurück —
angelegt mit `crc_ok = true` wie jeder andere. Nichts am Ergebnis sagte, dass
diese Bytes nie gelesen wurden.

Und `open()` prüft die Dateigröße nicht. Das war also kein Randfall: **jede**
zu kurze oder abgeschnittene Datei ergab eine vollständig aussehende
35-Spur-Diskette aus Füllbytes. Genau die Sorte Befund, gegen die der erste
Satz der Mission geschrieben ist.

Dazu: weder `read_track` noch `write_track` begrenzten den Zylinder. Spur 5000
ergab einen Offset weit hinter dem Dateiende — beim Lesen eine erfundene Spur,
beim Schreiben eine still verlängerte Datei.

Jetzt: Zylinder begrenzt, und was nicht gelesen wurde, wird nicht ausgegeben.
Die Spur meldet, was existiert.

**Rot-Probe:** `tests/test_apple_do_po_bounds.c`, beide Plugins, beide Fälle
rot auf der alten Fassung.

#### Warum DO trotzdem T3 bleibt

DO und PO sind **inhaltlich nicht unterscheidbar** — nicht in dieser Probe,
und nicht in irgendeiner Probe mit dem heutigen Puffer:

- Beide sind 143.360-Byte-Rohabzüge derselben Diskette.
- Sie unterscheiden sich **nur in der Reihenfolge der Sektoren 1..14**;
  Sektor 0 und 15 liegen in beiden Ordnungen an derselben Stelle.
- Die Struktur, die es entscheiden würde — das DOS-3.3-VTOC — liegt auf
  Spur 17 Sektor 0, also bei Dateioffset `0x11000` = 69.632. Der Probe-Puffer
  ist 65.536 Byte groß (`UFT_PROBE_BUFFER_SIZE`). Und selbst dort gelesen
  läge das VTOC in **beiden** Ordnungen am selben Offset; entscheiden ließe
  es sich erst, indem man die Katalogkette durch die Sektoren 14..1 verfolgt.

SAMdisk löst es genauso und sagt es offen: sein `ReadDO()` ist im Quelltext
mit `// not used` markiert und stützt sich auf Größe **plus Dateiendung**
(`src/samdisk/do.cpp:6-13`). Bei uns tut die Endung dieselbe Arbeit über die
Extension-Zuordnung der Registry.

Damit bleibt als „Spec" für DO nur die Geometrie 35×16×256 — zu wenig für
einen T2-Eintrag. Ein Eintrag in `docs/spec_verification.json` wäre hier
billig zu haben und wertlos; genau davor warnt die EINFRIER-REGEL. Beide
Dateiköpfe sagen jetzt aus, was geprüft wurde und was nicht.

| Stufe | vorher | nachher |
|---|---:|---:|
| T2 | 16 | 16 |
| T3 | 58 | 58 |

---

### ARCH-20 — `uft_format_add_sector()` erfindet die Sektornummer (2026-08-22, MF-463) → ✓ BEHOBEN in MF-465

Beim DO-Vergleich aufgefallen. Der Helfer, den **71 der 88 Plugins** benutzen,
setzt die Sektor-ID hart:

```c
/* include/uft/uft_format_common.h:78 */
sector.id.sector = sector_num + 1;  // 1-basiert in ID
```

`uft_sector_t.id` ist die CHS-Kennung — die Nummer, die **auf der Diskette
steht**. Für IBM-PC-Formate ist 1-basiert richtig. Für mindestens drei ganze
Plattformfamilien nicht:

| Familie | Sektornummern auf der Diskette | gemeldet |
|---|---|---|
| Apple II (DOS 3.3 / ProDOS) | 0..15 | 1..16 |
| Amiga (AmigaDOS) | 0..10 | 1..11 |
| Commodore (1541/71/81) | 0..20 | 1..21 |

Die Folge ist konkret, nicht theoretisch:
`uft_track_find_sector(track, 0)` findet auf einer Apple- oder
Commodore-Diskette **nie** einen Sektor, und wer nach 16 fragt, bekommt den,
der in Wirklichkeit die 15 trägt.

Dass es auch anders geht, steht im selben Baum: die CP/M-Plugins setzen
`sect->id.sector = def->first_sector + s` (`uft_cpm_diskdefs.c:1344`) — die
Basis kommt aus der Formatdefinition.

**Nicht in MF-463 geändert.** Der Helfer wird von 71 Plugins benutzt; die
Korrektur ist ein eigener Arbeitsblock (expliziter ID-Helfer, dann die
0-basierten Familien umstellen, dazu die Tests, die die falsche Nummerierung
festschreiben — `test_convert_via_plugin.c:179` prüft ausdrücklich
`bam.sectors[0].id.sector == 1` mit dem Kommentar „IDs are 1-based", und das
ist eine D64-Spur). Das im selben Commit mit einer Format-Korrektur zu
vermischen hieße, zwei verschiedene Dinge unter einen Beweis zu stellen.

Nächster Schritt: `uft_format_add_sector_with_id()` einführen (keine
Verhaltensänderung), dann Familie für Familie umstellen, jede mit eigener
Rot-Probe.

---

### FMT-21 — Atari ST: jedes erweiterte Format wurde abgewiesen (2026-08-22, MF-462) → ✓ BEHOBEN

ST-Abbilder haben keinen Kopf. Die Geometrie muss also von woanders kommen —
und woher genau, war die ganze Frage.

**Was fehlte.** Der Leser kannte sechs Dateigrößen und rechnete sonst nur
80 Zylinder durch. Die erweiterten ST-Formate mit **82 oder 83 Spuren und 10
oder 11 Sektoren** — auf dem ST alltäglich, weil das Format serienmäßig
formatiert wurde — fielen damit alle durch. Nicht falsch gelesen: **gar nicht
geöffnet.**

SAMdisk durchsucht 80..84 Zylinder × 1..2 Köpfe × 8..11 Sektoren
(`src/samdisk/st.cpp:47-67`). Genau dieser Bereich fehlte.

**Was schwerer wiegt.** 368.640 Byte sind **80×1×9 und 40×2×9**. Der Dateikopf
des Plugins listete beide Lesarten auf:

```
 *   360K  = 80 cyl x 1 head x  9 spt x 512 = 368,640
 *   360K  = 40 cyl x 2 head x  9 spt x 512 = 368,640  (alt, rare)
```

— und der Code nahm still die erste. Die zweite Verzweigung war dabei
nachweislich **unerreichbar**: die `switch`-Anweisung darüber fing die Größe
schon ab.

Der Bootsektor beantwortet die Frage. TOS schreibt dort einen
DOS-kompatiblen BPB (Bytes/Sektor 0x0B, Gesamtsektoren 0x13, Sektoren/Spur
0x18, Köpfe 0x1A). Der Leser benutzte ihn nur, um der Probe eine Zahl zu
geben — für die Geometrie nicht.

Jetzt gilt SAMdisks Reihenfolge: **BPB zuerst**, aber nur wenn er in sich
stimmig ist *und* die Dateigröße exakt erklärt — ein BPB, der eine andere
Diskette beschreibt als die vorliegende, ist kein Beleg. Sonst der
Größen-Scan.

**Die Reihenfolge des Scans ist der eigentliche Inhalt.** Mehrere
Kombinationen ergeben dieselbe Größe, der erste Treffer gewinnt. Sie ist so
gewählt, dass alle sechs bisher bekannten Größen **unverändert** aufgelöst
werden — ein erweiterter Suchraum, der nebenbei Abbilder anders liest als
vorher, wäre schlimmer als die Lücke, die er schließt. Dafür gibt es einen
eigenen Test (`the_six_legacy_sizes_resolve_unchanged`), der auf **beiden**
Fassungen grün ist; das ist seine Aufgabe.

**Dazu übernommen:** die TOS-Boot-Prüfsumme — die 256 Big-Endian-Wörter des
Bootsektors summieren zu `0x1234`, sonst bootet TOS nicht
(`samdisk/st.cpp:6`). Das ist das einzige *positive* Erkennungsmerkmal, das
ein kopfloses Format überhaupt anbieten kann; die Probe stuft danach ab
(90 statt vorher 80 für ein rohes `0x60`-Byte, das jede sechzehnte Datei
zufällig hat). Ihr Fehlen beweist nichts — nicht bootfähige Disketten
vermeiden den Wert absichtlich.

Nebenbei: `read_track`/`write_track` prüften nur die obere Grenze, ein
negativer Zylinder rechnete einen negativen Offset aus.

**Rot-Probe** (alte Fassung, neuer Test):

```
  [TEST] bpb_resolves_the_ambiguous_360k_size     ... FAIL @ 126
  [TEST] extended_formats_are_accepted            ... FAIL @ 156
  [TEST] the_six_legacy_sizes_resolve_unchanged   ... OK      <- Absicht
  [TEST] boot_checksum_outranks_a_bare_size_match ... FAIL @ 227
```

| Stufe | vorher | nachher |
|---|---:|---:|
| T2 | 15 | **16** |
| T3 | 59 | **58** |

**Ehrlich zur Grenze:** verifiziert sind Geometrie-Herleitung und
Erkennungsmerkmal, nicht das Verhalten an einem realen ST-Abbild — es liegt
keines im Korpus. Deshalb T2.

---

### FMT-20 — CQM: der Leser las ein Layout, das es nicht gibt (2026-08-22, MF-461) → ✓ BEHOBEN

Dritter Schritt des T3-Abbaus, und der erste, bei dem die Prüfung nicht eine
Beschreibung korrigiert hat, sondern den Leser.

**Was verglichen wurde.** Zwei unabhängige Quellen, die sich in jedem
benutzten Feld einig sind:

1. „CopyQM Format (*.cqm) — Disk image layout", RPN, 2023-03-31,
   `https://rio.early8bitz.de/cqm/cqm-format.pdf` — abgeleitet aus den
   LibDsk-Treibern `drvqm.c` / `crctable.c`
2. SAMdisk 4.0, `src/samdisk/cqm.cpp:10-41` (MIT, Referenz-Orakel, wird nicht
   gebaut — siehe `src/samdisk/README.md`)

Die Primärquelle sagt zusätzlich, dass der Block 0x03..0x1B **der BPB einer
DOS-Diskette** ist. Das ist eine dritte, strukturelle Bestätigung: Feldfolge
und -breiten dort sind exakt der DOS-3.31-BPB.

**Was unser Leser stattdessen las.** Kein Feld traf.

| Feld | belegt | `uft_cqm.c` vorher | was dort wirklich steht |
|---|---|---|---|
| Sektorgröße | 0x03,0x04 LE16, **Byteanzahl** | `hdr[3]`, als Code `128<<n` | Low-Byte derselben Zahl |
| Sektoren/Spur | **0x10,0x11** | `hdr[8]` | Anzahl FAT-Kopien |
| Köpfe | **0x12,0x13** | `hdr[9]` | Verzeichniseinträge (low) |
| Zylinder | **0x5A** (`u-cyl`) | `hdr[15]` | Sektoren/FAT (high) |
| Kommentarlänge | **0x6F,0x70** | `hdr[16,17]` | Sektoren/Spur |
| Datenbeginn | **133 + Kommentarlänge** | 18 + Kommentarlänge | — |
| RLE-Vorzeichen | positiv = Literalfolge | positiv = Wiederholung | invertiert |

Dazu fehlten: die Kopfprüfsumme (Summe über alle 133 Byte ≡ 0 mod 256, laut
Spec verbindlich), der Daten-CRC, die Sektorbasis (0x71) und das Füllbyte nach
Blind-Modus (0x58).

Ein DOS-konformer CQM-Kopf hat an 0x08 die Zwei (zwei FAT-Kopien) und an 0x0F
die Null. Der alte Leser hat daraus „2 Sektoren pro Spur, 0 Zylinder" gemacht
und eine leere Diskette geliefert — **still**, ohne Fehler. Er kann nie ein
echtes CopyQM-Abbild gelesen haben.

Das ist dieselbe Klasse wie FMT-1/2/3: ein Parser gegen eine erfundene Spec,
gebaut weil Code schneller entsteht als Prüfung. Genau der Grund für die
EINFRIER-REGEL (MF-363).

**Rot-Probe.** `tests/test_cqm_layout.c` baut ein Abbild bytegenau nach Spec
(beide RLE-Zweige, Kommentar, korrekter Daten-CRC) und prüft Geometrie,
Sektor-Offsets, Sektornummerierung und beide Integritätspfade. Gegen die alte
Fassung fallen **alle fünf** Fälle um:

```
  [TEST] probe_grades_full_header_above_bare_marker ... FAIL @ 173
  [TEST] geometry_comes_from_the_documented_offsets  ... FAIL @ 204
  [TEST] sector_data_lands_at_the_right_offset       ... FAIL @ 232
  [TEST] broken_header_checksum_is_refused           ... FAIL @ 271
  [TEST] broken_data_crc_still_yields_the_data       ... FAIL @ 293
  === 0 passed, 5 failed ===
```

**Zwei Integritätsfelder, zwei bewusst verschiedene Antworten.**

- *Kopfprüfsumme falsch* → `UFT_ERR_CRC`, Abbruch. Stimmt sie nicht, ist jedes
  Geometriefeld darunter geraten; eine Diskette daraus zu erfinden wäre
  schlimmer als nichts zu liefern.
- *Daten-CRC falsch* → Warnung, Bytes werden unverändert geliefert. Der CRC
  deckt das ganze Abbild ab, ein Fehler lässt sich keinem Sektor zuordnen —
  und ein beschädigtes Abbild zurückzuweisen, das größtenteils lesbar ist,
  widerspricht dem Zweck des Werkzeugs.

Der CRC ist übrigens kein normaler CRC-32: CopyQM indiziert eine 1024-Byte-
Tabelle mit einem 8-Bit-Register, weshalb nur die unteren sechs Bit jedes
Bytes in die Tabelle gehen (`& 0x3f`). Beide Quellen beschreiben denselben
Quirk. Der Test rechnet ihn unabhängig nach — ein Test, der seine Erwartung
vom geprüften Code holt, beweist nichts.

**Ehrlich zur Grenze:** verifiziert ist das Layout, nicht das Verhalten an
einem echten CopyQM-Abbild. Es liegt keines im Korpus. Deshalb T2, nicht T1.

| Stufe | vorher | nachher |
|---|---:|---:|
| T2 | 14 | **15** |
| T3 | 60 | **59** |

---

### ARCH-24 — 97 gebaute Dateien haben keinen Aufrufer (2026-08-22, MF-476) → ⚠ OFFEN

ARCH-19 hat den Befund für `src/formats/misc/` von Hand ausgezählt. Beim
Verdrahten von MF-475 tauchte dasselbe Bild in anderen Schichten auf, also
war die Frage: ist `misc/` ein Sonderfall oder ein Ausschnitt?

Es ist ein Ausschnitt. `scripts/audit_orphan_modules.py` misst es jetzt
reproduzierbar — **das Skript ist die SSOT, die Zahlen unten sind ein
Stand, kein Vertrag**:

| | |
|---|---|
| gebaute Quelldateien mit exportierten Symbolen | 531 |
| davon mit Aufrufern in `src/` | 223 |
| davon **nur** von Tests aufgerufen | **80** |
| davon **ohne jeden** Aufrufer | **228** |

Rund 58 % des Builds ist Code, den kein anderer Produktivcode aufruft. Er
kostet Bauzeit, er sieht im Baum nach Funktion aus, und er hat keine.

Die 80 „nur von Tests" sind der interessantere Teil — darunter
`uft_god_mode_api.c` und die fünf `analysis/events/*_bridge.c`. Das ist
derselbe Zustand, in dem MF-471, MF-473 und MF-474 gelandet sind: geprüft
und ohne Wirkung. Ein grüner Test über unverdrahtetem Code beweist, dass er
funktioniert, und verschweigt, dass ihn niemand ruft.

> **Stand 2026-08-23:** 225 / **78** / 228.
> Zwei Einträge sind von „nur von Tests" nach „benutzt" gewandert:
> `uft_multiread_pipeline.c` (MF-478, siehe REC-1) und `uft_interleave.c`
> (MF-479, siehe FMT-25). Die Zahl „ohne jeden Aufrufer" bleibt
> unverändert; das ist die andere Aufgabe.

#### Warum das Instrument viermal umgebaut wurde

Ein Audit-Skript, das falsch misst, ist schlimmer als keins: es lädt dazu
ein, funktionierenden Code zu löschen. Vier Falsch-Positiv-Klassen fielen
erst in der Gegenprobe auf, jede hätte für sich zu falschen Schlüssen
geführt:

1. **Nur Funktionen gezählt.** Ein Format-Plugin exportiert genau ein
   Symbol — die Plugin-Struktur — und hält jede Funktion `static`.
   `src/formats/cqm/uft_cqm.c` galt damit als verwaist, obwohl die Registry
   es führt und es täglich läuft.
2. **Regex-Überlappung.** `&plugin_a, &plugin_b,` lieferte nur den ersten
   Eintrag: das Match auf `, &plugin_a,` frisst das Komma, das `&plugin_b`
   als Anfang braucht. Registrierte Plugins galten reihenweise als verwaist.
3. **Konditionale `.pro`-Blöcke.** `uft_kalman_pll.c` steht in einem
   `kalman_pll { ... }`-Block und wird ohne `CONFIG+=kalman_pll` nicht
   gebaut. Behoben, indem der Parser aus `verify_build_sources.py`
   importiert statt nachgebaut wird — der kennt diese Falle seit MF-458.

4. **Der eigene Header galt als Aufrufer.** Ein Prototyp
   `multiread_execute(...);` sieht aus wie ein Aufruf. Damit galt praktisch
   jedes Modul mit Header als benutzt — das Skript meldete
   `uft_multiread_pipeline.c` als verdrahtet, obwohl der einzige Treffer
   sein eigener Prototyp war. Behoben über die Einrückung: Aufrufe stehen
   in einem Funktionsrumpf, Prototypen in Spalte 0.

> **Die erste „Validierung" war zwei Fehler, die sich aufhoben.** Fassung 3
> meldete für `src/formats/misc/` **19** — genau die Handzählung aus
> ARCH-19, und das galt hier als Beleg. Nach Behebung von Fehler 4 meldet
> das Skript **20**, und die Nachprüfung gibt ihm recht:
> `misc/polyglot_boot.c` ist gebaut, exportiert `poly_parse_bpb()` und
> Nachbarn, und **kein einziger Aufrufer** existiert im Baum. ARCH-19s
> „tatsächlich benutzt: `polyglot_boot.c`, `udi.c`" ist um einen Eintrag zu
> optimistisch; benutzt ist nur `udi.c`.
>
> Eine Übereinstimmung mit einer Handzählung ist also kein Beleg — beide
> können denselben blinden Fleck haben. Belastbar ist erst die Nachprüfung
> am Einzelfall, und die steht oben.

Stichproben in beide Richtungen: sechs sicher benutzte Module
(`uft_flux_decoder.c`, `uft_media_profile.c`, `uft_scp_parser.c`,
`uft_log.c`, `uft_cqm.c`, `uft_atx.c`) werden korrekt als benutzt gemeldet;
`uft_event_bridge.c` wird korrekt als nur-von-Tests gemeldet
(`tests/test_event_bridge.c` ist der einzige Aufrufer).

**Was das Skript nicht kann**, steht in seinem Kopfkommentar: kein
Präprozessor, keine makro-erzeugten Aufrufe. Der verbleibende Fehler zeigt
in die vorsichtige Richtung — es kann ein Modul fälschlich als *benutzt*
melden, kaum fälschlich als verwaist.

#### Kein Löschauftrag

97 Dateien sind kein Aufräumtask, sondern eine Landkarte. Ein Modul ohne
Aufrufer kann dreierlei sein, und die drei Fälle brauchen gegensätzliche
Antworten:

| Fall | Antwort |
|---|---|
| Doppelte Implementierung, die andere läuft | löschen, mit Löschbeweis (MF-369) |
| Fertig und nur nicht verdrahtet | verdrahten (wie MF-475) |
| Gegen eine erfundene Spec gebaut | löschen, nicht verdrahten (EINFRIER-REGEL) |

Der dritte Fall ist der Grund, warum „alles verdrahten" die falsche Antwort
wäre: unter der EINFRIER-REGEL (MF-363) ist ungeprüfter Format-/Decoder-Code
zu verifizieren oder zu entfernen, nicht in Betrieb zu nehmen.

---

### ARCH-19 — `src/formats/misc/`: 19 von 21 Dateien haben keinen Aufrufer (2026-08-22, MF-461) → ⚠ OFFEN

Beim CQM-Vergleich aufgefallen: es gibt eine **zweite** CQM-Implementierung,
`src/formats/misc/cqm.c`. Sie wird gebaut, registriert kein Plugin und wird von
niemandem aufgerufen. Ihre eigenen Kommentare geben zu, dass die Offsets
geraten sind:

```c
/* bytesPerSector at 0x18? sectorsPerTrack at 0x1A? heads at 0x1C?
   totalSectors at 0x10/0x13?
   If not plausible, fail. */
```

Gegen die jetzt belegte Spec: alle vier falsch. Die Funktion heißt
`bpb_guess()` — das ist wenigstens ehrlich benannt.

Gemessen über die ganze Schicht (öffentliche Symbole je Datei gegen alle
Aufrufer in `src/`, `include/`, `tests/`):

| | |
|---|---|
| Dateien in `src/formats/misc/` | 21 |
| davon mit **null** externen Aufrufern | **19** |
| tatsächlich benutzt | `polyglot_boot.c`, `udi.c` |

Die 19 tragen je fünf bis sieben `uft_msc_*`-Funktionen über eine eigene
`FloppyDevice`-API — eine dritte Format-Schicht neben den Plugins und neben
`uft_xdf_api_impl.c` (ARCH-18). Ihr Header `include/uft/formats/cqm.h` ist ein
leerer Stub mit dem Kommentar „Stub header for format module"; er wird von
`uft_format_registry_v2.c` eingebunden, ohne dass daraus etwas benutzt würde.

**Nicht in MF-461 gelöscht.** 19 Dateien zu entfernen ist ein eigener Schritt
mit der vollständigen MF-369-Beweispipeline (`.pro`-Eintrag, Symbolreferenzen,
Skript-Referenzen, qmake-Vollbuild-Abnahme) — im selben Commit wie eine
Format-Korrektur wäre das zweierlei Arbeit unter einem Beweis. Gehört zu
ARCH-6, mit ARCH-18 zusammen.

---

### FMT-19 — T3-Abbau begonnen: TD0 und MSA gegen SAMdisk verifiziert (2026-08-22, MF-460) → ◐ 2 von 62 gehoben

Erster Durchgang am eigentlichen Rückstand: **62 der 88 Formate waren T3**, also
unverifiziert. Der Hebel ist `docs/spec_verification.json` — ein Format mit Test
steigt auf T2, sobald ein **belegter** Spec-Bezug dokumentiert ist.

**Warum das kein Massengeschäft ist.** 16 der 62 haben bereits Tests und
bräuchten nur den Spec-Eintrag. Sie alle in einem Zug einzutragen wäre in
zwanzig Minuten erledigt — und wäre genau das, wogegen die EINFRIER-REGEL
(MF-363) geschrieben wurde: fünf Parser existierten gegen *erfundene* Specs,
weil Code schneller entstand als Prüfung. Ein Spec-Eintrag zählt nur, wenn das
Byte-Layout tatsächlich verglichen wurde.

Verglichen wurde gegen **SAMdisk 4.0** (MIT, `src/samdisk/`, siehe
`src/samdisk/README.md`) — eine unabhängig geschriebene, funktionierende
Implementierung derselben Formate.

#### TD0 — 12-Byte-Kopf, ein Fehler gefunden

Feld für Feld gegen `src/samdisk/td0.cpp:14-25`: Signatur, Volume-Sequenz,
Check-Signatur, Version, Quelldichte, Laufwerkstyp, Spurdichte, DOS-Modus,
Seiten, CRC. **Position und Breite stimmen überall.**

Eine Abweichung: **Byte 7** war bei uns beschrieben als

```c
uint8_t stepping;   /**< Stepping type (0=SS, 1=DS, 2=EDS) */
```

Das kann nicht stimmen — Byte 9 (`sides`) trägt die Seitenzahl bereits. SAMdisk
liest es als Spurdichte, mit **Bit 7 als „Kommentarblock folgt"**
(`td0.cpp:28` und `:208`, `th.bTrackDensity & 0x80`).

Und unser eigener Code tut genau das schon: `uft_td0_lzss.c:469` prüft
`header.stepping & 0x80`. **Falsch war die Beschreibung, nicht das Verhalten** —
aber eine falsche Beschreibung ist die Vorlage für den nächsten, der danach
implementiert.

Dazu eine **dritte** Lesart desselben Bytes: `uft_td0_parser_v2.c:76` nannte es
„Track stepping (1 or 2)" und druckte `"Stepping: %d:1"` — bei gesetztem Bit 7
also `"Stepping: 129:1"`. Beides korrigiert.

#### MSA — 10-Byte-Kopf, eine verschwiegene Off-by-one und eine zu weiche Probe

Gegen `src/samdisk/msa.cpp:9-16`, alle Felder big-endian: Magic `0x0E0F`,
Sektoren/Spur, **Seiten minus eins**, Startspur, Endspur.

Das „minus eins" stand in unserer Beschreibung nicht. Der Code rechnet es
richtig (`+ 1`, wie `msa.cpp:44`) — aber eine Beschreibung, die eine Off-by-one
verschweigt, ist die Vorlage für die nächste.

Die **Probe** prüfte nur das Magic und meldete Konfidenz 95. Zwei Bytes sind
dünn, seit die Registry wirklich alle Plugins fragt (MF-447) — dieselbe Klasse
wie `d88_probe()`, das jede Datei im Korpus mit 90 beanspruchte. Sie prüft
jetzt zusätzlich, was `msa_plugin_open()` unmittelbar darunter ohnehin verlangt,
plus SAMdisks Nullbyte-Prüfung der oberen Feldbytes (`msa.cpp:38-40`).

**Ein vorher grüner Test wurde geändert:**
`test_plugin_probe_real.c::msa_magic_is_big_endian` prüfte einen Nullpuffer mit
gesetztem Magic — also einen Kopf mit `sectors per track = 0`, den
`msa_plugin_open()` selbst ablehnt. Der Test verlangte, dass die Probe
akzeptiert, was der Leser verweigert; dieselbe Form wie die D88-Fixture in
MF-447. Fixture ist jetzt ein Kopf, dem der Leser folgen kann, und der Test
heißt `msa_magic_and_header_plausibility`.

#### Stand

| Stufe | vorher | nachher |
|---|---:|---:|
| T1 | 2 | 2 |
| T1b | 12 | 12 |
| T2 | 12 | **14** |
| T3 | 62 | **60** |

**Ehrlich zum Tempo:** zwei Formate pro Durchgang, mit echtem Feldvergleich und
je einem gefundenen Fehler. Das ist der Preis dafür, dass ein T2 etwas bedeutet.
14 der 16 Formate mit Test warten noch; für 15 davon hat SAMdisk einen Handler
(`cpm`, `cqm`, `do`, `ipf`, `st`, `edsk`, …), für die Amiga-Seite ist
`keirf/disk-utilities` das Gegenstück (siehe `docs/XCOPY_COMPARISON.md`).

Die übrigen **46 T3-Formate haben nicht einmal einen Test** — dort ist der
Engpass Referenzmaterial, nicht Spec-Arbeit.

---

### AUD-5 — Der Build-Paritäts-Prüfer verschluckte 30 Dateien, CMake baute die Anwendung daraus (2026-08-22, MF-458) → ✓ BEHOBEN, Baseline 160 → 0

Angegangen als „Baseline aufräumen". Herausgekommen ist ein echter Fehler im
Prüfer selbst.

**1. Der Parser-Fehler.** `verify_build_sources.py` verband erst
Zeilenfortsetzungen und strippte dann Kommentare. In der `.pro` steht:

```
    src/gw_output_parser.cpp \
    # src/qmake_stubs/uft_protection_stubs.cpp \ # DISABLED: conflicts with real impls
    src/gui/uft_otdr_panel.cpp \
```

Eine auskommentierte Zeile, die selbst auf einem Backslash endet. Nach dem
Verbinden war **alles ab dem ersten `#` eine einzige Zeile** — das
anschließende Kommentar-Strippen löschte den gesamten Rest des SOURCES-Blocks.
**30 Dateien fielen weg**, darunter `src/flux/uft_flux_decoder.c`,
`src/gui/uft_otdr_panel.cpp`, `src/gui/uft_sector_editor.cpp`,
`src/gui/ProtectionAnalysisWidget.cpp` und `src/flux/uft_scp_parser.c`.

**2. Warum das mehr war als ein Meldefehler.** `CMakeLists.txt:202` ruft
dieses Skript mit `--emit-cmake-sources` auf und baut daraus seine Quellliste.
Gemessen: **559 Quellen vor dem Fix, 589 danach.** Die CMake-Anwendung wurde
also ohne den Flux-Decoder, das OTDR-Panel und den Sektor-Editor gebaut.

Der **qmake-Release-Build war nie betroffen** — der parst seine Datei selbst.
Nachgeprüft mit `qmake6 -o` und einem Blick ins erzeugte Makefile: alle sieben
oben stehen als Ziele drin. Ich hatte den Befund zuerst umgekehrt gelesen
(„fehlt im Release-Build") und erst durch diese Messung korrigiert.

**3. Die Prämisse des Prüfers stimmte nicht mehr.** Der Docstring sagte: CMake
globt automatisch, qmake nicht, also fängt dieses Skript die stille Lücke ab.
Das war einmal so. Heute leitet CMake seine Liste **aus der `.pro` ab** — „CMake
hat mehr als qmake" kann strukturell nicht mehr vorkommen. Gap A bedeutet jetzt
etwas Schwächeres: *eine Datei liegt unter `src/`, steht aber in keinem Build.*
Docstring entsprechend neu geschrieben.

**4. Was die Baseline wirklich enthielt.** Von 160 akzeptierten Abweichungen:

| | |
|---|---|
| 30 | Phantome des Parser-Fehlers |
| 100 | `src/samdisk/` — Referenzbestand, absichtlich nicht gebaut |
| 3 | Quellen hinter Opt-in-Flags (`kalman_pll`, `experimental_vfo`) — der Prüfer übersprang sie auf der `.pro`-Seite und meldete dann seine eigene Auslassung |
| 26 | zwischenzeitlich erledigt |
| **1** | **echter Befund** |

samdisk und die Opt-in-Quellen stehen jetzt als `NOT_BUILT_BY_DESIGN` im
Prüfer statt in einer Baseline. Eine Baseline sagt „bekannter Mangel, noch
nicht behoben" — hier war der Zustand der richtige, und 100 Zeilen Altlast
mitzuschleppen machte die Zahl bedeutungslos.

**5. Der eine echte Befund:** `src/core/uft_error_strings.c` stand in keinem
Build, definiert aber `uft_strerror()` — deklariert in
`include/uft/core/uft_error_ext.h:78`, samt Makro-Alias
`uft_error_string(rc)`. Ein Aufruf hätte einen **Link-Fehler** gegeben: eine
Zusage im öffentlichen Header ohne Deckung, dieselbe Klasse wie die falschen
`extern`-Deklarationen aus MF-442. Jetzt gebaut — als eigenes `SOURCES +=`,
nicht an den Block darüber angehängt, weil eine Kommentarzeile innerhalb einer
Fortsetzung genau die Kette bricht, um die es in Punkt 1 ging.

**Gap A: 0. Gap B: 0. Baseline leer.**

---

### DOC-3 — `src/samdisk/` hatte keinen Lizenztext, obwohl MIT ihn verlangt (2026-08-22, MF-458) → ✓ BEHOBEN

SAMdisk 4.0 ALPHA, © 2002–2024 Simon Owen, steht unter **MIT** (bestätigt über
die GitHub-API von `simonowen/samdisk`). Unsere Kopie hatte keine
`License.txt`. MIT verlangt ausdrücklich, dass Copyright-Vermerk und
Lizenztext „in all copies or substantial portions" mitgeführt werden — die
Datei ist Pflicht, nicht Höflichkeit. Nachgetragen.

Dazu `src/samdisk/README.md`, das drei Dinge festhält:

- **Der Ordner wird von keinem Build kompiliert** — beide Builds binden ihn nur
  als Include-Pfad ein. Wer nach „wird nicht gebaut" filtert, hält ihn für tot;
  in MF-271 wurden aus genau diesem Muster `tests/test_switch.c` und
  `tests/test_provider_switch.cpp` fälschlich gelöscht.
- **Er wird bereits als Referenz-Orakel benutzt**, mit Datei-und-Zeile-Zitaten
  in `uft_td0.c`, `uft_fdi_plugin.c`, `uft_scp_direct.h` und
  `scp_provider_v2.h`. Genau der Ansatz, den `docs/XCOPY_COMPARISON.md` für
  libdisk vorschlägt — hier schon etabliert, nur nirgends dokumentiert.
- **MIT erlaubt mehr als die Nachbarn:** Codeübernahme ist zulässig, solange
  der Vermerk mitgeht. X-Copy hat gar keine Lizenz, `xvs.library` sagt „All
  Rights Reserved".

Für den T3-Abbau relevant: SAMdisk hat für **17 der 62 T3-Formate** einen
eigenen Handler (`adf_arc`, `cfi`, `cpm`, `cqm`, `do`, `fdi_pc98`, `ipf`,
`mfi`, `mgt`, `msa`, `sad`, `sap_thomson`, `scl`, `st`, `td0`, `trd`, `udi`).
Für die ist ein Cross-Check möglich, **ohne eine reale Referenzdiskette zu
besitzen** — und Referenzmaterial ist dort der Engpass, nicht Code.

---

### CI-2 — `-Werror=format` brach acht Commits lang die Linux-CI, und ich habe sie nicht angesehen (2026-08-21, MF-457) → ✓ BEHOBEN

**Prozessfehler, nicht nur Codefehler.**

MF-448 schaltete `-Wformat -Werror=format -Werror=format-extra-args` ein, um
eine reale Fehlerklasse zu schließen: `uft_smart_report()` hatte ein `%s` mehr
als Argumente (MF-444). Der Zweck war richtig, das Flag zu breit.

`-Werror=format` schaltet auf GCC die **ganze** `format`-Gruppe scharf, also
auch `-Wformat-truncation` und `-Wformat-overflow`. Die melden, dass `snprintf`
abschneiden *könnte* — bei einem Pfad in einer Fehlermeldung der Normalfall,
und `snprintf` schneidet sicher ab. Eine andere Klasse als „Argument fehlt".

**Folge:** auf CI-Linux brachen **12 Testziele** nicht mehr durchs Kompilat:

```
test_convert_file_detection, test_cpm_fs, test_geos, test_gw_encoder,
test_gw_protocol, test_register_all_formats, test_write_gate,
test_greaseweazle_v2, test_hal_conformance, test_kryoflux_emulator,
test_hardware_tab_gui, test_provider_switch
```

Sie erscheinen als ctest **„Not Run"** und färben den CI-Job rot. Die
Workflows *Sanitizer*, *Coverage*, *Audit* und *Emulator* blieben grün, der
qmake-Build ebenfalls (dort steht `-Wall -Wextra` ohne `-Werror`) — nur der
CMake-Testbau kippte.

**Warum es lokal nicht auffiel — und warum es nicht auffallen konnte:**

| | lokal | CI |
|---|---|---|
| Compiler | MinGW GCC 13 | GCC auf glibc |
| Build-Typ | Debug (`-O0`) | Release (`-O2`) |
| `_FORTIFY_SOURCE`-Builtins | nein | ja (`__builtin___snprintf_chk`) |

`-Wformat-truncation` braucht **Optimierung und** die glibc-Builtins.
Nachgeprüft: mit `-O2` auf MinGW meldet dieselbe Datei weiterhin nichts. Der
Fehler war lokal **nicht reproduzierbar** — genau der Fall, für den
[`ci_test_gating`](../.claude/) notiert ist: *local-green ≠ CI-green*.

**Der eigentliche Fehler ist aber, dass ich acht Commits lang „ctest 211/211,
alle Gates grün" gemeldet und CI nicht aufgerufen habe.** MF-448 bis MF-456
sind alle mit rotem CI gelandet. Der letzte grüne Lauf war MF-447.

**Fix:** `-Wno-error=format-truncation` und `-Wno-error=format-overflow`. Damit
bleibt scharf, wofür der Gate da ist (falsche und fehlende Argumente,
`-Werror=format-extra-args`), und die Truncation-Analyse bleibt Warnung.

**Konsequenz für den Arbeitsablauf:** `gh run list` nach jedem Push, bevor
„fertig" gesagt wird. Ein lokal grüner Lauf beweist bei
compiler-flag-Änderungen nichts über die anderen beiden Plattformen — und
gerade Flags, die Warnungen zu Fehlern machen, sind plattformabhängig.

Der `|| true` im CMake-Testbau ist **kein** Mangel, sondern dokumentierte
Politik (CI-1): qmake ist der kanonische Build, der CMake-Testbau ist
best-effort, und der scharfe Schritt ist ctest. Genau deshalb wurden die 12
nicht gebauten Ziele auch korrekt als Fehler sichtbar.

---

### ARCH-17 — Das Compiler-Attribut-Vokabular stand in bis zu vier Headern, mit widersprüchlichen Ergebnissen (2026-08-21, MF-456) → ✓ BEHOBEN

Direkter Anschluss an ARCH-1: dieselben Header, dieselbe Klasse. Nachgezählt
über `uft_common.h`, `uft_compiler.h`, `uft_config.h`, `uft_platform.h`,
`uft_simd.h` und `compat/uft_platform_base.h`:

| Makro | Definitionen | Nutzungen in `.c`/`.cpp` |
|---|---:|---:|
| `UFT_ALIGNED` | 12 | **0** |
| `UFT_INLINE` | 11 | **0** |
| `UFT_ARCH_NAME` | 10 | **0** |
| `UFT_NOINLINE` | 9 | **0** |
| `UFT_RESTRICT` | 9 | **0** |
| `UFT_THREAD_LOCAL` | 7 | **0** |
| `UFT_CACHE_LINE_SIZE` | 7 | **0** |
| `UFT_ALIGNOF` | 3 | **0** |
| `UFT_CACHE_ALIGNED` | 2 | **0** |

**70 Definitionen, null Aufrufer.** Das allein wäre nur Ballast. Die
Widersprüche sind das Problem:

- **`UFT_INLINE` hatte drei Bedeutungen:** `static inline` (`uft_compiler.h`),
  `static inline __attribute__((always_inline))` (`uft_common.h`) und
  **`inline __attribute__((always_inline))`** ohne `static`
  (`uft_config.h`, `compat/uft_platform_base.h` auf POSIX). Die dritte ist in C
  ein **Link-Fehler**, sobald der Compiler nicht inlinet — eine `inline`-Funktion
  ohne externe Definition hat keine.
- **`UFT_CACHE_LINE_SIZE`**: `uft_config.h` wusste **32** auf ARM32,
  `uft_compiler.h` setzte pauschal 64 hinter `#ifndef`, und `uft_platform.h`
  setzte **ungeschützt** 64 — die dritte hätte den richtigen Wert auf ARM32
  überschrieben.
- **`UFT_ARCH_NAME`**: `"ARM64"` in `uft_platform.h`, `"arm64"` in
  `uft_config.h`. Ein String-Makro mit zwei Schreibweisen.
- **`uft_simd.h`** definierte `UFT_ALIGNED` hinter **`#ifndef UFT_LIKELY`** —
  der Wächter nannte ein anderes Makro als das, was er schützte. Ob
  `UFT_ALIGNED` hier entstand, hing davon ab, ob ein anderer Header vorher
  `UFT_LIKELY` gesetzt hatte.

**Zwei Eigentümer, nach Sachgebiet getrennt:**

| Vokabular | Eigentümer |
|---|---|
| `UFT_INLINE`, `UFT_FORCE_INLINE`, `UFT_NOINLINE`, `UFT_RESTRICT`, `UFT_THREAD_LOCAL`, `UFT_ALIGNED`, `UFT_ALIGNOF`, `UFT_CACHE_ALIGNED`, `UFT_SSE_ALIGNED` | `include/uft/uft_compiler.h` |
| `UFT_ARCH_*`, `UFT_ARCH_NAME`, `UFT_ARCH_BITS`, `UFT_CACHE_LINE_SIZE`, `UFT_BIG_ENDIAN`, `UFT_LITTLE_ENDIAN`, `UFT_PATH_SEP` | `include/uft/compat/uft_platform_base.h` |

Übernommen wurde jeweils die **informiertere** Fassung: `static inline` (kein
Link-Fehler), 32 Byte Cache-Line auf ARM32, Großschreibung bei `UFT_ARCH_NAME`.
Die Duplikate in `uft_config.h` sind **gelöscht**, nicht mit `#ifndef`
abgesichert — seit die Datei den Eigentümer selbst einbindet, könnte ein
Rückfall nie feuern und wäre toter Code mit einem anderen Wert.

**Regel C im 19. Gate** (`scripts/platform_header_gate.py`) hält es zu.

> **Beim Verifizieren der Regel:** Sie prüfte zunächst **nichts**. In das
> Regex-Literal war beim Schreiben ein echtes Backspace-Byte (0x08) statt der
> Zeichenfolge `` geraten — `(UFT_[A-Z_0-9]+)` trifft nie. Der Gate lief
> grün, weil er blind war. Aufgefallen erst bei der Rot-Probe: eine absichtlich
> eingebaute Doppeldefinition wurde nicht gemeldet. Nach der Reparatur fand die
> Regel sofort eine echte, von mir übersehene Stelle —
> `uft_config.h:163 UFT_CACHE_ALIGNED`.
>
> Lehre, dieselbe wie bei MF-446: **ein Wächter, der nicht rot werden kann, ist
> schlimmer als keiner.** Jede neue Regel braucht eine Rot-Probe, bevor sie
> zählt.

Makro-Konflikt-Baseline: **11 → 5**. Der Rest sind Format-Konstanten
(`UFT_IMD_MAX_*`, `UFT_MAX_SECTOR_SIZE`, `UFT_VERIFY_OPTIONS_DEFAULT`) — eine
andere Familie, noch offen.

**Offen gelassen:** dass alle neun Makros null Aufrufer haben. Sie stehen jetzt
je einmal an einem sinnvollen Ort; ob eine Compiler-Abstraktion ohne Nutzer
bleiben soll, ist eine Entscheidung über die künftige API und keine
Fehlerbehebung.

---

### ARCH-1 — Zwei `uft_platform.h`, die einander nie sehen (2026-08-18, MF-411) → ✓ BEHOBEN (MF-455), 19. Gate steht

> **Gelöst durch Trennen statt Zusammenführen.** Die Zusammenführung war zweimal
> versucht worden und brach beide Male den Build (MF-416: 187 von 197 Tests).
> Auflösung, Wächter und die dabei gefundene scharfe Falle stehen unten am Ende
> dieses Eintrags.

**Architecture.** Gefunden beim Auflösen der Include-Guard-Kollisionen.

`include/uft/uft_platform.h` (420 Zeilen) setzt seinen Guard `UFT_PLATFORM_H`
und inkludiert **danach** `uft/compat/uft_platform.h` (277 Zeilen) — mit dem
Kommentar „Include compatibility layer first for POSIX functions on Windows".
Der compat-Header trägt jedoch **denselben** Guard. Der Effekt:

- Wer `uft/uft_platform.h` einbindet, bekommt die Kompatibilitätsschicht
  **nie** — sie wird bei jedem Mal übersprungen. Der Kommentar beschreibt eine
  Absicht, die nie wirksam war.
- Umgekehrt binden `src/formats/legacy/uft_imd.c` und
  `src/hal/uft_greaseweazle_full.c` compat direkt als **erste** Zeile ein und
  sehen deshalb den großen Header **nie**. Diese beiden Dateien übersetzen also
  gegen eine andere Plattform-Definition als der gesamte Rest des Baums.

Beim probeweisen Auftrennen der Guards traten sofort **9 doppelt definierte
Makros** zutage (`UFT_PACKED`, `UFT_PACKED_END`, `uft_bswap16/32/64` u. a.).
Sie sind semantisch gleich (compiler-bedingt MSVC gegen GCC), das Verhalten
ändert sich also nicht — aber die Duplikation ist real und wurde bisher
ausschließlich durch die Guard-Kollision verdeckt.

**Bewusst nicht in MF-411 mitgemacht.** Die Entflechtung zweier
Plattform-Header ist eine eigene Aufgabe; sie im Zuge einer
Guard-Umbenennung nebenbei zu erledigen hieße, 9 Makro-Definitionen unter
Zeitdruck zu verschieben. Der Guard von `compat/uft_platform.h` wurde daher
als **einzige** Ausnahme belassen und ist in
`scripts/update_inventory.py::GUARD_COLLISION_ALLOWED` mit Begründung
eingetragen — der neue Kollisions-Wächter meldet also weiterhin 0, ohne die
Ausnahme zu verschweigen.

**Zusammenführung versucht und wieder zurückgenommen (MF-416).** Der
naheliegende Weg — Guard auftrennen, die doppelten Makros idempotent machen —
wurde gebaut und **bricht den Build**: 187 von 197 Tests fallen aus.

Ursache, gemessen statt vermutet: `compat/uft_platform.h` ist keine reine
Definitionssammlung, sondern enthält **invasive POSIX-Shims für Windows**, die
Standardnamen umdefinieren:

```c
#define close   _close
#define read    _read
#define write   _write
#define access  _access
#define unlink  _unlink
#define mkdir(path, mode) _mkdir(path)
```

Solange der Header nur von zwei Dateien direkt eingebunden wird, ist das
beherrschbar. Macht man ihn über `uft/uft_platform.h` baumweit sichtbar, trifft
`mkdir(path, mode)` auf jeden echten zweiargumentigen POSIX-Aufruf —
`error: macro "mkdir" requires 2 arguments, but only 1 given` und
`'mkdir' redeclared as different kind of symbol`.

**Die Überschneidung selbst ist klein:** von 46 bzw. 60 Makros teilen sich die
beiden Header genau **sechs** — `CLOCK_MONOTONIC`, `UFT_BIG_ENDIAN`,
`UFT_PACKED`, `uft_bswap16/32/64`. Compat hat über 40 Makros, die der große
Header nicht kennt (POSIX-Konstanten, `UFT_PATH_SEP`, `UFT_THREAD_LOCAL`,
`UFT_INLINE`). Genau diese Schicht kommt heute bei keinem Nutzer des großen
Headers an.

**Ein latenter Unterschied ist dabei aufgefallen, nicht behoben:**
`UFT_BIG_ENDIAN` ist im großen Header *definiert-oder-abwesend*, in compat
*0-oder-1*. `#ifdef UFT_BIG_ENDIAN` antwortet also je nach gesehenem Header
entgegengesetzt. Geprüft: **kein einziges `#ifdef` darauf im Baum**, nur eine
Wert-Abfrage `#if UFT_BIG_ENDIAN` innerhalb von compat selbst. Damit ist es
latent, nicht aktiv — in neuem Code trotzdem `#if` statt `#ifdef` benutzen.

*Nächster Schritt, jetzt konkret:* compat in **zwei** Dateien trennen —
gefahrlose Plattform-/Compiler-Erkennung und Endianness auf der einen Seite,
die namensumdefinierenden POSIX-Shims auf der anderen. Nur der erste Teil darf
baumweit sichtbar werden; die Shims bleiben Opt-in für die Dateien, die sie
brauchen (`uft_imd.c`, `uft_greaseweazle_full.c` — Letzterer ist geschützter
Pfad, Änderungen dort erst nach Rückfrage). Erst danach lässt sich der Guard
auftrennen und der Ausnahme-Eintrag in `GUARD_COLLISION_ALLOWED` entfernen.

---

## Auflösung (2026-08-21, MF-455)

**Genau so gemacht — und der geschützte Pfad blieb unberührt.**

`src/hal/uft_greaseweazle_full.c` und `src/formats/legacy/uft_imd.c` binden
`uft/compat/uft_platform.h` als erste Zeile ein. Vor dem Schnitt geprüft
(`gcc -MM`): **keine der beiden zieht `uft/uft_platform.h` transitiv**. Der
compat-Header behält daher seinen vollen Inhalt für sie; nur sein Guard heißt
jetzt `UFT_COMPAT_PLATFORM_H`. Damit ändert sich für beide Dateien nichts, und
die geschützte Datei musste nicht angefasst werden.

| Datei | Inhalt | Sichtbarkeit |
|---|---|---|
| `include/uft/compat/uft_platform_base.h` *(neu)* | Plattform-/Compiler-Erkennung, Endianness, `uft_bswap*`, `uft_htole*`, `UFT_PATH_SEP`, `UFT_THREAD_LOCAL`, `UFT_INLINE`, POSIX-Konstanten, `ssize_t` | **baumweit**, über `uft/uft_platform.h` |
| `include/uft/compat/uft_platform.h` | `open`/`close`/`read`/`write`/`lseek`/`stat`/`fstat`/`fileno`/`access`/`unlink`/`mkdir`, `usleep`/`sleep`, `strcasecmp`, Shims `memmem`/`clock_gettime`/`gettimeofday` | **Opt-in**, zwei Dateien |

**Die ~40 Makros kommen jetzt zum ersten Mal an.** Der Kommentar „Include
compatibility layer first for POSIX functions on Windows" beschrieb seit jeher
eine Absicht, die die Guard-Kollision zunichtemachte.

### Die latente Falle war nicht latent — sie wurde durch den Schnitt scharf

Der Eintrag oben hielt fest, `UFT_BIG_ENDIAN` sei „definiert-oder-abwesend"
gegen „0-oder-1" und das sei latent, weil es im Baum kein `#ifdef` darauf gebe.
Das stimmte für `UFT_BIG_ENDIAN`. Für **`UFT_LITTLE_ENDIAN`** nicht:

```c
/* include/uft/uft_platform.h, vor MF-455 */
#ifdef UFT_LITTLE_ENDIAN
    #define uft_le16(x) (x)
    ...
#else
    #define uft_le16(x) uft_bswap16(x)
```

Der Name war bis dahin **nur auf Little-Endian überhaupt definiert** — die
Abfrage war also zufällig richtig. Sobald die Basis ihn immer setzt (als
`(!UFT_BIG_ENDIAN)`), ist `#ifdef` immer wahr und `uft_le16()` auf einer
Big-Endian-Maschine die Identität, also **falsch**. `scripts/platform_header_gate.py`
hat die Stelle beim ersten Lauf gemeldet, bevor sie ausgeliefert wurde.

Es gab außerdem eine **dritte** Endianness-Erkennung in `include/uft/uft_config.h`,
mit derselben „definiert-oder-abwesend"-Semantik und einem ratenden letzten
Zweig (`/* Default assumption - can be overridden */`). Auch sie liest jetzt aus
der Basis. Ergebnis: eine Definition, immer 0 oder 1.

### Zwei weitere Doppelungen dabei aufgelöst

- **`uft_bswap16/32/64`** stand in beiden Headern, im großen nach
  `UFT_COMPILER_GCC`/`MSVC` unterschieden, in compat nach `_WIN32`. **MinGW ist
  beides** — die beiden hätten sich gegenseitig überschrieben, sobald die
  Guard-Kollision fällt. Die Basis unterscheidet nach Compiler, was die
  richtige Regel ist (`_byteswap_ushort` ist eine MSVC-Intrinsic). Nebenbei
  hatte der Fallback im großen Header einen Fehler: `uft_bswap64` rief
  `uft_bswap32(x)` mit dem vollen `uint64_t` auf, ohne Cast — die oberen 32 Bit
  gingen in die Maskenrechnung ein.
- **`clock_gettime`**: der Shim hing an `#ifndef CLOCK_MONOTONIC`. Seit die
  Basis die Konstante selbst setzt, braucht es ein eigenes Merkmal
  (`UFT_COMPAT_NEEDS_CLOCK_GETTIME`) — sonst kollidiert der `static`-Shim mit
  MinGWs eigener Deklaration in `<time.h>`.

### Wächter (19. Gate, `scripts/platform_header_gate.py`)

- **A:** `uft/compat/uft_platform.h` darf nur aus `.c`/`.cpp` und nur aus den
  zwei gelisteten Dateien kommen — nie aus einem Header, weil sich die Shims
  darüber unkontrolliert verbreiten. Wer sie wirklich braucht, trägt sich ein
  und begründet es; wer aus der Liste verschwindet, fliegt auch aus ihr raus.
- **B:** kein `#ifdef`/`defined()` auf `UFT_BIG_ENDIAN`/`UFT_LITTLE_ENDIAN`.

Beide Regeln verifiziert: `#if` zurück auf `#ifdef` gedreht → gemeldet;
compat-Include in eine dritte Datei gesetzt → gemeldet.

`GUARD_COLLISION_ALLOWED` in `scripts/update_inventory.py` ist **leer**; die
Makro-Konflikt-Baseline schrumpft von 12 auf 11 (`UFT_BIG_ENDIAN` erledigt).

**Offen bleibt:** verifiziert ist der MinGW-Bau (211/211 Tests). Der Schnitt
berührt jede Datei, die POSIX-Namen unter Windows benutzt; Linux und macOS
prüft die CI.

**Getrennt davon, ebenfalls offen:** vier Makros sind unabhängig von diesem
Paar doppelt definiert und erzeugen seit jeher Build-Warnungen —
`UFT_COMPILER_VERSION` (`uft_compiler.h` gegen `uft_platform.h`),
`UFT_PREFETCH` (`uft_compiler.h` gegen `uft_config.h`) und `UFT_ENCODING_FM`
/ `UFT_ENCODING_MFM` **dreifach** (`uft_flux_pll.h`, `uft_god_mode.h`,
`uft_types.h`). Nicht von MF-411 verursacht, hier nur festgehalten, weil sie
beim Vermessen auffielen.

> **Nachtrag MF-451:** dieselbe Klasse, eine Ebene tiefer und mit echtem
> Schaden, war das Packing-Vokabular — acht Definitionen, `fat12_bpb_t`
> ungepackt. Siehe ARCH-16. Die Entflechtung der beiden `uft_platform.h`
> bleibt offen; das Packing ist jetzt unabhängig davon in einer Datei.

### FMT-14 — Drei Sync-Definitionen im Baum, sie weichen genau auf geschützten Disks ab (2026-08-18, MF-395) → ✓ RESOLVED (MF-401, benannt)

**Correctness.** Gefunden beim Umbau von `test_d64_writer` auf echten Code:
UFT zählt Sync-Marken an zwei Stellen, mit **unterschiedlicher Definition**.

| Ort | Bedingung | entspricht |
|---|---|---|
| `gcr_find_sync()` (`src/formats/c64/uft_gcr_ops.c:193`) | Byte `0xFF`, gefolgt von einem Byte mit gesetztem MSB | **9** Eins-Bits, **byte-ausgerichtet** |
| `ufm_c64_metrics_from_gcr()` (`src/protection/ufm_c64_metrics.c`) | ≥ 10 aufeinanderfolgende Eins-Bits an beliebiger Position | **10** Eins-Bits, **bitweise** |

Die zweite entspricht der 1541-Hardwarebedingung (der Schieberegister-Vergleich
kennt keine Byte-Grenzen). GCR ist ein **5-Bit**-Code, Sync-Marken liegen daher
nicht zwangsläufig byte-ausgerichtet.

**Gemessen an realen Disks** (Werkzeug: beide Zähler über dieselben G64-Spuren):

| Disk | Byte-Zähler | Bit-Zähler | Abweichung |
|---|---|---|---|
| VICE-formatiert, 35 Spuren | 1366 | 1366 | **0** |
| Alien Syndrome (kommerziell, unprotected), 40 Spuren | 1536 | 1536 | **0** |
| Bounty Bob (**geschützt**), 71 Slots | — | — | **auf 15+ Spuren, bis ±4** |

Auf wohlgeformten Disks sind die beiden also austauschbar. Sie laufen genau
dort auseinander, wo das Werkzeug seinen Zweck hat: auf kopiergeschützten
Medien. Beide Richtungen kommen vor — der Byte-Zähler übersieht nicht
ausgerichtete Sync-Läufe (bis +4 Spurdifferenz), der Bit-Zähler verwirft
9-Bit-Läufe, die der Byte-Zähler mitzählt (−1).

**Nicht „einfach vereinheitlichen":** welche Definition richtig ist, hängt vom
Zweck ab. Für die Nachbildung des 1541-Verhaltens ist die bitweise korrekt;
für Werkzeuge, die byte-ausgerichtete Strukturen suchen, kann die andere
angemessen sein. Was nicht angeht, ist dass beide unbenannt nebeneinander
existieren und je nach Aufrufer ein anderes Ergebnis liefern.

**Aufgelöst (MF-401) — durch Benennung, nicht durch Vereinheitlichung.** Die
Untersuchung der Aufrufer zeigte, dass beide Definitionen an ihrem jeweiligen
Ort korrekt sind und eine Vereinheitlichung deshalb falsch gewesen wäre:

`gcr_find_sync()` wird 13× **innerhalb** von `uft_gcr_ops.c` von den
Header-/Datenblock-Dekodierern benutzt. Die suchen byte-ausgerichtete
GCR-Strukturen; dass die Funktion einen Byte-Index zurückgibt, ist kein
Implementierungsdetail, sondern ihr Vertrag — sie *kann* keine mitten im Byte
beginnende Sync ausdrücken. Der Bit-Zähler charakterisiert dagegen die Spur als
Medium, was der Kopierschutz-Pfad braucht.

Beim Aufräumen kam eine **dritte**, bis dahin unbemerkte Definition zutage:
`uft_xum_gcr_count_syncs()` (`tests/flux_gen/xum1541/flux_gen.c`) zählt Läufe
von ≥ 2 ganzen `0xFF`-Bytes, also ≥ 16 Eins-Bits. Sie ist dort richtig, weil sie
ausschließlich auf Spuren angewandt wird, die dieselbe Datei erzeugt hat — auf
einem realen Capture würde sie jede unausgerichtete oder kurze Sync übersehen.

Umgesetzt:
- `gcr_count_syncs()` → **`gcr_count_syncs_bytealigned()`** (kein
  Produktions-Aufrufer, nur 3 Teststellen; kein ABI-Baseline-Eintrag)
- Alle drei Definitionen (A) byte-ausgerichtet / (B) 1541-Hardware /
  (C) generator-lokal sind im jeweiligen Header bzw. am Code benannt und
  verweisen gegenseitig aufeinander, mit den gemessenen Zahlen
- `ufm_c64_metrics.h` sagt jetzt ausdrücklich, dass `sync_count` **nicht**
  dasselbe ist wie der byte-ausgerichtete Zähler

Der Unterschied bleibt in `tests/test_d64_writer.c` festgenagelt — inklusive
eines Falls, der die 9-gegen-10-Bit-Schwelle direkt zeigt. Verhalten unverändert;
was sich ändert, ist dass kein Aufrufer mehr versehentlich den falschen Zähler
greifen kann.

### FMT-13 — TD0-Plugin erkannte unkomprimierte Teledisk-Images nie (2026-08-18, MF-389) → ✓ RESOLVED

**Correctness.** Gefunden beim Ersetzen der Replica-Tests durch echte
Plugin-Aufrufe — und ein Musterbeispiel dafür, warum diese Testklasse gefährlich
ist.

`src/formats/td0/uft_td0.c:8` definierte `TD0_MAGIC_NORMAL 0x5444`. Gelesen wird
mit `uft_read_le16()` (`p[0] | p[1] << 8`), also passt diese Konstante auf eine
Datei, die mit den Bytes **`D`,`T`** beginnt. Echte Teledisk-Images beginnen mit
`T`,`D` (normal/RLE) bzw. `t`,`d` (advanced/Huffman) — `LE16` davon ist `0x4454`
bzw. `0x6474`. Der Advanced-Wert war korrekt, der Normal-Wert vertauscht.

**Folge:** Das registrierte TD0-Plugin lehnte **unkomprimierte** TD0-Images —
den häufigsten Fall — in `probe` *und* `open` ab. Erkannt wurden nur
Huffman-komprimierte Dateien.

**Belegkette, drei unabhängige Quellen:**
1. `src/samdisk/td0.cpp:10,180` — die im Repo mitgelieferte kanonische
   SAMdisk-Implementierung vergleicht die Signatur als **Byte-String** `"TD"`
   bzw. `"td"` per `memcmp`, ganz ohne Endianness.
2. `src/formats/td0/uft_td0_parser_v2.c:37` — der **zweite** TD0-Leser im
   selben Repo hatte `0x4454`, also korrekt; sein Testvektor `{0x54,0x44}`
   (Zeile 859) ebenfalls.
3. `include/uft/uft_formats_extended.h:120` — dort steht `TD0_MAGIC_NORMAL "TD"`
   als String, ebenfalls korrekt.

Zwei von drei Definitionen im Baum waren richtig; ausgerechnet die
ausgelieferte war falsch.

**Warum es niemand bemerkte:** `tests/test_td0_plugin.c` war ein Replica-Test.
Er übernahm dieselbe falsche Konstante, **erklärte den Fehler sogar im
Kommentar** („0x5444 = on-disk bytes {'D','T'}") und baute seine Testdatei aus
genau dieser Konstante — er stimmte also zwangsläufig immer mit sich selbst
überein. Ein Test, der die Fehlannahme mitkopiert, kann sie nie widerlegen.

**Fix:** Konstante auf `0x4454` korrigiert, mit Herleitung im Code.
Regressionsschutz in `tests/test_plugin_probe_real.c`
(`td0_accepts_the_real_teledisk_signatures`): „TD" und „td" müssen akzeptiert,
die vertauschte Schreibweise „DT" muss abgelehnt werden.

**Nicht geprüft:** ob der Rest des TD0-Lesepfads (Header-Felder, RLE-Dekodierung)
gegen ein reales Teledisk-Image stimmt. Dafür fehlt ein TD0 im Korpus; das
Format bleibt T3.

### PROT-7 — Fünf Funktionen erfanden forensische Befunde → ✓ RESOLVED (2026-08-18, MF-384)

**Correctness / Forensik-Integrität.** Ein systematischer Scan der Schutz- und
Kernschicht fand Funktionen, die **Erfolg meldeten, ohne ihre Eingabe je
anzusehen**. Jede lieferte damit einen erfundenen Negativbefund:

| Funktion | Behauptete |
|---|---|
| `uft_c64_scan_all_protection()` | „keine Protection", Konfidenz 0.0 — direkt neben einem JSON-Report-Export |
| `uft_c64_scan_fat_tracks()` | „keine Fat Tracks" (`*found = 0`) |
| `uft_rapidlok_scan_disk()` | „kein RapidLok" — Bildpfad nie geöffnet |
| `uft_ir_detect_weak_bits()` | „keine Weak Bits" für jede Spur |
| `uft_protection_detect_pirateslayer()` | „nicht erkannt", alle drei Eingaben weggecastet |
| `uft_speedlock_write()` | erzeugte eine **erfundene** Spur: 16 KB `0x4E` plus 11 Sektoren mit Sync `0xA1` — dem IBM-Address-Mark — in einer als Amiga dokumentierten Routine (Amiga-Sync ist `0x4489`) |

Anders als der mit MF-379 gelöschte `uft_dec0de_detect` waren fünf davon in
**öffentlichen Headern** deklariert, also von jedem Aufrufer erreichbar. Keine
hatte aktuell Aufrufer — geladen, aber nicht abgefeuert.

**Fix (MF-384):** alle sechs melden jetzt Fehlschlag statt erfundenen Erfolg,
mit Begründung an Implementierung und Deklaration. `uft_speedlock_write()`
lässt den Ausgabepuffer unangetastet — Schutzverfahren werden dokumentiert,
nicht neu gemastert (read-only by design). Festgenagelt in
`tests/test_protection_honest_failure.c`; die Asserts sind so geschrieben,
dass eine **echte** Implementierung den Test rot macht — das ist dann das
Signal, den Assert durch einen Verhaltenstest zu ersetzen.

Der Test fand beim ersten Lauf sofort einen sechsten Pfad, den ich übersehen
hatte: `uft_ir_detect_weak_bits(NULL)` gab ebenfalls Erfolg zurück, ebenso der
Fall „weniger als zwei Umdrehungen" — letzterer ist eine echte Messgrenze und
meldet jetzt Fehlschlag statt „nichts gefunden".

**Warum das Gate sie durchließ:** `check_lazy_stubs()` verlangt, dass der Rumpf
**exakt** `return UFT_OK;` ist, matcht nur `uft_*`-Namen und überspringt jede
Funktion mit geschweiften Klammern. Diese Klasse — „füllt die Ausgabe mit
Nullen und meldet Erfolg" — liegt vollständig außerhalb. Zwei Versuche, daraus
eine automatische Regel zu bauen, sind gescheitert (ein Fehlalarm bei
korrektem Code, dann 411 Treffer). Ein belastbares Gate dafür ist offene
Werkzeugarbeit, kein Einzeiler.

### PROT-6 — G64-Plugin liefert keine Halbspuren; Halbspur-Schutz ist unsichtbar (2026-08-18, MF-383) → ✓ RESOLVED (MF-404)

**Correctness / Forensik-Integrität.** `g64_read_track()` ruft
`track_to_g64_index(g64_track, false)` — der `half_track`-Parameter ist fest
`false` verdrahtet. Das Plugin adressiert also ausschließlich Ganzspuren,
obwohl das G64-Format 84 Halbspur-Slots führt und die Datei sie enthält.

Am realen Beispiel messbar: Bounty Bob hat **35 echte Halbspuren** im Dump.
Über die Plugin-API sind sie sämtlich unerreichbar — die Daten liegen in der
Datei, die Pipeline sieht sie nie. Das verstößt gegen „Kein Bit verloren":
das Plugin verschweigt vorhandene Spuren, ohne das zu melden.

Folgen: (a) die gesamte Klasse der Halbspur-Kopierschutzverfahren ist über den
regulären Weg nicht detektierbar; (b) `ufm_cbm_check_rapidlok()`
(`has_half_track && track >= 36`) kann **nie** feuern, obwohl der Extraktor
`has_half_track` korrekt setzt, sobald man ihm eine Halbspur gibt.

*Nächster Schritt:* `read_track` um Halbspur-Zugriff erweitern — entweder über
eine explizite API (`read_half_track`) oder indem `cylinder` als Halbspur-Index
interpretiert wird. Das ist eine Plugin-/API-Frage mit Auswirkung auf alle
Aufrufer und deshalb bewusst nicht in MF-383 mitgemacht.

**Nachtrag (MF-384):** Es gibt im Baum bereits einen G64-Leser, der Halbspuren
kann — `g64_get_track()` (`src/formats/c64/uft_d64_g64.c:684`), benutzt vom
Konvertierungspfad und von `ProtectionAnalysisWidget`. Die Lücke war also nicht
„UFT kann keine Halbspuren lesen", sondern **zwei G64-Leser mit ungleichem
Können**, von denen ausgerechnet der Plugin-Pfad — der reguläre Weg — der
schwächere war.

**Behoben (MF-404).** Das Plugin-Interface hat einen optionalen Einstiegspunkt
bekommen:

```c
uft_error_t (*read_half_track)(uft_disk_t*, int halftrack_index,
                               int head, uft_track_t*);
```

`halftrack_index` adressiert die Datei so, wie sie aufgebaut ist: zwei Slots je
Zylinder, gerade = ganze Spur, ungerade = Halbspur. Im G64-Plugin teilen sich
`read_track()` und `read_half_track()` jetzt **einen** Rumpf
(`g64_read_slot()`) — der alte Weg rechnete `cylinder * 2` und konnte ungerade
Slots konstruktionsbedingt nie erreichen.

Die beiden Alternativen aus dem ursprünglichen Eintrag wurden verworfen:
`cylinder` als Halbspur-Index umzudeuten hätte die Bedeutung für jeden
bestehenden Aufrufer verschoben (Zylinder 17 hätte Spur 9.5 statt Spur 18
geliefert). Der additive Einstiegspunkt lässt die 87 anderen Plugins unberührt.

ABI: das Feld liegt direkt **vor** `api_version`, das laut Konvention letztes
Feld bleibt. Der Größen-Guard hat wie vorgesehen ausgelöst; die Pin-Größe ist
von 216 auf **224** gemessen (nicht geschätzt) nachgezogen. Alle Plugins nutzen
Designated Initializer, das Feld nullinitialisiert sich also in den 87, die es
nicht setzen. Nebenbei korrigiert: der Kommentar an `api_version` behauptete,
ein Feld dahinter breche „silently every plugin's designated initializer" — das
widersprach direkt der Prozedur im Guard darunter („ADD field at end") und
stimmt nicht, da Designated Initializer per Name binden.

Verifiziert am realen Korpus (`test_c64_protection_real_corpus`):
- Bounty Bob: **35 Halbspuren**, 36 Ganzspuren, 71 belegte Slots — exakt die im
  Korpus-Manifest hinterlegten Zahlen, jetzt erstmals über die Plugin-API
  erreichbar
- VICE-formatierte Disk (rechtefrei, Negativkontrolle): 0 Halbspuren,
  35 Ganzspuren — `read_half_track()` erfindet nichts
- gerade Slots liefern byteidentisch dasselbe wie `read_track()`, damit die
  beiden Einstiegspunkte nicht auseinanderdriften

**Offen:** `NULL` bei den übrigen 87 Plugins heißt „bietet keinen
Halbspur-Zugriff an", **nicht** „das Format hat keine". Formate mit Halb- oder
Viertelspuren — allen voran **WOZ und A2R** (Apple II nutzt Viertelspuren) —
sind noch nicht geprüft. Das ist derselbe Befundtyp, nur an anderer Stelle.

### PROT-9 — Weak-Bit-Positionen wurden jenseits Byte 8191 falsch gemeldet (2026-08-18, MF-400) — BEHOBEN

**Correctness / Forensik-Integrität.** `uft_protection_detect_weak_bits()`
schrieb die Bitposition in ein `uint16_t`. Damit sind nur Positionen bis 65535
darstellbar, also Bits bis Byte 8191. Reale Rohspuren sind größer — Amiga DD
~12798 Bytes, PC HD ~12500 —, sodass jedes Weak-Bit in der zweiten Spurhälfte
mit `(Position mod 65536)` gemeldet wurde.

Das ist kein Datenverlust, sondern eine Falschangabe: empirisch reproduziert
lieferte ein Weak-Bit bei Byte 8192 die Position 0 und eines bei Byte 8600 die
Position 3271 — beides Bits nahe dem Spuranfang, die nie schwach waren. Ein
Writer, der diese Liste zum Reproduzieren des Schutzes benutzt, hätte die
falschen Bits randomisiert.

Zweiter Defekt derselben Funktion: die Suche brach ab, sobald das Zielarray
voll war, und gab dessen Kapazität zurück. 128 Weak-Bits mit Platz für 8
meldeten „8". Der Aufrufer konnte eine vollständige Liste nicht von einer
abgeschnittenen unterscheiden — genau die stille Kürzung, die
`DESIGN_PRINCIPLES.md` untersagt.

*Behoben:* Positionen sind jetzt `uint32_t`; der Rückgabewert ist immer die
wahre Gesamtzahl, nur der gespeicherte Präfix ist durch `max_positions`
begrenzt. Regressionsschutz: `tests/test_protection_pipeline.c`. Der
Signaturwechsel im öffentlichen Header war unkritisch — die Funktion hatte
repo-weit keinen Aufrufer (siehe PROT-10).

### PROT-10 — `uft_protection_detect_weak_bits()` hat keinen Aufrufer (2026-08-18, MF-400) → ✓ RESOLVED (MF-406, entfernt)

**Architecture.** Die Funktion ist implementiert, seit MF-400 getestet und
korrekt, wird aber von keiner Stelle im Repository aufgerufen — weder aus der
Recovery-Pipeline noch aus der GUI noch aus einem Format-Plugin. Der
Multiread-Pfad (`include/uft/recovery/uft_multiread_pipeline.h:99`) führt ein
eigenes `weak_mask`-Feld, füllt es aber nicht über diese Funktion.

Damit ist die Weak-Bit-Erfassung als Bibliotheks-API vorhanden, im Produkt aber
nicht wirksam. Das ist dieselbe Klasse wie PROT-8: eine getestete
Implementierung, die niemand benutzt.

**Die Bestandsaufnahme ergab nicht zwei, sondern sechs.** Vor der Entscheidung
wurde der ganze Baum durchgesehen — UFT hat sechs Weak-Bit-Produzenten in vier
verschiedenen Darstellungen:

| Produzent | Eingabe | Darstellung | Zustand |
|---|---|---|---|
| `multiread_vote_buffer()` | N Byte-Durchgänge | **byteweise** Maske (0/1) | verdrahtet, von `uft_format_verify.c` gelesen |
| ATX (`uft_atx.c`, `uft_atx_parser_v2.c`) | Format-Metadaten | **byteweise** Maske (0xFF) | verdrahtet, gelesen |
| HFE (`uft_hfe.c:711`) | Bitstream-Dekode | `uft_track_t.weak_mask`, **per Bit** | erzeugt, **nur freigegeben — nie gelesen** |
| `uft_scp_multirev` | N Flux-Umdrehungen | **Flux-Zellen**-Indizes | verdrahtet, modulintern |
| `a2r_fuse_captures()` | N Captures | **bit-gepackte** Maske | **keine Aufrufer** |
| `uft_protection_detect_weak_bits()` | genau 2 Lesungen | **Bitpositions-Liste** | **keine Aufrufer** |

Der Multiread-Pfad füllte `weak_mask` also durchaus — nur über seinen eigenen,
mächtigeren Mechanismus (N Durchgänge statt zwei). Die ursprüngliche Annahme
„das Feld wird nicht gefüllt" war falsch.

**Entschieden: entfernt (MF-406).** Die Funktion war korrekt und getestet — MF-400
hat zwei echte Defekte darin behoben —, aber **nichts im Baum konsumiert ihre
Darstellung**. Einen Aufrufer zu ergänzen hätte bedeutet, einen Konsumenten für
eine Repräsentation zu erfinden, nach der niemand fragt; genau das ist der
Mechanismus, der PROT-3 und FMT-2/3/10/11/12 erzeugt hat. Ihre Fähigkeit —
Weak-Bits aus zwei Lesungen dekodierter Spurbytes — ist zudem ein
Zwei-Umdrehungs-Sonderfall der N-Durchgangs-Abstimmung, die der Multiread-Pfad
bereits leistet und bereits herausgibt.

Verworfen wurde die Alternative „behalten und dokumentieren": sie hätte den
Befund nicht aufgelöst, sondern einen sechsten Weg konserviert, aus dem ein
künftiger Aufrufer blind den schwächsten hätte greifen können.

Das Wissen aus MF-400 überlebt den Code in PROT-9: Positionen dürfen nicht in
ein `uint16_t` (reale Rohspuren gehen über Byte 8191 hinaus), und ein volles
Ausgabearray muss als abgeschnitten gemeldet werden statt still gekappt.
`tests/test_protection_pipeline.c` entfällt mit der Funktion (Suite 194 → 193).

### PROT-12 — Weak-Bit-Produzenten ohne Konsument, plus vier Granularitäten von `weak_mask` (2026-08-18, MF-406) → ✓ RESOLVED (MF-408, benannt und entschieden)

**Architecture.** Aus derselben Bestandsaufnahme, nicht mit MF-406 behoben:

1. **`a2r_fuse_captures()`** (`src/parsers/a2r/uft_a2r_parser.c:935`) hat
   repo-weit keinen Aufrufer — und erzeugt eine **bit-gepackte** Maske
   (`weak_mask[i/8] |= 1 << (i%8)`), während der einzige Maskenkonsument im
   Baum, `uft_format_verify.c`, **byteweise** liest. Dessen Kommentar hält
   ausdrücklich fest, dass eine frühere per-Bit-Lesart dort ein Fehler war.
   Solange die Funktion niemand aufruft, ist das latent; ein Aufrufer, der die
   Maske an einen unified sector weiterreicht, würde die falschen Bytes
   überspringen.

2. **HFE setzt `uft_track_t.weak_mask` per Bit** (`uft_hfe.c:711`), und dieses
   Feld wird im ganzen Baum ausschließlich **freigegeben**, nie gelesen. Die
   Information, welche Bits einer HFE-Spur schwach sind, wird also erfasst und
   dann verworfen — grenzwertig zu „Kein Bit verloren", auch wenn nichts
   Falsches behauptet wird.

Damit trägt der Name `weak_mask` im Baum drei Bedeutungen: byteweise
(`uft_sector_unified_t`, gelesen), per Bit (`uft_track_t`, ungelesen) und
bit-gepackt (A2R, ohne Aufrufer). Das ist derselbe Befundtyp wie FMT-14, nur auf
einem Feldnamen statt auf einer Funktion.

**Bestandsaufnahme (MF-408) — drei Korrekturen an der Fassung oben.**

Die genaue Durchsicht ergab **fünf** `weak_mask`-Deklarationen in **vier**
Granularitäten, nicht drei Bedeutungen:

| # | Ort | Typ | Granularität | Zustand |
|---|---|---|---|---|
| A | `struct uft_track` (`uft_format_plugin.h`) | `bool *` | **pro Bit**, nicht gepackt | von HFE geschrieben, **kein Produktions-Leser** |
| B | `struct uft_sector` (`uft_types.h`) | `uint8_t *` | **pro Byte** | ATX + Multiread schreiben, `uft_format_verify.c` liest |
| C | `uft_bitstream_layer_t` (`uft_track.h`) | `uint8_t *` | war **unbenannt** | Layer von keinem Code befüllt |
| D | `uft_sector_data_version_t` (`core/uft_sector.h`) | `uint8_t *` | war **unbenannt** | von keinem Code befüllt |
| E | `a2r_fuse_captures()`-Parameter | `uint8_t *` | **bit-gepackt** | keine Aufrufer, Packung war undokumentiert |

**Korrektur 1 — das Kollisionsrisiko war geringer als oben behauptet.** A und B
liegen in *verschiedenen* Strukturen mit *verschiedenen Typen* (`bool *` gegen
`uint8_t *`). Der Compiler unterscheidet sie also; eine Verwechslung hätte nicht
still passieren können. Bestehen bleibt das Risiko bei E, weil dort ein
`uint8_t *` mit gepackter Bedeutung an einen Konsumenten gehen könnte, der die
Byte-Bedeutung erwartet.

**Korrektur 2 — die Byte-Semantik ist bereits regressionsgesichert.**
`tests/test_marginal_data_preserved.c` nagelt sie fest
(`weak_mask[2] == 1` für das abweichende Byte, Nachbarn 0). Das war beim
Schreiben von PROT-12 nicht bekannt.

**Korrektur 3 — die HFE-Maske wird nicht „verworfen".** Sie wird am öffentlichen
Track-Struct mitgeführt und ist am Produzenten getestet
(`tests/test_hfe_v3_weak.c`); es fehlt nur ein Konsument. „Erfasst und
weitergereicht, aber niemand handelt darauf" ist die richtige Beschreibung.

**Umgesetzt: alle fünf Stellen benannt** (FMT-14-Behandlung). Jede Deklaration
nennt jetzt Typ, Granularität, wer schreibt, wer liest und wo die Semantik
durch einen Test festgehalten ist. C und D bekamen die Granularität
*festgeschrieben*, obwohl sie unbefüllt sind — eine unbenannte Granularität auf
einem leeren Feld ist genau das, was ein späterer Implementierer hätte raten
müssen. C folgt A (Bitstream-Layer, indexiert Bits), D folgt B (Sektor-Ebene).

**Entschieden pro ungenutztem Produzenten:**

- **`a2r_fuse_captures()` bleibt.** Bewusst anders als bei PROT-10, wo
  `uft_protection_detect_weak_bits()` entfernt wurde: dort war die Fähigkeit
  anderswo abgedeckt, hier nicht — A2R-Multi-Capture-Fusion gibt es im Baum
  kein zweites Mal. Sie zu entfernen hieße eine Fähigkeit zu löschen statt eines
  Duplikats. Die Packung ist jetzt im Header exakt beschrieben, mit
  ausdrücklicher Warnung, den Puffer nicht an einen Byte-Konsumenten zu geben.
- **Die HFE-Maske bleibt.** Sie enthält reale Messwerte aus einem getesteten
  Dekoder; sie zu streichen wäre Datenverlust.

**Nachtrag (MF-417) — die beiden „fehlenden Konsumenten" sind keine Lücken.**
Die Formulierung oben („braucht einen Aufrufer", „braucht einen Konsumenten")
unterstellte Versäumnisse. Beim Nachsehen erwies sich beides als *richtige
Abwesenheit*:

**Die per-Bit-HFE-Maske.** Der einzige Pfad, der sie benutzen würde, ist das
v3-Zurückschreiben — und `hfe_write_track()` **verweigert es ausdrücklich**:

```c
/* v3 write-back is not lossless: read decodes the opcode stream into a clean
 * bitstream + weak_mask (MF-362), and that decode drops NOP/SETINDEX/
 * SETBITRATE and replaces RAND bits with a placeholder … Refuse rather than
 * write a corrupt/degraded v3 track. v1/v2 write is unaffected. */
if (pdata->is_v3) return UFT_ERROR_NOT_SUPPORTED;
```

Das ist „keine stille Veränderung" mustergültig umgesetzt. Die Maske wird
erfasst und mitgeführt, weil sie forensisch zählt; sie hat keinen Konsumenten,
weil der einzig denkbare bewusst nicht existiert, solange es keinen echten
v3-Encoder gibt. Ein Konsument wäre hier nicht die Behebung einer Lücke,
sondern ihre Verletzung.

**`a2r_fuse_captures()`.** Der A2R-Parser hält **alle** Captures einer Spur
(`captures[]` + `capture_count`) und gibt am Ende alle frei — er fusioniert
nicht. Auch das ist die forensisch richtige Wahl: Fusion verdichtet mehrere
unabhängige Lesungen zu einer und wirft damit genau die Evidenz weg, wegen der
A2R mehrere speichert. Diese Funktion in den Lesepfad zu hängen wäre ein
**Defekt**, keine Verdrahtung. Ihr Platz ist eine ausdrücklich angeforderte
Analyse, die es noch nicht gibt.

Damit bleibt von PROT-12 nur, was MF-408 bereits erledigt hat: die
Granularitäten sind benannt, und die Entscheidung „behalten" ist für beide
begründet. Es ist **keine Folgearbeit offen** — was fehlt, ist ein
Analyse-Einstiegspunkt für A2R-Fusion, und der ist ein Feature, kein Mangel.



### PROT-8 — Zwei konkurrierende C64-Metrik-Extraktoren, der getestete wird nicht benutzt (2026-08-18, MF-384) → ✓ RESOLVED (MF-405)

**Architecture.** Seit MF-381 existiert `ufm_c64_metrics_from_gcr()`: in C,
gegen reale Disks verifiziert, füllt genau die Felder, die
`ufm_c64_prot_analyze()` liest. Unabhängig davon rechnet
`ProtectionAnalysisWidget` seine Metriken weiterhin selbst — byteweise statt
bitweise, in C++, ohne einen einzigen Test (`src/gui/` hat keine Testabdeckung),
und in die falschen Strukturfelder (siehe Korrektur unter PROT-2).

Damit hat das Projekt zwei Implementierungen desselben Fakts, von denen die
getestete nicht benutzt wird und die benutzte nicht funktioniert.

**Behoben (MF-405).** Das Widget rechnet nicht mehr selbst; der G64-Pfad ruft
`ufm_c64_metrics_from_gcr()`.

**Die Auswirkung war vollständig, nicht graduell.** Am realen Korpus gemessen,
bevor irgendetwas geändert wurde:

| Bounty Bob (geschützt) | Spuren | Treffer | Schema | Konfidenz |
|---|---|---|---|---|
| Widget-Rechnung (Ist) | 71 | **0** | None | 0 |
| `ufm_c64_metrics_from_gcr()` | 71 | 76 | Half Track | 85 |

Die GUI meldete „kein Schutz" auf einer Disk mit 35 Halbspuren und 15
headerlosen Spuren — und zwar **immer**, für jede Disk. Der Grund ist kein
Rundungsfehler: die beiden Feldmengen überschneiden sich in **keinem einzigen
Feld**. Das Widget füllte `track_x2`, `revolutions`, `is_half_track`,
`bitlen_*`, `weak_region_*`, `illegal_gcr_events`, `max_sync_run_bits`;
`ufm_c64_prot_analyze()` liest `track`, `has_half_track`,
`track_length_ratio`, `has_custom_sync`, `sector_count`, `bad_gcr_count`,
`duplicate_ids`. Der Klassifikator bekam in jedem gelesenen Feld eine Null.

Nebenbei entfernt: byteweise Sync-Zählung (Läufe von `0xFF` × 8 statt ≥ 10
Eins-Bits an beliebiger Bitposition, vgl. FMT-14) und die Heuristik
„Bytewerte 0x00–0x03 sind ungültiges GCR", die die Struktur eines 5-Bit-Codes
verfehlt.

**Beim Umbau gefunden: eine Index-Konventions-Kollision.** Die beiden G64-Leser
im Baum nummerieren verschieden — `g64_get_track()` als Spur × 2 (Index 2 =
Spur 1), `ufm_c64_metrics_from_gcr()` als 0-basierten Slot (Index 0 = Spur 1).
Den Index unkonvertiert durchzureichen verschiebt jede Spur um eins und trifft
exakt die drei Geschwindigkeitszonen-Grenzen (17→18, 24→25, 30→31), wo sich die
Sollkapazität ändert. Messbar: drei erfundene „Long Track"-Treffer auf **beiden**
sauberen Referenzdisks. Mit der Umrechnung liefern beide Leser dort
übereinstimmend null Treffer — das ist die Kreuzprobe, die die Konvention
belegt. Die Detailanzeige und die Heatmap schlüsseln jetzt über `track` /
`is_half_track` statt über den rohen Slot-Index, damit sie nicht davon abhängen,
welcher Leser die Liste gefüllt hat.

**Erstmals sichtbar:** 13 der 35 Halbspuren tragen Custom-Sync. Diese Daten
konnte vorher **kein** Pfad sehen — das Plugin konnte Halbspuren nicht
adressieren (PROT-6), und die Widget-Rechnung setzte `has_custom_sync`
überhaupt nie.

`src/gui/` hat keine Testabdeckung, deshalb ist der Datenpfad des Widgets
(`g64_load` → `g64_get_track` → Metriken → Klassifikation) jetzt ohne Qt in
`test_c64_protection_real_corpus` nachgebaut: die 15 Ganzspuren mit Custom-Sync
decken sich exakt mit der Menge, die der Plugin-Pfad pinnt, und beide sauberen
Disks bleiben still.

### PROT-11 — Der SCP-Pfad des Widgets liefert nie Schutz-Treffer (2026-08-18, MF-405) → Ursache korrigiert (MF-407), Fix gehört zu MF-106

**Correctness.** `ProtectionAnalysisWidget::loadFlux()` füllt für SCP-Quellen
weiterhin nur Flux-Statistiken (`bitlen_*`, `weak_region_*`, `revolutions`) —
also genau die Felder, die `ufm_c64_prot_analyze()` **nicht** liest. Eine über
SCP geladene Disk liefert deshalb nach wie vor null Schutz-Treffer, aus demselben
Grund wie PROT-8 vor der Korrektur.

**Ursache korrigiert (MF-407) — die obige Diagnose war in zwei Punkten falsch.**

**1. Der Flux → GCR → Metriken-Pfad fehlt nicht, er existiert.**
`flux_decode_gcr_c64()` (`src/flux/uft_flux_decoder.c:671`) liefert mit
`opts.keep_raw_bits = true` genau den rohen GCR-Bitstrom, den
`ufm_c64_metrics_from_gcr()` konsumiert. Die Kette ist jetzt belegt statt
vermutet — `tests/unit/test_flux_gcr_c64_sync.c` führt sie in beiden Richtungen
vor:

- normale Spur → 2 Sektoren erkannt, `has_custom_sync == false`
- **headerlose Spur** (Sync vorhanden, kein einziger 0x08-Header — genau das,
  wonach die Schutzanalyse sucht) → `sync_count == 4`, `sector_count == 0`,
  **`has_custom_sync == true`**

Dabei festgehaltener Vertrag: `flux_decode_gcr_c64()` gibt `FLUX_ERR_NO_SYNC`
zurück, wenn sie keinen dekodierbaren Sektor findet — also gerade auf einer
geschützten Spur. Der Bitstrom ist dennoch vollständig und gültig; er wird vor
dem Return angehängt. Ein Aufrufer für Schutzanalyse muss `raw_bits`
**unabhängig vom Statuscode** verwenden. Der Test nagelt das fest.

Reichweite dieser Aussage, ausdrücklich: die Flux-Zeiten im Test sind
**synthetisch** und exakt am Bitzell-Raster erzeugt. Das belegt die Kette und
die Symbol-Dekodierung, **nicht**, dass ein reales SCP-Capture mit Jitter,
Drehzahlschwankung und Weak-Bits korrekt dekodiert. Dafür fehlt ein
C64-Flux-Abbild mit dokumentierter Herkunft im Korpus — vorhanden ist nur
`gw_amigados.scp` (Amiga MFM). Greaseweazle ist lokal nicht installiert und
nicht über PyPI beziehbar, das Abbild konnte deshalb nicht selbst erzeugt
werden.

**2. Das Widget bekommt gar keine Flux-Daten — es liest über einen Stub.**
`uft_scp_read()` und `uft_scp_get_track_flux()` (`uft_format_parsers.h`) sind
Honest-Stubs in `src/core/uft_core_stubs.c:234,255`, die unbedingt `-1` liefern.
Der SCP-Zweig erzeugt also nicht falsche Metriken, sondern **überhaupt keine**:
jede Spur fällt durch `count <= 0` heraus, `m_trackMetrics` bleibt leer.

Daneben existiert der **echte** SCP-Leser (`uft_scp_ctx_t`,
`uft_scp_open_memory()`, `src/flux/uft_scp_parser.c`), und das SCP-Plugin ist
**T1b-verifiziert** (7 Tests, Cross-Tool-Korpus `gw_amigados.scp`, cbmstuff-Spec).

Das ist die **dritte** Instanz desselben Musters, nach PROT-6 (Plugin-Pfad
schwächer als der zweite G64-Leser) und PROT-8 (Widget rechnet selbst statt den
verifizierten Extraktor zu rufen): *der reguläre Weg greift die schwächere von
zwei vorhandenen Implementierungen.*

**Der Fix gehört zu MF-106, nicht hierher.** `docs/MASTER_PLAN.md` beschreibt
die Ursache bereits als „SCP-API-Dualität" und bündelt sie als
**MF-106-bundle = SCP-API-Unifizierung + Inline-MFM-Replacement** (geschätzt
5–6 Tage), womit SCP→IMG, SCP→ADF und SCP→D64 gemeinsam grün werden. Der
GUI-Schutzpfad ist ein **fünfter** Konsument desselben toten Stubs, den die
Schätzung noch nicht enthält — dazu die vier Konvertierungen
`uftc_convert_scp_to_{d64,mfm_sectors,hfe,g64}`, die alle über
`uft_scp_read()` einsteigen und deshalb sämtlich nicht funktionieren, obwohl
`CLAUDE.md` sie als Feature führt.

Den GUI-Zweig einzeln auf den echten Leser umzuhängen wurde **verworfen**: es
würde MF-106 aufspalten und ließe sich für C64 mangels Korpus ohnehin nicht
verifizieren.

*Nächster Schritt (unverändert MF-106, jetzt kleiner):* die SCP-API
vereinheitlichen. Die C64-Hälfte der Kette ist mit MF-407 belegt und damit aus
der Unbekannten-Liste raus; offen bleibt der Leser-Umbau. Parallel dazu ein
C64-SCP mit dokumentierter Herkunft ins Korpus holen (`gw convert
--format=commodore.1541` aus dem rechtefreien `vice_c1541_35trk.d64` ergäbe
einen T1b-Eintrag), sonst bleibt auch nach dem Umbau nur eine synthetische
Verifikation.



### PROT-5 — `ufm_cbm_check_vmax()` ist unter der belegten Sync-Definition degeneriert (2026-08-18, MF-382) → ✓ RESOLVED (MF-402, Behauptung entfernt)

**Correctness / Forensik-Integrität.** Aufgefallen beim Aktivieren von
`has_custom_sync`: die V-MAX-Prüfung lautet

```c
return m->has_custom_sync && m->sector_count != 21 && ... != 17;
```

Da `has_custom_sync` per Definition `sector_count == 0` impliziert, ist die
zweite Hälfte **immer** wahr. Die Funktion ist damit logisch identisch mit
`ufm_cbm_check_custom_sync()` — sie unterscheidet nichts, hängt aber den
konkreten Namen **„V-MAX!" mit 85 % Konfidenz** an jede headerlose Spur, ohne
ein einziges V-MAX-spezifisches Indiz. Das ist eine Behauptung ohne Beleg,
also genau das, was `docs/DESIGN_PRINCIPLES.md` untersagt.

**Bewusst nicht stillschweigend „repariert":** eine engere V-MAX-Regel zu
erfinden, ohne je eine echte V-MAX-Disk gesehen zu haben, wäre die
Wiederholung von PROT-3. Stattdessen ist der Ist-Zustand in
`test_c64_metrics_corpus` als Defekt **festgenagelt**
(`vmax_check_is_degenerate_documented_defect`), damit er sichtbar bleibt und
jede spätere Änderung eine bewusste Entscheidung ist.

**An realen Daten bestätigt (MF-383).** Auf der realen Bounty-Bob-Disk (15
Ganzspuren mit Custom-Sync) nimmt der Klassifikator den Zweig
`custom_syncs > 5` und meldet als Hauptschema **„V-MAX!"** — hergeleitet
ausschließlich aus generischer Struktur, ohne ein einziges V-MAX-spezifisches
Indiz irgendwo im Codepfad. Der Ist-Zustand ist in
`test_c64_protection_real_corpus` festgenagelt
(`vmax_label_on_this_disk_is_unsupported_PROT5`); der Assert dokumentiert, was
das Werkzeug *behauptet*, und ist ausdrücklich **keine** Aussage darüber,
welches Verfahren diese Disk tatsächlich verwendet.

**Aufgelöst (MF-402) — die unbelegte Behauptung entfernt, keine neue erfunden.**
Der Kern des Befunds war nie „uns fehlt eine V-MAX-Regel", sondern „das Werkzeug
behauptet etwas, das es nicht geprüft hat". Eine falsche Behauptung zu
*entfernen* braucht keine neue Evidenz — nur eine *positive* V-MAX-Aussage
täte das. Beides wurde getrennt:

Der Klassifikator `ufm_c64_scheme_detect.c` **misst Struktur und benannte daraus
Produkte**. Die Taxonomie trennt beides bereits sauber: `VMAX`, `RAPIDLOK`,
`COPYLOCK`, … sind Produktnamen, `CUSTOM_SYNC`, `HALF_TRACK`, `LONG_TRACK`,
`BAD_GCR`, `DUPLICATE_ID` sind Messungen. Dieser Pfad vergibt jetzt nur noch
Letztere.

- `ufm_cbm_check_vmax()` **gelöscht** — tautologisch, ohne jede
  Unterscheidungskraft; der Treffer war ein Duplikat des CUSTOM_SYNC-Treffers
  mit erfundenem Namen und einer Beschreibung, die eine nie erfolgte
  Sektorzahl-Prüfung behauptete.
- `ufm_cbm_check_rapidlok()` → **`ufm_cbm_has_half_track_beyond_35()`**. Die
  Bedingung (`has_half_track && track >= 36`) ist eine sinnvolle *strukturelle*
  Beobachtung, trägt aber keine RapidLok-Identifikation. Sie fließt jetzt in die
  Beschreibung des ohnehin erzeugten HALF_TRACK-Treffers ein — eine Beobachtung,
  ein Treffer, statt zwei.
- Primärschema: `custom_syncs > 3 && half_tracks > 0` → `HALF_TRACK`,
  `custom_syncs > 5` → `CUSTOM_SYNC` statt RapidLok bzw. V-MAX.

**Wichtig für PROT-8:** der RapidLok-Zweig war bisher nur deshalb harmlos, weil
`has_half_track` über den Plugin-Pfad nie gesetzt wurde (PROT-6). Sobald PROT-8
den GUI-Pfad an den echten Extraktor hängt, hätte er gefeuert — die Korrektur
musste also *vor* PROT-8 kommen, sonst hätte dessen Fix eine neue Falschaussage
eingeführt.

**Offen bleibt die positive Aussage.** Ein V-MAX-spezifischer Detektor existiert
im Baum: `c64_detect_vmax_version()`
(`src/protection/c64/c64_protection_analysis.c`) sieht sich den Loader auf
Track 20 auf die Marker `$49`/`$5A`/`$EE` an. Er ist bewusst **nicht** an diesen
Pfad verdrahtet — das wäre eine Integration, die ohne reale V-MAX-Disk nicht
validierbar ist, und Raten hat PROT-3 erzeugt. Der nächste Schritt bleibt: reales
V-MAX-geschütztes G64 mit dokumentierter Herkunft beschaffen, `c64_detect_vmax_version()`
dagegen halten (er ist selbst unverifiziert, Tier 3), erst dann verdrahten.

Regressionsschutz: `structural_analysis_emits_no_product_names` und
`half_track_beyond_35_is_reported_structurally` (`test_c64_metrics_corpus`)
sowie `real_protected_disk_is_described_not_named_PROT5` auf der realen
Bounty-Bob-Disk. Die beiden Tests, die den Defekt festnagelten, sind zu
Regressionswächtern umgebaut — genau die „bewusste Entscheidung", für die sie
angelegt wurden.

2. **`test_protection_pipeline` prüft ausschließlich `mock_detect_weak_bits()`.**
   Unverändert gültig: Der Test beweist die Pipeline-Verdrahtung, nicht einen
   einzigen echten Detektor. Er darf nicht als Scheme-Verifikation gezählt
   werden. Die echte Verdrahtung deckt jetzt `test_c64_metrics_corpus` ab.

### PROT-3 — `uft_dec0de_detect()` ist fabriziert: 0 von 34 realen Samples korrekt (2026-08-16, MF-378) → ✓ RESOLVED (MF-379, entfernt)

**Correctness / Forensik-Integrität.** Derselbe reale Korpus wie PROT-1, nur
gegen den zweiten ST-Detektor gehalten: `src/protection/uft_atarist_dec0de.c`.
Ground Truth = die Zuordnung Datei→Schutzsystem aus `samples/README.txt` des
dec0de-Projekts (34 Samples, 10 verschiedene Schutzsysteme).

**Ergebnis: 0 richtige Erkennungen, 3 falsche Labels.**

| Ground Truth | Samples | `uft_dec0de_detect()` sagt |
|---|---|---|
| Rob Northen 1988 | 15 | 15× „keine Protection" |
| Rob Northen 1989 | 13 | 13× „keine Protection" |
| Rob Northen 1989 | 2 (BIGNOSE, SWORDROS) | **„Rob Northen Copylock (1988)"** — falsche Serie |
| Illegal Anti-bitos | 5 | 5× „keine Protection" |
| NTM/Cameo Toxic Packer | 1 (TOX100) | **„Rob Northen Copylock (1988)", Konfidenz 0.9** |
| CID / Cooper / Lock-o-matic / Zippy / R.AL / Sly | 9 | kein Detektor vorhanden (Namen stehen nur in der Enum-Tabelle) |

**Warum es nicht funktionieren kann — die Patterns sind erfunden:**
- `ROBN88_PATTERN` = `4E75 41FA` (RTS + LEA). Vier Byte generischer
  68000-Code, keine Signatur. Trifft in 3 von 43 beliebigen Dateien —
  darunter der Toxic-Packer-Sample, der deshalb als CopyLock gemeldet wird.
- `TOXIC_PATTERN` verlangt den ASCII-String `TOXIC` hinter einem `BRA.W`.
  **`TOX100.PRG` enthält den String `TOXIC` an keiner Stelle.** Die Signatur
  wurde nicht aus der Datei abgeleitet, sondern aus dem Namen des Packers.
- `ROBN89_PATTERN` (NOP NOP LEA) und `ANTIBITOS_PATTERN` treffen keines der
  jeweils zugehörigen realen Samples.
- Die Prüfreihenfolge setzt das generischste Pattern an den Anfang, also
  gewinnt der falsche Treffer strukturell gegen jeden späteren richtigen.

**Status:** Die Funktion hat **null Aufrufer** im gesamten Baum und ihr
Ergebnis-Typ ist lokal in der `.c` definiert, also von außen gar nicht
verwendbar — bisher konnte niemand ein falsches Label sehen. Genau derselbe
Fabrikations-Mechanismus wie FMT-2/3/10/11/12, nur in der Protection-Schicht.

**Erledigt (MF-379): `src/protection/uft_atarist_dec0de.c` gelöscht.**
Lösch-Beweispipeline vollständig durchlaufen: keine der fünf dort definierten
Funktionen war in **irgendeinem** Header deklariert (also aus keiner anderen
Übersetzungseinheit erreichbar), 0 Referenzen im gesamten Baum, 0 CMake- und
0 Skript-Referenzen, eine aktive `.pro`-Zeile (3285) entfernt, qmake-Vollbuild
abgenommen. Mitgelöscht wurden zwei *korrekte* GEMDOS-Funktionen — sie sind
Duplikate der bereits im Header vorhandenen `uft_gemdos_is_valid()` /
`uft_gemdos_get_size()`. Einziger inhaltlicher Unterschied war die zusätzliche
Akzeptanz von Magic `0x601B`; von den 25 realen `.PRG` im Korpus trägt das
keine einzige, der Unterschied ist also durch nichts Belegtes gedeckt.
Der Header `uft_atarist_dec0de.h` bleibt — dort steckt der **echte** Port
(PROT-4), der im Korpus-Test läuft.

### PROT-4 — Header-Port des Series-2-Finders: OOB-Read gefixt + gegen reale Daten verifiziert (2026-08-16, MF-378) → ✓ RESOLVED

Im Gegensatz zur fabrizierten `.c` ist `uft_robn89_find_start()` in
`include/uft/protection/uft_atarist_dec0de.h` der **echte** Algorithmus
(Trampolin-Suche `lea/move.l/add.l` mit XOR-Key-Kette). Zwei Punkte:

1. **Bug (Memory-Safety):** Die Schleife startete bei `i = 0`, leitet das
   Magic aber aus der *vorherigen* Instruktion `buf[j-4]` ab — bei `i == 0`
   also ein Lesezugriff 4 Byte **vor** dem Puffer. Latent, weil die Funktion
   bisher nirgends aufgerufen wurde; ASan hätte sie beim ersten echten Aufruf
   zerlegt. Gefixt: Schleifenstart `i = 4` (reale Trampoline liegen bei 1960
   bzw. 2214, es geht nichts verloren).
2. **Verifikation:** `test_corpus_protection_copylock` ruft den Header-Primitiv
   jetzt zusätzlich auf und prüft ihn gegen dieselbe gepinnte Ground Truth wie
   `uft_copylock_st_analyze()`. Beide unabhängigen Implementierungen liefern
   auf realen Loadern identisch Trampolin-Offset **und** `magic32`
   (2214/0x6B1D1929, 1960/0x0).

### AUD-7 — Rest-Einträge (klein)
- `test_mega65` ist der letzte EXCLUDED_TESTS-Eintrag (Header stub-only) —
  implementieren oder Test löschen.
- Vendored-Bäume (`src/samdisk/`, `src/switch/hactool/` inkl. mbedtls) haben
  kein Herkunfts-/Versions-Manifest — je eine `VENDORED.md` mit Upstream +
  Commit/Version wäre billig und beendet „woher stammt das?"-Fragen.

---

## Protection-Real-Korpus 2026-08-16 (MF-377) — Befunde

Erster Lauf eines Protection-Detektors gegen echte geschützte Originale
(dec0de-Samples, `tests/corpus_manifest/manifest.json`). Ergebnis: Series-2-
Erkennung real bestätigt (14

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
