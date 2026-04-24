# UFT Masterplan

**Stand:** 2026-04-23
**Zweck:** Einheitlicher Fahrplan der alle bisherigen Findings
(`KNOWN_ISSUES.md`, must-fix-hunter-Backlog, Skeleton-Audit,
XCopy/a8rawconv-Todos) zusammenführt. Soll verhindern, dass jede
Session mit einem neuen Audit „von vorne" anfängt.

---

## Die ehrliche Bestandsaufnahme

**UFT als Codebase:**
- ~860 Quelldateien, ~17 Subsysteme
- 969 Header in `include/uft/`
- **175 davon sind Skelette** (≥10 Deklarationen, ≥80 % nicht implementiert) — siehe `SKELETON_HEADERS_AUDIT.md`
- 3 355 nicht implementierte `uft_*`-Funktions-Deklarationen
- 80 Format-Plugins, davon ca. 15 mit realen Tests (19 %)

**UFT als Produkt:**
- v4.1.3 taggt auf Release-Basis
- GUI mit mehreren Tabs (XCopy, Hardware, Analyse, Format Converter)
- Davon **2 Tabs Phantom-Features** ohne Backend (XCopy, teile der FS-Browser)

**Fazit:** Die Kluft zwischen Anspruch (was Header und GUI versprechen)
und Umsetzung (was die `.c`-Files liefern) ist **das** strukturelle
Problem. Alle Sub-Findings (MF-001..MF-011, SSOT-Drift, Build-Divergenz)
sind Symptome dieser Kluft.

---

## Die drei Regeln die von jetzt an gelten

Diese drei Regeln verhindern das „immer-wieder-von-vorne"-Muster:

### Regel 1 — Keine neuen Skelette

Ab jetzt:
- **Jede neue `.h`-Datei in `include/uft/` die `uft_*`-Funktionen
  deklariert, muss im selben Commit eine entsprechende `.c`-Datei mit
  mindestens einer echten Implementierung haben.**
- Alternative: Die `.h` wird mit `/* PLANNED FEATURE — <scope> */`
  markiert UND in `KNOWN_ISSUES.md` als bewusster Platzhalter
  dokumentiert.
- Enforcement: `audit_skeleton_headers.py` wird erweitert um
  Regression-Check (neue Skelette = CI-Fail, wie bei
  `build_system_parity`).

### Regel 2 — Keine neuen GUI-Elemente ohne Backend

Ab jetzt:
- **Jeder neue Qt-Tab / Button / Dialog muss eine wirkende
  Backend-Funktion triggern, oder `setEnabled(false)` mit
  Tooltip „Feature X: planned for release Y" haben.**
- Enforcement: neuer Test `tests/test_gui_backend_wiring.cpp` der
  prüft dass jeder `QPushButton::clicked`-Connect auf eine nicht-
  leere Methode zeigt.

### Regel 3 — Keine Audit-Expeditionen ohne Prozess-Abschluss

Ab jetzt:
- Wenn ein Audit neue Findings produziert, werden sie **in bestehende
  Tracking-Files** (`KNOWN_ISSUES.md`, `MASTER_PLAN.md`) aufgenommen,
  bevor neuer Code geschrieben wird.
- **Nicht mehr als 3 offene Findings gleichzeitig**. Wenn der Backlog
  > 3 Einträge hat, wird abgearbeitet bevor neue dazukommen.
- Jede Session beginnt mit einem Blick in diesen Masterplan, nicht
  mit einem frischen Scan.

---

## Der Backlog konsolidiert

Alle bisher aufgedeckten Findings in einer priorisierten Liste:

