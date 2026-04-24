# XCopy / DiskSalv / XVS Integration — TODO-Liste

**Stand:** 2026-04-23
**Kontext:** Analyse des Amiga-Utility-Ordners `C:\Users\Axel\Github\xcopy`
(XCopy Pro, DiskSalv 4, FixDisk, xvs.library). UFT hat bereits umfangreiche
Header-Spezifikationen für diese Features, aber **keine einzige Zeile
Backend-Implementation** jenseits von selbst-deklarierten „Stub"-
Dateien.

Dies ist ein **Prinzip-1-Problem**: GUI und API lassen Features real
wirken, die in der Ausführung nichts tun. Jeder Eintrag unten entweder
implementieren oder durch `is_stub = true` + `KNOWN_ISSUES`-Eintrag
ehrlich markieren.

---

## Quellmaterial

| Quelle | LOC | Was drin ist |
|---|---|---|
| `xcopy_src/xcop.s` | 4 943 Z. 68k-Asm | DOSCOPY, BAMCOPY, NIBBLE modes; VirusCheck; Track-Erase; MFM decode/encode; Sync-Finding |
| `xcopy_src/xio.s` | 4 476 Z. 68k-Asm | Low-level trackdisk I/O, MFM framing, RLE depack |
| `xvslibrary/xvs/HISTORY` | ~80 KB Changelog | Liste aller Virus-Signaturen seit 1997 (100+), Updates bis 2025 |
| `xvslibrary/xvs/Developer/` | C/ASM/E/SFD | Ready-to-call API (C-Headers + AmigaOS shared library bindings) |
| `DiskSalv/DiskSalv4/DiskSalv.guide` | AmigaGuide | Konzept-Doku: recover-by-copy, bitmap-reconstruction, file-salvage |
| `FixDisk/FixDisk.doc` | Textdoku | Directory-rebuild, block-chain repair, undelete — **Prinzip-1 kritisch: in-place repair** |

---

## Anti-Features (NICHT übernehmen)

