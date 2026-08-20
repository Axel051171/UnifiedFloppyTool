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

### ARCH-7 — 24 Kopien der CBM-Zonentabelle in drei Indexkonventionen (2026-08-20, MF-434) → ◐ SSOT STEHT, 22 Migrationen offen

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

### ARCH-1 — Zwei `uft_platform.h`, die einander nie sehen (2026-08-18, MF-411)

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

*Warum nicht in dieser Sitzung:* die Aufteilung berührt jede Datei, die
POSIX-Namen unter Windows benutzt, und ist ohne Windows-**und**-Linux-Bau nicht
verantwortbar zu verifizieren. Der Versuch ist dokumentiert, damit der nächste
Anlauf nicht bei null anfängt.

**Getrennt davon, ebenfalls offen:** vier Makros sind unabhängig von diesem
Paar doppelt definiert und erzeugen seit jeher Build-Warnungen —
`UFT_COMPILER_VERSION` (`uft_compiler.h` gegen `uft_platform.h`),
`UFT_PREFETCH` (`uft_compiler.h` gegen `uft_config.h`) und `UFT_ENCODING_FM`
/ `UFT_ENCODING_MFM` **dreifach** (`uft_flux_pll.h`, `uft_god_mode.h`,
`uft_types.h`). Nicht von MF-411 verursacht, hier nur festgehalten, weil sie
beim Vermessen auffielen.

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
