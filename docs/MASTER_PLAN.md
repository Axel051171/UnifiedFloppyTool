# UFT Masterplan

**Stand:** 2026-07-02 (Konsolidierungs-Pass MF-289; ursprünglich 2026-04-23)
**Zweck:** DAS führende Status-Dokument. Einheitlicher Fahrplan der alle
Findings (`KNOWN_ISSUES.md`, must-fix-hunter-Backlog, Skeleton-Audit,
XCopy/a8rawconv-Todos) zusammenführt. Jede Session beginnt hier —
nicht mit einem frischen Scan.

---

## Die ehrliche Bestandsaufnahme

**UFT als Codebase (Zahlen aktualisiert 2026-08-16, MF-366):**
- ~756 Quelldateien, ~644 Header (nach MF-011 19-Wellen-Cleanup)
- **Skeleton-Header: 0** (≥10 Deklarationen, ≥80 % nicht implementiert),
  **0** nicht implementierte `uft_*`-Deklarationen über der Skeleton-Schwelle —
  die letzten 9 Phantom-Header (162 Deklarationen, darunter die nie
  implementierte Audit-Trail-/Forensic-Report-C-API) wurden in der
  MF-366-Welle gelöscht; live nachprüfbar via
  `python scripts/audit_skeleton_headers.py`. Historischer Stand 2026-07-02
  war 133 Header / 2 613 Deklarationen.
- 88 Format-Plugin-Registrierungen (138 Format-IDs); Verifikations-Stand
  skript-generiert in `docs/VERIFICATION_TIERS.md` (T1=0, T1b=3, T2=14,
  T3=71 — die frühere „41 % mit realen Tests"-Angabe war hand-gepflegt und
  zählte synthetische Tests mit)
- 45 Konvertierungspfade, 13 Roundtrip-Matrix-Einträge
- Noch 11 Alt-Dateien im Pattern-A-Stil (`*_parser_v3.c`) — Rest der
  ehemals „287 Stubs" wurde in MF-011 gelöscht oder nach Pattern B migriert

**UFT als Produkt:**
- **v4.1.5 released** (Hardware Hardening + Tier-2.5 Simulators)
- Type-Driven-HAL-Refactor abgeschlossen und auf `main` gemerged
  (MF-150/169/174/176)
- Voll-Audit 2026-06-10 mit 16 Findings: 14 geschlossen (3 P0-Forensik-
  Blocker im Converter-Stack, ABI-Bombe, Versions-SSOT, GUI-Consent) —
  Details in `KNOWN_ISSUES.md` §Audit-Closing-Note
- Emulator-Sequenz: SCP ✓, Greaseweazle ✓ (2 von 9 Controllern),
  XUM1541 in Arbeit

**Fazit:** Die Kluft zwischen Anspruch (Header/GUI) und Umsetzung
(`.c`-Files) bleibt das strukturelle Kern-Problem — aber sie ist jetzt
vollständig inventarisiert und banner-markiert. Der Abbau läuft nach
`docs/STUB_ELIMINATION_PLAN.md` (6 Phasen mit Gates).

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
| MF-007 | P1 | Plugin-Test-Coverage 11.8 % | ✓ CLOSED — `audit_plugin_compliance.py` (2026-08-18): 88/88 Plugins Prinzip-7-konform, 0 Stubs |
| MF-008 | P2 | docs / KNOWN_ISSUES stellenweise stale | offen |
| MF-009 | P3 | TODO/FIXME-Marker (Census 2026-04-25) | scoped (siehe §MF-009-Census) |
| MF-010 | P2 | Non-kanonische Includes — Census 2026-05-25: 81 unqualifizierte `uft_*.h`-Includes (Sibling-Pattern, technisch OK durch -I-Pfade) + ~140 SAMdisk-imported Header (third-party). Echte Cleanup-Kandidaten: 81 Sibling-Includes → `uft/...`-Pfad. Multi-Session-Arbeit, P2. | scoped |
| MF-011 | P0 | 175 Skeleton-Header, 3355 Phantom-Funktionen | ✓ CLOSED — `audit_skeleton_headers.py` (2026-08-18): **0** Skelette, 0 unimplementierte Deklarationen |
| MF-012 | P0 | XCopy-Tab Phantom-Feature (GUI ohne Backend) | ✓ CLOSED in M1 (Start-Button disabled + Tooltip) |

**Kein P0 mehr offen** (Stand 2026-08-18, MF-409). MF-011 und MF-012 waren in
der Tabelle noch als offen geführt, während die M1-Sektion darunter beide als
erledigt abhakte — der Widerspruch ist gegen die Audit-Skripte aufgelöst:
`audit_skeleton_headers.py` meldet 0 Skelette, `audit_plugin_compliance.py`
88/88 konforme Plugins.

**Offen ist jetzt anderes** — siehe §Sprint-3 unten. Der größte Einzelposten
ist MF-106 (SCP-API-Unifizierung), an dem vier Konvertierungspfade und der
GUI-Schutzpfad hängen.

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
- [x] Tag v4.1.4 — überholt: `v4.1.4-rc1` und **`v4.1.5`** sind getaggt,
      VERSION.txt steht auf 4.1.5. M1 ist damit vollständig abgeschlossen.

Abschluss-Kriterium: `audit_skeleton_headers.py` zeigt <50 Skelette
(Reduktion 70 %), `audit_plugin_compliance.py` zeigt >40 % Coverage.

### M2 — Feature-Komplettierung Amiga + Atari (6-8 Wochen)

**Ziel:** Die zwei Platformen die die meisten Skelett-Header haben auf
Production-Niveau.