- ❌ **Auto-kill bei Virusfund** (XCopy „KILL"-Button) — Prinzip 1: keine
  stille Datenmodifikation. UFT detektiert und meldet; löscht nie
  unaufgefordert.
- ❌ **In-Place Disk-Repair als Default** (FixDisk-Stil) — Prinzip 1.
  UFT macht `recover-by-copy` (DiskSalv-Stil: Ergebnis auf neuen
  Datenträger, Original bleibt bitidentisch).
- ❌ **„OPTIMIZE" aus XCopy** (defragmentiert, re-packt) — ändert
  Original. Forensisch verboten.
- ❌ **„QFORMAT" / „ERASE"** auf Quelldatenträger — destruktiv, nur für
  Target-Disks anbieten und hinter `--allow-destructive`-Flag.

---

## Todo-Liste (priorisiert)

### T1 — [P0] Virus-Signatur-Datenbank als SSOT

**Problem:** `uft_amiga_bootblock.h` deklariert `uft_amiga_virus_sig_t` und
`uft_amiga_get_virus_db()`, aber die DB ist leer. `uft_bootblock_scanner.h`
definiert 17 Kategorien ohne einen einzigen Eintrag.

**Quelle:** `xvslibrary/xvs/HISTORY` listet 100+ Bootblock-Viren
(SCA, LSD, AEK, DAG, Byte Bandit, Lamer Exterminator, ...) mit Versionen
und Fundorten. XCopy's `xcop.s` `VirusCheck`-Routine ruft auf Byte-Match-
Patterns, die aus der `xvs.library` extrahierbar sind (Developer/C + ASM).

**Action:**
- Neue SSOT-Datei `data/amiga_bootblock_viruses.tsv` mit Spalten:
  `id, name, category, pattern_hex, pattern_mask_hex, first_seen, source, disinfect_strategy`
- `disinfect_strategy` ist immer `REPORT_ONLY` für UFT (kein Auto-Kill).
- Generator `scripts/generators/gen_virus_db.py` →
  `src/fs/uft_amiga_virus_db.c` (const array).
- Minimum-Startlauf: 30 häufigste Viren aus xvs.HISTORY importieren.

**Aufwand:** M (2 Tage)

---

### T2 — [P0] `uft_bootblock_scanner.c` Implementation

**Problem:** 433-Zeilen-Header mit Scan-API, keine `.c`-Datei existiert.

**Quelle:** xvs.library Autodocs (`xvs/Developer/Autodocs/`) + XCopy's
`VirusCheck`-Routine in `xcop.s`. Beide lesen den Bootblock (2 × 512 B),
matchen gegen Signature-Patterns, liefern Kategorie + Risiko-Score.

**Action:**
- `src/fs/uft_bootblock_scanner.c` anlegen, Input: 1024-Byte-Buffer
  (Sektor 0+1 einer Amiga-Disk), Output: `uft_bb_scan_result_t`.
- Signature-Matching gegen die DB aus T1.
- Checksum-Validierung (`uft_amiga_calc_bootblock_checksum` existiert
  schon als Header-Decl).
- Test: `tests/test_bootblock_scanner.c` mit 5 echten Bootblock-Beispielen
  (saubere OFS, saubere FFS, SCA-Virus, Byte-Bandit-Virus, custom boot).

**Aufwand:** M (3 Tage, hängt an T1)

---

### T3 — [P1] `uft_amigados_extended.c` aus dem Stub-Zustand heben

**Problem:** Datei ist selbst-dokumentierte „Stub Implementation - pending
API alignment". Funktionen `uft_amiga_validate_ext`, `uft_amiga_repair_bitmap`,
`uft_amiga_salvage`, `uft_amiga_unpack_to_host` geben alle `-1` oder
leeres Resultat zurück.

**Quelle:** DiskSalv.guide Konzepte + FixDisk.doc Algorithmen. Die
AmigaDOS-Strukturen (root-block, header-block, hash-chain) sind im
existierenden `uft_amigados.h` schon definiert.

**Action:**
- `_validate_ext` → walk jedes header-block, prüfe hash-chain + checksums.
- `_repair_bitmap` → rekonstruiere Bitmap aus gelaufenem header-walk.
  Schreibt **NICHT** zurück ins Original; output in neuen Block auf dem
  Ziel-Image (recover-by-copy, DiskSalv-Stil).
- `_salvage` → file-by-file copy aller header-blocks die hash-chain-
  konsistent sind. Prinzip 2 (marginale Daten erfassen, nicht verwerfen).
- Pro Funktion 1 Test mit synthetischem kaputtem ADF-Image als Fixture.

**Aufwand:** L (1 Woche)

---

### T4 — [P1] XCopy-Panel: ehrlich markieren oder verkabeln

**Problem:** `src/gui/uft_xcopy_panel.cpp` ist 428 Zeilen Qt-UI (Mode-
Auswahl DOSCOPY/BAMCOPY/NIBBLE, Error-Handling, Multi-Copy), aber ohne
irgendeine Backend-Anbindung. User sieht Feature, beim Klick passiert
nichts.

**Action (entweder):**

**4A.** **Markieren:** Im Panel `setEnabled(false)` + Hinweistext
  „XCopy-Backend noch nicht implementiert (UFT-Issue #NNN)".
  + Eintrag in `docs/KNOWN_ISSUES.md` unter Prinzip 1.
  + Plugin-Katalog-Integration: falls XCopy-Tab registriert ist, mit
  `is_stub = true` markieren (siehe Prinzip 7 §7.3).

**4B.** **Verkabeln (später wenn T5 fertig):** `startCopy()`-Slot ruft
  echten Backend an, der die Mode-Enum (`DOSCOPY|BAMCOPY|NIBBLE`)
  entsprechend dispatcht.

**Empfehlung: 4A jetzt** (1 Stunde), 4B wenn Backend (T5) steht.

**Aufwand:** 4A: S, 4B: abhängig von T5

---

### T5 — [P1] BAMCOPY-Modus im ADF-Plugin

**Problem:** Bei Batch-Preservation hunderter Disketten ist BAMCOPY
(„nur benutzte Sektoren lesen") 3-4× schneller als DOSCOPY, weil ungenutzte
Sektoren übersprungen werden. UFT kann das aktuell nicht.

**Quelle:** `xcop.s` `bamtest` + `BamCopy` Routinen. AmigaDOS BAM ist
block 1 (root block lemmt bitmap-offsets).

**Action:**
- `src/formats/adf/uft_adf_plugin.c` — neuer optionaler Parameter
  `uft_adf_open_ext(path, UFT_ADF_MODE_BAM_AWARE)`.
- Bei BAM-aware open: Track `N`-Map lesen; `read_track(N)` skippt
  Sektoren die in der Bitmap als frei markiert sind (liefert leeres
  Sector-Struct mit `.is_unused = true`).
- Prinzip-1-Disclaimer: Flux-Level-Reads sind davon nicht betroffen
  (die lesen immer alle Bits). BAM-Skip ist nur für DOS-Level-Semantik.
- Feature-Flag im Plugin-Feature-Matrix: neue Row
  `{ "BAM-aware fast copy", UFT_FEATURE_SUPPORTED, NULL }`.

**Aufwand:** M (3 Tage)

---

### T6 — [P2] CHECKDISK als Pre-Capture-Scan

**Problem:** Vor einer vollen Flux-Capture (mehrere Minuten pro Disk)
wäre ein 30s-Scan nützlich: „wie viele Read-Errors erwarten wir?"
Archivare können so Disks triagieren.

**Quelle:** XCopy's `CHECKDISK` scannt alle 80 Tracks schnell (kein
Decode, nur ob Sync + CRC ok), liefert Error-Histogramm.

**Action:**
- Neue Helper-Funktion `uft_hal_quick_scan(ctx, &result)` —
  track-sweep mit minimalem Timeout pro Track, kein Retry, kein Flux-
  Capture, nur Sync-detect.
- CLI: `uft scan <device> --quick` → grünes/gelbes/rotes Heatmap-JSON.
- Integriert sich in **MF-008**-Dokumentation (advertise als Pre-Capture-
  Check).

**Aufwand:** M (4 Tage, braucht HAL-Erweiterung pro Controller)

---

### T7 — [P2] DiskSalv-Konzept in Recovery-Pipeline

**Problem:** UFT hat `src/recovery/` (Multiread, Adaptive Decode), aber
die Higher-Level-Recovery-Strategie nach kaputter Disk (file-by-file-
salvage mit validen header-chains) existiert nicht.

**Quelle:** DiskSalv.guide beschreibt 3 Strategien:
1. **Repair in place** — Prinzip-1-verboten, skip.
2. **Recover by copy** — bit-exakte Kopie auf neuen Datenträger, Scan
   der Kopie nach salvageablen Einheiten.
3. **File-by-file extract** — pro header-block separat kopieren, korrupte
   chains überspringen, Flag-Reports schreiben.

**Action:**
- `src/recovery/uft_salvage_fs.c` mit 3 Strategien implementieren.
- Jede Strategie produziert einen `.loss.json`-Sidecar (Prinzip 1).
- Output-Konvention: `<source>.salvage/` Verzeichnis mit extrahierten
  Files + `manifest.json` + `corrupted-chains.log`.

**Aufwand:** L (1-2 Wochen)

---

### T8 — [P3] `src/gui/uft_xcopy_panel.cpp`: Legacy-Features entsorgen

**Problem:** Panel enthält UI-Elemente für Features die UFT bewusst
**nicht** übernimmt (Auto-kill bei Virus, In-place erase).

**Action:**
- `createErrorGroup`-Handling: „On virus: kill" Option entfernen oder
  durch „On virus: log + abort" ersetzen.
- ERASE/FORMAT-Buttons deaktivieren wenn Target == Source.
- Doku-Tooltip auf jedem Mode: „Aktionen am Quell-Datenträger:
  read-only" / „Aktionen am Ziel-Datenträger: destruktiv (--allow-
  destructive nötig)".

**Aufwand:** S (1 Tag)

---

## Reihenfolge (falls man T1-T8 der Reihe nach abarbeitet)

1. **T1** (SSOT für Viren-DB) — entblockt T2
2. **T2** (Bootblock-Scanner impl) — entblockt echte Virus-Detection
3. **T4A** (XCopy-Panel ehrlich markieren) — 1h, schließt Prinzip-1-Lücke sofort
4. **T3** (AmigaDOS extended aus Stub heben) — entblockt T7
5. **T5** (BAMCOPY) — bringt Batch-Preservation-Speedup
6. **T6** (CHECKDISK) — Pre-Capture-Triage
7. **T7** (DiskSalv Recovery) — größter Brocken
8. **T4B + T8** (XCopy-Panel verkabeln + aufräumen) — wenn Backend steht

---

## Bezug zum must-fix-hunter-Backlog

- **T4A** schließt einen Prinzip-1-Verstoß der bisher nicht als MF-Eintrag
  erfasst war — gehört nachträglich in `KNOWN_ISSUES.md` unter §1.
- **T3** ist ein konkreter MF-004-Anwendungsfall (Stub-Elimination für
  AmigaDOS-Extended-Modul).
- **T2** würde die Plugin-Feature-Matrix von 5/80 auf 6/80 (§7.2)
  erhöhen.

---

**Gesamt-Aufwand:** ~5-7 Wochen konzentrierte Arbeit für vollständige
Integration. **T1 + T2 + T4A** (~1 Woche) sind der minimale Prinzip-1-
Rettungsanker.