| ID | Severity | Thema | Status |
|---|---|---|---|
| MF-001 | P1 | CMakeLists.txt + docs/API.md Versions-Drift | ✓ CLOSED |
| MF-002 | P1 | CHANGELOG-Duplikate | ✓ CLOSED |
| MF-003 | P2 | Error-Code-Dualismus | ✓ CLOSED (SSOT) |
| MF-004 | P1 | `_parser_v3.c` Proliferation (146 deleted, 5 kept) | ✓ CLOSED |
| MF-005 | P2 | 656 tote Deklarationen | ⊂ MF-011 (Untermenge) |
| MF-006 | P0 | qmake/CMake Divergenz | ✓ CLOSED (baseline+guard) |
| **MF-007** | **P1** | **Plugin-Test-Coverage 11.8 %** | **offen** |
| MF-008 | P2 | docs / KNOWN_ISSUES stellenweise stale | offen |
| MF-009 | P3 | 30 TODO/FIXME im Code | offen |
| MF-010 | P2 | 318 nicht-kanonische Includes | offen |
| **MF-011** | **P0** | **175 Skeleton-Header, 3355 Phantom-Funktionen** | **offen** |
| **MF-012** | **P0** | **XCopy-Tab Phantom-Feature** (GUI ohne Backend) | **offen** |

**Zwei P0 verbleibend: MF-011 und MF-012.** Das sind die strukturellen
Phantome.

---

## Die Milestones

Drei klare Releases mit Abschluss-Kriterien statt endlosem Backlog-
Abarbeiten:

### M1 — Wahrheit vor Features (4-6 Wochen)

**Ziel:** Kein Phantom-Feature mehr. Was UFT anbietet, tut UFT auch.

Muss:
- [x] MF-011 Skeleton-Header bereinigen (Triage abgeschlossen)
  - [x] DELETE-Welle: **24 Header** ohne Konsumenten gelöscht (Commit 5b551fb)
    (Grep-Check ergab nur 24, nicht ~100 wie geschätzt — viele vermeintliche
    Skeletons hatten Call-Sites die als DOCUMENT/IMPLEMENT gewertet wurden)
  - [x] DOCUMENT-Welle: **98 Header** mit `/* PLANNED FEATURE */`-Banner markiert,
    Index in `docs/PLANNED_APIS.md`, KNOWN_ISSUES.md §M.0 (Commit d9aa7a7)
  - [x] IMPLEMENT-Welle: **53 Header** mit `/* PARTIALLY IMPLEMENTED */`-Banner
    markiert (some impls, some stubs, live hazard). Per-Funktion-Triage in M2/M3.
  - **Ergebnis M1:** 175 Skeleton-Header → 24 gelöscht + 151 markiert. Jedes
    verbliebene Skeleton trägt jetzt einen sichtbaren Banner. Master-Plan Regel 1
    (keine neuen Skelette) ist durchsetzbar weil Altbestand dokumentiert ist.
- [x] MF-012 XCopy-Tab: `btnStartCopy->setEnabled(false)` + Tooltip.
  Die aktuelle `CopyWorker`-Backend-Implementierung macht nur generisches
  Byte-Copy mit MD5-Verify, keine Amiga-spezifischen Features (Virus-Scan,
  Bootblock-Analyse, BAMCOPY). Browse/Configure bleiben aktiv, so dass die
  geplante UI-Form sichtbar ist. Start-Button bleibt disabled bis M2 den
  echten XCopy-Backend liefert (`XCOPY_INTEGRATION_TODO.md` T1..T5).
- [x] MF-007 Plugin-Test-Coverage 40% TARGET ERREICHT (33 / 80 = 41.25 %)
  - Start: 10 Plugin-Tests (12.5 %).
  - Neu diese Session (+23 Tests): CRT, T64, DC42, D64, MOOF, A2R, ADF,
    P00, D71, D81, WOZ, IMG, MSA, DSK, NIB, EDSK, SCP, FDI, XFD, DO, PO,
    JV1, ATX. Alle 167 neuen Probe-Assertions grün unter `-Wall -Wextra -Werror`.
  - Beifund: ATX-Probe-Bug entdeckt UND behoben in derselben Session
    (Byte-Order-Mismatch in `ATX_SIGNATURE`). Fix via 0x41543858→0x58385441,
    Regression-Test mit 8 Assertions. KNOWN_ISSUES.md §M.-1 als CLOSED markiert.
- [ ] Tag v4.1.4 mit allen P0/P1-Fixes aus dieser Session

