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

### 2. Format-Unterstützung (88 registered plugins, 138 format IDs)

> **EINFRIER-REGEL (MF-363):** Kein neuer ungeprüfter Code im Format-/
> Decoder-Layer. Moratorium bis Label-Skript (T1/T1b/T2/T3) läuft und
> ATR/D64/ADF/FDI/NFD-r0 auf T1/T1b gehoben sind; danach 1:2 (ein neues
> Format = zwei Hebungen). Details: `.claude/CLAUDE.md` Daueraufgabe 5.
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

### 3. Format-Konvertierung (44 Pfade)
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

### 5. Kopierschutz-Analyse (35+ Schemes)
Erkennt und dokumentiert historische Kopierschutz-Verfahren:
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
│  DeepRead (8 Module) │ Protection (35+ Schemes)          │
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
- 138 Format-IDs, 88 Plugin-B Registrierungen (84 auto + 4 manuell; SSOT:
  `scripts/gen_format_list.py`), 45 Konvertierungspfade,
  13 Roundtrip-Matrix-Einträge (SSOT in `src/core/uft_roundtrip.c`)
- 6 Hardware-Controller — SCP-Direct M3.1 libusb wiring LANDED (MF-254,
  HW-bench UFT-008 pending); XUM1541 M3.2 + Applesauce M3.3 weiterhin
  [~] partial scaffold (Pure-Utility + Lifecycle real, USB-/Serial-
  Wiring pending; siehe `docs/MASTER_PLAN.md` §M3)
- HAL-Tests grün: Greaseweazle (production) + 10 SCP-Direct + 16 XUM1541
  + 17 Applesauce = 43 Stub-Honesty-Asserts, 0 Failures
- 55+ Kopierschutz-Schemes
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
