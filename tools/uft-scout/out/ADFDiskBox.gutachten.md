<!-- uebernommen: MF-639 -->
# Gutachten: hippy666/ADFDiskBox

* **Zyklus:** 8 (2026-08-28), Auftrag des Eigentümers (ADF-Zweitmeinungs-Suche)
* **Repo:** https://github.com/hippy666/ADFDiskBox.git
* **Vermessener Stand:** Commit `50dcbeb` (2025-10-24, "win11"), 27 Dateien,
  11 × C#, 6 265 Zeilen `.cs` gesamt (`wc -l *.cs`)
* **Messung:** `work/ADFDiskBox.messung.json` (vermessen.py, 2026-08-28)
* **Hinweis zur Erstellung:** `gutachten.py` hat den Entwurf verweigert
  (Ratenbremse: 5 Gutachten in `out/` ohne Übernahme-Marke). Dieses
  Gutachten ist von Hand nach Playbook 03 geschrieben; es trägt **0**
  OPEN_ITEMS-Vorschläge aus dem Repo selbst und **1** abgeleiteten
  Vorschlag — Regel 5 (max. 5 je Zyklus) ist damit nicht berührt.

## TL;DR

ADFDiskBox ist eine Windows-Forms-Oberfläche, die per
`cmd.exe /C gw …` das Greaseweazle-Host-Tool aufruft. Es enthält
**keinerlei eigenen ADF-, MFM- oder Dateisystem-Code** — die zentrale
Frage des Auftrags (ADFlib-abhängig oder eigenständig?) beantwortet
sich mit: **weder noch, es gibt gar keine ADF-Ebene**. Als Oracle
untauglich, als Code-Quelle substanzlos. **Verwerfen.**

Ein einziger verwertbarer Zeiger fällt ab: der Autor pflegt eine eigene
Diskdef `amiga.amigados.plus` mit **82 Zylindern** und benutzt sie für
Lesen UND Schreiben — Praxis-Beleg, dass 81/82-Spur-ADFs im Feld
vorkommen. UFTs registriertes ADF-Plugin weist genau diese Dateien hart
ab. Das ist der eine Fund (Kategorie Verbesserung, Aufwand S), und er
stützt sich primär auf WinUAE als Referenz, nicht auf ADFDiskBox.

## Die ADFlib-Frage (Auftragskern)

**Antwort: ADFDiskBox baut weder auf ADFlib auf noch implementiert es
ADF selbst. Es fasst nie ein Byte einer ADF-Datei an.**

Belege (Datei + Zeile, nicht README):

* Es gibt keine vendorten Verzeichnisse und keine Fremd-DLL-Referenzen:
  das Repo besteht aus 27 Dateien, alle top-level bzw. `Properties/`
  (Dateiliste in `work/ADFDiskBox.messung.json`); `ADFDiskBox.csproj`
  referenziert nur .NET-Framework-Assemblies.
* Methode der Negativ-Messung: `grep -n "ReadAllBytes|FileStream|
  BinaryReader|adflib|ADFlib|RootBlock|checksum|Checksum" *.cs` über
  alle 11 C#-Dateien → **0 Treffer**. Kein Code-Pfad öffnet eine
  Image-Datei; Dateipfade werden ausschließlich als Strings in
  Kommandozeilen eingesetzt.
* Was stattdessen passiert: `frmMainForm.cs:960` (`gw read … .adf`),
  `frmMainForm.cs:1419` (`gw write`), `frmMainForm.cs:1097`
  (`gw convert`), `frmMainForm.cs:133/398` (`gw info`),
  `frmMainForm.cs:1538` (`gw update`) — alles über
  `FileName = "C:\WINDOWS\SYSTEM32\cmd.exe"` (z. B. `frmMainForm.cs:147`).
  `frmMainForm.cs:1629` (`BtnLoadHxC_Click`) startet zusätzlich die
  externe HxC-GUI.