Abschluss-Kriterium: `audit_skeleton_headers.py` zeigt <50 Skelette
(Reduktion 70 %), `audit_plugin_compliance.py` zeigt >40 % Coverage.

### M2 — Feature-Komplettierung Amiga + Atari (6-8 Wochen)

**Ziel:** Die zwei Platformen die die meisten Skelett-Header haben auf
Production-Niveau.

Muss:
- [ ] **Amiga-Block** (`XCOPY_INTEGRATION_TODO.md`):
  - T1 Virus-Signature-SSOT (`amiga_bootblock_viruses.tsv`)
  - T2 `uft_bootblock_scanner.c` Implementation
  - T3 `uft_amigados_extended.c` aus Stub-Zustand heben
  - T5 BAMCOPY-Modus im ADF-Plugin
- [ ] **Atari-Block** (`A8RAWCONV_INTEGRATION_TODO.md`):
  - [x] TA1 `uft_write_precomp.c` (portiert aus `compensation.cpp`,
        Mac-800K Peak-Shift-Compensation, 13 Tests grün)
  - [x] TA2 `uft_interleave.c` (Sektor-Interleave-Calculator, 11 Tests grün)
  - [ ] TA3 ATX-Plugin auf 400 LOC (weak sectors, extra sectors)
- [ ] Tag v4.2.0 nach M2-Abschluss

Abschluss-Kriterium: XCopy-Tab wieder `setEnabled(true)`, ADF und ATX
haben voll-populierte Feature-Matrizen (Prinzip 7 §7.2).

### M3 — HAL-Stabilisierung + Release-Pipeline (4 Wochen)

**Ziel:** Die Hardware-Integrationen die heute als „stubbed" markiert
sind auf Production-Niveau.

Muss:
- [ ] T4 SCP-Direct-HAL (aus `A8RAWCONV_INTEGRATION_TODO.md` TA4)
- [ ] XUM1541 HAL real statt stubbed
- [ ] Applesauce HAL real statt stubbed
- [ ] CLAUDE.md Hardware-Abschnitt aktualisiert (keine „stubbed"-Einträge
  mehr, alles entweder „production" oder „not planned")
- [ ] Emulator-CI-Pipeline (§6.1 aus `KNOWN_ISSUES.md`) — mindestens
  WinUAE für ADF, VICE für D64
- [ ] Tag v4.3.0 nach M3

---

## Was wir NICHT mehr tun

Diese Dinge verbieten wir uns bis zum M3-Abschluss:

- **Neue Audits starten** — jeder neue Audit-Fund geht in die
  Regeln-Enforcement (siehe oben), nicht in eine neue Todo-Datei
- **Neue Subsysteme anfangen** — kein neuer ML-Modul, kein neues
  OCR, keine neuen Format-Plugins außerhalb der M2-Plattformen
- **Architektur-Refactorings ohne konkreten Bug** — die SSOT-Arbeit
  ist abgeschlossen, keine weiteren „lass uns auch dieses Fact SSOT-
  ifizieren"-Runden bis M3 durch ist
- **Feature-Migrationen zwischen Build-Systemen** — qmake bleibt
  primär, CMake bleibt Test-System, keine Migrations-Diskussion
- **DeepRead-Erweiterungen** — die 5 DeepRead-Module sind genug,
  bis M2 fertig ist und tatsächlich ein Nutzer sie ausprobiert hat

---

## Governance

Damit der Plan nicht wieder zerfällt:

**Pro Session:**
1. Start: `MASTER_PLAN.md` öffnen, aktuelles Milestone identifizieren
2. Innerhalb des Milestones: ein einzelnes Backlog-Item wählen (nicht
   mehrere parallel)
3. Vor Session-Ende: Status updaten (✓ / offen / blockiert + Grund)
4. Kein neuer Audit ohne Abschluss des laufenden Items

**Pro Commit:**
1. `ctest -L ssot-compliance` muss grün sein (drei Checks)
2. Wenn das Commit einen `.h`-Skeleton-Header hinzufügt oder ein
   neues Phantom-Feature im GUI anlegt: blockieren (Regeln 1+2)

