# `src/a8rawconv/` — Referenzbestand, kein Baubestandteil

**Herkunft:** [a8rawconv](https://github.com/Axel051171/a8rawconv-0.95) 0.95,
© 2014–2023 Avery Lee (phaeron, Autor von Altirra).
**Lizenz:** GPL-2.0-or-later — Volltext in [`COPYING`](COPYING).

---

## Was das hier ist

Diese 44 Quelldateien (9 194 Zeilen) werden **von keinem Build kompiliert**.
Weder `UnifiedFloppyTool.pro` noch `CMakeLists.txt` führen eine davon; anders
als bei `src/samdisk/` wird nicht einmal ein Include-Pfad gebunden.
`scripts/verify_build_sources.py` kennt den Ordner als `NOT_BUILT_BY_DESIGN`.

Der Bestand ist das **zweite Referenz-Orakel** des Projekts, mit einer klaren
Arbeitsteilung:

| Orakel | Deckt ab | Lizenz |
|---|---|---|
| `src/samdisk/` | PC, CPC, Sinclair, Atari ST, generische FDC-Formate | MIT |
| `src/a8rawconv/` | **Atari 8-bit (ATX/VAPI, FM), Interleave, Apple/Mac-GCR** | GPL-2.0-or-later |

a8rawconv ist dafür die richtige Quelle, weil es kein Leser nebenbei ist,
sondern der vollständige Flux→Sektor-Konverter der Atari-Seite — mit
Encoder-Rückweg. Wo eine Spec schweigt, sagt der **Schreiber** eindeutig, wie
das Format aussieht: `write_atx()` legt Feld für Feld fest, was `read_atx()`
dann konsumiert.

## Wofür er schon gebraucht wurde

| Wo | Was |
|---|---|
| `src/formats/atx/uft_atx.c` | Spuraufzeichnungen, Chunk-Layout, FDC-Status-Semantik, Weak-Chunks — MF-467. Der Leser las vorher ein Layout, das es nicht gibt, und lieferte für **jede** ATX-Datei null Sektoren |
| `docs/spec_verification.json` → `atx` | der Beleg dazu |

## Was die Lizenz erlaubt

GPL-2.0-or-later. UFT steht selbst unter GPL-2, damit ist ein Port
verträglich — aber er ist **kennzeichnungspflichtig**:

```c
/*
 * <Zweck>
 *
 * Adapted from a8rawconv by Avery Lee (2014-2023), GPL-2-or-later.
 * Original: https://github.com/Axel051171/a8rawconv-0.95
 */
```

GPL-2 §2a verlangt zusätzlich, dass Änderungen gegenüber dem Original
dokumentiert werden; eine Commit-Zeile `Adapted from a8rawconv: <Datei> —
<Änderung>` genügt dafür.

Als **Orakel** — lesen, vergleichen, zitieren — entsteht keine Ableitung: eine
unabhängig geschriebene Implementierung gegen eine andere zu prüfen ist keine
Übernahme. Genau so wird der Bestand hier benutzt.

> **Zur Lizenzlage des Forks (2026-08-22):** Der Fork trug eine
> `LICENSE`-Datei mit CC0-1.0, während sein README und 17 Quelldateien
> ausdrücklich GPL-2-or-later nennen. CC0 ist eine Rechteverzichtserklärung
> und für fremden GPL-Code nicht abgebbar. Der Widerspruch ist behoben
> (Commit `5db54b4`): CC0 entfernt, `COPYING` mit dem GPL-2.0-Text angelegt,
> wie es das README beschreibt. GitHub meldet den Fork seither als GPL-2.0.

## Warum es nicht gelöscht werden darf

Ein Aufräum-Durchgang, der nach „wird nicht gebaut" filtert, hält diesen
Ordner für tot. Er ist es nicht — er ist die Beweisgrundlage für die
ATX-Entscheidungen und die nächste für FM/Interleave. Dieselbe Warnung steht
in `src/samdisk/README.md`, und sie hat dort einen Grund: in MF-271 wurden
zwei Testdateien gelöscht, weil sie das Wort „switch" im Namen trugen.

## Was noch offensteht

- **TA5** (`docs/A8RAWCONV_INTEGRATION_TODO.md`): Vergleich unseres
  FM/MFM-Sektorparsers gegen `sectorparser.cpp` (615 Zeilen, WD177x-genau).
- **Testvektor-Generator:** `rawdiskscript.cpp` ist eine DSL, die erzeugten
  Flux-Muster mit *bekannten* Defekten beschreibt. Nicht portieren — extern
  aufrufen und die Ausgabe als Referenzmaterial ablegen. Siehe den Nachtrag
  im TODO.
- **Interleave/Precompensation** (`interleave.cpp`, `compensation.cpp`) sind
  in UFT bereits eigenständig umgesetzt (TA1/TA2); der Bestand bleibt hier als
  Gegenprobe.