Für die gesuchte **ADFlib-unabhängige ADF-Zweitmeinung** ist dieses Repo
damit wertlos. Beifang der Recherche: **WinUAE `disk.cpp`** ist eine
nachweislich ADFlib-unabhängige Plain-ADF-Implementierung (eigene
Geometrie-Ableitung aus der Dateigröße, s. u.) — aber als GUI-Emulator
kein skriptbares Oracle und im Repo `tonioni/WinUAE` **ohne
Lizenzdatei** (Root-Verzeichnis geprüft via GitHub-API 2026-08-28: keine
COPYING/LICENSE; `disk.cpp`-Header nennt nur Copyright, keinen Grant).
Die Suche nach einem skriptbaren ADF-Zweit-Oracle bleibt offen.

## Lizenzurteil

* **Zone: GRÜN.** `LICENSE.txt:1` — „MIT License", Copyright (c) 2025
  John Brett. Eine Lizenzdatei, keine Unterverzeichnis-Vendorings.
* **Konsequenz:** Code wäre mit Attribution portierbar — es gibt aber
  nichts zu portieren (s. Charakter).
* **Attribution (Pflichtfeld):** `LICENSE.txt:4` — „Parts from
  GreasewesalGUI .NET Copyright (c) 2020 Don Mankin". Quelle
  identifiziert als `Foosie/GreaseweazleGUI-dotNET` (Don Mankin),
  Lizenz dort: **MIT** (LICENSE-Datei im Repo, geprüft 2026-08-28).
  MIT-aus-MIT — sauber, keine Altlast.