**Pro Milestone-Abschluss:**
1. Tag setzen
2. `KNOWN_ISSUES.md` durchgehen: CLOSED-Einträge archivieren
3. `MASTER_PLAN.md` aktualisieren: Abschluss-Status pro Muss-Punkt

---

## Performance- & Algorithmen-Review (externes Review, 2026-04-24)

Externer Reviewer hat 10 Hot-Spots identifiziert. Stichproben-Verifikation
bestätigt die Befunde:

- `flux_find_sync` @ `src/flux/uft_flux_decoder.c:223` — Bit-für-Bit-Loop
  exakt wie im Review beschrieben (Shift-Register könnte O(N) statt O(N·16) sein).
- `flux_mfm_decode_byte` @ `src/flux/uft_flux_decoder.c:115` — Bit-Loop
  statt Bulk-Reader bestätigt.
- **Drei parallele Decoder-Pfade** bestätigt: `uft_flux_decoder.c` (977 LOC) +
  `algorithms/bitstream/FluxDecoder.cpp` (119 LOC) + `flux/fdc_bitstream/mfm_codec.cpp`
  (530 LOC) = **1626 LOC Redundanz**.
- Kalman „bidirectional RTS smoother" @ `src/algorithms/uft_kalman_pll.c:358-390`
  — tatsächlich nur Backward-Kalman + Averaging, **kein echter RTS**.

**Die 10 Befunde und Zuordnung zu Milestones:**

| # | Befund | Aufwand | Milestone | Begründung |
|---|---|---|---|---|
| 3 | PEXT/LUT Bit-Deinterleave | ~15 LOC | M1 | Quick-Win während Cleanup |
| 4 | CRC-Engine malloc-Fix | ~10 LOC | M1 | Offensichtlicher Bug |
| 8 | Histogram-Encoding-Detect | ~25 LOC | M1 | Kleiner Hot-Spot |
| 10 | Diverse kleine Fixes | <50 LOC | M1 | Opportunistisch |
| 1 | Rolling-Shift-Register Sync | mittel | M2 | ADF/ATX-Decoder profitieren direkt |
| 2 | Bulk-Bit-Reader | mittel | M2 | Gleicher Hot-Path wie #1 |
| 6 | Echter RTS-Smoother (Kalman) | groß | M2 | DeepRead-Qualität, nicht Performance |
| 5 | Single-Pass-Sync-Scan | mittel | M3 | Architektur-Eingriff |
| 7 | Scratch-Buffer-Pool | groß | M3 | Subsystem-übergreifend |
| 9 | Drei-Decoder-Konsolidierung | >500 LOC | M3 | Architektur, 1626 LOC Redundanz |

**Regel: Keine Optimierung von Phantom-Features.** Befunde #5, #7, #9 werden
bewusst auf M3 geschoben — solange ein Decoder-Pfad noch Skeleton-Konsumenten
hat, bringt Performance-Optimierung nichts. Zuerst Wahrheit (M1), dann Features
(M2), dann Geschwindigkeit (M3).

Detail-Dokument: `docs/PERFORMANCE_REVIEW.md` (falls Review-Volltext vorliegt).

---

## X-Copy Pro → UFT Algorithm-Migration (2026-04-24)

Externer Report analysiert X-Copy Professional (Amiga, 1989–1991, 68k-Assembly,
~10k LOC) und identifiziert 7 portable Algorithmen. Spot-Check bestätigt alle
nachprüfbaren Claims:

- `BitstreamDecoder.cpp:991-1017` `amiga_read_dwords` — Code verbatim identisch
  mit Report-Zitat (2-Pass, `std::vector<uint32_t> evens`, heap-alloc pro Sektor).
- `uft_amiga_protection.c` — exakt 367 LOC wie behauptet.
- `flux_find_sync` O(N·S)-Pattern — bereits in Performance-Review validiert.

