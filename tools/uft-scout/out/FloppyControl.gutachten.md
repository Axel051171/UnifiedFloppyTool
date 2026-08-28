# Gutachten: imqqmi/FloppyControl (Wiedervorlage, Zyklus 6)
Stand: 2026-08-28 · Messung: `work/FloppyControl.messung.json`
(HEAD `0633bc7ad5`, letzter Commit 2026-01-26)
· Inventar: UFT `7d6a595e` (`work/inv.json`, 88 Plugins, SSOT ok,
22 Korpus-Abbilder) · vom Eigentümer benannt.

## Regel-6-Lage (Negativliste) — zuerst

`data/known_negatives.json:108`: `imqqmi/FloppyControl`, Status
**`bewertet`**, Grund „Umsetzungsplan liegt vor (CRC-Orakel, Fundus),
GPL-3.0" (Mammut-Evaluierung 2026-08-23).

**Gemessen: 0 Commits seit der Bewertung.** Methode: `git log --oneline
--since=2026-08-23 | wc -l` im Klon → 0; HEAD `0633bc7` vom 2026-01-26;
240 Commits gesamt. Es gibt also **keine** wesentliche Änderung im Sinne
von AGENT.md Regel 6.

**Warum dieses Gutachten trotzdem existiert:** direkter Auftrag des
Eigentümers (2026-08-28), und der Alt-Umsetzungsplan liegt **nicht im
Baum** — sein Umfang ist nur fragmentarisch belegt. Nachweisbar aus dem
UFT-Baum gehörte dazu:

| Alt-Plan-Baustein | Beleg | Stand |
|---|---|---|
| Baustein A: Zellendauer aus Histogramm | `src/flux/uft_flux_histogram.c`, KNOWN_ISSUES FLUX-12 | gelandet (MF-488) |
| §2 Fundus (append-only-Archiv) | Commits MF-503/504/505/506/561 | gelandet |
| Multi-Capture-Overlay | `include/uft/forensic/uft_fundus.h:258` („notierter Baustein") | notiert, MAMMUT §2.3 |
| Mining-Schleife | `uft_fundus.h:260`, MAMMUT §2.4 | notiert, blockiert (kein Gerät, MF-310) |
| „CRC-Orakel" | nur der Negativlisten-Eintrag | **Bedeutung UNGEKLÄRT** |

Konsequenz: die drei Vorschläge unten betreffen ausschließlich Bereiche,
die in **keinem** Baum-Dokument als abgedeckt nachweisbar sind, und jeder
trägt den Regel-6-Hinweis. Ob der (externe) Alt-Plan sie bereits geprüft
und verworfen hat, ist UNGEKLÄRT — die Übernahme ist wie immer
Eigentümer-Entscheid am menschlichen Tor.

## Messwerte

- 865 Dateien; .cs=129, .js=76, .css=67, .png=49, .pdf=45 (die .js/.css
  stammen fast vollständig aus als Webseite gespeicherten Doku-Ordnern
  `*_files/` — siehe Werkzeug-Befund unten)
- C#-Kernanwendung (WinForms, `FloppyControlApp/`), .NET-7-Port
  (`FloppyControlApp.net7/`), PIC16-Firmware (`PICversion/`),
  Arduino-Due-Firmware+Shield (`ArduinoDueVersion/`)
- Hauptmodule (Zeilen, `wc -l`, ohne Designer/obj): FormHelper 2612,
  Graphics 2487, FloppyControlApp 1372, FDDProcessingPC 1316,
  FDDCapture 1284, FileIO 1265, FDDProcessing 1138, WaveformEdit 969,
  FloppyControlErrorCorrection 880, FDDProcessingAmiga 861,
  FDDProcessingAmigaEC 851
- Formate (eigene Aufzählung `MyClasses/Helpers.cs:28`): amigados,
  diskspare, pcdd, pchd, pc2m, pcssdd, diskspare984KB, pc360kb525in
- Import: eigene `.bin`-Periodenströme, SCP, KryoFlux-raw
  (`README.md`, Abschnitt „File formats that can be imported")

## Lizenz (aus Dateien, je Unterverzeichnis — Handprüfung)

| Fundort | Kennung | Zone |
|---|---|---|
| `LICENSE` (Wurzel) | GPL-3.0 | **GELB** |
| `ArduinoDueVersion/CaptureTimer/tc_lib-master/COPYING` | GPL-3.0 (vendorte Arduino-Timer-Lib) | GELB |
| 7× `notice`/`notice_002` in `*_files/`-Ordnern | UNGEKLÄRT | PRÜFEN |
| `FloppyControlApp.net7/.../FAT12Extractor/` | keine eigene Lizenzdatei → Wurzel-GPL-3.0 | GELB |

Die 7 PRÜFEN-Treffer sind **keine vendorten Bibliotheken**, sondern
Bestandteile von im Browser gespeicherten Webseiten (Foren-/Blog-Seiten
samt Assets in `*_files/`-Ordnern). Für die Frage „was dürfen wir mit dem
Code" sind sie ohne Belang; für die Frage „was dürfen wir aus `Docs/`
weiterverbreiten" gilt: nichts davon übernehmen (teils PDF-Scans
kommerzieller Bücher, z. B. `Docs/Amiga_System_Programmers_Guide_1988_
Abacus.pdf` — nur lokal als Lektüre).

**Zonen-Urteil für den Code: GELB (GPL-3.0).** Konsequenz nach
`playbook/lizenzmatrix.md`: **kein Code portierbar** in dieses
GPL-2.0-Projekt; erlaubt sind Verhaltens-Spec und Oracle-Nutzung der
Ausgabe. Randnotiz für den Eigentümer: `docs/MAMMUT_PLAN.md` §4 stellt
fest, dass `LICENSE:35` des UFT-Baums GPL-2.0-**or-later** sagt; ein
Wechsel nach GPLv3 würde die Zone ändern — das ist eine
Eigentümer-Entscheidung, keine Scout-Auslegung, und die Matrix bleibt
bis dahin bindend.

## Funde

### F1 — `dskx`: FAT12-Inhalts-CLI als Oracle-Kandidat (Kategorie: Oracle)

Neu seit 2026-01 (Commits `fffa725`/`fd1316b`, „Phase 1"):
`FloppyControlApp.net7/WindowsFormsApplication2/FAT12Extractor/` enthält
neben der GUI eine **Konsolen-CLI**:

- `src/DskX.Cli/Program.cs:7-33` — Kommandos `list`/`extract`,
  `--version`, Fehler auf stderr, Exit-Codes 0/1
- `Program.cs:128-132` — `--version` → `"DskX v1.0.0"` (Versionsabfrage
  vorhanden; **aus der Quelle zitiert, nicht aus Prozessausgabe** — in
  diesem Zyklus nicht gebaut, siehe UNGEKLÄRT)
- `src/DskX.Cli/DskX.Cli.csproj` — `OutputType Exe`, `net7.0`,
  AssemblyName `dskx` (Konsolen-Binary, plattformneutral baubar)
- `src/DskX.Core/` — 2100 Zeilen (wc -l über 17 Dateien): FAT12-Boot-
  Sektor, FAT-Tabelle, Verzeichnis-Parser, Extraktion; führt
  **gelöschte Einträge** (`list --deleted`) und **Bad-Cluster 0xFF7**
  (`DiskSectorMap.cs:45-47`) getrennt
- `tests/DskX.Core.Tests/` — 63 xUnit-Facts/Theories (grep-Zählung über
  8 Dateien)

**Was es entscheiden kann:** den Dateisystem-Inhalt eines FAT12-Abbilds
(Dateiliste, Größen, gelöschte Einträge, Bad-Cluster) — exakt die
Behauptung, für die floptool seit MF-623 ausdrücklich **nicht** Referenz
ist („Container wird geprüft, Dateisystem nicht"). PLAN_v4.1.7 Phase 1
(„VFS-P1 lesend — macht T1b inhaltlich statt strukturell") braucht genau
so einen unabhängigen Zweitleser.

**Oracle-Eintrag (Gerüst für `tests/differential/oracles.py`, von
Stufe 4 zu setzen, nicht von diesem Agenten):** name=`dskx`,
env=`UFT_ORACLE_DSKX`, exes=`("dskx",)`, version_args=`("--version",)`,
version_re=`r"DskX v([\d.]+)"`, reference_for=„FAT12-Inhalt eines
IMG/IMA: Dateinamen, Größen, gelöschte Einträge, Bad-Cluster",
origin=`github.com/imqqmi/FloppyControl` (FAT12Extractor),
licence=`GPL-3.0`, code_import=`False`.

**Differenzlauf-Plan** (keine „besser"-Behauptung, sondern
Oracle-Betrieb): beide Seiten lesen dieselben FAT12-Abbilder — UFT-Kern
(`src/fs/` FAT12) gegen `dskx list`; Korpus: cross-tool erzeugte Abbilder
(mtools `mformat`+`mcopy`, Version notieren) plus je ein Abbild mit
gelöschtem Eintrag und markiertem Bad-Cluster; Metrik: Menge
(Name, Größe, Startcluster) je Datei identisch; Toleranzen: 8.3- vs.
LFN-Darstellung, Groß-/Kleinschreibung, Reihenfolge.

Aufwand: **S–M** (SDK-Build + Registry-Eintrag + ein Vergleichstest).

### F2 — Flux-Scatterplot: die fehlende Fluss-Visualisierung (Kategorie: Verbesserung, GUI-Folge)

`MyClasses/Graphics.cs:1942-2160`, Klasse `ScatterPlot`:

- X = Position im Periodenstrom, Y = Intervalldauer (0–255),
  Farbe = Nähe zum 4/6/8-µs-Band über eine Gradiententabelle
  (`:1994-2010`), gezeichnet per LockBits-SetPixel (`:2097`)
- Index-Impulse als senkrechte Linien (`:2098-2099`), nominale Bänder
  als schwarze, aktive Schwellen als rote Horizontalen (`:2101-2108`)
- Zoom/Drag mit Mausrad (`AnScatViewoffset`/`Dragging`), Punkt-Klick
  liefert `RxbufClickIndex`, und mit `EditScatterplot` (`:1967`) lassen
  sich **einzelne Flussintervalle editieren**
- `ShowEntropy` (`:1971`) blendet die Abweichungs-Spur aus
  `ProcTypeAdaptiveEntropy` ein (`ProcessingTypes/
  ProcTypeAdaptiveEntropy.cs:20`, Rückgabe `Entropy`/`Threshold4/6/8`)

**Inventar/Baum-Abgleich:** `flux visualization` → `vorhanden: false`,
nur Teilwort-Treffer `flux` (Abfrage `inventar.py query work/inv.json`,
2026-08-28); Handprüfung: UFT hat `UftFluxHistogramWidget`
(`src/uft_flux_histogram_widget.h:41`), `VisualDiskWindow`
(`src/visualdisk.h`), `DualDiskWidget`/`DiskVisualizationWindow`
(`src/widgets/diskvisualizationwindow.h:63/95`) — **alles Sektor-/
Spur-Ebene bzw. Histogramm-Aggregat, keine Strom-über-Zeit-Darstellung**.
`grep -rn scatter src/ include/` → 3 Treffer, alle Kommentare. Das ist
derselbe Befund wie der Rotbeweis MF-610: UFT hat keine
Fluss-Visualisierung.

**Einhängepunkt:** die offene Fit-Anzeige MAMMUT_PLAN §2.1.3 (Spanne +
gemessene Zellendauer liegen vor, der Mensch sieht sie nicht) und das
OTDR-Panel (`src/gui/uft_otdr_panel.cpp`). Datenquelle liegt komplett im
Baum (Intervalle, Histogramm-Berge aus MF-488, Dewarp-`warp_span`).

**Konsequenz aus GELB + GUI-Folge:** nur Verhaltens-Spec (was die
Darstellung zeigt, nicht wie der C#-Code zeichnet), und nach AGENT.md
Regel 8 als **Entscheidungsvorlage an den Eigentümer**, weil GUI-Folge.
Die Editier-Funktion (Intervalle von Hand ändern) ist forensisch nur mit
Provenienz-Kette zulässig (DESIGN_PRINCIPLES: keine stille Veränderung)
— die Vorlage muss das benennen. Aufwand: **M**.

### F3 — Analog-Oszilloskop-Rettungspfad (Kategorie: Innovation, Beschaffungs-/Hardware-Folge)

Der einzige mir bekannte offene Baustein dieser Art in einem
Floppy-Werkzeug: wenn die Laufwerkselektronik das Signal nicht mehr
sauber digitalisiert, wird das **analoge Differenzsignal des Lesekopfs**
mit einem Oszilloskop aufgenommen und in Software zurückgewonnen.

- Ingest: `MyClasses/WaveformEdit.cs:51-120` liest `.wvfrm`
  (Header: 1 Byte Kanalzahl + int32 Länge, dann 8-bit-Samples) und
  Rigol-`.wfm`; Netz-Abruf vom Scope über
  `Classes/connectsocket.cs` (Rigol DS1054Z, README „Other features")
- Verarbeitung: `WaveformEdit.cs:334` `Filter` (gleitende Glättung,
  DC-Offset, Differenzierung, adaptive Verstärkung),
  `:636` `Filter2` (Nulldurchgangs-Erkennung → Perioden), `:796`
  `Fix8us` (gezielte Reparatur schwacher 8-µs-Übergänge: gefensterte
  Amplituden-Adaption je 500 Samples, Nulldurchgangs-Vor-/Nachlauf)
- **Injektion in den normalen Decode-Pfad:** `Filter2` schreibt die
  gewonnenen Perioden direkt in den Capture-Puffer
  (`rxbuf[processing.indexrxbuf++]`, `WaveformEdit.cs:712/:744`;
  Umrechnung Sample-Takt→Capture-Tick `:687-689`) — danach läuft die
  unveränderte MFM-Dekodierung
- Handreparatur: der Waveform-Editor erlaubt das Korrigieren einzelner
  Übergänge vor der Wandlung (README, `ZeroCrossingData`
  `WaveformEdit.cs:12-19`)

**Baum-Abgleich (Handprüfung):** `grep -rn -i
"oszilloskop|oscilloscope|wvfrm"` über `src/ include/ tests/ docs/` → 0
Treffer im Produktcode; die einzigen Zero-Crossing-Treffer sind
Kassetten-Audio (`src/formats/bbc/uft_bbc_tape.c:237`,
`src/formats/tzx/uft_tzx_wav.c:277`). UFT hat **keinen** Analog-Ingest.

**Fixture liegt bei:** `ExampleWaveforms/A001 Modules1_T005_000.wvfrm`,
18 000 005 Bytes (= 5-Byte-Header + 18·10⁶ Samples, Spur 5 einer realen
Amiga-Diskette des Autors). Damit wäre eine Verhaltens-Spec samt
Referenzlauf **vollständig offline** prüfbar — Hardware (Scope +
modifiziertes Laufwerk) erst für *neue* Aufnahmen nötig; in-house-Bench
ist ohnehin ausgeschlossen (MF-310, kein Gerät).

**Konsequenz aus GELB:** Verhaltens-Spec (Filterkette als Mathematik,
nicht als Code) + Fixture als Testdatum. Wegen Hardware-Folge und
ungeklärtem Inhalts-Urheberrecht des Fixtures (Aufnahme einer fremden
Diskette): **Eigentümer-Vorlage**. Aufwand: **L**.

### Fundus (bewertet, KEINE Vorschläge in diesem Zyklus)

| Fund | Beleg | Warum kein Vorschlag |
|---|---|---|
| DiskSpare-Format (2 Varianten, 984 KB) mit vollständiger MFM-Sektorstruktur | `Docs/DiskSpareDiskFormat.txt`; Decoder `FDDProcessingAmiga.cs`; UFT kennt DiskSpare nur als aufruferloses Protection-Enum (`src/protection/uft_amiga_protection_full.c:211`) | neues Format = EINFRIER-Moratorium (erst NFD-r0, dann 1:2); Spec-Fundort ist notiert |
| „2M"-DOS-Format (`pc2m`, `Helpers.cs:28`) | Inventar-Abfrage `2m` trifft nur `2mg`/`d2m` — Namens-, kein Fähigkeits-Treffer | dito Moratorium |
| AddNoise-Dither-Mining (Zufallsrauschen auf Grenz-Perioden + Neudecodierung bis Prüfsumme passt) | `ProcTypeAdaptiveEntropy.cs:172-180` (`Procsettings.AddNoise`, `rnd.Next`) | kollidiert mit „Keine erfundenen Daten"; UFT zieht die ehrliche Linie bereits enger: Ein-Bit-Korrektur nur bei eindeutigem Fix (`src/recovery/uft_bitstream_recovery.c:97-114`) |
| StepStick-Mikroschritt-Kopfpositionierung (8×, TRK00-Offset in Mikroschritten, „head alignment off"-Ausgleich) | `FDDCapture.cs:54-65`, `FloppyControlApp.Designer.cs:2048/2059`, PIC-Firmware `main.c:333-343` | Hardware-Folge, kein Gerät (MF-310), Hardware selbstgebaut/nicht beschaffbar; als Konzept für M3-Zukunft notiert |
| Recapture-bad-sectors-only / „bad sectors only"-Ströme | README „Other features" | deckungsgleich mit MAMMUT §2.4 Mining-Targets (notiert, blockiert) |
| Histogramm-Klick → automatische Peak-Übernahme in Decoder-Settings | README „The histogram can be clicked" | GUI-Rest von Baustein A; gehört in dieselbe Vorlage wie F2 |
| 7. Controller (PIC16/Arduino Due, 1-Zeichen-Befehle über Serial, 8-bit-Perioden) | `PICversion/.../main.c:258-343`, `FDDCapture.cs` | Selbstbau-Hardware, 8-bit-Quantisierung unter Greaseweazle-Niveau, kein Gerät beschaffbar — als Controller-Kandidat **irrelevant** |

### Oracle-Prüfung FloppyControl selbst (negativ)

Als Decoder-Oracle (SCP→ADF-Vergleich) ungeeignet: beide Anwendungen
sind `WinExe` (WinForms, `OutputType`-Zählung über alle .csproj: 2×
WinExe, 1× Exe = nur dskx, 1× Library), keine Kommandozeilen-Schnittstelle,
Dekodierung nur über GUI-Interaktion. Einzig `dskx` (F1) ist
automatisierbar.

## Beschaffungsliste

Gegen `inv["korpus"]` geprüft (22 Einträge, kein `.wvfrm`, kein
FAT12-`IMG`; Methode: Dateiendungen der Korpusliste gegen Bedarf):

1. **Kein Beschaffungsbedarf für F1-Fixtures aus dem Fremd-Repo:**
   FAT12-Referenzabbilder cross-tool selbst erzeugen (mtools `mformat`,
   Version notieren) — liegt nicht im Korpus, ist aber Stufe-4-Arbeit,
   keine Anfrage an den Eigentümer.
2. **`dskx`-Binary:** aus dem Klon bauen (`dotnet build`, .NET-7-SDK
   erforderlich); kein Fertig-Release im Repo gefunden.
3. **`ExampleWaveforms/A001 Modules1_T005_000.wvfrm`** (18 000 005 B):
   einziges Analog-Fixture; Übernahme in `tests/corpus/` **erst nach
   Eigentümer-Vorlage** (Inhalts-Urheberrecht der aufgenommenen
   Diskette UNGEKLÄRT; GPL-3.0 deckt das Repo, nicht zwingend den
   Diskette­ninhalt).
4. **`Docs/DiskSpareDiskFormat.txt`** und **`Docs/SCPFileFormat.txt`**:
   als Fundort zitieren, nicht kopieren (Zone GELB; Fakten frei, Datei
   nicht).

## UNGEKLÄRT

1. **Voller Umfang des Alt-Umsetzungsplans** (2026-08-23): das Dokument
   liegt nicht im Baum; ob Scatterplot, Analog-Pfad oder dskx dort
   bereits bewertet/verworfen wurden, ist nicht feststellbar.
2. **Was „CRC-Orakel" im Alt-Plan bezeichnete** (Brute-Force-EC aus
   `FDDProcessingAmigaEC.cs`? etwas anderes?).
3. **`dskx` nicht gebaut/ausgeführt:** Versionsstring und Exit-Codes aus
   der Quelle zitiert; Bauprobe (.NET-7-SDK) steht aus.
4. **Urheberrecht des `.wvfrm`-Inhalts** (Aufnahme „Modules1", laut
   README private Dateien des Autors — nicht verifizierbar).
5. **7 `notice`-Dateien** in `*_files/`-Ordnern: als
   Webseiten-Mitschnitte eingestuft, nicht einzeln durchgesehen.
6. **PIC-Widerspruch im Fremd-Repo:** README sagt PIC16F1938@80 MHz,
   `main.c:4` sagt PIC16F1827 — nicht aufgelöst, für UFT ohne Folge.
7. **Ob FloppyControls SCP-/KryoFlux-Import** fachlich korrekt genug
   wäre, um je als Zweitmeinung zu dienen — nicht geprüft, da GUI-only
   ohnehin nicht automatisierbar.

## Werkzeug-Befunde (Scout-Werkzeug selbst)

- **W12:** `vermessen.py` zählt Domänen-Fundstellen auch in
  `*_files/`-Webseiten-Mitschnitten und PDF-Binärdaten — der
  Domänen-Score 24 und fast alle Fundstellen-Listen des Entwurfs sind
  dadurch Rauschen (z. B. „GCR" in `dashicons.css`). Ein
  Ausschluss-Muster für `*_files/` und Binärformate würde die Messung
  belegbar machen.
- **W13:** die Ratenbremse in `gutachten.py:106-110` liest nur die
  ersten 400 Bytes — Übernahme-Marken am Dateiende (naheliegender Ort)
  werden nicht erkannt. Vier nachweislich abgearbeitete Gutachten
  (fdc_bitstream→MF-626, mame→MF-623, greaseweazle-restorer→MF-611,
  hxcfe_file_selector→MF-614) trugen keine Marke; die Marken sind in
  diesem Zyklus mit gemessenem MF-Anker nachgetragen (am Dateikopf).

## OPEN_ITEMS-Vorschläge (3 von max. 5, nach Priorität)

> Alle drei tragen den Regel-6-Hinweis: Repo `bewertet`, 0 Commits seit
> Bewertung; vorgeschlagen wird nur, was in keinem Baum-Dokument als
> abgedeckt nachweisbar ist. Übernahme = Eigentümer-Entscheid.

**SCOUT-F1 (P2) — `dskx` als FAT12-Inhalts-Oracle registrieren.**
FloppyControl führt seit 2026-01 die Konsolen-CLI `dskx`
(`FAT12Extractor/src/DskX.Cli/Program.cs:7-140`, `--version` → „DskX
v1.0.0", `list`/`extract` inkl. gelöschter Einträge und Bad-Cluster
0xFF7, `DiskSectorMap.cs:45-47`), die genau die Behauptung entscheidet,
für die floptool seit MF-623 blind ist: den Dateisystem-Inhalt eines
FAT12-Abbilds. Vorschlag: siebter Eintrag in
`tests/differential/oracles.py` (env `UFT_ORACLE_DSKX`, version_re
`DskX v([\d.]+)`, licence GPL-3.0, code_import False) mit Einhängepunkt
PLAN_v4.1.7 Phase 1 („inhaltlich statt strukturell"); Bauprobe
(.NET-7-SDK) ist Voraussetzung und steht aus. Aufwand S–M; Messquelle:
`out/FloppyControl.gutachten.md` §F1.

**SCOUT-F2 (P2, Entscheidungsvorlage GUI) — Fluss-Scatterplot nach
FloppyControl-Vorbild.** UFT hat gemessen keine Fluss-Visualisierung
(Rotbeweis MF-610; Handprüfung 2026-08-28: Histogramm-, Sektor- und
Spur-Widgets vorhanden, keine Strom-über-Zeit-Darstellung), während
FloppyControl den Periodenstrom als interaktiven Scatterplot mit
4/6/8-µs-Farbbändern, Index-Marken, Schwellenlinien und Zoom/Drag
zeichnet (`Graphics.cs:1942-2160`). Vorgeschlagen wird eine
Verhaltens-Spec (GPL-3.0 → kein Code) mit Einhängepunkt MAMMUT_PLAN
§2.1.3 Fit-Anzeige/OTDR-Panel — Datenquellen (Intervalle, MF-488-Peaks,
`warp_span`) liegen im Baum; die FloppyControl-Editierfunktion nur mit
Provenienz-Kette. GUI-Folge → Vorlage nach AGENT.md Regel 8, Aufwand M;
Messquelle: §F2.

**SCOUT-F3 (P3, Eigentümer-Vorlage) — Verhaltens-Spec
Analog-Oszilloskop-Rettungspfad.** FloppyControl gewinnt Daten aus
Oszilloskop-Aufnahmen des analogen Lesekopfsignals zurück (Glättung,
Differenzierung, adaptive Schwellen, Nulldurchgänge → Perioden,
Injektion in den normalen Decode-Puffer; `WaveformEdit.cs:334/:636-744/
:796`) und liefert ein reales 18-MB-Fixture mit
(`ExampleWaveforms/A001 Modules1_T005_000.wvfrm`, 18 000 005 B); UFT hat
keinerlei Analog-Ingest (grep-Handprüfung §F3). Als Verhaltens-Spec plus
Fixture wäre der Pfad vollständig offline prüfbar — Hardware nur für
neue Aufnahmen, in-house ohnehin ausgeschlossen (MF-310).
Inhalts-Urheberrecht des Fixtures UNGEKLÄRT → Beschaffung nur nach
Vorlage; Aufwand L; Messquelle: §F3.

## Pflichtfelder-Kurzfassung

| Feld | Wert |
|---|---|
| Kategorie | F1 Oracle · F2 Verbesserung (GUI-Vorlage) · F3 Innovation (Vorlage) · Fundus: Daten/irrelevant |
| Lizenzzone | GELB (GPL-3.0, Wurzel + tc_lib); 7× PRÜFEN nur Webseiten-Mitschnitte → kein Code, keine Portierung, Spec+Oracle erlaubt |
| Inventar | Abfragen zitiert in §F1-F3; Formatliste vollständig (SSOT ok) |
| Einhängepunkte | PLAN_v4.1.7 Phase 1 (F1); MAMMUT §2.1.3/OTDR-Panel (F2); keiner für F3 → deshalb Vorlage, kein Plan-Baustein |
| Oracle-Kandidat | `dskx` (einziges Konsolen-Binary im Repo) |
| Beschaffung | siehe Liste (2 Selbstbau-Punkte, 1 Vorlage-Punkt, 2 Nur-Zitieren) |
| Aufwand | F1 S–M · F2 M · F3 L |
| Differenzlauf | F1-Betriebsplan in §F1; keine „besser"-Behauptung erhoben |
| Regel 6 | 0 Commits seit Bewertung — alle Vorschläge als Eigentümer-Vorlage gekennzeichnet |
