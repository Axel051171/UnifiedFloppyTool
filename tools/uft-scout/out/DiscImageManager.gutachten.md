# Gutachten: geraldholdsworth/DiscImageManager

- **Repo:** https://github.com/geraldholdsworth/DiscImageManager.git
- **Stand:** HEAD `5ffe4796fe`, letzter Commit 2026-08-24 („Updated to 1.50.4")
- **Messung:** `work/DiscImageManager.messung.json` (2026-08-28, 722 Dateien,
  100 `.pas`, Domänen-Score 19)
- **Anlass (Regel 6):** Eigentümer-Auftrag Zyklus 9 (2026-08-28) — Acorn/
  RISC-OS ist UFTs schwächste Ecke; `ssd` und `adl` stehen auf T3
  (`docs/VERIFICATION_TIERS.md:99` bzw. `:57`), der Korpus führt **kein**
  einziges Acorn-Abbild (`inv["korpus"]`, Suchmuster
  `ssd|dsd|adl|acorn|bbc|uef` → 0 Treffer). Repo ist neu, nicht in
  `data/known_negatives.json`.
- **Werkzeug:** Lazarus/Free-Pascal-Anwendung von Gerald Holdsworth zum
  Lesen/Schreiben von Acorn- (DFS/ADFS/CFS/RFS/AFS/DOS Plus), FAT-,
  Commodore- (D64/71/81, AmigaDOS), Spectrum+3/Amstrad- und ISO-Abbildern
  (README.md Z. 3–17).

## 1. Lizenzurteil (Zone, mit Fundstelle)

**GPL-3.0 → Zone GELB.** Einzige Lizenzdatei im Repo ist `LICENSE` an der
Wurzel („GNU GENERAL PUBLIC LICENSE Version 3, 29 June 2007", Z. 1–2;
Methode: `find -iname "LICENSE*" -o -iname "COPYING*"` über den ganzen
Klon → genau 1 Treffer, kein abweichendes Vendoring-Verzeichnis).
Quelltext-Header bestätigen „version 3 of the Licence, or (at your
option) any later version" (`LazarusSource/DIMConsole.lpr:4-19`).

**Konsequenz nach `playbook/lizenzmatrix.md`:** kein Code-Port, kein
Vendoring. Erlaubt: Verhaltens-Spec (aus Code-Lektüre dokumentiertes
Verhalten, neu implementiert nach Spec durch Stufe 4 mit eigener
Referenz) und Oracle-Binary (selbst gebaut, nur Ausgaben verglichen).
Pascal-Code wäre ohnehin kein Port-Kandidat — hier zählen nur Spec und
Oracle.

**Attributionen im Kandidaten (Pflichtfeld):** Methode:
`grep -ri "based on|port of|adapted from|derived from|courtesy|thanks to|originally"`
über `LazarusSource/*.pas`, `README.md`, `Contributing.md` → 15 Zeilen,
davon **1 relevante**:

- `DiscImage_ADFS.pas:4422` — „Adapted from the RISC OS RamFS ARM code
  procedure InitDiscRec in RamFS50". Quelle ist RISC OS Open;
  RISC-OS-Quellcode steht unter **Apache-2.0** (RISC OS Open, seit 2018).
  Für UFT folgenlos, weil aus diesem Repo ohnehin nichts portiert wird —
  aber wer je die `InitDiscRec`-Umgebung (Disc-Record-Erzeugung beim
  Formatieren) als Spec-Quelle nutzt, hat dort eine GELB-auf-GELB-Kette
  und muss auf die RISC-OS-Primärquelle ausweichen, nicht auf DIMs
  Nachbau.

**Beigelegte Abbilder (`Blank Images/`):** Daten in einem GPL-3-Repo,
Schutzstatus als Sammlung unklar → **Zone PRÜFEN** für direkte Übernahme.
Der saubere Weg umgeht die Frage vollständig: die Abbilder mit dem selbst
gebauten `DIMConsole`-Binary per `new`-Befehl **selbst erzeugen**
(Programmausgabe, Provenienz dokumentierbar) — siehe Beschaffungsliste.

## 2. Was das Inventar sagt (Abfrage zitiert)

`inventar.py query work/inv.json ssd dsd adf adl uef adfs dfs acorn watford interleave riscos` (2026-08-28):

| Begriff | Antwort | Konsequenz |
|---|---|---|
| `ssd` | `vorhanden: true`, Treffer `ssd`, `ssd_dsd`, **Tier T3** | Format da, ungeprüft — Hebungsziel |
| `adl` | `vorhanden: true`, Treffer `adf_adl`, `adl`, **Tier T3** | dito |
| `adf` | `vorhanden: true`, Tier T1b (Amiga) | kein Fund nötig |
| `uef` | `vorhanden: true`, Tier `null` | kein Tier-Eintrag; nicht weiter verfolgt |
| `dfs` | `vorhanden: true`, Treffer `uft_bbc_dfs` | Katalog-Bibliothek existiert |
| `adfs`, `acorn`, `watford`, `interleave`, `riscos` | `abgedeckt: false` | von Hand im Baum nachgesehen — Ergebnis unten |

**Handprüfung bei `abgedeckt: false`** (gelesene Dateien:
`src/formats/bbc/{ssd_dsd,adf_adl,uft_bbc_dfs}.c`,
`src/formats/ssd/uft_ssd_plugin.c`, `src/formats/adl/uft_adl.c`,
`src/formats/adf_arc/uft_adf_arc.c`, `ls src/fs/`):

- **Kein Acorn-Dateisystem-Layer.** `src/fs/` führt AmigaDOS, FAT12,
  CBM-Hilfen — nichts für DFS/ADFS. `uft_bbc_dfs.c` (358 Z.) parst den
  DFS-Katalog (Name, Lock, Load/Exec 18-bit, Boot-Option), ist aber eine
  Bibliothek ohne ADFS-Gegenstück; ADFS-Verzeichnisse, Old/New Map,
  RISC-OS-Attribute/Filetypes liest **nichts** im Baum.
- **Keine Interleave-Behandlung.** `adf_adl.c:73` und
  `uft_adf_arc.c` lesen fest zylinder-sequenziell; keine
  SEQ/INT/MUX-Unterscheidung, keine Erkennung.
- **Kein Watford-DFS.** `grep -ri watford src/ include/` → 0 Treffer.

## 3. Funde

### F1 · Korrektheit (eigener Baum): `uft_adl.c` beschreibt ein Format, das es nicht gibt

`src/formats/adl/uft_adl.c:4-13` nennt ADL „Acorn **DFS** Large … 80
tracks × **1 head** × 16 sectors × 256 = **327.680**". Beides ist falsch:
`.adl` ist **ADFS L** (Advanced Disc Filing System), und ADFS L ist
640 KB = 80×**2**×16×256 = **655.360** Byte. 327.680 ist ADFS **M**
(einseitig). Referenz: `DiscImage_ADFS.pas:73-75` (163840=S, 327680=M,
655360=L) — und unabhängig davon die mitgelieferte Datei
`Blank Images/Acorn ADFS/ADFS_L.adl`, gemessen **655.360 Byte**.

Praktische Folge im Baum: ein echtes `.adl` (655.360 B) fällt durch
`adl_probe` (`uft_adl.c:17-21`, exakt 327.680) und landet — Endungsliste
`adf;adl;adm` — bei `adf_arc` (`uft_adf_arc.c:112`), das 655.360 im
Kommentar als „ADFS-D" führt (`:8`; ADFS D ist tatsächlich 819.200 mit
5×1024) und ohne Interleave-Logik liest. Ein 327.680-Byte-ADFS-M wird
umgekehrt als „ADL" mit falschem Namen ausgewiesen. Das ist dieselbe
Fehlerklasse wie FMT-2/3/10/11/12 (Memory
`format_layer_fabrication`): Geometrie erfunden statt belegt.
`git log` auf beide Dateien zeigt nur die MF-519/529-Bounds-Fixes, nie
eine Format-Verifikation — konsistent mit T3.