* `Jdiskdefs.cfg` ist zu ~100 % das Greaseweazle-Original (s. Messung
  unten); keirf/greaseweazle steht unter Unlicense/Public Domain
  (COPYING: „free and unencumbered … public domain") — unkritisch.

## Charakter des Repos (Messung)

| Bestandteil | Umfang | Inhalt |
|---|---|---|
| `frmMainForm.cs` | 1 847 Z. | Haupt-GUI, gw-Kommandozeilen-Bau |
| `frmMainFormCode.cs` | 1 174 Z. | ADF2DISK/DISK2ADF/SCP2DISK/… (je ein `gw`-Aufruf), INI-Load/Save |
| `FrmOptions*.cs` | 2 294 Z. | Optionen-Dialog, Pfad-Auswahl |
| `Jdiskdefs.cfg` | 79 `disk`-Einträge | Greaseweazle-diskdefs |
| Rest | | Designer-/Resource-/Projekt-Dateien |

**Jdiskdefs.cfg-Abgleich (Methode):** `diff` der `^disk `-Zeilen gegen
`keirf/greaseweazle` Tag `v1.16.2` (`src/greaseweazle/data/diskdefs.cfg`,
74 Einträge; Version gewählt, weil `README.md` „Tested on host tools
1.16.2 and up" nennt). Ergebnis: Eintragsmenge identisch bis auf
**genau einen Zusatz**: `amiga.amigados.plus` = `cyls = 82, heads = 2,
secs = 11` (`Jdiskdefs.cfg`, Block `disk amiga.amigados.plus…end`).
Benutzt in `frmMainForm.cs:887` (Read) und `frmMainForm.cs:1298`
(Write) als `--format=amiga.amigados.plus`.

## Der eine Fund: 81–83-Zylinder-Plain-ADF (Kategorie: Verbesserung)

**UFT-Ist (Datei + Zeile):** Das per SSOT registrierte Amiga-ADF-Plugin
ist `src/formats/adf/uft_adf_plugin.c` (`gen_format_list.py --md`,
2026-08-28; die anderen `adf`-Treffer sind Acorn: `adf_arc`, `adl`).
Probe und Open akzeptieren **nur exakt** 901 120 B (80×2×11×512, DD)
oder 1 802 240 B (HD): `uft_adf_plugin.c:19`
(`if (file_size != ADF_DD_SIZE && file_size != ADF_HD_SIZE) return false;`)
und `:36` (`UFT_ERROR_FORMAT_INVALID`). Ein 82-Zylinder-ADF
(923 648 B) wird von keinem Plugin im Baum geöffnet — `adf_ext`
(`src/formats/adf_ext/uft_adf_ext.c:53`) verlangt das
`UAE-1ADF`-Magic, ist also ein anderer Container. Die
Dateisystem-Schicht wäre bereit: `src/fs/uft_amigados.c:172` rechnet
`root_block = total_blocks / 2` größenparametrisch.

**Praxis-Beleg (dieses Repo):** s. o. — ein Feld-Werkzeug definiert
sich eigens eine 82-Zylinder-Diskdef und schreibt/liest damit ADFs.

**Autoritative Referenz (Verhaltens-Spec):** WinUAE `disk.cpp`
(Master `989ce7b4b9`, 2026-08-27), Plain-ADF-Zweig `disk.cpp:1705-1736`:
für `size > 160*11*512 + 511` iteriert eine Schleife
`for (int i = 80; i <= 83; i++)` und akzeptiert je Zylinderzahl
DD (11 Sek.), HD (22 Sek.), DiskSpare (12/24 Sek.) sowie
Header-Varianten (512+16 B/Sektor). Akzeptierte Plain-DD-Größen
81/82/83 Zyl.: **912 384 / 923 648 / 934 912 B** (81·2·11·512 usw.,
nachgerechnet). Zweite Referenz, GRÜN: greaseweazle behandelt ADF
größenparametrisch über Diskdefs (`image/adf.py` = `IMG`-Subklasse
mit `default_format amiga.amigados`; Unlicense) — genau den Weg nutzt
ADFDiskBox.

**Stufe-4-Weg (einfrier-konform):** Das ist eine Hebung/Korrektur am
**bestehenden** Plugin, kein neues Format. (a) **Rotbeweis zuerst:**
Fixture 923 648 B echtes 82-Zyl-ADF → `uft_disk_open()` liefert heute
NULL; Test muss rot sein, bevor Code entsteht. (b) Benannte Referenz im
Header: WinUAE `disk.cpp:1705-1736` (Verhaltens-Fakten; Repo ohne
Lizenzdatei → **kein** Code-Port, nur die Größen-Tabelle als Fakt) +
greaseweazle-Diskdef-Praxis. (c) Jede Größe nachgerechnet im Commit.
DiskSpare-Größen (12/24 Sektoren) NICHT mitnehmen — das ist ein
eigenes Format und liegt bereits bewusst im Fundus
(`docs/OPEN_ITEMS.md:1476`, FloppyControl-Zyklus); dieser Fund stärkt
höchstens dessen Akte um die WinUAE-Fundstelle.

**Einhängepunkt:** `docs/PLAN_v4.1.7.md` Phase 1 Nr. 2 (AmigaDOS/ADF-
Hebung auf T1/T1b; Moratoriums-Rückstand ATR/D64/**ADF**/FDI/NFD-r0).
Der Rotbeweis-Fixture dient doppelt: ADF-Hebung und Größenvarianten.

**Aufwandsklasse: S** (Größen-Tabelle in Probe/Open, Geometrie aus
Größe ableiten; FS-Schicht ist bereits größenparametrisch).

## Inventar-Abgleich (zitiert)

* `adf` → `vorhanden: true`, Treffer `adf, adf-copy, adf_adl, adf_arc,
  adf_ext, extadf, uft_adf`, `plugin_liste_vollstaendig: true` → als
  Format vorhanden; der Fund betrifft eine Größenvariante, kein neues
  Format. Handprüfung im Baum durchgeführt (s. o., Datei+Zeile).
* `greaseweazle` → `vorhanden: true` (Controller, production) → der
  gesamte Funktionsumfang von ADFDiskBox (gw read/write/convert via
  GUI) ist in UFT nativ vorhanden; der Wrapper bietet nichts darüber.
* `amigados` → `vorhanden: true` (Treffer `amiga`).
* `diskdefs` → `vorhanden: true` (Treffer `uft_cpm_diskdefs`).
* `diskspare` → `abgedeckt: false` → Handprüfung: nur Katalog-Eintrag
  `src/protection/uft_amiga_protection_full.c:211`, kein Leser; bereits
  als Fundus geführt (`docs/OPEN_ITEMS.md:1476`) — kein neuer Vorschlag.
* Korpus: `xdftool_dd_ofs.adf` (80 Zyl. Standard), `gw_amigados.hfe`,
  `gw_amigados.scp` liegen — **kein** 81/82-Zyl-Abbild liegt.

## Oracle-Kandidat

**Nein.** WinForms-GUI, nicht skriptbar; jeder Pfad endet in
`cmd.exe /C gw …` und setzt Greaseweazle-**Hardware** voraus
(`gw read/write` gegen Gerät); keine Hashes je Datei, kein
Dateisystem-Zugriff, keine Verzeichnisdarstellung. Das
`flophashes`-Kriterium (Inhalt byteweise, CRC32/SHA-1 je Datei) ist
strukturell unerfüllbar.

## OPEN_ITEMS-Vorschlag (1 von max. 5)

> **SCOUT-G1 (P3, S): ADF-Plugin weist 81–83-Zylinder-ADFs ab, die
> WinUAE und greaseweazle im Feld erzeugen und lesen.**
> UFT: `src/formats/adf/uft_adf_plugin.c:19,36` akzeptiert nur exakt
> 901 120/1 802 240 B. Feld-Beleg: hippy666/ADFDiskBox pflegt Diskdef
> `amiga.amigados.plus` (82 Zyl.) und schreibt/liest damit ADFs
> (`Jdiskdefs.cfg`; `frmMainForm.cs:887,1298`). Referenz:
> WinUAE `disk.cpp:1705-1736` (Master `989ce7b4b9`) akzeptiert
> 80–83 Zyl. DD/HD; Plain-DD-Größen 912 384/923 648/934 912 B
> (nachgerechnet). Stufe 4: Rotbeweis-Fixture (echtes 82-Zyl-ADF,
> Beschaffung s. Gutachten) → heute NULL; dann Größen-Tabelle in
> Probe/Open, Referenz im Header. Einhängepunkt: PLAN_v4.1.7 Phase 1
> Nr. 2 (ADF-Hebung). DiskSpare ausdrücklich NICHT im Scope (Fundus,
> `docs/OPEN_ITEMS.md:1476`). Kein Code-Port aus WinUAE (Repo ohne
> Lizenzdatei) — nur Verhaltens-Fakten.
> Quelle: `tools/uft-scout/out/ADFDiskBox.gutachten.md`.

## Beschaffungsliste

| Was | Warum | Weg | Status |
|---|---|---|---|
| 1 echtes 82-Zyl-DD-ADF (923 648 B) | Rotbeweis SCOUT-G1 | Kandidat A: `gw convert` aus `tests/corpus/gw_amigados.scp` mit Diskdef `cyls=82` — **UNGEKLÄRT**, ob gw fehlende Spuren 80/81 füllt oder abbricht (Korpus-SCP hat mutmaßlich 80 Zyl.). Kandidat B: WinUAE-Sitzung (82-Spur-Image anlegen/speichern). Kandidat C: reale Sicherung aus der Community | fehlt im Korpus |

Nicht anfordern (liegt bereits): 80-Zyl-ADF (`xdftool_dd_ofs.adf`),
AmigaDOS-HFE/SCP (`gw_amigados.hfe`, `gw_amigados.scp`).

## UNGEKLÄRT

1. Lizenz von WinUAE (`tonioni/WinUAE`): keine LICENSE/COPYING im Repo
   gefunden (GitHub-API + Raw-Zugriff 2026-08-28); Projekt gilt
   allgemein als GPL, aber das ist README-Wissen, kein Beleg. Für die
   Verhaltens-Spec irrelevant; für jeden Code-Port: Eigentümer-Vorlage.
2. Herstellungsweg des 82-Zyl-Fixtures (s. Beschaffungsliste) — welcher
   Kandidat ein *echtes* Abbild liefert, ist ungetestet.
3. Ob 81/83-Zyl-ADFs (neben 82) im Feld real vorkommen, ist nicht
   belegt — WinUAE akzeptiert sie, ADFDiskBox erzeugt nur 82. Für die
   Größen-Tabelle empfiehlt sich trotzdem die volle WinUAE-Menge, mit
   niedriger Probe-Konfidenz.
4. HD-Varianten >80 Zyl. (1 847 296 B usw.): von WinUAE akzeptiert,
   kein Feld-Beleg gesichtet.

## Verdikt

Repo: **verworfen** (Wrapper ohne eigene Substanz; alles Angebotene hat
UFT nativ). Der abgeleitete Fund SCOUT-G1 steht oben als einziger
Vorschlag. Eintrag in `data/known_negatives.json` gesetzt.
