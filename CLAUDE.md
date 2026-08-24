# UnifiedFloppyTool (UFT) v4.1.0

## Was ist das?

UnifiedFloppyTool ist eine Qt6 C/C++ Desktop-Anwendung für die **forensische Sicherung und Analyse historischer Floppy-Disketten**. Es ist das umfassendste Open-Source-Tool dieser Art — vergleichbar mit einer Kombination aus dd, Wireshark und einem Oszilloskop, aber spezialisiert auf magnetische Speichermedien.

**Zielgruppe:** Archive, Museen, Retrocomputing-Enthusiasten, digitale Forensiker, Kopierschutz-Forscher.

**Philosophie:** "Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten." — jede Information auf der Diskette wird erfasst, auch Timing-Anomalien, Kopierschutz-Signaturen und beschädigte Bereiche.

**Design-Prinzipien (verbindlich):** Siehe [`docs/DESIGN_PRINCIPLES.md`](docs/DESIGN_PRINCIPLES.md).
Bei Konflikt zwischen Prinzip und Code-Änderung gewinnt das Prinzip. Bekannte
Compliance-Lücken: [`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md).

## Kernfunktionen

### 1. Disk-Imaging (Lesen/Schreiben)
Unterstützt 6 Hardware-Controller (HAL teilweise wired — siehe pro Eintrag):
- **Greaseweazle** (72 MHz Flux-Capture, USB) — read+write+flux, production
- **SuperCard Pro** (40 MHz / 25 ns sample, USB FT240-X 12 Mbps) —
  HAL [~] M3.1 libusb wiring LANDED (MF-254); Tier-3 HW-bench pending
  (UFT-008); CLI available
- **KryoFlux** (24 MHz, USB via DTC-Tool) — read via subprocess
- **FC5025** (USB 5.25" Read-Only) — read via fcimage CLI
- **XUM1541/ZoomFloppy** (IEC-Bus für Commodore-Laufwerke) — HAL [~] M3.2
  partial (drive-tables + lifecycle real, USB I/O pending libusb wiring)
- **Applesauce** (Apple-spezialisiert, 8 MHz / 125 ns, Text-Protokoll
  über serielle USB-Verbindung) — HAL [~] M3.3 partial (utility + tick-
  conversion + lifecycle real, serial I/O pending)

(FluxEngine + UFI/USB-Floppy exist as Qt providers but not as HAL backends.
 M3 plan: alle drei stubbed-HALs sind jetzt [~] partial scaffold mit
 echten Pure-Utility-Funktionen + honest USB/Serial-Stubs; libusb-/Serial-
 Wiring multi-session — siehe `docs/MASTER_PLAN.md` §M3.)

### 2. Format-Unterstützung (137 Plugins definiert, 138 format IDs)

> **MF-446/447:** 137 = 88 ausgeschrieben + 49 aus dem `DSK_PLUGIN()`-Makro.
> Seit MF-447 registriert `main()` sie beim Start — vorher war die Registry
> zur Laufzeit leer und `uft_disk_open()` lieferte für jede Datei NULL.

> **EINFRIER-REGEL (MF-363, präzisiert MF-498):** Kein neuer **ungeprüfter**
> Code im Format-/Decoder-Layer. „Geprüft" heißt: benannte Referenz oder
> Rotbeweis-zuerst, jede Zahl im Commit gemessen, Referenz im Header — alle
> drei. Moratorium für neue Format-Plugins bis Label-Skript (T1/T1b/T2/T3)
> läuft und ATR/D64/ADF/FDI/NFD-r0 auf T1/T1b gehoben sind; danach 1:2 (ein
> neues Format = zwei Hebungen). Verbindliche Fassung:
> [`docs/VERIFICATION_PLAN.md` §Einfrier-Regel](docs/VERIFICATION_PLAN.md).
> **Was „unterstützt" hier heißt (MF-509):** von den 88 tier-geführten
> Plugins stehen **57 auf T3 — ungeprüft**: kein Test, oder ein
> synthetischer Test ohne Abgleich gegen eine autoritative Quelle. Genau
> in dieser Lage waren die fünf fabrizierten Parser grün
> (FMT-2/3/10/11/12). Belegt sind T1=2, T1b=12, T2=17. Die Liste unten
> nennt, was **gelesen werden soll**, nicht was **geprüft ist** — pro
> Format: [`docs/VERIFICATION_TIERS.md`](docs/VERIFICATION_TIERS.md).

Liest/schreibt Disk-Images von praktisch jedem 8-Bit- und 16-Bit-Computer:
- **Commodore:** D64, D71, D81, G64, T64, CRT, PRG, P00
- **Apple:** DO, PO, WOZ (v1/v2/2.1), A2R, MOOF, 2MG, NIB, DC42
- **Atari:** ATR, ATX, ST, STX, MSA, DCM, XFD
- **IBM PC:** IMG, IMA, IMD, TD0, DMK, CQM
- **Amstrad/Spectrum:** DSK, EDSK, TRD, SCL, MGT, TAP, TZX
- **BBC/Acorn:** SSD, DSD, ADF, UEF
- **Flux-Formate:** SCP, HFE (v1/v2/v3), KryoFlux RAW
- **Japanisch:** D88, D77, NFD, HDM, XDF, DIM, FDX
- Plus: MSX, Thomson, TI-99, Roland, HP LIF, CP/M, Micropolis, Victor, Zilog, etc.

### 3. Format-Konvertierung (44 Pfade registriert, **15 angeboten**)

> **Ehrlichkeits-Hinweis (MF-526, Zahlen neu gemessen MF-541):** die
> Wandlungstabelle fuehrt **44** Paare. Davon haben **15** einen Eintrag in
> der Rundlauf-Matrix und werden angeboten:
>
> * **4 verlustfrei (je mit Messung)** — D64→D64, ADF→ADF, D64→G64,
>   IMG→HFE. Jedes einzelne mit einer Bit-Identitaets-Messung im Baum
>   (MF-532/533/539), keines auf Zusicherung.
> * **11 nur mit ausdruecklichem `accept_data_loss`.**
>
> **29 weist das Preflight-Tor als UNGEPRUEFT ab** („conversion pair is
> UNTESTED — not offered until an entry exists“), weil sie keinen Eintrag
> in `src/core/uft_roundtrip.c` haben. Das ist Absicht (MF-263/UFT-A01)
> und richtig.
>
> Die Matrix selbst hat **17** Eintraege — zwei davon (`SCP→IMD`,
> `STX→ST`) haben kein Gegenstueck in der Wandlungstabelle: Verdikte ohne
> Konsumenten.
>
> Frueher standen hier drei einander widersprechende Zahlen („8 angeboten“
> in der Ueberschrift, „11“ zwei Zeilen weiter, „33 abgewiesen“). Seit
> MF-541 haengen die Matrix-Zahlen an `scripts/update_inventory.py`
> (DERIVED_CLAIMS) und werden bei jedem Commit gegen
> `src/core/uft_roundtrip.c` geprueft — von Hand gepflegte Zahlen driften,
> das ist in diesem Baum dreimal belegt.
>
> Die frueheren LOSSLESS-Eintraege SCP↔HFE trugen keinen Beweis und sind
> seit MF-527 herabgestuft. `ADF→HFE` stand kurzzeitig als verlustbehaftet
> mit bezifferter Liste und ist seit MF-538 zurueckgenommen; seit MF-539
> lehnt der Wandler ausdruecklich ab, weil dem Baum ein AmigaDOS-Encoder
> fehlt.

Konvertiert zwischen allen gängigen Formaten:
- Sektor↔Sektor (D64↔IMG, IMD↔IMG)
- Sektor→Bitstream (D64→G64, ADF→HFE)
- Flux→Sektor (SCP→D64, SCP→ADF, HFE→IMG)
- Flux→Bitstream (SCP→HFE, SCP→G64)
- Flux→Flux (KryoFlux→SCP)

### 4. DeepRead — Adaptive Signal Recovery
Eigenentwickeltes OTDR-basiertes Analyse-System (inspiriert von Glasfaser-Messtechnik):

**3 Decode-Booster:**
- **Adaptive Decode:** Bei CRC-Fehler → OTDR-Analyse → LOW-Confidence-Regionen → aggressiver PLL-Re-Decode (±33%) → Fusion gewichtet nach Qualitätsprofil
- **Weighted Voting:** Float-gewichtete Multi-Revolution-Fusion statt einfacher Majority-Vote
- **Encoding Boost:** OTDR-Histogramm-Analyse verbessert Format-/Encoding-Erkennung

**5 Forensik-Module:**
- **Write-Splice Detection:** Erkennt Schreibkopf-Ein/Aus-Übergänge
- **Magnetic Aging Profile:** Unterscheidet Alterung von physischem Schaden
- **Cross-Track Correlation:** Identifiziert radiale vs. magnetische Schäden
- **Revolution Fingerprint:** Einzigartiger Jitter-Fingerabdruck pro Diskette
- **Soft-Decision LLR:** Log-Likelihood-Ratios für Viterbi Soft-Input

### 5. Kopierschutz-Analyse

> **Ehrlichkeits-Hinweis (MF-508):** Automatisch laeuft die Erkennung von
> Schutz-SIGNALEN (Fuzzy Bits, lange/kurze Spuren, No-Flux-Bereiche,
> Overlap, Desync, Weak Bits, Illegal GCR) plus drei heuristisch
> benannten Schemata. Der Katalog der 55+ BENANNTEN Verfahren unten liegt
> in `src/protection/` und hat **keinen Aufrufer** — siehe
> `docs/OPEN_ITEMS.md` P0-2. Die Liste ist Bestand, nicht Fähigkeit.

Im Katalog dokumentierte historische Kopierschutz-Verfahren:
- V-MAX!, RapidLok, CopyLock, Speedlock, ProLok, Vorpal
- Dungeon Master Fuzzy Bits, FatBits, Pirate Slayer
- Lange Tracks, Halb-Tracks, Custom Sync, Density Mismatch
- Amiga: Rob Northen, CAPS/SPS-kompatibel
- Atari ST: CopyLock, Macrodos, dec0de

### 6. Forensischer Report & Audit Trail

> **Ehrlichkeits-Hinweis (MF-366):** Die frühere Audit-Trail-/Forensic-
> Report-C-API (`uft_audit_trail.h`, `uft_forensic_report.h`) war
> Phantom-API (100 % unimplementiert, keine Aufrufer) und wurde entfernt.
> Real existieren GUI-Panels + Provenance (`src/forensic/uft_provenance.c`);
> der volle Audit-Trail unten ist **Zielbild**, nicht Ist-Stand — siehe
> [`docs/SUBSYSTEM_MATURITY.md`](docs/SUBSYSTEM_MATURITY.md).
- Hash-Verifizierung (MD5, SHA1, SHA256, SHA512 parallel)
- Hash-Chain für Integritätsnachweis
- Vollständiger Audit Trail (40+ Event-Typen, Timestamps, CHS-Kontext)
- Export: JSON, HTML, PDF, Markdown, XML, Plain Text
- Risiko-Scoring (0-100) mit Recovery-Empfehlung

## Architektur

```
┌─────────────────────────────────────────────────────────┐
│                    Qt6 GUI (C++)                         │
│  UftMainWindow, UftOtdrPanel (DeepRead), Sector Editor  │
│  ADF Browser, Hex Panel, Heatmap, Histogram             │
├─────────────────────────────────────────────────────────┤
│               Hardware Abstraction Layer (C)             │
│  Greaseweazle │ SCP │ KryoFlux │ FC5025 │ XUM1541 │ AS  │
├─────────────────────────────────────────────────────────┤
│                   Core Engine (C)                        │
│  Format Parsers (80 plugins, 138 IDs) │ PLL Decoder │ MFM/FM/GCR Codec │
│  Flux Decoder │ Sector Extractor │ CRC Engine            │
├─────────────────────────────────────────────────────────┤
│              Analysis Pipeline (C)                       │
│  OTDR (12 Module) │ TDFC │ φ-OTDR Denoise │ Confidence  │
│  DeepRead (8 Module) │ Protection (Signale; Katalog unwired) │
├─────────────────────────────────────────────────────────┤
│              Recovery Pipeline (C)                       │
│  Multiread Voting │ Adaptive Decode │ Partial Recovery   │
│  Forensic Flux Decoder │ CRC Correction                  │
├─────────────────────────────────────────────────────────┤
│              Filesystem Layer (C)                        │
│  AmigaDOS │ FAT12 │ CBM DOS │ Apple DOS/ProDOS │ CP/M   │
│  TRSDOS │ TI-99 │ Atari DOS                             │
└─────────────────────────────────────────────────────────┘
```

## Build-System

- **Primär:** qmake (`.pro`-Datei), ~693 Source-Dateien / ~515 Header
  (Stand 2026-08-19 nach MF-271; zaehlen mit
  `find src -name '*.c' -o -name '*.cpp' | wc -l`)
- **Tests:** CMake (`tests/CMakeLists.txt`), 205/205 grün (Stand 2026-08-16);
  GLOB-discovered von 228+ `test_*.c`/`*.cpp` Quelldateien, 39 in
  `EXCLUDED_TESTS` (fehlende Module / WIP-Subsysteme). Zahlen driften —
  bis `update_inventory.py` (Phase 1, MF-363) existiert, gilt: `ctest -N`
  im Build-Verzeichnis ist die einzige Wahrheit, nicht diese Datei.
- **CI:** GitHub Actions — Linux (GCC), macOS (Clang), Windows (MinGW)
- **Sanitizer:** ASan + UBSan Workflows
- **Coverage:** lcov + Codecov

### Wichtig für Entwickler:
- `CONFIG += object_parallel_to_source` ist ZWINGEND (35+ Basename-Kollisionen)
- C-Header mit `protected` als Feldname → nicht direkt in C++ includierbar
- Qt6 erfordert `static_cast<char>()` für `QByteArray::append()`

## Verzeichnisstruktur

```
include/uft/          — Alle öffentlichen C-Header
  analysis/            — OTDR, DeepRead, TDFC, Confidence
  core/                — Fehler, Typen, Pfad-Sicherheit
  encoding/            — Encoding Detection Boost
  flux/                — Flux Decoder, SCP Parser
  formats/             — Format-spezifische Header
  hal/                 — Hardware Abstraction
  protection/          — Kopierschutz-Typen
  recovery/            — Adaptive Decode, Multiread

src/
  analysis/deepread/   — 5 DeepRead Forensik-Module
  analysis/otdr/       — OTDR Core + Widget
  algorithms/          — God-Mode Decoder, Viterbi, Encoding
  core/                — Kernmodule (Multirev, MFM, Error)
  decoder/             — PLL, Sync, Multi-Rev Fusion
  formats/             — 84 Format-Plugins (138 IDs registriert) (nach System sortiert)
  fs/                  — Dateisystem-Implementierungen
  flux/                — KryoFlux, Flux Loader
  gui/                 — Qt6 Widgets (OTDR Panel, Sector Editor, etc.)
  hal/                 — HAL-Implementierungen pro Controller
  hardware_providers/  — Qt-basierte Hardware-Provider
  protection/          — Kopierschutz-Erkennung
  recovery/            — Recovery-Pipeline

tests/                 — 77 C-Tests + 1 Qt-Test
.github/workflows/     — CI, Sanitizer, Coverage
```

## Schlüssel-Metriken

- ~693 Source-Dateien, ~515 Header — Stand 2026-08-20 nach MF-441
  (src/switch/ + src/cart7/ entfernt, 801 Dateien); davor MF-011 19-Welle
  Cleanup (785 dead-code Files / ~140k LOC entfernt, davon `src/fluxengine/`,
  `src/algorithms/{core,data,fluxio,imageio,tracks}`, `src/loaders/`,
  `src/filesystems/`, `src/encoding/`, plus 250+ einzelne orphan-Header)
- 138 Format-IDs, 137 Plugin-Definitionen (88 ausgeschrieben + 49 DSK-Makro;
  84 davon mit Registrar-Funktion, die niemand aufruft — MF-446; SSOT:
  `scripts/gen_format_list.py`), 44 Konvertierungspfade registriert /
  **15 angeboten**, davon **4 verlustfrei (je mit Messung)** (MF-541),
  17 Roundtrip-Matrix-Einträge (SSOT in `src/core/uft_roundtrip.c`;
  die Zahlen sind seit MF-541 abgeleitet, nicht gepflegt)
- 6 Hardware-Controller — SCP-Direct M3.1 libusb wiring LANDED (MF-254,
  HW-bench UFT-008 pending); XUM1541 M3.2 + Applesauce M3.3 weiterhin
  [~] partial scaffold (Pure-Utility + Lifecycle real, USB-/Serial-
  Wiring pending; siehe `docs/MASTER_PLAN.md` §M3)
- HAL-Tests grün: Greaseweazle (production) + 10 SCP-Direct + 16 XUM1541
  + 17 Applesauce = 43 Stub-Honesty-Asserts, 0 Failures
- 55+ Kopierschutz-Schemes **im Katalog** (`src/protection/`), davon
  erreichbar: Signal-Erkennung + 3 heuristisch benannte — MF-508
- 8 DeepRead-Module + 12 OTDR-Pipeline-Stufen
- 9 SIMD-Dispatch-Punkte (SSE2/AVX2 Runtime)
- ~610 Error-Handling-Fixes (fseek + I/O)
- Thread-Safety: 3 Subsysteme mit Mutex
- Compiler-Hardening: stack-protector, FORTIFY_SOURCE, ASLR
- 22 Agent-Definitionen (`.claude/agents/`, alle auf claude-fable-5)

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