**Rotbeweis liegt auf der Hand:** `ADFS_L.adl` (oder ein per DIMConsole
frisch erzeugtes L-Abbild) gegen `uft_disk_open()` — der ADL-Plugin darf
nicht ablehnen bzw. adf_arc darf nicht mit falscher Geometrie öffnen.

### F2 · Daten/Spec: Interleave-Varianten SEQ/INT/MUX mit Auto-Erkennung

ADFS-L-/AFS-Abbilder existieren in mehreren physischen Anordnungen; DIM
unterscheidet **vier** und erkennt sie automatisch:

- Abbildungsformeln (logische Disc-Adresse → Datei-Offset):
  `DiscImage_Private.pas:529-583` — SEQ (identisch), INT
  (spurverschränkt: `(track_size*side)+(track*track_size*2)+offset`),
  MUX-2 und MUX-4 (Track-Multiplex, Formeln Z. 573 und 579);
  Benennung `DiscImage.pas:958` („Sequential/Interleave/Multiplex").
- **Auto-Erkennung über den Verzeichnis-Zustand:**
  `DiscImage_ADFS.pas:1265-1276` — Abbild mit Kandidat-Interleave lesen;
  sind kaputte Verzeichnisse da (`brokendircount>0`), nächste Methode
  probieren, bis 0 kaputte Verzeichnisse oder alle Methoden durch. Für
  das ADFS-L/DOS-Hybrid zusätzlich der FAT-Test `ReadByte($1000)=$FF`
  (`DiscImage_ADFS.pas:90-100`).
- AFS L3 startet mit MUX als Default (`DiscImage_AFS.pas:39`).

UFT hat davon nichts (Handprüfung §2). Ohne diese Unterscheidung ist
jeder künftige ADL-Leser auf der Hälfte der real existierenden Abbilder
still falsch.

### F3 · Daten/Spec: ADFS-Identifikation, Prüfsummen, Broken-Directory-Codes

Vollständige, aus dem Code zitierfähige Erkennungs- und Prüfkette:

- **Old Map (S/M/L/D):** Freikarten-Prüfsummen — Byte `$0FF` ==
  `ByteCheckSum($0000,$100)`, Byte `$1FF` == `ByteCheckSum($0100,$100)`
  (`DiscImage_ADFS.pas:29-35`); Root-Verifikation `Read24b($6D6)=2` mit
  Verzeichnis-Checkbytes `$200`==`$6FA` (Old Dir) bzw. `Read24b($BDA)=4`
  mit `$400`==`$BFA` (New Dir → ADFS D) (`:43-48`); Null-Größen-Wächter
  gegen Fehlerkennung von C64-Abbildern (`:54-60`); Größenauflösung
  erst über das **eingetragene** Disc-Size-Feld `$0FC`, erst dann über
  die Dateilänge (`:64-81`) — nie nur über die Dateigröße.
- **New Map (E/E+/F/F+/HDD):** Disc Record bei `$0004`, dann `$0DC0`
  (Bootblock), dann Emulator-Header `$0200` (`:149-151`); Bootmap-Lage
  aus secsize/zone_spare/bpmb errechnet (`:176`); je Zone
  ZoneCheck-Prüfsumme (`GeneralChecksum`) plus CrossCheck-XOR aller
  Zonen == `$FF` (`:183-197`); Bootblock-Prüfsumme
  `ByteChecksum($0C00,$200)` (`:201-203`).
- **Broken-Directory-Codes** — 8 benannte Bit-Ursachen
  (`DIMHelp.pas:188-195`): Sequenznummern-Mismatch, StartName/EndName-
  Mismatch, Big-Dir-Identity, CRC-Fehler, Identity nicht
  „Hugo"/„Nick"/„SBPr"+„oven", nicht sektor-aligniert, Parent-Sektor
  falsch. Dazu ein Reparatur-Kommando `fixdirs`
  (Konsole, `DIMHelp.pas:89-90`).

Das ist die fertige Verhaltens-Spec-Grundlage für jede künftige
ADFS-Arbeit — mit dem Vorbehalt, dass Stufe 4 die Werte gegen eine
**Primärquelle** (RISC OS PRM / Acorn-Dokumentation) gegenliest, nicht
nur gegen DIM (Einfrier-Regel: benannte Referenz).

### F4 · Daten/Spec: DFS-Validierung, Watford-DFS, 204.800-Byte-Mehrdeutigkeit

- **Watford-Erkennung je Seite:** 8×`$AA` bei `$0200` und 4×`$00` bei
  `$0300` (Seite 2: `$0C00`/`$0D00`) → vier Mischformen
  Acorn/Watford-DSD (`DiscImage_DFS.pas:151-197`); Watford hat 62 statt
  31 Katalogeinträge, Fortsetzungskatalog in Sektor 4 (`:473-505`).
- **DSD-Interleave-Formel** `((sector MOD 10)+(20*(sector DIV 10))
  +(10*side))*$100+offset` (`DiscImage_DFS.pas:222-226`) — **deckungs-
  gleich** mit UFTs `(t*heads+h)*10+(s-1)` (`uft_ssd_plugin.c:12,104`,
  `ssd_dsd.c:63`). Das ist eine **Bestätigung**, kein Fund: UFTs
  DSD-Ordnung stimmt mit der Referenz überein.
- **Mehrdeutigkeit:** 204.800 Byte sind sowohl 80-Spur-SSD als auch
  40-Spur-DSD. UFTs `ssd_detect` wählt still 80×1
  (`uft_ssd_plugin.c:29-39`); DIM entscheidet über Katalog-Sektorzahl
  und Katalog-Validierung. Gehört in die SSD-Hebungs-Spec.

### F5 · Korrektheit (eigener Baum): größenexakte Probes vs. gekürzte Abbilder

DIMs **eigene** Referenz-Blanks sind hinten gekürzt (gemessen im Klon):
`ADFS_M.adl` = 651.264 (nicht 327.680), `ADFS_S.adl` = 323.584,
`BlankDouble.dsd` = **3.072**, `BlankSingle.ssd` = **512** Byte.
Gekürzte Acorn-Abbilder sind also draußen Normalfall (Katalog sagt die
wahre Größe, Datei trägt nur belegte Sektoren). UFTs Probes verlangen
exakte Größen (`ssd_detect`: Vielfaches von 2560 und exakt 40/80/160
Spuren, `uft_ssd_plugin.c:29-39`; `adl_probe`: exakt 327.680,
`uft_adl.c:17-21`; `adf_arc_probe`: vier exakte Größen,
`uft_adf_arc.c:16`) und lehnen **alle vier** DIM-Blanks ab. DIM
identifiziert über FS-Strukturen (F3/F4) und nutzt die Größe nur als
Rückfall — das gehört als Verhalten in die Hebungs-Spec beider Formate.

### F6 · Oracle: DIMConsole — das einzige bekannte hardwarefreie Datei-Hash-Oracle für Acorn-Dateisysteme

- **Konsole, belegt:** eigenes Build-Target `DIMConsole.lpi/.lpr`
  (`{$DEFINE DIMCONSOLE}`, `DIMConsole.lpr:24`); unter diesem Define ist
  `TMainForm` eine **LCL-freie** Klasse (`MainUnit.pas:36,69`) — kein
  GUI-Zwang, reine Datei-I/O, keinerlei Hardware-Code im Repo.
  **Skriptbar:** `-c <scriptfile>` beim Start und `runscript`-Befehl
  (`ConsoleAppUnit.pas:206-247`); Befehlssatz u. a. `insert`, `cat all`,
  `extract`, `report`, `savecsv`, `fixdirs`, `interleave
  auto|seq|int|mux`, `new <format>` (`DIMHelp.pas`, komplette Liste).
- **Hashes je Datei, belegt:** `GetFileCrc`/`GetFileMD5` extrahieren den
  Dateiinhalt und hashen die Bytes (`DiscImage_Published.pas:98-115` →
  `ExtractFile`-Puffer; CRC32 `DiscImage_Private.pas:1159-1166`, MD5
  `:1150-1152` über FPC `crc`/`md5`). `savecsv` schreibt sie je Zeile,
  einschaltbar über die Konsolen-Config `CSVCRC32`/`CSVMD5`
  (`MainUnit_Console.pas:59,67`; CSV-Erzeugung `MainUnit.pas:5693-5698`).
  Das ist exakt das `flophashes`-Muster: Inhalt byteweise, nicht
  Verzeichnisdarstellung.
- **Warum nicht einfach floptool:** MAMEs floptool kann `ssd`/`adl` nur
  auf **Container**-Ebene (ACORN_SSD/ACORN_ADFS_OLD, acorn_dsk.cpp);
  seine 12 Dateisysteme enthalten **kein** Acorn-DFS/ADFS
  (`out/mame.gutachten.md:44-46`) — `flophashes` kann dort also keine
  Dateien hashen. DIM ist zudem eine **andere Hand** als MAME und UFT
  (Plan-Regel „Oracle und Korpus-Erzeuger nicht dieselbe Hand",
  `docs/PLAN_v4.1.7.md:127-131`): Container-Prüfung per floptool +
  Datei-Hash-Prüfung per DIMConsole ergibt zwei unabhängige Hände.
- **Reichweite gegen die T3-Liste, mit Methode:** Menge A = die 55
  T3-Formate (`docs/VERIFICATION_TIERS.md:54-108`, 55 Zeilen). Menge B =
  DIMs Lesermodule (README.md Z. 3–17 + `DiscImage_*.pas`). Ein Paar
  zählt, wenn ein DIM-Modul denselben Datei-Container liest: `ssd`
  (DiscImage_DFS.pas), `adl` (DiscImage_ADFS.pas), `img`/FAT12
  (DiscImage_DOSPlus.pas), `edsk` (DiscImage_Spectrum.pas:19,
  ‚EXTENDED CPC DSK', nur lesend) → **4 von 55**. Namensabgleich ist
  kein Fähigkeitsabgleich: ob DIM *unsere* Korpusdateien liest,
  entscheidet erst der Differenzlauf. Zusätzlich als Zweitoracle für
  T1b-Formate nutzbar: d64/d71/d81 (DiscImage_C64.pas), ADF-Amiga
  (DiscImage_Amiga.pas).

## 4. Differenzlauf-Plan (für die Oracle-Behauptung F6)

- **Binaries:** (1) `DIMConsole`, selbst gebaut aus
  `LazarusSource/DIMConsole.lpi` mit `lazbuild` (Lazarus/FPC; SHA-256
  des Binaries in die Oracle-Registry, Versionsbeleg
  `MainForm.ApplicationVersion` im Konsolen-Header); (2) UFT-Testtreiber
  gegen `uft_disk_open()`/Plugin-API (kein CLI — Tests gegen die
  Core-Lib, Memory `feedback_no_cli`).
- **Korpus:** per DIMConsole-Skript erzeugt: `new DFS S40|S80|D40|D80`,
  `new WDFS S80`, `new ADFS S|M|L|D|E|F`, dann `add` mit bekannten
  lokalen Dateien (deterministischer Inhalt, dokumentierte Namen),
  `save`; je Abbild `savecsv` mit `CSVCRC32=True`,`CSVMD5=True`.
  Für ADFS L zusätzlich dieselbe Disk in `interleave seq|int|mux`
  umgeschrieben (drei physische Varianten desselben Inhalts).
- **Metrik:** Stufe A (heute möglich): UFT-Probe/Geometrie je Abbild ==
  DIM-`report`; Rotbeweise F1/F5 (ADFS-L-Abbild, gekürzte Blanks).
  Stufe B (sobald ein DFS-/ADFS-Lesepfad in Stufe 4 entsteht): je Datei
  CRC32/MD5 aus UFT-Extraktion == `savecsv`-Spalten; Katalogliste ==
  `cat all`.
- **Toleranzliste:** BBC-Namensraum (7-bit, Verzeichniszeichen `$.`),
  Load/Exec-Darstellung (18-bit-Hex), DIMs gekürzte Abbilder
  (Padding-Konvention beim Vergleich benennen), `edsk` nur lesend,
  Watford-Mischformate nur wo beide Seiten gleich erkannt werden.

## 5. Einhängepunkte (bestehende Pläne)

- `docs/PLAN_v4.1.7.md` **Phase 1** („VFS-P1 lesend, je Format — macht
  T1b inhaltlich statt strukturell", Z. 61) und der dort etablierte
  Oracle-Differenzlaufsatz (Z. 115-125) samt „nicht dieselbe
  Hand"-Regel (Z. 127-131).
- `docs/VERIFICATION_PLAN.md` §Einfrier-Regel: SSD-/ADL-Hebung ist
  Verifikationsarbeit an Bestehendem — vom Moratorium ausdrücklich
  erlaubt; F1 ist ein Bugfix an Bestehendem.
- `docs/OPEN_ITEMS.md`: Fortsetzung der SCOUT-Reihe (höchste vergebene
  Nummer SCOUT-24, `docs/OPEN_ITEMS.md:1800`).

## 6. OPEN_ITEMS-Vorschläge (5, nach Priorität — Vorschlagsblock, kein Eintrag)

| Nr. | Vorschlag | Kennzahl, die er bewegt |
|---|---|---|
| SCOUT-25 | **`uft_adl.c` liest ein erfundenes Format — Identität und Geometrie gegen Referenz stellen.** Der Header nennt ADL „Acorn DFS Large, 80×1×16×256=327.680" (`uft_adl.c:4-13`); real ist `.adl` ADFS L mit 655.360 Byte und 2 Köpfen (`DiscImage_ADFS.pas:73-75`; Feldbeleg `Blank Images/Acorn ADFS/ADFS_L.adl` = 655.360 B gemessen). Echte L-Abbilder fallen an `adf_arc`, dessen Kommentar 655.360 „ADFS-D" nennt (`uft_adf_arc.c:8`) und ohne Interleave liest. Weg für Stufe 4: Rotbeweis mit einem per DIMConsole erzeugten L-Abbild gegen `uft_disk_open()`, Referenz (Acorn-/RISC-OS-Doku + DIM-Fundstellen) in den Header. | ungeprüfte Formate runter (adl T3 → mind. T2), Fabrikations-Fehlerklasse getilgt |
| SCOUT-26 | **DIMConsole als hardwarefreies Acorn-Datei-Hash-Oracle registrieren und Acorn-Korpus selbst erzeugen.** `lazbuild LazarusSource/DIMConsole.lpi` liefert eine LCL-freie, skriptbare Konsole (`DIMConsole.lpr:24`, `ConsoleAppUnit.pas:206-247`) mit Datei-Hashes nach dem `flophashes`-Muster (`DiscImage_Published.pas:98-115`, `savecsv` + Config `CSVCRC32/CSVMD5`, `MainUnit_Console.pas:59,67`); floptools 12 FS enthalten kein Acorn (`out/mame.gutachten.md:44-46`), DIM ist die fehlende zweite Hand. Korpus (heute: 0 Acorn-Abbilder in `inv["korpus"]`) per `new`+`add`+`savecsv`-Skript erzeugen statt die GPL-3-Blanks zu kopieren. GPL-3 unschädlich: nur Ausgaben werden verglichen. | Korpus Acorn 0 → n; T3→T1b-Weg für `ssd`+`adl` geöffnet; Bench-frei (kein Hardware-Bedarf) |
| SCOUT-27 | **ADFS-Verhaltens-Spec schreiben: Identifikation, Prüfsummen, Broken-Directory-Codes.** Old-Map-Freikarten-Prüfsummen `$0FF/$1FF` + Root-Checkbytes (`DiscImage_ADFS.pas:29-48`), New-Map-Disc-Record `$0004/$0DC0/$0200` mit Zonen-ZoneCheck + CrossCheck-XOR=`$FF` + Bootblock-Prüfsumme (`:149-203`), 8 Broken-Codes (`DIMHelp.pas:188-195`). Stufe 4 liest jede Zahl gegen die RISC-OS-/Acorn-Primärdoku gegen (DIM ist GELB — Spec-Quelle, nicht Port-Quelle; Achtung `DiscImage_ADFS.pas:4422` ist selbst „Adapted from RISC OS RamFS", Apache-2.0). | ungeprüfte Formate runter (Grundlage für adl/ssd-Hebung und jeden künftigen ADFS-Leser) |
| SCOUT-28 | **Größenexakte Acorn-Probes tolerant gegen gekürzte Abbilder machen — Rotbeweis liegt.** DIMs eigene Referenz-Blanks sind gekürzt (651.264/323.584/3.072/512 B gemessen); UFTs `ssd_detect` (`uft_ssd_plugin.c:29-39`) und `adl_probe`/`adf_arc_probe` lehnen alle vier ab, obwohl Katalog bzw. Disc-Size-Feld (`$0FC`) die wahre Größe tragen. DIM-Verhalten als Spec: Struktur zuerst, Größe als Rückfall (`DiscImage_ADFS.pas:64-81`). Dazu die 204.800-B-Mehrdeutigkeit (80×1-SSD vs. 40×2-DSD) über den Katalog entscheiden statt still 80×1 (`uft_ssd_plugin.c:29-39`). | ungeprüfte Formate runter (ssd T3 → T2-Weg); stille Fehlklassifikation raus |
| SCOUT-29 | **Watford-DFS in die SSD-Hebungs-Spec aufnehmen.** Erkennungsrezept 8×`$AA`@`$0200` + 4×`$00`@`$0300` je Seite, 62 Einträge, Fortsetzungskatalog Sektor 4, vier Acorn/Watford-Mischformen (`DiscImage_DFS.pas:151-197,473-505`); UFT kennt Watford nicht (`grep -ri watford src/ include/` → 0). Kein neues Plugin (Moratorium!), sondern Spec + Testfälle für den bestehenden `ssd`/`uft_bbc_dfs`-Bestand; Fixtures per DIMConsole `new WDFS S80` erzeugbar. | ungeprüfte Formate runter (SSD-Spec vollständig statt Acorn-only) |

**Bestätigung ohne Vorschlag:** UFTs DSD-Spurverschränkung stimmt mit
DIM überein (F4) — festhalten im Hebungs-Test, kein Handlungsbedarf.

## 7. Beschaffungsliste (gegen `inv["korpus"]` geprüft: nichts davon liegt)

1. **Lazarus/FPC-Toolchain** (`lazbuild`) — einmalig, nur zum Bau des
   Oracles; kein Eintrag im UFT-Build.
2. **DIMConsole-Binary** aus `LazarusSource/DIMConsole.lpi` @ `5ffe4796fe`,
   SHA-256 dokumentieren.
3. **Acorn-Korpus, selbst erzeugt** (DIMConsole-Skript): DFS S40/S80/
   D40/D80, WDFS S80, ADFS S/M/L (L in seq+int+mux), D, E, F — je mit
   `add`-Inhalt, `savecsv`-Hashdatei und Manifest (Werkzeug, Version,
   Skript, Datum). Die GPL-3-`Blank Images/` selbst **nicht** kopieren
   (Zone PRÜFEN), nur als Größen-/Verhaltensreferenz zitieren.
4. Für SCOUT-27: RISC-OS-/Acorn-Primärdokumentation der ADFS-Strukturen
   als benannte Referenz (Stufe-4-Pflicht; DIM allein genügt der
   Einfrier-Regel nicht).

## 8. Aufwandsklassen

- SCOUT-25: **S** (Bugfix + Rotbeweis, ein Plugin-Paar)
- SCOUT-26: **M** (Toolchain + Skript + Registry-Eintrag + Manifest)
- SCOUT-27: **M** (Spec-Dokument mit Doppel-Referenz)
- SCOUT-28: **S–M** (Probe-Verhalten + Tests)
- SCOUT-29: **S** (Spec-Abschnitt + Fixtures)

## 9. UNGEKLÄRT

1. Ob die offiziellen Releases ein Konsolen-Binary beilegen oder nur die
   GUI — offline nicht prüfbar; für den Plan egal (Eigenbau ohnehin
   nötig für SHA-256-Provenienz).
2. Ob `lazbuild` DIMConsole auf Windows/MinGW ohne LCL-Widgetset-Paket
   durchbaut — erst der Build selbst beweist es (Stufe-4-Schritt 1 von
   SCOUT-26).
3. Ob DIM auch **sequentiell** abgelegte `.dsd` (Seite 0 komplett, dann
   Seite 1) liest — `ConvertDFSSector` kennt nur die Spurverschränkung;
   ob diese Variante draußen vorkommt, ist unbelegt.
4. Warum `ADFS_M.adl`/`ADFS_S.adl` um exakt 4.096 Byte gekürzt sind
   (eine 16-Sektor-Spur) — Kürzungs-Konvention nicht im Code
   nachvollzogen, nur gemessen.
5. UFTs `uef`-Plugin (Tier `null`, kein Tier-Eintrag) vs. DIMs
   CFS/UEF-Leser (`DiscImage_CFS.pas`, inkl. zlib-komprimierter UEF) —
   nicht vertieft; Fundus für einen späteren Zyklus.
6. DIM2 („VirtualImageManager", README-Verweis) — nicht untersucht.

## 10. Verdikt für die Negativliste

`bewertet` — Gutachten liegt vor; GPL-3.0 (GELB), nur Spec+Oracle;
wertvollster Ertrag: einziges bekanntes hardwarefreies Acorn-FS-Hash-
Oracle + fertige ADFS-Spec-Quellen + zwei Korrektheitsbefunde im
eigenen Baum (SCOUT-25/28).