**Wichtiger Fund:** X-Copy hat das Rolling-Shift-Register-Muster (Performance-Review
#1) bereits 1989 gebaut — mit 16× Rotation, Multi-Pattern-Match und syncpos-Tabelle.
Bestätigt dass der Ansatz richtig ist.

**Die 7 Befunde und Zuordnung zu Milestones:**

| # | Befund | Aufwand | Milestone | Begründung |
|---|---|---|---|---|
| 6 | syncpos-Tabelle (Single-Pass) | 0 LOC | M1 | Nur Architektur-Validierung für Perf #5 |
| 4 | Reverse-Scan Track-Länge (`getracklen`) | ~15 LOC | M2 | Helper für Greaseweazle/SCP Multi-Rev |
| 1 | Single-Pass Amiga-MFM-Decode (`decolop`) | ~40 LOC | M2 | ~2× Amiga-Decode; kein heap alloc; ADF/DMS-Plugins profitieren |
| 5 | MFM-Grenzkorrektur Review (`clockbits`) | ~30 LOC | M2 | Korrektheit Write-Path; Bug-Fix falls fehlend |
| 3 | GAP-Detection Histogramm (`gapsearch`) | ~60 LOC | M2 | Neues Feature `src/analysis/uft_track_layout.c` |
| 2 | Multi-Pattern Sync 16× Rotation (`Analyse`) | ~50 LOC | M2 | Copylock/Rob-Northen-Detection in uft_amiga_protection.c |
| 7 | Index-synchronisierte 2-Rev-Capture | architektonisch | out-of-scope | UFI-Firmware (STM32H723), nicht UFT |

**Copyright-Kontext:** X-Copy war kommerzielle Software mit später veröffentlichtem
Source. Algorithmen sind nicht urheberrechtlich geschützt, Code-Ausdruck schon.
Migration erfolgt via Verstehen + Neu-Implementation in C, nicht 1:1-Translitterierung.

**Synergie mit XCOPY_INTEGRATION_TODO.md:** Das existierende Doc ist auf Virus-Scanner,
Bootblock-DB und BAMCOPY fokussiert (Anwendungs-Ebene). Dieses neue Doc ist auf
Decoder-/Sync-/Track-Layout-Algorithmen fokussiert (Engine-Ebene). Zusammen bilden
sie den M2 Amiga-Block.

Detail-Dokument: `docs/XCOPY_ALGORITHM_MIGRATION.md`

---

## Verwandte Dokumente

- `docs/DESIGN_PRINCIPLES.md` — die 7 Kernprinzipien (unverändert gültig)
- `docs/KNOWN_ISSUES.md` — laufender Principle-Compliance-Tracker
- `docs/SKELETON_HEADERS_AUDIT.md` — Detailliste zu MF-011
- `docs/XCOPY_INTEGRATION_TODO.md` — Detailplan M2 Amiga-Block
- `docs/A8RAWCONV_INTEGRATION_TODO.md` — Detailplan M2 Atari-Block
- `docs/PERFORMANCE_REVIEW.md` — 10 Algorithmus/Performance-Befunde (extern)
- `docs/XCOPY_ALGORITHM_MIGRATION.md` — 7 X-Copy-Algorithmen für M2 (extern)
- `.claude/CONSULT_PROTOCOL.md` — Agenten-Zusammenarbeit

---

## Eins noch: ehrliche Zeitabschätzung

Mit einer Session pro Woche und dem Tempo dieser Session:

- **M1 (Wahrheit vor Features):** 4-6 Wochen konzentriert, kann parallel
  zu anderer Arbeit
- **M2 (Amiga + Atari):** 6-8 Wochen, Hauptaufwand ist der ATX- und
  SCP-Direct-Port
- **M3 (HAL):** 4 Wochen

**Gesamt bis v4.3.0: ~14-18 Wochen / 3-4 Monate**

Das ist langsam. Aber nach M1 ist UFT **ehrlich** — kein Phantom mehr,
jedes Feature hat Backend. Nach M3 ist UFT **komplett** im Umfang den
CLAUDE.md ohnehin als Ziel beschreibt.

Die schnelle Alternative — „wir mergen einfach weiter, fixen was
kaputt geht" — ist exakt das Muster das uns in diese Situation
gebracht hat.