Muss:
- [~] **Amiga-Block** (`XCOPY_INTEGRATION_TODO.md`):
  - [x] T1 Virus-Signature-SSOT (`data/amiga_bootblock_viruses.tsv`,
        48 Einträge aus xvs.HISTORY + bekannten early viruses,
        Generator `scripts/generators/gen_virus_db.py` → `src/fs/uft_amiga_virus_db.c`,
        `include/uft/uft_amiga_virus_db.h` API, 8 Tests grün.
        Alle Signatures PENDING — Schema + Infrastruktur fertig, echte
        Byte-Pattern brauchen xvs.library-Binary-Extraktion in Folge-Commit.)
  - [x] T2 `uft_bootblock_scanner.c` Implementation — DOS-type check
        (OFS/FFS/Intl/DirCache), Amiga checksum (compute+validate),
        root-block extract, virus-DB matching, m68k-code heuristic,
        all-zeros detection. 15 Tests grün. Header + .c + tests wie in
        T2-Plan, ~180 LOC Implementation.
  - [~] T3 `uft_amigados_extended.c` aus Stub-Zustand heben —
        `uft_amiga_validate_ext` macht jetzt echte Validation
        (Bootblock via T2, Root-Block + Bitmap-Block-Checksummen,
        Unknown-DOS-Type-Flag, **Directory-Walker mit Hash-Table-
        Traversierung + Cycle-Detection + File-Header-Checksum-
        Validation**, **+ File-Data-Block-Chain + T_LIST-Extension-
        Block-Walk + BAM-Reconciliation**). Stubs `_repair_bitmap`
        + `_salvage` geben ehrliches -1 zurück (nicht faken). 7
        Tests grün, +303 LOC (T3+T7-partial-2 zusammen).
        Offen für T7-full: OFS-Data-Block-Header-Validation
        (T_DATA=8 + eigene checksum), salvage-API für orphan-Blocks
        (deleted-file-recovery), bm_extension für HD-Disks mit > 25
        BAM-Blöcken.
  - [x] T5 BAMCOPY-Modus: `uft_adf_bam` API für BAM-aware Reads.
        Standalone Reader (root→bitmap-pages→per-block-bit) mit
        safe-default (used-on-doubt). 9 Tests grün. ADF-Plugin-
        Integration (Fast-Imaging-Mode) folgt als M3-Item da sie
        HAL-Pfad berührt.
  - [x] T6 CHECKDISK Pre-Capture-Scan: `uft_disk_quickscan` API —
        Data-Model + Bitstream-Analyse (rolling-shift sync search +
        CRC16/IBM-3740), Heatmap-Klassifikation GOOD/DEGRADED/
        UNREADABLE/BLANK. 12 Tests grün. HAL-Sweep-Integration erfolgt
        in M3 (per-Controller-spezifisch).
  - [x] T7 DiskSalv-Konzept (Scaffold): 3 Strategien-API in
        `uft_salvage_fs`. REPAIR_IN_PLACE → UFT_ERR_UNSUPPORTED
        (Prinzip 1). RECOVER_BY_COPY → bit-exakte Kopie + loss.json.
        FILE_BY_FILE → header-candidate-walk + chain-validation
        (files_extracted bleibt 0 = honest scaffold). 7 Tests grün.
  - [x] T8 XCopy-Panel Legacy-Cleanup: `src/gui/uft_xcopy_panel.cpp`
        — Start-Button `setEnabled(false)` + Tooltip (analog MF-012),
        source==destination safety-check in `startCopy()` für
        späteren Backend-Merge, Status-Label ehrlich gemacht.
- [ ] **Atari-Block** (`A8RAWCONV_INTEGRATION_TODO.md`):
  - [x] TA1 `uft_write_precomp.c` (portiert aus `compensation.cpp`,
        Mac-800K Peak-Shift-Compensation, 13 Tests grün)
  - [x] TA2 `uft_interleave.c` (Sektor-Interleave-Calculator, 11 Tests grün)
  - [x] TA3 ATX-Plugin Rewrite (176 → 327 LOC, ~400 geplant — DONE)
        - BUG-FIX: Chunk-Type war als uint16 gelesen, jetzt korrekt
          uint8+uint8+uint16 — ohne diesen Fix hatte ATX nie
          Sektordaten geliefert (alle Tracks leer)
        - Hinzugefügt: WeakBits-Chunk (0x10), ExtSectorHeader-Chunk (0x11),
          Long-Sector-Handling via ext_size_bits
        - FDC-Status-Decoding: CRC-Error, Lost-Data, Missing-Data, Deleted
        - **weak_mask Byte-Mask vollständig populiert** (Welle-TA3 Closeout):
          calloc-Allocation, [0..weak_offset-1]=0 (solid),
          [weak_offset..end]=0xFF (weak). Speicher freigegeben in
          uft_track_free + Bug-Fix in uft_sector_cleanup (war Leak-Quelle).
- [ ] Tag v4.2.0 nach M2-Abschluss

Abschluss-Kriterium: XCopy-Tab wieder `setEnabled(true)`, ADF und ATX
haben voll-populierte Feature-Matrizen (Prinzip 7 §7.2).

### M3 — HAL-Stabilisierung + Release-Pipeline (4-6 Wochen)

**Ziel:** Die Hardware-Integrationen die heute als „stubbed" markiert
sind auf Production-Niveau.

Detail-Fahrplan: `docs/M3_HAL_PLAN.md` (M3.1 bis M3.7).

