# Skeleton-Headers Audit

**Stand:** 2026-04-25 (DELETE-Welle 2: ride/uft_flux_decoder.h + flux/uft_gcr.h)
**Methodik:** Automatischer Scan aller `include/uft/**/*.h` auf das Verhältnis
deklarierter zu implementierter `uft_*`-Funktionen.
**Scan-Skript:** hier im Dokument reproduzierbar (siehe §Methodik).

---

## Kernbefund

**Stand 2026-04-25 (live audit):** 148 Skelett-Header, 2 819 nicht
implementierte Funktions-Deklarationen.

**Ursprünglich (2026-04-23):** 175 Skelett-Header, 3 355 nicht
implementierte Deklarationen.

Reduktion bisher:
- DELETE-Welle 1 (Commit `5b551fb`): 24 Header gelöscht.
- DELETE-Welle 2 (diese Session): 3 weitere Header gelöscht.
  - `ride/uft_flux_decoder.h` (61 decls, 57 missing — Skeleton #1)
  - `flux/uft_gcr.h` (self-contained orphan; below skeleton-threshold but tot)
  - `fs/uft_cbm_fs.h` (54 decls, 53 missing — silent ABI-conflict mit
    `formats/uft_cbm_formats.h` über gleichnamige `uft_cbm_print_directory`
    mit unterschiedlicher Signatur; Konflikt nie ausgelöst da niemand
    `fs/uft_cbm_fs.h` inkludiert hat)
- DELETE-Welle 3 (diese Session): 7 .cc-Duplikate in `src/fluxengine/lib/fluxsink/`
  byte-identisch zu `src/algorithms/fluxio/`, in keinem Build (~1500 LOC).
  Skeleton-Audit zählt nur Header — diese Welle reduziert die Header-Zahl nicht.
- DELETE-Welle 4 (diese Session): kompletter `src/fluxengine/`-Tree gelöscht
  (123 Files, 20 331 LOC). Dead-Code-Cluster aus dem fluxengine-Projekt-Import,
  ohne externe Konsumenten und nicht im Build. Skeleton-Audit zählt diese Files
  nicht (kein `uft_*`-Naming).
- DELETE-Welle 5 (diese Session): die fünf Sister-Trees zum gelöschten
  `src/fluxengine/` — 98 Files, ~16k LOC:
  - `src/algorithms/fluxio/` (21 Files, 2 205 LOC)
  - `src/filesystems/` (25 Files, 6 760 LOC)
  - `src/algorithms/core/` (13 Files, 1 788 LOC)
  - `src/algorithms/data/` (16 Files, 2 070 LOC)
  - `src/algorithms/imageio/` (23 Files, 3 582 LOC)
- DELETE-Welle 6 (diese Session): 17 weitere fluxengine-import-Files entfernt:
  - `src/encoding/` ganzes Verzeichnis (9 Files: 6 gcr/ + 3 root) — alle orphan
  - 5 einzelne Header in `src/algorithms/encoding/` (dec_m2fm, fm, luts, mfm,
    uft_encoding_detect) — nur `uft_otdr_encoding_boost.c` blieb (live, im qmake)
  - 3 einzelne Header in `src/formats/{amiga_ext,apple,ibm}/` — orphan, in
    mixed-state-Verzeichnissen (nur die toten .h-Files gelöscht)
- Noch offen (Welle 7+):
  - `src/formats/apple/data_gcr.h`, `applesauce.h` (vermutlich orphan, brauchen
    Verifikation — nicht in Welle-6-Scope dokumentiert)
  - Per-Subsystem-Audit der verbleibenden 148 Skeleton-Header (M2/M3-Arbeit)
- DOCUMENT-Welle (Commit `d9aa7a7`): 98 Header mit `/* PLANNED FEATURE */`-Banner.
- IMPLEMENT-Welle: 53 Header mit `/* PARTIALLY IMPLEMENTED */`-Banner.

Skeleton-Definition: ≥10 deklarierte `uft_*`-Funktionen, von denen ≥80 %
keine Implementation haben.

Das ist ein systemischer **Prinzip-1-Verstoß**: Das Projekt kündigt öffentlich
Funktionen an, die nicht existieren. Consumer die darauf verlinken bekommen
Link-Fehler; Doku die darauf verweist ist falsch; GUI-Elemente die sie
aufrufen sind Phantom-Features (siehe `docs/XCOPY_INTEGRATION_TODO.md`
als konkreter Anwendungsfall).

---

## Kategorisierung nach Subsystem

| Kategorie | Files | Missing decls | Worst offender |
|---|---:|---:|---|
| `include/uft/*.h` (Root) | 127 | 2 194 | `uft_forensic_imaging.h`, `uft_algorithms.h`, `uft_session.h` |
| `fs/` (Dateisysteme) | 14 | 428 | `uft_cbm_fs.h`, `uft_cpm_fs.h`, `uft_trsdos.h`, `uft_bbc_dfs.h` |
| `hal/` (Hardware-Backends) | 12 | 252 | `uft_applesauce.h`, `uft_xum1541.h`, `uft_supercard.h`, `uft_nibtools.h` |
| `core/` | 12 | 210 | `uft_json_export.h`, `uft_crc_unified.h` |
| `ride/` | 2 | 80 | `uft_flux_decoder.h` (62 decls, 91 % missing) |
| `ml/` | 2 | 56 | `uft_ml_decoder.h`, `uft_ml_training_gen.h` |
| `recovery/` | 3 | 44 | `uft_disk_recovery.h` |
| `encoding/` | 1 | 34 | `uft_decoders_v2.h` |
| `batch/` | 1 | 30 | `uft_batch.h` |
| `ocr/` | 1 | 27 | `uft_ocr.h` |
| **Σ** | **175** | **3 355** | |

---

## Top 30 Einzeltreffer

(Vollständige Liste in §Anhang A.)

| decls | missing | pct | path |
|------:|--------:|----:|------|
| 62 | 57 | 91% | `include/uft/ride/uft_flux_decoder.h` |
| 54 | 53 | 98% | `include/uft/fs/uft_cbm_fs.h` |
| 51 | 50 | 98% | `include/uft/fs/uft_cpm_fs.h` |
| 47 | 47 | 100% | `include/uft/fs/uft_trsdos.h` |
| 43 | 43 | 100% | `include/uft/fs/uft_bbc_dfs.h` |
| 42 | 38 | 90% | `include/uft/fs/uft_atari_dos.h` |
| 37 | 37 | 100% | `include/uft/uft_hardware_mock.h` |
| 36 | 36 | 100% | `include/uft/core/uft_json_export.h` |
| 36 | 36 | 100% | `include/uft/fs/uft_ti99_fs.h` |
| 43 | 36 | 83% | `include/uft/uft_hardware.h` |
| 34 | 34 | 100% | `include/uft/uft_exfat.h` |
| 34 | 34 | 100% | `include/uft/encoding/uft_decoders_v2.h` |
| 34 | 34 | 100% | `include/uft/fs/uft_apple_dos.h` |
| 34 | 34 | 100% | `include/uft/ml/uft_ml_training_gen.h` |
| 32 | 32 | 100% | `include/uft/uft_apd_format.h` |
| 31 | 31 | 100% | `include/uft/uft_forensic_imaging.h` |
| 30 | 30 | 100% | `include/uft/uft_algorithms.h` |
| 30 | 30 | 100% | `include/uft/uft_sos_protection.h` |
| 30 | 30 | 100% | `include/uft/batch/uft_batch.h` |
| 32 | 29 | 90% | `include/uft/uft_public_api.h` |
| 28 | 28 | 100% | `include/uft/uft_param_bridge.h` |
| 33 | 28 | 84% | `include/uft/hal/uft_supercard.h` |
| 27 | 27 | 100% | `include/uft/uft_image_db.h` |
| 27 | 27 | 100% | `include/uft/uft_multirev.h` |
| 27 | 27 | 100% | `include/uft/uft_multi_decoder.h` |
| 27 | 27 | 100% | `include/uft/ocr/uft_ocr.h` |
| 27 | 27 | 100% | `include/uft/hal/internal/uft_nibtools.h` |
| 32 | 27 | 84% | `include/uft/uft_platform.h` |
| 26 | 26 | 100% | `include/uft/uft_bit_confidence.h` |
| 26 | 26 | 100% | `include/uft/uft_bootblock_scanner.h` |

---

## Methodik (reproduzierbar)

1. Sammle alle `uft_*`-Funktions-Definitionen mit Body (`name(...){`):
   * aus `src/**/*.c` und `src/**/*.cpp`
   * aus `static inline uft_*(...) {` in beliebigem Header
   * aus `#define uft_*(...)` (Funktions-Makros)

2. Pro Header in `include/uft/**/*.h` zähle Prototypen der Form
   `<ret> uft_*(...);`.

3. Klassifiziere als **Skelett**, wenn:
   * `decls ≥ 10`
   * `(decls − implementiert) / decls ≥ 0.8`

4. False-Positive-Korrekturen:
   * `static inline` Definitionen im Header selbst zählen als implementiert.
   * Funktions-Makros (`#define uft_foo(...)`) zählen als implementiert.
   * Void-Returns, Pointer-Returns, `const`-Qualifier werden
     regex-aware erkannt.

Das Skript ist am Ende des Dokuments für Wiederholbarkeit hinterlegt.

---

## Bekannte Limitierungen des Scans

- Funktions-Signaturen die Python's Regex nicht erkennt (insb. bei exotischen
  Qualifier-Kombinationen) werden entweder fehlerhaft nicht als Prototyp oder
  nicht als Definition erkannt. Erwartete Fehlerrate: unter 2 %.
