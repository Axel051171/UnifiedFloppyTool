# a8rawconv Integration — TODO-Liste

**Stand:** 2026-04-23
**Quelle:** `github.com/Axel051171/a8rawconv-0.95` (Fork von
Avery Lee's `a8rawconv`, 9194 LOC C++11)
**Lizenz:** GPL-2 or later (Code-Header); LICENSE-Datei nennt CC0 —
wir behandeln den Code als GPL-2 per per-file-Header. Kompatibel mit
UFT (GPL-2). Attribution: Avery Lee, 2014-2023.

---

## Was a8rawconv tut

CLI-Konverter Flux ↔ sektor-basierte Image-Formate für Atari-8-bit
und Apple-II-Disks. Liest SCP, KryoFlux-Streams, schreibt ATR, ATX, XFD,
VFD, ADF. Direkter Hardware-Zugriff auf SuperCard Pro über USB. Vom
Autor der Atari-Emulation Altirra — fundierter Hintergrund.

---

## Schon als Skelett in UFT vorhanden (nie implementiert)

| UFT-Header | LOC | Impl | Herkunft |
|---|---:|---:|---|
| `include/uft/hardware/uft_write_precomp.h` | 322 | 0 | DTC-Observation |
| `include/uft/uft_a8rawconv_algorithms.h` (nur im Worktree) | 661 | 0 | frühere a8rawconv-Eval |
| `include/uft/uft_a8rawconv_atari.h` (nur im Worktree) | 641 | 0 | frühere a8rawconv-Eval |

Dieses TODO bringt die bestehenden Skelette aus dem Skeleton-Audit
(`docs/SKELETON_HEADERS_AUDIT.md`) auf Implementierungs-Stand.

---

## Was aus a8rawconv wertvoll ist

### Wert ⭐⭐ — SCP-Direct-Hardware
`rawdiskscpdirect.cpp` (295 LOC) spricht direkt mit SuperCard Pro
Hardware über USB-Protokoll. CLAUDE.md beschreibt den UFT-SCP-HAL als
„HAL backend stubbed, CLI available". Das ist die nächstliegende
Plug-in-fähige Quelle für den fehlenden HAL-Backend-Code.

### Wert ⭐ — Write-Precompensation-Algorithmus
`compensation.cpp` (76 LOC) fürs Mac-800K-Format enthält eine
konkrete Implementierung des Peak-Shift-Kompensationsalgorithmus.
UFT's `uft_write_precomp.h` deklariert 15 Funktionen, implementiert
keine. Der a8rawconv-Code wäre der direkte Rohstoff.

### Wert ⭐ — Interleave-Calculator
`interleave.cpp` berechnet optimale Sektor-Reihenfolge je Format
(9:1 für 128B-SD, 13:1 für ED, 15:1 für 256B, 1:1 für 512B). UFT
hat keine vergleichbare Funktion — relevant wenn UFT ATR/ATX/XFD
schreibt.

### Wert ⭐ — ATX-Format-Vervollständigung
`diskatx.cpp` (465 LOC) ist eine vollständige ATX-Read-Write-
Implementierung inkl. Weak-Sector und Extra-Sector-Handling. UFT's
`src/formats/atx/uft_atx.c` ist nur 169 LOC — vermutlich probe+basic-
read. ATX ist das Atari-Äquivalent zu STX; vollständige Unterstützung
gehört zum forensischen Kernauftrag.

---

## Was NICHT übernehmen

- ❌ **`rawdiskscript.cpp` (952 LOC DSL)** — embedded Domain-Specific-
  Language für custom disk ops. Verstößt gegen Prinzip 3 (explizite
  API statt impliziter Konventionen). UFT bleibt bei CLI-Flags +
  CONSULT-Protokoll.
- ❌ **`diska2.cpp` Apple-GCR-6&2** — redundant zu UFT's WOZ und NIB
  Plugins.
- ❌ **CLI-eigene Architektur** — a8rawconv ist reine CLI, UFT ist
  Qt-GUI-first mit CLI als separate Aufgabe (siehe MF-Backlog).

---

## Todo-Liste (priorisiert)

### TA1 — [P1] Backe `uft_write_precomp.h` mit `compensation.cpp`-Algo

**Skelett heute:** `include/uft/hardware/uft_write_precomp.h` 322 LOC,
15 Funktions-Deklarationen, null Implementation.

**Port-Quelle:** `a8rawconv/compensation.cpp` (76 LOC). Hauptfunktion
`postcomp_track_mac800k()` demonstriert den generischen Ansatz:
zwischen drei aufeinanderfolgenden Flux-Transitions t0/t1/t2 die
zeitliche Distanz der mittleren so korrigieren dass Peak-Shift-Effekte
(benachbarte Transitions ziehen sich zeitlich an) aufgehoben werden.
Threshold bei ca. 1/45000 der Rotation, linear skalierend nach innerem
Track-Radius.

**Action:**
- `src/hardware/uft_write_precomp.c` neu anlegen (~200 LOC erwartet).
- Generische `uft_precomp_apply(transitions[], count, rate_ns, track_no)`
  implementieren — nicht Mac-spezifisch, sondern parametrisiert.
- Attribution in File-Header: "Write-precompensation based on
  a8rawconv by Avery Lee, GPL-2-or-later".
- Test: `tests/test_write_precomp.c` mit synthetischer Flux-Sequenz
  die bekanntes Peak-Shift hat.

**Aufwand:** M (2-3 Tage)

---

### TA2 — [P1] Sector-Interleave-Calculator

**Skelett heute:** keines. Nötig damit UFT beim ATR/XFD-Write einen
realistischen Sektor-Layout produziert (nicht einfach 1-2-3-...).

**Port-Quelle:** `a8rawconv/interleave.cpp` — Funktion
`compute_interleave(timings, mfm, sector_size, sector_count, track, side, mode)`.
Modi: `Auto`, `ForceAuto`, `Off`, vorgegebener Faktor.

**Action:**
- `include/uft/core/uft_interleave.h` + `src/core/uft_interleave.c`
  neu anlegen (Ca. 150 LOC inkl. Header).
- API:
  ```c
  uft_error_t uft_compute_interleave(
      float *timings, size_t sector_count,
      uint16_t sector_size, bool mfm,
      int track, int side,
      uft_interleave_mode_t mode);
  ```
- Integration: ATR/ATX/XFD-Writer rufen das beim Track-Encoding auf.
- Test: erwartete Interleave-Sequenzen pro Sektor-Größe aus a8rawconv
  als Fixture.

**Aufwand:** S (1-2 Tage)

---

### TA3 — [P1] ATX-Plugin beef up mit `diskatx.cpp`

**Heutiger Stand:** `src/formats/atx/uft_atx.c` 169 LOC —
vermutlich nur probe + basic read_track ohne weak-sector / extra-sector
Handling.

**Port-Quelle:** `a8rawconv/diskatx.cpp` (465 LOC).
Features dort:
- Chunk-Parser für AT8X (Header), TRCK (Track-Record), SECT (Sector-
  Record), EXTD (Extended-Data-Record).
- Weak-Sector-Markierung über `sect_flags` bit 0x40.
- Extra-Sector (Long-Track) Handling über EXTD-Chunks.
- Sector-Duplicates (mehrere SECT mit gleichem Sector-Number) —
  forensisch relevant weil Kopierschutz.

**Action:**
- `src/formats/atx/uft_atx.c` erweitern auf ~400 LOC.
- Feature-Matrix-Update in `uft_format_plugin_t`-Registration:
  `{ "Weak sectors", SUPPORTED, NULL }`,
  `{ "Extra sectors / long tracks", SUPPORTED, NULL }`,
  `{ "Duplicate sectors (protection)", SUPPORTED, NULL }`.
- `is_stub = false` bestätigen.
- Test: `tests/test_atx_plugin.c` gegen a8rawconv-erzeugtes ATX.

**Aufwand:** M (3-4 Tage)

---

### TA4 — [P2] SCP-Direct-HAL Backend

**Heutiger Stand:** CLAUDE.md: „SuperCard Pro (25MHz, USB) — HAL
backend stubbed, CLI available". Vermutliches Symptom im Skeleton-
Audit: die HAL-Header sind Teil der 12 „hal/"-Skelette.

**Port-Quelle:** `a8rawconv/rawdiskscpdirect.cpp` (295 LOC) +
`scp.cpp` (316 LOC Format-Core). Nutzt Windows-`CreateFile`/`DeviceIoControl`
auf Windows, Linux `/dev/ttyACM0` über `termios`. Protokoll: Byte-Commands
0x80-0xD2 (entspricht dem SuperCard-Pro-SDK v1.7 das bei UFT in der
Release 4.1.0 schon als Vorbild diente, per CHANGELOG-Eintrag).

**Action:**
- Prüfe erst ob `include/uft/hal/uft_supercard.h` (33 Decls, 84 %
  fehlen per Skeleton-Audit) das richtige Ziel-Skelett ist.
- `src/hal/uft_hal_scp_direct.c` anlegen.
- Portabilität: `serial_win32.cpp` (138 LOC) und `serial_linux.cpp`
  (118 LOC) aus a8rawconv als Referenz; macOS-Port fehlt in beiden
  Projekten.
- Integration ins HAL-Registry. `is_stub = false` für SCP.

**Aufwand:** L (1 Woche)

---

### TA5 — [P3] FM/MFM Sector-Parser-Review

**Heutiger Stand:** `src/detect/mfm/mfm_detect.c` existiert.

**Port-Quelle:** `a8rawconv/sectorparser.cpp` (615 LOC) — WD177x-
präzise Decodierung mit Gap-Handling, Sync-Pattern-Matching,
Address-Mark-Detection (`0xA1A1A1FE` für MFM-ID, `0xA1A1A1FB`/`0xA1A1A1F8`
für Data).

**Action:** Vergleich beider Implementierungen. Wenn a8rawconv
messbar genauer / kompletter ist: Merge / Ablöse. Wenn nicht: nur
als Regressions-Quelle dokumentieren.

**Aufwand:** S (Analyse, Port-Entscheidung)

---

### TA6 — [DELETE] Skelette aus `.claude/worktrees/` retten oder verwerfen

Im Worktree `.claude/worktrees/agent-a2146375/include/uft/` liegen
zwei Skeleton-Header:
- `uft_a8rawconv_algorithms.h` (661 LOC, 0 Impl)
- `uft_a8rawconv_atari.h` (641 LOC, 0 Impl)

Ziel-Zustand:
- Wenn TA1-TA5 umgesetzt: Teile dieser Skelette werden zu
  Implementation. Dann migrieren nach `include/uft/` regulär.
- Wenn nicht: Im Worktree-Scratch belassen. Nicht in main.

Die aktuellen Paths sind per `.gitignore` bereits ausgeschlossen —
kein Handlungsdruck, aber bei nächster Worktree-Aufräumung löschen
wenn nicht verwendet.

**Aufwand:** S (Entscheidung, keine Implementation)

---

## Bezug zu anderen Findings

- **MF-005** (656 tote Deklarationen) — TA1 und TA3 reduzieren diese
  um die 15 Precomp + 12 ATX Decls.
- **MF-007** (Plugin-Test-Coverage) — TA3 fügt einen neuen Test hinzu,
  schließt eine Zeile in der Test-Matrix.
- **MF-011** (Skeleton-Headers) — TA1 hebt `uft_write_precomp.h` aus
  dem Skelett-Zustand. TA4 reduziert den HAL-Subcategory-Anteil.
- **CLAUDE.md** — SuperCard-Pro-Status aktualisieren von „HAL backend
  stubbed" auf „production" nach TA4.

---

## Reihenfolge

1. **TA2** (Interleave, 1-2 Tage) — kleinste Einheit, entblockt
   saubere ATR/XFD-Writes.
2. **TA1** (Precomp, 2-3 Tage) — implementiert das 322-Zeilen-
   Skelett, schließt einen Feature-Phantom.
3. **TA3** (ATX, 3-4 Tage) — verbessert echtes Plugin das Nutzer
   konsumieren. Forensisch relevant (weak sectors).
4. **TA4** (SCP direct, 1 Woche) — größter Brocken, höchste
   Sichtbarkeit (HAL-Status in CLAUDE.md).
5. **TA5, TA6** — optional.

**Gesamtaufwand:** 3 Wochen für TA1-TA4 zusammen.

---

## Lizenz-Notiz

Beim Portieren aus a8rawconv:

```c
/*
 * <UFT module purpose>
 *
 * Write-precompensation algorithm adapted from a8rawconv by
 * Avery Lee (2014-2023), GPL-2-or-later. Original source:
 * https://github.com/Axel051171/a8rawconv-0.95
 *
 * Adaptation for UFT: 2026, <maintainer>, GPL-2-or-later.
 */
```

GPL-2 verlangt zudem dass Änderungen gegenüber dem Original
dokumentiert werden (Abschnitt 2a). Commit-Messages mit `Adapted
from a8rawconv: <file> — <change>` reichen dafür.
