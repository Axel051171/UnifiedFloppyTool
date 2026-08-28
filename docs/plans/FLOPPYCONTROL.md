# Umsetzungsplan FloppyControl — Baum-Fassung

> **Verdaut, nicht kopiert.** Diese Fassung trägt nur die **offenen**
> Bausteine. Erledigtes steht als MF-Verweis dabei, damit niemand es
> zweimal baut. Nach MF-641 Nachtrag 3.

**Quelle der Anregung:** [imqqmi/FloppyControl](https://github.com/imqqmi/FloppyControl)
(HEAD `0633bc7`, 2026-01-26, **GPL-3.0**). Zone GELB: **kein Code
übernehmbar** — Verhaltens-Spec und Oracle-Nutzung sind das Maximum.
Gutachten: `tools/uft-scout/out/FloppyControl.gutachten.md`.

## Bereits geliefert — nicht erneut einplanen

| Baustein | Stand |
|---|---|
| **A — Zellendauer aus dem Histogramm** | ✅ MF-488. Ein MFM-Strom mit Zellendauer T trägt 2T/3T/4T; die drei Berge stehen im Verhältnis 1 : 1,5 : 2, der erste bei 2T. Damit ist T aus den Daten allein lesbar. War **zweimal gebaut und zweimal tot** — siehe `KNOWN_ISSUES.md` FLUX-12 |
| **§2 Fundus** (append-only-Archiv) | ✅ MF-503/504/505/506/561 |

## Offene Bausteine

### FC-1 — Fluss-Scatterplot als Ansicht *(P2)*

**Kennzahl:** keine der vier direkt — er ist **Vorbedingung** für die
Fensterwahl des CRC-Orakels und damit für „ungeprüfte Formate runter"
auf dem Rettungsweg. Als Einzelfund wäre er Fundus; als Bedienelement
der Rettungskette ist er Auftrag.

**Stand nach MF-630/632: zur Hälfte erledigt, anders als gedacht.**
Die ursprüngliche Begründung („UFT hat keine Fluss-Visualisierung") war
**falsch**. Gemessen: `src/widgets/fluxvisualizerwidget.cpp` (1092
Zeilen) führt fünf Modi — `WAVEFORM`, `HISTOGRAM`, `SPECTROGRAM`,
`CELL_VIEW`, `COMPARISON` — und wurde nur **nirgends instanziiert**.

* ✅ verdrahtet (MF-632): hängt im Splitter unter dem OTDR-Panel,
  gespeist über `UftOtdrPanel::trackFlux()`
* ✅ zum ersten Mal ausgeführt (MF-631): alle fünf Ansichten gezeichnet
* **offen:** ob `WAVEFORM` um Periodendarstellung mit µs-Farbbändern und
  Index-Marken ergänzt werden muss — **erst nach der Klick-Abnahme
  entscheiden**, nicht vorher
* **offen:** kopfloser Qt-Test der *Verbindung* (das OTDR-Panel hängt in
  keiner Testbahn; es dort aufzunehmen zieht floppy_otdr, SCP-Parser,
  Anomaly/ML und Provenance nach)

### FC-2 — `dskx` als FAT12-Oracle für **gelöschte** Einträge *(P3)*

**Kennzahl:** ungeprüfte Formate runter (FAT12-Inhaltsebene).

Eng geschnitten, und der Zuschnitt ist gemessen: floptool liest normalen
FAT12-Inhalt **vollständig** (MF-629, gegen ein selbstgebautes 720K-Abbild
geprüft). Was `dskx` einzig hinzufügt, sind **gelöschte
Verzeichniseinträge** (`--deleted-only`) und Bad-Cluster `0xFF7`.

**Reihenfolge bindend:** .NET-7-SDK → `dskx` bauen → gegen **dasselbe**
720K-Fixture laufen lassen → **erst dann** Registry-Eintrag. Ein Oracle
auf Zusicherung wird nicht eingetragen.

**Zeitpunkt:** mit dem FAT-VFS-Baustein, nicht vorher — allein hat es
keinen Abnehmer.

### FC-3 — Analog-Oszilloskop-Rettungspfad, Verhaltens-Spec *(P3)*

**Kennzahl:** keine der vier — Fundus, bis der Analog-Ingest ein
Vorhaben ist.

Glättung → Differenzierung → adaptive Schwellen → Nulldurchgänge →
Perioden → Injektion in den normalen Decode-Puffer. UFT hat **keinerlei**
Analog-Ingest.

**Fixture abgelehnt** (Eigentümer-Entscheid MF-630): 18 MB mit
ungeklärtem Urheberrecht sind für **Daten** dieselbe ROT-Zone wie für
Code. Die Referenzaufnahme wird **selbst** erzeugt, wenn der Baustein
startet — eigenes Scope, eigene Diskette, Provenienz im README, und
nebenbei die erste echte Messung für die offene FLUX-15-Winkelfrage.

## Ausdrücklich im Fundus, nicht eingeplant

| | Grund |
|---|---|
| DiskSpare-Spec, „2M"-Format | Einfrier-Regel — neue Formate erst nach der 1:2-Bedingung |
| AddNoise-Dither-Mining | **erzeugt Daten.** Forensik-Vorbehalt, nicht verhandelbar |
| StepStick-Mikroschritte, siebter Controller | Hardware-Folge, MF-310 (kein Gerät im Haus) |