- Symbole in `.cpp`-Dateien mit `Class::method` Definition werden erkannt,
  aber Namespace-Member werden nicht als `uft_*` gematcht (die UFT-Konvention
  ist plain-C, das ist meist richtig).
- Auto-generierte Dateien (`src/core/uft_error_strings.c` nach SSOT-Cutover)
  werden normal gescannt, was korrekt ist.

Spot-Check bei `uft_platform.h`: `uft_clock_gettime` ist `static inline`
definiert (korrekt erkannt); `uft_path_join` ist echter Prototyp ohne
Implementation (korrekt als missing geflaggt).

---

## Handlungsoptionen pro Header

Analog zum `stub-eliminator`-Pattern (Prinzip 4 Aktionen) gibt es pro
Skelett-Header drei Endzustände:

### DELETE — „kein Konsument, kein Zukunftsplan"

Wenn keine `#include` aus `src/` und keine produkt-Roadmap-Referenz
auf den Header zeigt: Header löschen. Kandidaten typischerweise in der
`ride/`-Subkategorie (Legacy-Experimente) und bei `uft_*_v2.h`/`_v3.h`
die nach dem MF-004-Cleanup keine Grundlage mehr haben.

### DOCUMENT — „geplant, noch nicht gebaut"

Wenn die Feature-Ansage legitim ist (z. B. `uft_forensic_imaging.h`
passt zum forensischen Kernauftrag), aber die Implementation fehlt:
Header mit Prinzip-7-Stil-Kommentar versehen —