> **Hardware-Verfügbarkeit (2026-07-03, verbindlich):** Dem Projekt steht
> **kein physischer Floppy-Controller** zur Verfügung (kein Greaseweazle-
> Zweitgerät zum Gegentest, kein SCP/XUM1541/Applesauce/KryoFlux/FC5025/
> ADFCopy/UFI). Alle Tier-3-Bench-Verifikationen sind daher **nicht
> in-house durchführbar** und an die Community delegiert (jemand *mit*
> dem jeweiligen Gerät fährt das HIL-Protokoll und meldet Ergebnisse
> zurück). Das ist keine Terminfrage („next session"), sondern eine
> harte Ressourcengrenze. Der Wiring-Code bleibt bis zu einer solchen
> Fremd-Bench im Status **written-but-unbenched** — forensisch ehrlich,
> nie als „getestet" behauptet. Die 9 firmware-realistischen Emulatoren
> (`tests/emulators/`) maximieren, was ohne Gerät verifizierbar ist; sie
> ersetzen die Bench nicht, sie verkleinern ihren Suchraum. Bench-
> Protokoll für Fremd-Tester: `tests/HARDWARE_TRUTH_TESTS.md` +
> `tests/hil/run_hil.py`.

Muss:
- [x] M3.1 SCP-Direct HAL: Scaffold (Commit b52c491, 25-ns-Unit-Fix,
      10 Tests grün) + **libusb-Wiring LANDED (MF-254)**. Verbleibend:
      Tier-3 HW-Bench am realen Gerät (UFT-008) — **kein projekt-eigenes
      SCP-Gerät, Bench an Community delegiert**. Bis eine Fremd-Bench
      grün meldet gilt der Pfad als written-but-unbenched (nicht als
      offene In-house-Aufgabe geführt).
- [~] M3.2 XUM1541 HAL real statt stubbed — Commit 10f170f scaffold:
      `src/hal/uft_xum1541.c` (+359 LOC) mit 13/26 echten Funktionen
      (drive_name, tracks_for_drive, sectors_for_track 4-zone für 1541
      und IEEE-488 + fixed 40 für 1581, lifecycle + Setter mit Input-
      Validation, get_error). 13/26 honest USB-Stubs return -1 +
      not-implemented Error-String. 16 Tests grün. Header-Banner
      PLANNED → PARTIAL. Skeleton-Audit drops: 135 → 134, phantom
      decls 2659 → 2633. Multi-Session-libusb-Wiring offen.
- [~] M3.3 Applesauce HAL real statt stubbed — Commit df9c96e scaffold:
      `src/hal/uft_applesauce.c` (+257 LOC) mit 13/20 echten Funktionen
      (format_name 11 Formate, ticks_to_ns / ns_to_ticks / get_sample_clock
      pure-math 8 MHz / 125 ns, lifecycle + Setter mit Input-Validation
      inkl. revolutions 1..5, get_error). 7/20 honest Serial-Stubs.
      17 Tests grün. Header-Banner PLANNED → PARTIAL. Skeleton-Audit
      drops: 134 → 133, phantom-decls 2633 → 2613. Multi-Session-Serial-
      Wiring (115200 8N1 Text-Protokoll) offen.
- [ ] M3.4 UFI/Cowork-Backend (eigenes STM32H723-Firmware-Repo)
- [~] M3.5 Emulator-CI-Pipeline (§6.1 aus `KNOWN_ISSUES.md`) — Scaffold
  in `.github/workflows/emulator.yml` (Commit 792713e): VICE x64 +
  c1541 installiert, deterministische D64-Fixture via Inline-Python,
  Round-Trip-Assertion auf Disk-Header. ADF/WinUAE als Placeholder-Job
  dokumentiert. Voll-Closure (ADF/D64 decode-Regression auto-detected)
  braucht UFT-CLI-Tool das flux/sector → canonical D64/ADF kann (multi-
  session, neuer Subsystem-Drop). Bis dahin testet die Pipeline VICE-
  Substrate, nicht UFT-Output.
- [~] M3.6 CLAUDE.md HAL-Status aktualisiert — partial in Commit 9b1746d:
      alle drei previously-"stubbed" Einträge auf [~] M3.x partial scaffold
      promoted, plus side-fix der falschen "(25MHz)"-Angabe für SCP
      (richtig: 40 MHz / 25 ns / 12 Mbps USB FT240-X). Wording "stubbed"
      ist jetzt nirgends mehr in CLAUDE.md. Voll-Closure ("kein partial-
      scaffold mehr") wartet auf M3.1/M3.2/M3.3 libusb-/Serial-Wiring.
- [ ] M3.7 Tag v4.3.0 nach M3

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

## MF-009-Census — TODO/FIXME-Marker (2026-04-25)

Live audit (Stand 2026-04-25):

```
Total markers (TODO|FIXME|XXX|HACK):  214
  - third-party (switch/hactool, mbedtls):  138 — out of scope
  - todo_tracker.h (UFT-internal tracker):   28 — meta, not action items
  -> UFT-native:                              48 (76 raw incl. false positives)

Real comment-style TODO/FIXME (excl. third-party):  38
Davon:
  -  5  M3.1 SCP-Direct Stubs (uft_scp_direct.c) — getrackt im M3 Plan
  -  3  M2 T7 DiskSalv Scaffold (uft_salvage_fs) — getrackt im M2 Plan
  -  0  fluxengine duplicate code — komplett gelöscht in MF-011 Welle-3/4/5
  -  4  samdisk imported code — out of scope (third-party-like)
  -  1  lexy_experimental (parser PoC) — out of scope
  - 25  Real UFT action items, kategorisiert:
        * Format completion: ATX, XDF, ADF, IMD, SCP, JSON-serialize (12)
        * GUI features ohne Backend: explorertab, workflowtab, switch_panel,
          filesystem_browser, flux_histogram_widget (7)
          [UftMainController + Application + UftMainWindow trio entfernt
           als MF-113 — 2028 LOC dead architecture experiment, nie in
           qmake/CMake gewired, durch FluxCaptureJob/MF-110 obsolet.]
        * Decode/Recovery refinement: otdr_event_calibration, multiread_voting,
          rpm_sync_variance (3)
        * Format-convert pipelines: SCP→D64, GCR→D64, TD0→IMD (3)
```

**Master-Plan-Lesart:**

- Format-completion-TODOs gehören zu M2 (per-Format-Roadmap).
- GUI-Phantom-TODOs sind Regel-2-Verstöße: müssen entweder
  `setEnabled(false) + tooltip` bekommen oder echtes Backend in M2/M3.
- `fluxengine/lib/fluxsink/*.cc` waren byte-identisch zu
  `algorithms/fluxio/*.cc` und in keinem Build → in MF-011 Welle-3
  gelöscht (7 Files, ~1500 LOC).
- Der gesamte `src/fluxengine/`-Tree (123 Files, 20 331 LOC) wurde in
  MF-011 Welle-4 gelöscht — kein Build, keine externen Konsumenten,
  reiner fluxengine-Projekt-Import-Drop.
- Die fünf Sister-Trees (`src/algorithms/fluxio/`, `src/filesystems/`,
  `src/algorithms/{core,data,imageio}/`) wurden in MF-011 Welle-5 gelöscht
  (98 Files, ~16k LOC). Alle 0 qmake/CMake-Refs, nur Cross-Refs untereinander.
- MF-011 Welle-6: `src/encoding/` ganzes Verzeichnis gelöscht (9 Files), plus
  5 tote Header in `src/algorithms/encoding/` und 3 in `src/formats/{amiga_ext,
  apple,ibm}/`. Insgesamt 17 Files. `src/algorithms/encoding/uft_otdr_encoding_boost.c`
  blieb erhalten (live, qmake).
- Welle 7+ Kandidaten: `src/formats/apple/data_gcr.h`, `applesauce.h`
  (vermutlich orphan, Verifikation nötig). Plus Per-Subsystem-Audit der
  verbleibenden 148 Skeleton-Header (M2/M3-Arbeit).
- M3.1/M3.2-getrackte Stubs zählen nicht als offene TODOs — sie sind
  honest Scaffolding mit dokumentiertem Pfad zur Implementation.

**Was bleibt offen für MF-009 als eigenes Item:** ~26 echte UFT-Action-
Items, die nicht schon in M2/M3 gerouted sind. Praktisch alle landen
beim Bearbeiten ihres Format/Subsystems automatisch — keine separate
"TODO-Sweep"-Welle nötig. MF-009 wird als **scoped** (kein eigenes
Backlog mehr) geschlossen sobald die Format-/Subsystem-Items in M2/M3
abgearbeitet sind.

Reproduzierbar: `rg -n "(//|/\*|\*).*\b(TODO|FIXME)\b" src/ include/ |
grep -vE "switch.hactool|todo_tracker"`

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

## v4.1.5-hardening Audit (2026-05-24)

Vollaudit via `orchestrator`-Fan-Out auf Branch `tests/v4.1.5-hardening`.
Findings konsolidiert nach P0-P3 (Konflikt-Hierarchie: Forensik > Security >
Correctness > Architecture > Performance).

### In dieser Session geschlossen

| ID | Severity | Thema | Commit/Stelle |
|---|---|---|---|
| UFT-002 | P0 | CMakeLists.txt header-comment "Version: 4.1.3" stale gg. VERSION.txt=4.1.4 | CMakeLists.txt:3 — Version-Zahl entfernt, Verweis auf VERSION.txt-SSOT |
| UFT-006 | P1 | `.claude/CLAUDE.md` listete 6 Controller, Code hat 9 V2-Provider | .claude/CLAUDE.md §Wichtige Konstanten — auf 9 Provider + Status (1 production / 8 honest-stub) aktualisiert |
| UFT-T01 | P1 | `<threads.h>` Include ohne `__has_include`-Guard → MinGW-Build-Bruch | include/uft/core/uft_safe_io.h:41 — Guard ergänzt |
| UFT-T02 | P1 | 4 Tests (loss_report, roundtrip_matrix, preflight, salvage_fs) linkten Phantom-Symbole obwohl Sources existieren | tests/CMakeLists.txt — per-Test target_sources-Blöcke wie test_destructive_op_consent |
| UFT-T03 | P2 | test_smoke (uft_merge_engine.c+uft_decode_score.c gelöscht in MF-011) und test_safe_io_alloc (uft_safe_io.c+uft_safe_alloc.c existierten nie) | tests/CMakeLists.txt EXCLUDED_TESTS — ehrlich markiert |

**Test-Pass-Rate:** 26 % → 74 % (47 → 133 von 180 Tests passing).
Konsistenz-Check 0/0/0/0, verify_build_sources keine neuen Regressions
(5 baseline-entries resolved, optional `--rebuild-baseline` möglich).

### v4.1.5-Backlog (offen)

Findings für die folgenden Sessions / Tag-Gates:

| ID | Severity | Thema | Effort | Quelle |
|---|---|---|---|---|
| UFT-001 | ✓ **9/9 LIVE** MF-249..MF-258 | Alle 9 V2-Provider haben jetzt einen live Code-Pfad zu echter Hardware. 1 production (Greaseweazle), 8 Beta (live code, hardware-bench pending). Reality-Tracker in §UFT-001-Status. | — | siehe Status-Tabelle |
| UFT-003 | ✓ CLOSED MF-247 | HardwareTab honest-stub-Connection visuell distinkt: orange "Disconnect (Preview)" Button mit Tooltip statt grünem Default. Prinzip-4-Verstoß behoben. | — | src/hardwaretab.cpp:818-845 |
| UFT-004 | ✓ CLOSED MF-260 | `uft_format_plugin_t` bekam `api_version`-Field + `UFT_PLUGIN_API_VERSION` Macro + Runtime-Gate (`uft_register_format_plugin` lehnt `api_version > host` ab, warnt bei `== 0` als Legacy). Static_assert pinnt `sizeof == 224` (MinGW-w64 x86_64; war 216, nachgezogen in **MF-404**, als `read_half_track` direkt vor `api_version` eingefügt wurde — reine Anfügung, kein Reorder, alle Plugins nutzen Designated Initializer). Die Prozedur im Header verlangt ausdrücklich, den Wert *auch hier* zu bestätigen; MF-404 hatte das versäumt, nachgeholt in MF-409. Test `tests/test_plugin_abi.c` mit 8 Assertions: api_version-Macro, struct-size-floor, field-offset-bound, registrar-reject-null/unnamed/future, accept-current/legacy. | — | include/uft/uft_format_plugin.h:516-580, src/core/uft_format_plugin.c:30-65, tests/test_plugin_abi.c |
| UFT-005 | ✓ CLOSED MF-260 | `test_transitions_ns_contract` extended via `transitions_ns_kryoflux_contract_probe()` + `transitions_ns_fluxengine_contract_probe()` in FFI. Beide injizieren einen "binary not found" Runner und assertieren dass beide Provider mit honest non-Captured outcome antworten (kein fabriziertes FluxCaptured mit Container-Bytes). ARCH-2-Regression-Shield aktiv. | — | tests/unit/transitions_ns_ffi.cpp, tests/unit/test_transitions_ns_contract.c |
| UFT-007 | ✓ CLOSED MF-212 | ARCH-7 sub-B Status verifiziert: VID/PID jetzt SSOT in `uft_scp_direct.h:40-41` (`0x16D0:0x0F8C`), `hardwaretab.cpp:548` liest exakt das Macro. Orchestrator-Finding war stale. | — | include/uft/hal/uft_scp_direct.h:40-41 |
| UFT-008 | P1 · **COMMUNITY-DELEGATED** | HIL Hardware-Tier 14/15 NOT_RUN. Pro Controller eine Bench-Session nötig — **kein projekt-eigenes Gerät, nicht in-house durchführbar** (siehe M3-Banner). Bench an Fremd-Tester mit Hardware delegiert; Protokoll steht bereit. Nicht als offene In-house-Aufgabe geführt. | S pro Controller (1-2h Bench-Time, extern) | tests/hil/run_hil.py, tests/HARDWARE_TRUTH_TESTS.md, audit/rc1_field_notes.md |
| UFT-T04 | ✓ REDUCED MF-260 | Bulk-Triage Schritt 1: 4 stale Exclusions re-enabled (test_scp_direct_hal nach MF-254 libusb-Wiring, test_applesauce_hal Pure-Utility, test_fnmatch_shim, test_whdload_resload) + new test_plugin_abi. 146 → 151 tests passing. **Stand 2026-08-18 (MF-409): noch 2 Exclusions**, nicht 38 — `test_mfm_detect`, `test_cpm_fs`, `test_mfm_bridge` (alle drei dieselbe Header-Twin-Entwirrung), `test_libdsk_formats`, `test_fat_extensions`. Suite steht bei 193 laufenden Tests (nicht 151). | S (5 verbleibend) | tests/CMakeLists.txt:53-110 |
| UFT-T05 | ✓ CLOSED v4.1.5 pre-tag | Datei `src/analysis/events/CMakeLists.txt:13` nutzt bereits `CMAKE_CURRENT_SOURCE_DIR` für Include-Pfad — `add_subdirectory()`-sicher. Subdir noch nicht ins Root-CMake verkabelt (separate Entscheidung, Out-of-Scope für T05). | — | src/analysis/events/CMakeLists.txt:13 |

### Was diese Session NICHT geprüft hat

- Byte-genaue Decoder-Korrektheit auf echten SCP/HFE/KryoFlux-Samples
- Memory-Safety mit ASan/UBSan/TSan-Runs (CI-Workflows existieren)
- Vollständige differential-Suite gegen Greaseweazle CLI v1.23
- audit_plugin_compliance.py-Lauf (80 Plugins × Prinzip 7 Status-Felder)
- Plugin-für-Plugin Format-Korrektheit
- Performance-Hotpath-Review (algorithm-hotpath-optimizer)
- Forensic-integrity-Agent gegen jeden Konversions-Pfad einzeln

### UFT-001-Status — 9/9 Controller mit live Code-Pfad (2026-05-24)

Closed via MF-249..MF-258 in der v4.1.5-hardening-Session. Vor diesen
Commits hatten 8 von 9 V2-Provider `nullptr`-Runner — jeder Aufruf
gab ProviderError zurück. Jetzt:

| # | Controller | Transport | Status | MF |
|---|---|---|---|---|
| 1 | Greaseweazle | C-HAL (libusb-frei, custom USB) | ✅ Production | pre-session |
| 2 | Applesauce  | Qt6::SerialPort + 7 Runner-Factories | ✅ Beta | MF-249, MF-250 |
| 3 | ADFCopy     | Qt6::SerialPort + 5 Runner-Factories | ✅ Beta | MF-252 |
| 4 | SCP-Direct  | libusb-1.0 (open/close/seek)         | ✅ Beta | MF-254 |
| 5 | XUM1541     | libusb-1.0 (open/close/detect/IEC)   | ✅ Beta | MF-255 |
| 6 | KryoFlux    | QProcess→dtc subprocess              | ✅ Beta | MF-256 |
| 7 | FluxEngine  | QProcess→fluxengine subprocess       | ✅ Beta | MF-256 |
| 8 | FC5025      | QProcess→fcimage subprocess          | ✅ Beta | MF-257 |
| 9 | USBFloppy   | UFI C-HAL (SG_IO Linux, SCSI-PT Win/macOS pending) | ✅ Beta | MF-258 |

"Beta" = Code-Pfad live + compile-verified, hardware-bench
verification pending. Yellow "Disconnect (Beta)" Styling im
HardwareTab unterscheidet visuell von production green-stripe.

Drei Test-Tiers für jeden Controller:
- **Tier 1** (in CI): Protokoll-/API-Wiring via scripted mocks
- **Tier 2** (optional): Virtual COM-Pair / Subprocess-Simulator
- **Tier 3** (Bench-Session): physische Hardware

Aktuell: Tier 1 grün für alle. Tier 2 dokumentiert (Applesauce
Simulator als Vorlage) und seit 2026-07 auf 9/9 firmware-realistische
Emulatoren ausgebaut (`tests/emulators/`, 439 Assertions). Tier 3 =
**community-delegiert** — kein projekt-eigenes Gerät, kann in-house
nicht gefahren werden (siehe M3-Banner). Ein Fremd-Tester mit dem
jeweiligen Controller führt Tier 3 gegen das HIL-Protokoll aus und
meldet zurück; erst dann yellow „Beta" → green „Production" pro
Controller.

### Recommended Sequence

1. **Bench-Session (pro Controller) — COMMUNITY-DELEGATED:** Tier-3-
   Verifikation gegen echtes Gerät. **Kein projekt-eigenes Gerät → nicht
   in-house durchführbar**; delegiert an Fremd-Tester mit Hardware.
   Sobald ein Fremd-Bench grün meldet → yellow "Beta" → green
   "Production" per Controller. Bis dahin bleibt der Wiring-Code
   written-but-unbenched (ehrlich, nicht als Bug/offene Aufgabe geführt).
2. **Vor v4.1.5-Tag:** UFT-T04 Bulk-Triage entscheiden (löschen vs. rekonstruieren der ~140k-LOC-Tests)
3. **Vor v4.1.5-Tag:** UFT-T05 events-CMakeLists Pfad-Fix
4. **Bei v4.1.5-Tag:** UFT-001 Ehrlichkeits-Statement im README + RELEASE_NOTES — entweder "Greaseweazle-only" oder Tag verschieben
5. **Multi-Session:** UFT-004, UFT-005, UFT-007 — pure Engineering-Arbeit
6. **Bench-Session (community-delegiert):** UFT-008 — kein projekt-
   eigenes Gerät, Tier-3 an Fremd-Tester mit Hardware ausgelagert

---

## Sprint-2 (post-v4.1.3, geschlossen 2026-04-27)

Audit: `must-fix-hunter` lieferte 8 Findings (MF-101..MF-108), 4 ausgewählt
für ein 6.5-Tage-Sprint. Verlauf:

| Sprint-Item | Beschreibung | Status | Commit |
|---|---|---|---|
| MF-101 | `uft_types.h` schattete `UFT_VERSION_STRING="0.1.0-dev"` über `uft_version.h` "4.1.3" | ✓ CLOSED | `2886916` |
| MF-104 | WOZ-Plugin behauptete `CAP_FLUX`+`CAP_WEAK_BITS` ohne Bridge | ✓ CLOSED | `f87858b` |
| MF-102 | `uft_ipf_air.c` (859 LOC) hatte 0 externe Caller — opaque-handle API gebaut, IPF-Plugin gebrückt | ✓ CLOSED | `bd75221` |
| **MF-106** | Inline-MFM-Decoder im Flux-Converter doppelt + kaputt; `flux_decode_mfm()` existiert kanonisch | **DEFERRED → Sprint-3** | — |

**MF-106 Re-Scope-Entdeckung beim Implementieren:** Der eigentliche Blocker
für SCP→IMG ist nicht der kaputte Inline-MFM, sondern die **SCP-API-Dualität**:

- Stub-Path: `uft_scp_file_t` + `uft_scp_read()` + `uft_scp_get_track_flux()` —
  Honest-Stubs in `src/core/uft_core_stubs.c:234,250` returnen `-1`
- Real-Path: `uft_scp_ctx_t` + `uft_scp_open_memory()` + `uft_scp_read_track()`
  in `src/flux/uft_scp_parser.c` mit anderem Type-Family

Der Converter (`uft_format_convert_flux.c`) nutzt den Stub-Path und scheitert
in Line 198 vor dem MFM-Code. Selbst mit perfektem `flux_decode_mfm()`-
Wireup bleibt SCP→IMG dead. Korrekte Sprint-3-Bündelung: **MF-106-bundle =
SCP-API-Unifizierung + Inline-MFM-Replacement** (geschätzt 5-6 Tage). Macht
3 Conversion-Pfade gleichzeitig grün (SCP→IMG, SCP→ADF, SCP→D64).

**Aufgeschoben mit Begründung:** MF-103, MF-105, MF-107, MF-108 — siehe
must-fix-hunter-Report (2026-04-27). Kandidaten für Sprint-3-Auffüllung
nach MF-106-bundle.

---

## Sprint-3 (offen — angelegt MF-409, 2026-08-18)

Sprint-2 verschob MF-106 „→ Sprint-3", aber **dieser Abschnitt existierte
nicht**. Die Aufgabe lag damit seit April 2026 in einem Meilenstein, den es
nicht gab. Hier ist er, mit dem verifizierten Ist-Stand.

### S3-1 — MF-106-bundle: SCP-API-Unifizierung (P1, ~5-6 Tage)

Der einzige große Posten. UFT hat **zwei SCP-Lesepfade**:

- **Stub-Pfad** — `uft_scp_file_t` + `uft_scp_read()` + `uft_scp_get_track_flux()`
  (`uft_format_parsers.h`), Honest-Stubs in `src/core/uft_core_stubs.c:234,255`,
  liefern unbedingt `-1`
- **Realer Pfad** — `uft_scp_ctx_t` + `uft_scp_open_memory()` +
  `uft_scp_read_track()` in `src/flux/uft_scp_parser.c`; das SCP-Plugin ist
  **T1b-verifiziert** (7 Tests, Cross-Tool-Korpus `gw_amigados.scp`)

Alles, was den Stub-Pfad benutzt, ist tot. Konsumenten, verifiziert 2026-08-18:

| Konsument | Wirkung |
|---|---|
| `uftc_convert_scp_to_d64` | nicht funktional |
| `uftc_convert_scp_to_mfm_sectors` | nicht funktional (Blocker für SCP→IMG) |
| `uftc_convert_scp_to_hfe` | nicht funktional |
| `uftc_convert_scp_to_g64` | nicht funktional |
| `ProtectionAnalysisWidget` (SCP-Zweig) | liefert nie Schutz-Treffer → KNOWN_ISSUES **PROT-11** |

Der GUI-Schutzpfad ist ein **fünfter** Konsument, den die 5-6-Tage-Schätzung
von 2026-04 noch nicht enthielt. `CLAUDE.md` führt die vier Konvertierungen
als Feature.

**Bereits erledigt und aus der Unbekannten-Liste raus (MF-407):** die
C64-Hälfte der Kette ist belegt — `flux_decode_gcr_c64()` liefert mit
`keep_raw_bits` genau den GCR-Bitstrom, den `ufm_c64_metrics_from_gcr()`
konsumiert, inklusive des headerlosen Falls
(`tests/unit/test_flux_gcr_c64_sync.c`). Offen bleibt der Leser-Umbau.

**Korpus-Vorbedingung:** ein C64-Flux-Abbild mit dokumentierter Herkunft fehlt
(vorhanden nur `gw_amigados.scp`, Amiga MFM). `gw convert
--format=commodore.1541` aus dem rechtefreien `vice_c1541_35trk.d64` ergäbe
einen T1b-Eintrag; greaseweazle ist lokal nicht installiert und nicht über
PyPI beziehbar. Ohne dieses Abbild bleibt auch nach dem Umbau nur eine
synthetische Verifikation.

### S3-2 — Prinzip-6-Lücke: Kompatibilitätsmatrix 1/88 (P2)

`audit_plugin_compliance.py` (2026-08-18): 88/88 Plugins haben `spec_status`
und Feature-Matrix, aber nur **1 von 88** hat eine Emulator-Kompatibilitäts-
matrix. Prinzip 6 verlangt sie; `compat_entries == NULL` heißt „nichts
explizit getestet". Das ist die größte verbliebene Prinzip-Lücke und in
keinem Meilenstein geführt.

### S3-3 — Verbleibende Test-Exclusions (P2, klein) → ✓ ERLEDIGT (MF-411)

5 statt der im v4.1.5-Backlog genannten 38. Drei davon
(`test_mfm_detect`, `test_cpm_fs`, `test_mfm_bridge`) waren **dieselbe**
Aufgabe — und der notierte Grund war falsch.

**Behoben (MF-411).** Die Ausschluss-Kommentare sagten, die Tests erwarteten
Typen, die es „in KEINEM Header" gebe (`uft_mfm_detect_info_t`, `UFT_MFMD_*`,
`mfm_error_t`, `MFM_OK`, `fat_bpb_t`, `cpm_dpb_t.bsh`). Es gibt sie alle — in
`include/uft/detect/`. Die Tests inkludierten **Phantom-Zwillinge** unter
`include/uft/analysis/`: gleichnamige Header mit 34–56 Zeilen gegenüber
186–756 Zeilen der echten, die eine Handvoll Typen und eine Funktion
(`mfm_detect_encoding()`) deklarierten, für die es **keine Implementierung**
gibt. Die CMake-Blöcke zeigten längst auf `detect/`; nur die `#include`-Zeilen
griffen über den Basis-Include-Pfad weiter die Phantome.

Warum das durch `audit_skeleton_headers.py` fiel: dessen Schwelle ist
„≥ 10 Deklarationen". Diese drei Header liegen darunter — kleine Phantome sind
unsichtbar für den Skelett-Audit.

Umgesetzt: Includes auf `uft/detect/` umgestellt, die drei Phantom-Header
gelöscht (sie hatten repo-weit keinen anderen Konsumenten), fehlende
Abhängigkeiten (`mfm_detect.c`, `cpm_fs.c`) in den Bridge-Test aufgenommen.
Suite **193 → 196**, alle grün. Verbleibende Exclusions: **2**
(`test_libdsk_formats`, `test_fat_extensions`), beide mit benanntem
fehlendem Baustein.

**Beifund, verfolgt und weitgehend behoben.** Die beiden
`uft_mfm_detect_bridge.h` teilten denselben Include-Guard — wer zuerst
inkludiert wurde, gewann, der andere wurde still übersprungen. Die Frage „gibt
es weitere?" wurde nachgemessen: **23 weitere gleichnamige Header-Paare** mit
identischem Guard, teils dreifach (`uft_woz.h`, `uft_fat12.h`, `uft_bbc_dfs.h`).

Vor dem Eingriff gemessen, ob akut oder latent: keine einzige Datei bindet zwei
Varianten **direkt** ein, und kein Code fragt einen dieser Guards ab. Die
Umbenennung auf pfadabgeleitete Guards (25 Header) war damit risikofrei und
rein vorbeugend — aus stillem Falschverhalten wird im Kollisionsfall ein
sichtbarer Compilerfehler.

Ein Fall blieb bewusst offen: `uft_platform.h`. Dort brachte das Auftrennen
sofort 9 doppelt definierte Makros zutage, weil der große Header den compat-
Header einbindet, ihn durch den geteilten Guard aber nie wirksam werden ließ.
Das ist eine eigene Entflechtung → **KNOWN_ISSUES ARCH-1**; der Guard bleibt
vorerst geteilt und ist als einzige Ausnahme im Wächter eingetragen.

**Wächter ergänzt:** `check_consistency.py` hat eine neue Kategorie
„include guard collisions". Gegenprobe gemacht — mit einem wiederhergestellten
`UFT_WOZ_H` meldet sie den Fall namentlich, sonst 0.

### S3-4 — Doku-Governance (P2) → ✓ ERLEDIGT (MF-410)

`GOVERNED_DOCS` in `scripts/update_inventory.py` umfasst nur `README.md`,
`CLAUDE.md`, `.claude/CLAUDE.md`. **`MASTER_PLAN.md` und `KNOWN_ISSUES.md`
prüft nichts** — genau deshalb konnte dieser Plan drei erledigte P0/P1 als
offen führen, eine Struct-Größe von 216 nennen (real 224 seit MF-404) und
38 Test-Exclusions statt 5. Die Sync-Arbeit in MF-409 behebt die Symptome,
nicht die Ursache.

**Behoben (MF-410).** `docs/MASTER_PLAN.md` steht jetzt in `GOVERNED_DOCS`, und
`update_inventory.py` hat einen neuen Mechanismus `DERIVED_CLAIMS`: pro Eintrag
ein Satz im Dokument, eine maschinenlesbare Quelle, Abgleich bei jedem Commit.

| geprüfter Satz | Quelle der Wahrheit |
|---|---|
| „Static_assert pinnt `sizeof == N`" | `_Static_assert` in `uft_format_plugin.h` |
| „noch N Exclusions" | aktive Einträge in `EXCLUDED_TESTS`, `tests/CMakeLists.txt` |

Gegenprobe gemacht: mit den alten Werten (216 bzw. 38) meldet
`check_consistency.py` beide Abweichungen namentlich und schlägt fehl; mit den
korrekten Werten läuft es durch.

Bewusst **nicht** getan: `docs/KNOWN_ISSUES.md` in `GOVERNED_DOCS` aufnehmen.
Es führt Historie (etwa die Pre-Tag-Bestehensquote von v4.1.5, die damals
korrekt notiert wurde und heute genau deshalb wie eine veraltete Zahl aussieht),
und ein pauschaler
Stale-Pattern-Lauf würde korrekte historische Einträge als Fehler melden.
`DERIVED_CLAIMS` prüft deshalb auch keine Zahlen im Allgemeinen, sondern je
genau einen Satz — eine historische Zeile daneben bleibt unberührt. Ein Eintrag,
dessen Satz verschwindet, meldet das ebenfalls, damit die Prüfung nicht still
wirkungslos wird.

### Aufgeschoben aus Sprint-2, unverändert

MF-103, MF-105, MF-107, MF-108 — Kandidaten für die Auffüllung nach S3-1.

---

## Verwandte Dokumente (Doku-Index, konsolidiert MF-289)

**Lebend (bei jeder Änderung aktuell halten):**
- `docs/KNOWN_ISSUES.md` — laufender Principle-Compliance-Tracker
  + Audit-Closing-Notes
- `docs/STUB_ELIMINATION_PLAN.md` — 6-Phasen-Plan für Skeleton/Stub-Abbau
  (löst das frühere Nebeneinander von PLANNED_APIS-Zählung,
  Skeleton-Audit und Stub-Guide als Steuerungs-Dokument ab)
- `docs/M3_HAL_PLAN.md` — Detail-Fahrplan HAL-Wiring M3.1–M3.7
- `docs/DESIGN_PRINCIPLES.md` — die 7 Kernprinzipien (unverändert gültig)
- `.claude/CONSULT_PROTOCOL.md` — Agenten-Zusammenarbeit

**Backlog-Detail (Stand eingefroren, Items noch offen):**
- `docs/XCOPY_INTEGRATION_TODO.md` — M2 Amiga-Block (T1-T8, Status oben in §M2)
- `docs/A8RAWCONV_INTEGRATION_TODO.md` — M2 Atari-Block (TA1-TA3)

**Auto-generiert (nicht von Hand editieren):**
- `docs/SKELETON_HEADERS_AUDIT.md` — via `scripts/audit_skeleton_headers.py`
- `docs/PLANNED_APIS.md` — M1-Wellen-Snapshot; Live-Zahlen via Audit-Script

**Historisch (abgeschlossen, nur Referenz):**
- `docs/REFACTOR_BRIEF.md` + `docs/REFACTOR_TASKS.md` — Type-Driven-HAL-
  Refactor, gemerged MF-176
- `docs/XCOPY_ALGORITHM_MIGRATION.md` — externes Review, in M2-Items überführt

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
