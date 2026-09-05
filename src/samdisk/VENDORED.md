# Vendored: SAMdisk (Auszüge)

- **Upstream:** https://github.com/simonowen/samdisk (Simon Owen)
- **Lizenz:** **MIT** — Volltext in [`License.txt`](License.txt),
  © 2002–2020 Simon Owen laut der **mitgelieferten** Datei.
  ([`README.md`](README.md) nennt für Upstream © 2002–2024; für *unsere*
  Kopie gilt der Vermerk, der hier liegt.)
  MIT verlangt, dass Copyright-Vermerk **und** Lizenztext „in all copies
  or substantial portions" mitgeführt werden — `License.txt` ist damit
  Pflicht, nicht Höflichkeit.
- **Stand:** **SAMdisk 4.0 ALPHA** (siehe unten, „Der Stand — was der Baum
  schon wusste und was gemessen ist")
- **Zweck in UFT:** **Referenz-Implementierung**, kein Produktions-Code.
  Diese Dateien werden vom Primär-Build (qmake) NICHT kompiliert. Sie dienen
  als autoritative Byte-Spec-Quelle bei Format-Verifikationen — z. B. wurde
  `fdi.cpp::ReadFDI` als Ground-Truth für die ZX-Spectrum-FDI-Neuimplemen-
  tierung genutzt (MF-359, docs/KNOWN_ISSUES.md FMT-12).
- **Regel:** pristine lassen. Keine UFT-Anpassungen an den **Upstream**-
  Dateien; Fakten (Strukturen, Offsets) extrahieren statt Code zu kopieren.
  (`config.h` ist **nicht** upstream — sie trägt `PACKAGE_NAME
  "UnifiedFloppyTool"` und ist hier geschrieben worden.)
- **Bestand, gemessen am 2026-09-05** mit `git ls-files src/samdisk`:
  **147 Dateien** — 100 `.cpp`, 44 `.h`, 3 Textdateien
  (`README.md`, `VENDORED.md`, `License.txt`).

---

## Der Stand — was der Baum schon wusste und was gemessen ist (MF-899)

Hier stand bis MF-899:

> **Version/Commit:** nicht dokumentiert — vor v4.1.0 vendored (AUD-7,
> MF-369: Herkunfts-Manifest nachgereicht; exakter Stand unbekannt)

Das war seit MF-458 **überholt, ohne dass es jemand nachgezogen hat** —
und die Antwort lag im **Nachbarordner derselben Verzeichnisebene**:
[`README.md`](README.md) nennt seither „SAMdisk 4.0 ALPHA, © 2002–2024
Simon Owen" und die Lizenz MIT mit Volltext.

Damit trug dieses Verzeichnis **drei einander widersprechende Aussagen**
über denselben Gegenstand:

| Datei | sagte |
|---|---|
| `README.md` | SAMdisk **4.0 ALPHA** |
| `config.h:13` | `#define PACKAGE_VERSION "3.7.3"` |
| `VENDORED.md` (hier) | „exakter Stand **unbekannt**" |

`config.h` ist dabei **keine Upstream-Datei** — sie definiert
`PACKAGE_NAME "UnifiedFloppyTool"` und trägt den Doc-Kommentar
„SAMdisk configuration header for UnifiedFloppyTool". Ihre 3.7.3 hatte im
ganzen Baum **keinen Konsumenten** (gemessen über `git ls-files`: die
einzige Fundstelle ist ihre eigene Definition) und ist seit MF-899
berichtigt.

Für **4.0 ALPHA** spricht außerdem der Bestand selbst: die angegebene
Upstream-URL führt auf den GitHub-Master, und der ist die 4.0-Linie;
3.7.x war die letzte Windows-Release davor. Im vendorten Code stehen
`constexpr std::string_view` (C++17) und `Encoding::Agat`
(`cmd_view.cpp:231`) — beides 4.0-Merkmale.

### Wie eng lässt sich der Stand eingrenzen?

Der Bericht **UFT-18** (2026-09-05) hat alle 100 vendorten `.cpp` gegen
den damaligen `HEAD` von `simonowen/samdisk` verglichen und meldet
**92 byteidentisch**, 5 reine Kommentar-Tippfehler und 3 mit
Funktionsänderung — **keine** davon in den Kern-Formatparsern, aus denen
dieser Baum zitiert (`fdi.cpp` gehört zu den identischen).

> ⚠ **Diese Zahl ist eine Fremdaussage und hier NICHT nachgerechnet.**
> Sie setzt einen Klon des Upstream-Repositoriums voraus, den dieser Baum
> nicht hat. Was sie zur eigenen Messung machen würde: `simonowen/samdisk`
> klonen, den Commit notieren, `git ls-files 'src/samdisk/*.cpp'` gegen
> die Upstream-Pfade hashen, und den Commit **hier** eintragen.

**Sechs der acht benannten Abweichungen sind dagegen im eigenen Baum
nachprüfbar — und alle sechs bestätigen die Richtung** (gemessen
2026-09-05):

| Beleg | erwartet, wenn UFT **vor** jener Runde vendorte | gemessen |
|---|---|---|
| `BlockDevice.cpp` | trägt noch „removeable" | ✓ 1 Treffer |
| `MemFile.cpp` | trägt noch „Rememeber" | ✓ 1 Treffer |
| `cmd_dir.cpp` | trägt noch „informations" | ✓ 1 Treffer |
| `SAMdisk.cpp` | kennt `"trinity"`/`"lba"` noch nicht | ✓ 0 Treffer |
| `cmd_format.cpp` | kennt `opt.lba` noch nicht | ✓ 0 Treffer |
| `cmd_view.cpp` | kennt `ViewTrack_Agat` noch nicht | ✓ 0 Treffer |

Nicht nachprüfbar blieben die zwei unbenannten Kommentarkorrekturen
(`cmd_list.cpp`, `FluxTrackBuilder.cpp`) — der Bericht nennt ihren
Wortlaut nicht.

**Fazit, so genau wie belegbar:** der vendorte Stand ist ein Auszug aus
der 4.0-ALPHA-Linie, genommen **vor** der oben belegten Upstream-Runde
(Tippfehler-Bereinigung + `--lba`/`--trinity` + `ViewTrack_Agat`). Ein
Commit-Hash fehlt weiter, und das steht hier als offener Punkt statt als
Vermutung.