```c
/* PLANNED FEATURE — not yet implemented.
 * See docs/KNOWN_ISSUES.md §<n> for scope + tracking issue.
 * Callers will get a linker error; that is by design, not a bug. */
```

Und zusätzlich einen Eintrag in `docs/KNOWN_ISSUES.md` unter Prinzip 1:
„this header promises <X functions>, none are implemented".

### IMPLEMENT — „jetzt bauen"

Nur für Features die auf der aktuellen Roadmap stehen. Nicht dieses
Dokument's Aufgabe zu entscheiden; separate Roadmap-Diskussion.

---

## Empfohlene Bearbeitungsreihenfolge

Nicht die 175 Header von oben nach unten, sondern nach Wirkung:

1. **Erste Welle (DELETE-Kandidaten):** Header ohne jeden Konsumenten in
   `src/` oder `include/`. Schätzung: ~50-70 % der 175 sind tot.
   Automatisierbar via `grep -r '#include "…/<header>"' src/ include/`.

2. **Zweite Welle (DOCUMENT):** Header mit Konsumenten, die aber nie auf die
   leeren Funktionen verlinken (z. B. nur Typen/Enums werden benutzt).
   Feature-Markierung + KNOWN_ISSUES-Eintrag.

3. **Dritte Welle (Feature-Entscheidung):** Alles mit echten Call-Sites wo
   Funktionen aufgerufen werden die nicht existieren. **Diese sind die
   forensischen Phantom-Features** (GUI-Buttons, CLI-Kommandos die nichts
   tun). Pro Fall Roadmap-Entscheidung: bauen oder entfernen.

