<!-- stufe: 3 — Tiefenpruefung durchgefuehrt (MF-927), Befund korrigiert -->
# Gutachten-Entwurf: superdiskindex
Stand: 2026-08-26 · Messung: `superdiskindex.messung.json`
(HEAD `52463ced6d`, letzter Commit 2020-05-07)
· Inventar: UFT `3f80164`

## Messwerte
- Dateien: 59, Sprachen: [['.cc', 23], ['.h', 22], ['.md', 4], ['.deb', 2], ['.html', 1]]
- Letzte Commit-Botschaft: version bump
- Domänen-Score: 17 (Schwelle 3)

## Lizenz (aus Dateien, nicht README)
  - `LICENSE` → MIT (GRUEN)
- **Zone: GRUEN** → Konsequenz: Code portierbar mit Attribution (samdisk-Muster)

## Inventar-Abgleich (Abfrage, kein Urteil)
- `adf` → vorhanden: adf, adf,adl, adf-copy, adf_adl, adf_arc (Tier T1b)
- `crc` → vorhanden: uft_air_crc32, uft_crc
- `d64` → vorhanden: d64 (Tier T1b)
- `decode` → vorhanden: dec
- `filesystem` → vorhanden: st
- `floppy` → vorhanden: flashfloppy
- `flux` → vorhanden: flux, flux_profiles, fluxdec, fluxengine, kryoflux
- `gcr` → vorhanden: uft_c64_gcr
- `greaseweazle` → vorhanden: greaseweazle
- `mfm` → vorhanden: dsk_mfm, mfm, mfm_native, uft_mfm
- `revolution` → vorhanden: ti
- `scp` → vorhanden: scp, uft_scp, uft_scp_writer (Tier T1b)
- `sector` → vorhanden: hardsector, uft_hardsector

## Domänen-Fundstellen
  - ADF: FormatDiskAmiga.cc, NOTES.md, README.md
  - CRC: CRC.cc, CRC.h, DiskMap.cc
  - D64: FormatDiskC64_1541.cc, VirtualDisk.cc, VirtualDisk.h
  - GCR: Buffer.cc, Buffer.h, FluxData.cc
  - MFM: Buffer.cc, Buffer.h, FluxData.cc
  - SCP: FluxData.cc, NOTES.md, README.md
  - bitcell: FluxData.cc, legacy/main2.cc
  - checksum: NOTES.md
  - decode: Buffer.cc, Buffer.h, FormatDiskAmiga.cc
  - directory: DiskMap.cc, DiskMap.h, FormatDiskAmiga.cc
  - filesystem: DiskMap.cc, NOTES.md
  - floppy: FormatDiskC64_1541.cc
  - flux: CHANGELOG, Config.cc, Config.h
  - greaseweazle: README.md, legacy/main2.cc
  - revolution: Config.h, FluxData.cc, FluxData.h
  - sector: CHANGELOG, DiskLayout.cc, DiskLayout.h
  - track: BitStream.cc, BitStream.h, Config.cc

## UNGEKLÄRT — von der Tiefenprüfung zu füllen, NIE zu raten
- [ ] Kategorie: Innovation / Verbesserung / Daten / Oracle / irrelevant
- [ ] Was genau fehlt UFT bzw. was wäre besser? (Datei+Zeile zitieren)
- [ ] Bei "besser": Differenzlauf-Plan (Binaries, Korpus, Metrik, Toleranz)
- [ ] Einhängepunkt in bestehende Pläne (welcher Baustein?)
- [ ] Oracle-Kandidat + Baubarkeit
- [ ] Beschaffungsliste (Fixtures, Referenzdateien, Specs)
- [ ] Aufwandsklasse S/M/L
- [ ] OPEN_ITEMS-Vorschlagstext (max. 3 Sätze, mit Messquelle)

## Regeln, die für diesen Fund gelten
- Zone GRUEN: Code portierbar mit Attribution (samdisk-Muster)
- Kein Code aus diesem Agenten (AGENT.md Regel 1)


---

## Tiefenprüfung (MF-927) — und der Befund war anders als erwartet

> Anlass: Bericht `UFT-29-superdiskindex-Diskmap`. Er hat die Checkliste
> unten ausgefüllt und einen scharfen Befund gemeldet:
> *„UFT hat bereits eine Diskettenkarten-Ansicht (`src/visualdisk.cpp`),
> aber sie zeigt erfundene Werte."*
>
> **Die Fundstelle stimmt. Die Folgerung nicht.**

### Gemessen: der Baum hat DREI Diskettenkarten, nicht eine

| Datei | Zeilen | erreichbar? | erfindet? |
|---|---|---|---|
| `src/visualdiskdialog.cpp` | 778 | **ja** — `toolstab.cpp:817` erzeugt `VisualDiskDialog` | **nein** — seit MF-892 ruft sie `read_track()` (`:517`) |
| `src/visualdisk.cpp` | 197 | **nein** — `VisualDiskWindow` wird **nirgends** erzeugt; `m_visualDiskWindow` in `mainwindow.cpp` ist `nullptr` und wird nur `delete`d | **ja** — `:137-141` „simulate a few bad ones", `t==15 && s==3`; `:181` dasselbe in der Gitteransicht |
| `src/widgets/diskvisualizationwindow.cpp` | 416 | **nein** — kein Nenner außerhalb der eigenen Datei | — |

**Was der Bericht nicht wissen konnte:** die *erreichbare* Karte wurde
bereits repariert (MF-892, bewacht von
`tests/test_visual_disk_no_fiction.cpp`, das den BAM-Sektor der
Korpusdiskette gegen die Anzeige hält). Die Erfindung, die er gefunden
hat, sitzt in einer **toten** Zwillingsdatei.

Das ist dieselbe Form wie P3-196 (zwei G64-Leser, nur einer richtig):
**eine Reparatur landet auf einem von mehreren Zwillingen, und der
Befund überlebt im anderen.** Der Bericht hat den falschen Zwilling
gemessen — aber ohne ihn wäre der zweite nicht aufgefallen.

### Was trotzdem trägt

Das `DiskMap`-Muster selbst ist gut und unabhängig von der obigen
Korrektur: **ein Flag-Wort je Adresseinheit mit zwei getrennt
maskierten Dimensionen** — Gesundheit (`DMF_HEALTH_MASK`) und
Inhaltsart (`DMF_CONTENT_MASK`). Damit kann ein Sektor gleichzeitig
„Verzeichnis" **und** „CRC schlecht" sein, ohne dass eine Aussage die
andere überschreibt. UFT führt beides heute getrennt und nirgends
zusammen.

Lizenz **MIT** — Zone GRÜN. Ein Port wäre erlaubt; nötig ist er nicht,
das Muster sind zwölf Zeilen Aufzählung.

### Checkliste, gefüllt

| Feld | Inhalt |
|---|---|
| Kategorie | **Verbesserung** (Datenmodell), nicht fehlende Fähigkeit |
| Einhängepunkt | `src/visualdiskdialog.cpp` — die **erreichbare** Karte, nicht `visualdisk.cpp` |
| Oracle-Kandidat | keiner nötig — UI-Datenquelle, kein Formatparser |
| Beschaffung | keine |
| Aufwandsklasse | **M**, und zwar unverändert: das Modell einzuführen ist billig, es an **jedem** Formatparser echt zu befüllen ist die Arbeit |
| Kennzahl | keine der vier — Fundus nach Regel 9 |
