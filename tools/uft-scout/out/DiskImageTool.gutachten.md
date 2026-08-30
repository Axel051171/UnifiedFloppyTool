# Gutachten: Digitoxin1/DiskImageTool

> Gemessen 2026-08-30 gegen HEAD `0b0d6883c9` (2026-08-28, aktiv).
> Messdatei: `tools/uft-scout/work/DiskImageTool.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692), Andockstellen **FMT-15**, **86f (T3)**,
> Prüfauftrag **FS-2**.
>
> **Neubesuch-Anlass (Regel 6):** `data/known_negatives.json` führt das
> Repo als `bewertet` („Gutachten: HOST_OS_CONTAMINATION u.a."); jener
> Befund lebt heute als Mammut-Baustein 3.3 weiter
> (`docs/MAMMUT_PLAN.md:174`), ein eigenes Gutachten unter `out/` gab
> es aber **nie** — `scripts/scout_stand.py` hielt es fälschlich für
> begutachtet, weil der Name in fremden Gutachten vorkommt (im
> fluxfox-Gutachten als Verweis; in `cbm_erzeuger.gutachten.md` sind
> die Treffer sogar ein anderes Repo, `FloppyDiskImageTool` von
> markusC64). Die neuen Fragen: FMT-15 existiert erst seit MF-691
> (2026-08-28), der 86f-Abgleich und der FS-2-Prüfauftrag kommen aus
> Block 4. Dieses Gutachten schließt zugleich die Zähllücke.

## Kategorie

**Verbesserung (zweite Verhaltens-Referenz für FMT-15 und 86f).**
Der FS-2-Prüfauftrag wird **negativ** beantwortet, mit Messung.

## 1. Was es ist

VB.NET-WinForms-Werkzeug (439 Dateien, 328 `.vb`) für IBM-PC-Disk-
Images: FAT12-Editor, Bootsektor-Editor, Hex-Editor, Konvertierung,
Flux über Greaseweazle/KryoFlux (README:1-16). Bitstream-Schicht
(`DiskImageTool/Bitstream/IBM_MFM/`), Formatleser unter
`DiskImageTool/ImageFormats/`: **86F, HFE, IMD, MFM, PRI, PSI, TC,
TD0** (Verzeichnisliste).

## 2. Fund A — FMT-15 Schließweg 1: die zweite unabhängige BPB-Prüfung

FMT-15 (`docs/OPEN_ITEMS.md:4081` ff., MF-691) verlangt für IMG
„Bootsektor-Plausibilität … eine eigene, belegbare Prüfung". Hier
liegt sie: `DiskImageTool/DiskImage/BiosParameterBlock.vb:267-281`
`IsValid()` — **zwölf Einzelbedingungen** (elf `AndAlso`, gezählt im
Quelltext): Pufferlänge ≥ BPB_SIZE, BytesPerSector, SectorsPerCluster,
ReservedSectorCount, NumberOfFATs, RootEntryCount ∈ (0, 512],
SectorCount > 0, MediaDescriptor, SectorsPerFAT, SectorsPerTrack,
NumberOfHeads. Zusammen mit fluxfox' `BiosParameterBlock2::is_valid()`
(6 Bereichsprüfungen, eigenes Gutachten) sind das **zwei unabhängige
Implementierungen** derselben Plausibilitätsidee — die Zwei-Quellen-
Lage, die eine Stufe-4-Spec für FMT-15 braucht. GPL-3.0: kein Port;
die Wertebereiche sind als Fakten frei, gehören aber als **eigene
Erhebung gegen die FAT-Spezifikation** in die Spec, nicht als Kopie
der Sammlung (Matrix-Sonderfall „Fakten/Parameter").

Dazu passend, als Warnbestand: `DiskImageTool/Resources/bootsector.xml`
und `bootstrap.xml` (60 251 B) — kuratierte Bootsektor-/OEM-Kataloge.
**Datenbank, sui generis → PRÜFEN**; falls je gewollt, nur per
Laufzeit-Parser oder eigener Erhebung (diskdefs-Muster).

## 3. Fund B — 86f: zweite Verhaltens-Referenz für ein T3-Format

Inventar-Abfrage zitiert: `"86f": vorhanden: true, tier: "T3",
plugin_liste_vollstaendig: true`. DiskImageTool bringt einen
eigenständigen 86F-Leser: `DiskImageTool/ImageFormats/86F/` — 6
Dateien, **871 Zeilen** (`wc -l`, `86FImage.vb`, `86FTrack.vb`,
`86FLoader.vb`, `86FFloppyImage.vb`, `Enums.vb`, `Functions.vb`).
Neben fluxfox' `86f`-Parser (MIT) ist das die zweite unabhängige
Implementierung; für die 86f-Hebung dient sie als
Verhaltens-Quervergleich (GPL-3.0 → nur lesen und beschreiben, nichts
portieren). Der tragende Differenzlauf bleibt der aus dem
fluxfox-Gutachten (fftool als Oracle).

## 4. Fund C — FS-2: trägt nichts bei, gemessen

Der Prüfauftrag lautete: beitragen zu FS-2 (ADF/AmigaDOS)? **Nein.**
Messung: `grep -rin amiga DiskImageTool --include=*.vb -l` liefert nur
drei Treffer, alle Enum-/Namenslisten fremder Formate
(`Flux/Kryoflux/CommandLineBuilder.vb`, `ImageFormats/HFE/Enums.vb`,
`ImageFormats/TC/…`); `grep -n ADF DiskImage/DirectoryEntry.vb`
liefert **0** (der ADF-Treffer der Messdatei war ein
Teilwort-Artefakt). Es gibt kein Amiga-Dateisystem in diesem Repo.

## 5. Lizenz

`LICENSE` = **GPL-3.0** (aus der Datei; zweitbestätigt
`DiskImageTool/Resources/License.txt`, Messung). Zone **GELB**:
Verhaltens-Spec und Oracle zulässig, kein Port. **Attribution:**
DiskImageTool (GPL-3.0, Digitoxin1). Kein Code übernommen, keiner
übernehmbar.

## 6. Bewegte Kennzahl

* Fund B: **ungeprüfte Formate (T3) ↓** — als Zweitreferenz im
  86f-Hebungsweg (Methode: Formatname `86F` in
  `ImageFormats/`-Verzeichnisliste gegen die T3-Zeile `86f` der
  Inventar-Abfrage; exakter Namenstreffer — und wie AGENT.md Regel 2
  mahnt: Namensgleichheit ist noch kein Fähigkeitsbeweis, der
  Differenzlauf entscheidet).
* Fund A: FMT-15 führt selbst „Kennzahl: keine" — Zulieferung, kein
  eigener Auftrag.

## 7. Einhängepunkte (im Baum auffindbar)

* `docs/OPEN_ITEMS.md` § FMT-15 (MF-691), Schließweg 1.
* `docs/VERIFICATION_TIERS.md`, Zeile `86f`.
* `docs/MAMMUT_PLAN.md:174` (Baustein 3.3 — der Altbefund dieses
  Repos, hier nur verknüpft, nicht neu bewertet).

## 8. Oracle-Kandidat

Schwach: WinForms-GUI; eine CLI wurde nicht gefunden (Suche nach
`Sub Main`/`CommandLine` in `DiskImageTool/*.vb`: nur
`My.Application`-GUI-Treffer, `MainForm.vb:688,1013`). Als Oracle nur
mit dokumentiertem Klick-Protokoll brauchbar — teuer und schlecht
reproduzierbar. Nicht vorgeschlagen; fftool (fluxfox) ist der bessere
86f-Kandidat.

## 9. Beschaffungsliste

Für die 86f-Hebung (identisch mit fluxfox-Gutachten §8, nicht
doppelt): mindestens ein fremd erzeugtes 86F-Abbild (86Box-Export);
liegt laut `inv["korpus"]` (24 Einträge) nicht.

## 10. Aufwandsklasse

**S** je Zulieferung (FMT-15-Spec-Referenz; 86f-Quervergleichsliste).

## UNGEKLÄRT

* Ob `IsValid()` in DiskImageTool selbst am Öffnungspfad hängt oder
  nur im Editor (Aufrufer nicht verfolgt — für die Referenz-Nutzung
  unerheblich, für Verhaltens-Erwartungen nicht).
* Ob die `MFM`-Leser-Implementierung (`ImageFormats/MFM/`) das
  HxC-MFM ist, das unser `mfm_native` meint — nicht verglichen.
* Der Umfang der FloppyDB (`Modules/FloppyDB/`, 5 Dateien,
  `FloppyData.vb` 408 Zeilen) und ihre Datenherkunft — als Datenbank
  ohnehin PRÜFEN, nicht weiter verfolgt.