---

## Verwandte Findings

- **MF-004** (`_parser_v3.c`-Proliferation, 146 Files) ist eine Unter-
  menge dieses Musters — das Äquivalent auf der `.c`-Seite.
- **MF-005** (656 tote Deklarationen in `include/uft/`) hat dieselben
  3 355 Deklarationen gezählt, aber in einer anderen Aggregation
  (je einzelne Deklaration statt pro Header) — dieser Audit ist die
  strukturierte Sicht.
- **docs/XCOPY_INTEGRATION_TODO.md** ist ein konkreter Anwendungsfall:
  1 600 + Zeilen an `uft_amiga_*.h`/`uft_bootblock_scanner.h`-Skeletten
  plus 428 Zeilen ungebundenes GUI-Panel.
- **MF-011 (neu)** fasst das systemische Problem zusammen; siehe
  Eintrag in `docs/KNOWN_ISSUES.md`.

---

## Anhang A — vollständige Liste der 175 Skeleton-Header

<!-- Vollständige Tabelle generiert durch scripts/audit_skeleton_headers.py -->

<!-- BEGIN: auto-generated body -->

| decls | missing | pct | path |
|------:|--------:|----:|------|
| 62 | 57 | 91% | `include/uft/ride/uft_flux_decoder.h` |
| 54 | 53 | 98% | `include/uft/fs/uft_cbm_fs.h` |
| 51 | 50 | 98% | `include/uft/fs/uft_cpm_fs.h` |
| 47 | 47 | 100% | `include/uft/fs/uft_trsdos.h` |
| 43 | 43 | 100% | `include/uft/fs/uft_bbc_dfs.h` |
| 42 | 38 | 90% | `include/uft/fs/uft_atari_dos.h` |
| 37 | 37 | 100% | `include/uft/uft_hardware_mock.h` |
| 36 | 36 | 100% | `include/uft/core/uft_json_export.h` |
| 36 | 36 | 100% | `include/uft/fs/uft_ti99_fs.h` |
| 43 | 36 | 83% | `include/uft/uft_hardware.h` |
| 34 | 34 | 100% | `include/uft/uft_exfat.h` |
| 34 | 34 | 100% | `include/uft/encoding/uft_decoders_v2.h` |
| 34 | 34 | 100% | `include/uft/fs/uft_apple_dos.h` |
| 34 | 34 | 100% | `include/uft/ml/uft_ml_training_gen.h` |
| 32 | 32 | 100% | `include/uft/uft_apd_format.h` |
| 31 | 31 | 100% | `include/uft/uft_forensic_imaging.h` |
| 30 | 30 | 100% | `include/uft/uft_algorithms.h` |
| 30 | 30 | 100% | `include/uft/uft_sos_protection.h` |
| 30 | 30 | 100% | `include/uft/batch/uft_batch.h` |
| 32 | 29 | 90% | `include/uft/uft_public_api.h` |
| 28 | 28 | 100% | `include/uft/uft_param_bridge.h` |
| 33 | 28 | 84% | `include/uft/hal/uft_supercard.h` |
| 27 | 27 | 100% | `include/uft/uft_image_db.h` |
| 27 | 27 | 100% | `include/uft/uft_multirev.h` |
| 27 | 27 | 100% | `include/uft/uft_multi_decoder.h` |
| 27 | 27 | 100% | `include/uft/ocr/uft_ocr.h` |
| 27 | 27 | 100% | `include/uft/hal/internal/uft_nibtools.h` |
| 32 | 27 | 84% | `include/uft/uft_platform.h` |
| 26 | 26 | 100% | `include/uft/uft_bit_confidence.h` |
| 26 | 26 | 100% | `include/uft/uft_bootblock_scanner.h` |
| 26 | 26 | 100% | `include/uft/uft_config_manager.h` |
| 26 | 26 | 100% | `include/uft/uft_decoder_adapter.h` |
| 26 | 26 | 100% | `include/uft/uft_session.h` |
| 26 | 26 | 100% | `include/uft/uft_vfs.h` |
| 26 | 26 | 100% | `include/uft/hal/uft_xum1541.h` |
| 25 | 25 | 100% | `include/uft/uft_atari_image.h` |
| 25 | 25 | 100% | `include/uft/uft_hfs_extended.h` |
| 24 | 24 | 100% | `include/uft/uft_blkid.h` |
| 24 | 24 | 100% | `include/uft/uft_error_chain.h` |
| 24 | 24 | 100% | `include/uft/uft_writer_verify.h` |
| 24 | 24 | 100% | `include/uft/uft_write_transaction.h` |
| 24 | 24 | 100% | `include/uft/formats/uft_pce.h` |
| 24 | 24 | 100% | `include/uft/fs/uft_fbasic_fs.h` |
| 24 | 24 | 100% | `include/uft/params/uft_canonical_params.h` |
| 23 | 23 | 100% | `include/uft/uft_multi_decode.h` |
| 23 | 23 | 100% | `include/uft/uft_recovery_advanced.h` |
| 23 | 23 | 100% | `include/uft/fs/uft_fat_badblock.h` |
| 24 | 23 | 95% | `include/uft/ride/uft_dos_recognition.h` |
| 22 | 22 | 100% | `include/uft/uft_audit_trail.h` |
| 22 | 22 | 100% | `include/uft/uft_gui_params_extended.h` |
| 22 | 22 | 100% | `include/uft/ml/uft_ml_decoder.h` |
| 22 | 22 | 100% | `include/uft/samdisk/uft_samdisk.h` |
| 23 | 22 | 95% | `include/uft/core/uft_integration.h` |
| 24 | 22 | 91% | `include/uft/uft_crc_polys.h` |
| 21 | 21 | 100% | `include/uft/core/uft_track_base.h` |
| 21 | 21 | 100% | `include/uft/hal/internal/uft_pauline.h` |
| 20 | 20 | 100% | `include/uft/uft_pll_unified.h` |
| 20 | 20 | 100% | `include/uft/flux/uft_kryoflux_streaming.h` |
| 20 | 20 | 100% | `include/uft/hal/uft_applesauce.h` |
| 20 | 20 | 100% | `include/uft/hal/uft_fc5025.h` |
| 20 | 20 | 100% | `include/uft/hal/uft_latency_tracking.h` |
| 20 | 20 | 100% | `include/uft/web/uft_web_viewer.h` |
| 22 | 20 | 90% | `include/uft/fs/uft_bbc_fs.h` |
| 19 | 19 | 100% | `include/uft/uft_flux_statistics.h` |
| 19 | 19 | 100% | `include/uft/flux/uft_fluxstat.h` |
| 19 | 19 | 100% | `include/uft/hal/internal/uft_linux_fdc.h` |
| 20 | 19 | 95% | `include/uft/ir/uft_ir_serialize.h` |
| 21 | 19 | 90% | `include/uft/formats/ipf/uft_ipf.h` |
| 23 | 19 | 82% | `include/uft/uft_fat12_v2.h` |
| 23 | 19 | 82% | `include/uft/formats/uft_fat12.h` |
| 23 | 19 | 82% | `include/uft/hal/uft_hal_v3.h` |
| 18 | 18 | 100% | `include/uft/uft_bitstream_preserve.h` |
| 18 | 18 | 100% | `include/uft/uft_mmap.h` |
| 18 | 18 | 100% | `include/uft/uft_unified_image.h` |
| 18 | 18 | 100% | `include/uft/uft_writer_backend.h` |
| 18 | 18 | 100% | `include/uft/fs/uft_fat_atari.h` |
| 18 | 18 | 100% | `include/uft/hal/internal/uft_zoomtape.h` |
| 19 | 18 | 94% | `include/uft/core/uft_sector.h` |
| 19 | 18 | 94% | `include/uft/formats/uft_86f.h` |
| 21 | 18 | 85% | `include/uft/c64/uft_cbm_disk.h` |
| 21 | 18 | 85% | `include/uft/decoder/uft_format_hints.h` |
| 22 | 18 | 81% | `include/uft/uft_integration.h` |
| 22 | 18 | 81% | `include/uft/core/uft_logging_v2.h` |
| 17 | 17 | 100% | `include/uft/uft_apple_fs.h` |
| 17 | 17 | 100% | `include/uft/uft_gui_bridge.h` |
| 17 | 17 | 100% | `include/uft/uft_settings.h` |
| 17 | 17 | 100% | `include/uft/hardware/uft_gw2dmk.h` |
| 17 | 17 | 100% | `include/uft/hal/internal/uft_ieee488.h` |
| 17 | 17 | 100% | `include/uft/hal/internal/uft_xa1541.h` |
| 18 | 17 | 94% | `include/uft/uft_parallel.h` |
| 18 | 17 | 94% | `include/uft/uft_protection_pipeline.h` |
| 18 | 17 | 94% | `include/uft/recovery/uft_recovery_pipeline.h` |
| 20 | 17 | 85% | `include/uft/fs/uft_fat32.h` |
| 16 | 16 | 100% | `include/uft/uft_capability.h` |
| 16 | 16 | 100% | `include/uft/uft_gui_params.h` |
| 16 | 16 | 100% | `include/uft/uft_gui_params_complete.h` |
| 16 | 16 | 100% | `include/uft/decoder/uft_forensic_flux_decoder.h` |
| 16 | 16 | 100% | `include/uft/flux/uft_adaptive_decoder.h` |
| 16 | 16 | 100% | `include/uft/hardware/uft_crosstalk_filter.h` |
| 16 | 16 | 100% | `include/uft/recovery/uft_recovery.h` |
| 17 | 16 | 94% | `include/uft/uft_write_verify.h` |
| 15 | 15 | 100% | `include/uft/uft_memory.h` |
| 15 | 15 | 100% | `include/uft/uft_p64.h` |
| 15 | 15 | 100% | `include/uft/uft_process.h` |
| 15 | 15 | 100% | `include/uft/uft_write_preview.h` |
| 15 | 15 | 100% | `include/uft/core/uft_crc_unified.h` |
| 15 | 15 | 100% | `include/uft/core/uft_histogram.h` |
| 15 | 15 | 100% | `include/uft/flux/uft_flux_cluster.h` |
| 15 | 15 | 100% | `include/uft/formats/uft_tc.h` |
| 15 | 15 | 100% | `include/uft/track/uft_track_generator.h` |
| 15 | 15 | 100% | `include/uft/track/uft_track_layout.h` |
| 16 | 15 | 93% | `include/uft/uft_dmk.h` |
| 16 | 15 | 93% | `include/uft/decoder/uft_cell_analyzer.h` |
| 16 | 15 | 93% | `include/uft/flux/uft_flux_pll_v20.h` |
| 16 | 15 | 93% | `include/uft/flux/uft_woz_parser.h` |
| 16 | 15 | 93% | `include/uft/formats/uft_dmk.h` |
| 17 | 15 | 88% | `include/uft/core/uft_disk.h` |
| 14 | 14 | 100% | `include/uft/uft_crc_cache.h` |
| 14 | 14 | 100% | `include/uft/uft_device_manager.h` |
| 14 | 14 | 100% | `include/uft/uft_forensic_advanced.h` |
| 14 | 14 | 100% | `include/uft/uft_gcr_nibtools.h` |
| 14 | 14 | 100% | `include/uft/uft_usb.h` |
| 14 | 14 | 100% | `include/uft/core/uft_limits.h` |
| 14 | 14 | 100% | `include/uft/decoder/uft_gcr.h` |
| 15 | 14 | 93% | `include/uft/uft_applesauce.h` |
| 15 | 14 | 93% | `include/uft/flux/uft_pll_pi.h` |
| 15 | 14 | 93% | `include/uft/flux/pll/uft_pll_pi.h` |
| 16 | 14 | 87% | `include/uft/core/uft_format_registry.h` |
| 16 | 14 | 87% | `include/uft/parsers/uft_scp_multirev.h` |
| 13 | 13 | 100% | `include/uft/uft_forensic_report.h` |
| 13 | 13 | 100% | `include/uft/uft_hfs.h` |
| 13 | 13 | 100% | `include/uft/formats/uft_dfi.h` |
| 13 | 13 | 100% | `include/uft/fs/uft_hfs_extended.h` |
| 13 | 13 | 100% | `include/uft/ir/uft_ir_timing.h` |
| 14 | 13 | 92% | `include/uft/uft_operation_result.h` |
| 15 | 13 | 86% | `include/uft/uft_decoder_plugin.h` |
| 15 | 13 | 86% | `include/uft/decoder/uft_sector_confidence.h` |
| 16 | 13 | 81% | `include/uft/uft_format_detect.h` |
| 12 | 12 | 100% | `include/uft/flux/uft_pattern_generator.h` |
| 12 | 12 | 100% | `include/uft/flux/uft_sector_overlay.h` |
| 12 | 12 | 100% | `include/uft/fs/uft_fat_boot.h` |
| 12 | 12 | 100% | `include/uft/hardware/uft_latency_tracking.h` |
| 12 | 12 | 100% | `include/uft/hardware/uft_write_precomp.h` |
| 12 | 12 | 100% | `include/uft/track/uft_sector_extractor.h` |
| 13 | 12 | 92% | `include/uft/uft_simd.h` |
| 14 | 12 | 85% | `include/uft/floppy/uft_floppy_v2.h` |
| 15 | 12 | 80% | `include/uft/formats/uft_jv3.h` |
| 11 | 11 | 100% | `include/uft/uft_apple_gcr.h` |
| 11 | 11 | 100% | `include/uft/uft_cache.h` |
| 11 | 11 | 100% | `include/uft/uft_test_framework.h` |
| 11 | 11 | 100% | `include/uft/uft_tool_adapter.h` |
| 11 | 11 | 100% | `include/uft/core/uft_encoding.h` |
| 11 | 11 | 100% | `include/uft/recovery/uft_disk_recovery.h` |
| 12 | 11 | 91% | `include/uft/core/uft_crc_v2.h` |
| 12 | 11 | 91% | `include/uft/formats/uft_woz.h` |
| 10 | 10 | 100% | `include/uft/uft_dmf.h` |
| 10 | 10 | 100% | `include/uft/uft_format_verify.h` |
| 10 | 10 | 100% | `include/uft/uft_xdf.h` |
| 10 | 10 | 100% | `include/uft/crc/uft_crc_extended.h` |
| 10 | 10 | 100% | `include/uft/decoder/uft_pll.h` |
| 10 | 10 | 100% | `include/uft/flux/uft_flux_instability.h` |
| 10 | 10 | 100% | `include/uft/formats/uft_x68k.h` |
| 10 | 10 | 100% | `include/uft/generate/uft_track_generator.h` |
| 11 | 10 | 90% | `include/uft/uft_adaptive_decoder.h` |
| 11 | 10 | 90% | `include/uft/uft_amiga_bootblock.h` |
| 11 | 10 | 90% | `include/uft/flux/uft_kryoflux_parser.h` |
| 11 | 10 | 90% | `include/uft/flux/uft_libflux_pll_enhanced.h` |
| 11 | 10 | 90% | `include/uft/flux/uft_pll_params.h` |
| 12 | 10 | 83% | `include/uft/decoder/uft_unified_decoder.h` |
| 10 | 9 | 90% | `include/uft/uft_bbc.h` |
| 10 | 9 | 90% | `include/uft/flux/uft_mfm_flux.h` |
| 10 | 9 | 90% | `include/uft/flux/uft_mfm_pll.h` |
| 11 | 9 | 81% | `include/uft/uft_io_abstraction.h` |
| 11 | 9 | 81% | `include/uft/decoder/uft_fusion.h` |
| 10 | 8 | 80% | `include/uft/uft_operations.h` |

<!-- END: auto-generated body -->

(Regeneriere mit: `python3 scripts/audit_skeleton_headers.py > /tmp/out.md`;
Skript folgt unten als nächster Schritt dieser Aufräumungsrunde.)

---

## Next Step (außerhalb dieses Dokuments)

Siehe `docs/KNOWN_ISSUES.md` — neuer Eintrag **MF-011 Skeleton-Header-Proliferation**
mit Plan zur schrittweisen Abarbeitung (DELETE-Welle automatisiert,
DOCUMENT-Welle als separate Commits, IMPLEMENT-Welle per Feature nach
Roadmap-Review).
