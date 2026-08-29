# Gutachten: CBM-Korpus-Erzeuger (Dreier-Zyklus)

**Repos:** `Zirias/c64_tool_mkd64` (Klon `work/mkd64/`, HEAD `59009db7`,
2018-12-20) · `beddy70/DiskToolC64` (Klon `work/DiskToolC64/`, HEAD
`02b314a0`, 2022-10-12) · `markusC64/floppydiskimagetool` (Klon
`work/floppydiskimagetool/`, HEAD `efba72fd`, 2026-08-07)

**Messungen:** `work/mkd64.messung.json` (69 Dateien, 6 276 LOC C) ·
`work/DiskToolC64.messung.json` (30 Dateien, 1 246 LOC Java) ·
`work/floppydiskimagetool.messung.json` (40 Dateien, 2 163 LOC psm1 +
1 001 984 B geschlossene .NET-DLL) — jede Zahl aus `git ls-files` +
`wc -l` im jeweiligen Klon.

**Anlass (neu, seit MF-649/650):** Der A5-Fix
(`src/formats/d64/uft_d64_parser_v3.c:1049-1075`, MF-649) sondiert
DolphinDOS/SpeedDOS-BAM — aber im Korpus liegt **kein einziges
40-Spur-D64** (Abfrage `inv["korpus"]`, 22 Einträge, einziges D64:
`vice_c1541_35trk.d64`). Gesucht war die **Erzeuger**-Seite; die
Leseseite (floptool, libcbmimage) ist abgedeckt.

**Inventar-Stand:** `inventar.py build` lief zuerst, rc=0
(`OK: 88 Plugins (SSOT ok), 232 Format-Dirs, 88 Tier-Zeilen,
22 Korpus-Abbilder, HEAD 18fe2bcc`).

---

## 1. Kurzurteil je Repo

| Repo | Kategorie | Lizenzzone | Urteil in einem Satz |
|---|---|---|---|
| mkd64 | **Daten-Erzeuger** (Fixture-Generator) | **PRÜFEN** (Eigenlizenz, s. §5) | Erzeugt exakt die fehlenden 40-Spur-Prüfstücke; gebaut, gelaufen, kreuzverifiziert — aber **gleiche Hand wie lib1541img**, darum nie Oracle für den A5-Fix |
| DiskToolC64 | Oracle-Reserve (dritte Hand, 35 Spuren) | **ROT** (keine Lizenzdatei) | Läuft, extrahiert korrekt (127-B-Wahrheit, §7) — bewegt aber keine Kennzahl über das hinaus, was cbmimage schon leistet → Fundus |
| floppydiskimagetool | Oracle-Reserve (G64/GCR-Pipeline) | **ROT** („All rights reserved", Kern ist geschlossene DLL) | D64→G64→D64 35-Spur byteidentisch gemessen; 40-Spur-Mastering versagt still (§4); G64→D64 hat bereits einen Matrix-Eintrag → keine Kennzahl bewegt → Fundus |

## 2. Was das Inventar sagt (Abfrage zitiert)

`inventar.py query work/inv.json …` (2026-08-29):

```
"d64":        vorhanden: true,  tier: "T1b", plugin_liste_vollstaendig: true
"g64":        vorhanden: true,  tier: "T1",  plugin_liste_vollstaendig: true
"speeddos":   abgedeckt: false  → von Hand geprüft: MF-649-Fix vorhanden,
"dolphindos": abgedeckt: false    src/formats/d64/uft_d64_parser_v3.c:1049
                                  (D64_BAM_DOLPHIN_BASE 0x1c, SPEED 0x30)
"bam":        abgedeckt: false  → Fähigkeit, kein Formatname; Handprüfung s.o.
```

Formate sind vorhanden; was fehlt, ist **Korpus**, nicht Code. Kein
Format-Vorschlag in diesem Zyklus (Einfrier-Regel; neue Formate wären
ohnehin Fundus).

## 3. mkd64 — der Fund

### 3.1 Was es ist

Modularer D64-Ersteller von Felix Palmen (Zirias), C11, Kern +
ladbare Module (`cbmdos` = BAM/Directory, `xtracks` = Spuren 36-40,
`sepgen` = Verzeichnis-Trenner). Letzter Commit 2018 — für einen
Erzeuger unerheblich, das D64-Format ist eingefroren.

### 3.2 Die entscheidenden Zeilen

`src/lib/mkd64/xtracks/module.c:96-113`: Option `-X d` schreibt
DolphinDOS-Einträge nach `bam + 0xac + 4*i`, `-X s` SpeedDOS nach
`bam + 0xc0 + 4*i` (je `11 ff ff 01` = 17 frei);
`module.c:170-176` hält die Einträge bei Belegung nach
(`0xac + 4*(track-36)` bzw. `0xc0 + …`).

Das deckt sich **byte-genau** mit dem MF-649-Fix
(`uft_d64_parser_v3.c:1049`: Dolphin `0x1c+4*track` → Spur 36 = 0xAC;
Speed `0x30+4*track` → 0xC0) — und, wichtiger, mit einer **dritten,
unabhängigen** Quelle: die VICE-Dokumentation (vice_17, Abschnitt
D64-Format) nennt wörtlich „SPEED DOS stores them from $C0 to $D3" und
„DOLPHIN DOS stores them from $AC to $BF" (WebFetch 2026-08-29,
vice-emu.sourceforge.io/vice_17.html). Die Offsets hängen damit nicht
mehr allein an der Zirias-Werkstatt.

### 3.3 Bau- und Laufbeleg (nichts auf Zusicherung)

* Bau: `git submodule update --init` (zimk), dann unter CMD mit
  bereinigtem PATH `mingw32-make CC=gcc` (gcc 13.1.0) → `mkd64.exe` +
  `cbmdos.dll`/`xtracks.dll`/`sepgen.dll`, rc=0. Zwei Fallstricke
  dokumentiert: das zimk-Buildsystem kippt, sobald `sh` im PATH liegt,
  und `cc` existiert in MinGW nicht (`CC=gcc` nötig).
* Lauf (Scratchpad, Kopien in `work/cbm_fixtures/`, SHA-256 in
  `work/cbm_fixtures/SHA256SUMS.txt`):

| Abbild | Kommando (verkürzt) | Größe | Inhalt |
|---|---|---|---|
| `uft35.d64` | `-m cbmdos -d "UFT SCOUT 35" -i 82 -g -o … -f payload1.prg -n "FILE ONE" -w -f payload2.prg -n "FILE TWO" -T s -w` | 174 848 B | 2 Dateien (PRG+SEQ), 656 Blöcke frei |
| `uft40_dolphin.d64` | `… -m xtracks -X d -g …` + Datei mit `-t 36 -s 0` | **196 608 B** | Datei „BIG XTRACK" belegt Spuren 36-37; Dolphin-BAM @0xAC: `00…`, `0a b6 d9 00`, 3×`11 ff ff 01`; Speed-Bereich 0x00 |
| `uft40_speed.d64` | `… -X s …` | **196 608 B** | spiegelbildlich @0xC0 |

* **Deterministisch:** zweiter Lauf mit identischen Optionen →
  identische SHA-256 (`cff71680…`), gemessen.

### 3.4 Kreuzverifikation (zwei fremde Hände + Rohbytes)

* **libcbmimage `cbmimage.exe`** (Spiro Trikaliotis, GPL-2.0, Commit
  `1e2673ff`, in MF-650 gebaut — unabhängig von Zirias): `dir` listet
  alle Dateien inkl. „BIG XTRACK 36/0"; `showfile --numerical=N`
  liefert die Inhalte **byteidentisch** zu den Payloads — auch die
  6 002-B-Datei, die über Spuren 36-37 läuft (Hexdump geparst,
  sha1-gleich). Das ist die **starke** Stufe des
  Differenzlauf-Standards (Inhalt, nicht Listing; MF-629).
* **floptool** (registriertes Oracle, Hash `6973d1b5…` = Registry):
  `flopdir d64 cbmdos uft35.d64` → Volume + beide Dateien korrekt.
  Auf den 40-Spur-Abbildern: **„Block number overflow: requiring block
  683 on device of size 683"** — floptools cbmdos-Sicht ist auf 35
  Spuren festgenagelt. Gemessen, nicht vermutet: floptool ist **kein**
  40-Spur-Oracle.
* **Rohbytes:** BAM-Hexdump der erzeugten Abbilder (Python, §3.3) —
  Zählbyte = Anzahl gesetzter Bitmap-Bits, exakt das
  Selbstbestätigungs-Kriterium des MF-649-Fixes.

### 3.5 Zirkularität — die fünfte Registrierungsfrage (MF-644)

**mkd64 und lib1541img sind dieselbe Hand** (Felix Palmen; mkd64
README/`debian/copyright`, lib1541img = Zirias/excess-c64). lib1541img
ist seit MF-649 die benannte Referenz des BAM-Fixes. Daraus folgt die
Rollentrennung, ausdrücklich:

* **Als Fixture-ERZEUGER zulässig.** Die erzeugten Abbilder wurden
  nicht von der Zirias-Werkstatt beurteilt, sondern von libcbmimage
  (Inhalte), floptool (35-Spur-Fall) und der VICE-Doku (Offsets).
* **Als ORACLE für den A5/MF-649-Fix unzulässig.** mkd64-erzeugt +
  lib1541img-referenziert wäre Selbstkonsistenz derselben Hand, grün
  ohne Beweiskraft.

Wer die Fixtures in T1b-Einträge übernimmt, schreibt als Erzeuger
„mkd64 1.4b (Zirias)" ins Manifest und als prüfende Gegenstelle
**nicht** lib1541img.

## 4. floppydiskimagetool — gemessen, dann eingeordnet

pwsh-7-Modul + geschlossene .NET-DLL (markusC64). Import läuft
(`Import-Module …\FloppyDiskImageTool.psd1`; ein Provider-Init-Fehler
`CarFSWritableProvider`, danach 40+ Cmdlets nutzbar). Gemessen:

* **D64→G64→D64, 35 Spuren, byteidentisch:** `Get-FloppyDiskImage
  -SectorImage -SectorImageType D64` → `New-FloppyDiskTemplate -Floppy
  floppy1541` → `Convert-FloppyDiskImage -TemplateToText -SourceDxx` →
  `-TextToBitstream` → `Export … -Bitstream` ergibt `GCR-1541`-G64
  (278 234 B, 84 Halbspuren, tracksize 7928); Rückweg
  `Convert-G64resp71ToDxx -numTracks 35` → 174 848 B, **identisch zur
  mkd64-Quelle** (`work/cbm_fixtures/fdit_uft35.g64`).
* **40 Spuren: still unvollständig.** Dieselbe Kette auf
  `uft40_dolphin.d64` mastert nur Spuren 1-35; die Rückwandlung liefert
  alle 85 Blöcke der Spuren 36-40 leer, Error-Map-Code 2 (Diff-Zählung:
  21 760 Bytes, ausschließlich T36-T40). Kein Fehler, keine Warnung —
  ein stiller Datenverlust im Sinne unserer Verfassung.
* G64→D64 hat in `src/core/uft_roundtrip.c:146-177` bereits einen
  Eintrag (LOSSY_DOCUMENTED, gegen das VICE-Paar gemessen) — dieses
  Werkzeug **bewegt keinen Wandlungspfad-Zähler**. SCP-/KryoFlux-/
  P64-Ingestion von CBM-Disketten ist vorhanden (`Get-FloppyDiskImage
  -SCPImage/-KryofluxImage`, ungemessen) → Fundus.

## 5. Lizenz (aus den Dateien, je Repo)

* **mkd64:** keine Wurzel-LICENSE; einzige Lizenzaussage
  `debian/copyright` — Eigenlizenz „mkd64": *„free software and may be
  used or modified for any purpose"*, Bedingungen: Lizenz+Copyright
  beilegen, Änderungen kenntlich machen. Kein Quell-/Header-File trägt
  eine Lizenz (grep über `src/ include/`: 0 Treffer). Liest sich
  BSD-artig, **ist aber keine gelistete Zone der Matrix → PRÜFEN,
  Eigentümer-Vorlage, falls je Code portiert werden soll.** Für diesen
  Zyklus irrelevant: es wandert kein Code, nur **Werkzeug-Ausgaben mit
  eigenem Inhalt** (unsere Payloads, unser Diskname) — die erzeugten
  D64s sind keine Ableitung des mkd64-Codes. Submodul `zimk/` hat eine
  eigene LICENSE (Build-System, wird nicht verteilt).
* **DiskToolC64:** **keine** Lizenzdatei, keine Lizenz-Header (nur
  NetBeans-Platzhalter) → ROT, alle Rechte vorbehalten (Eddy Briere,
  2019). Ausführen des vom Autor selbst veröffentlichten
  `dist/DiskToolC64.jar` bleibt möglich (Matrix: Oracle-Binary „falls
  legal beziehbar"); Code lesen ja, portieren nein.
* **floppydiskimagetool:** keine Lizenzdatei; beide `.psd1` sagen
  wörtlich `Copyright = '(c) markusC64. All rights reserved.'`;
  `LicenseUri` auskommentiert. Kern `CommodoreDiskImageTool.dll`
  (1 001 984 B, PE32 .NET) **ohne Quelltext im Repo** → ROT. Nur
  Blackbox-Ausführung; keine Verhaltens-Spec aus der DLL ableiten, die
  über beobachtetes I/O-Verhalten hinausgeht.
* **Attribution (MF-636):** Es wird keine Portierung vorgeschlagen;
  keine „based on"-Erklärung nötig. Die Fixtures nennen ihren Erzeuger
  im Manifest (mkd64 1.4b, Eigenlizenz, Commit `59009db7`).

## 6. Oracle-Tauglichkeit — die drei Fragen aus `docs/ORACLES.md`

| Frage | mkd64 | DiskToolC64 | floppydiskimagetool |
|---|---|---|---|
| Konsole? | ja (CLI, Optionssprache) | ja (`java -jar … -d/-g/-p`) | ja (pwsh-Cmdlets), aber Objektmodell statt Klartext |
| Inhalte/Hashes je Datei? | **nein** — nur Erzeugen, kein Lesen | **ja, stark**: `-g` extrahiert; 127-B-Probe byte-korrekt (§7) | mittelbar (FS-Provider `Mount-FloppyDiskImage`, ungemessen); Bitstream-Ebene stark |
| Unabhängige Hand? | **nein** für CBM-BAM (= Zirias = lib1541img) | ja (eigene Java-Codebasis) | ja (markusC64; libcbmimage ist Trikaliotis, `git log`: 8/8 Commits) |
| Herkunfts-Anker | `-V` → „mkd64 1.4b" + Commit | keine Versionsabfrage → SHA-256 des jar nötig | DLL ohne Versionsabfrage → SHA-256 |

**Keiner der drei wird als Oracle-Registrierung vorgeschlagen.** mkd64
scheitert an Frage 5, die beiden anderen bewegen keine Kennzahl über
den Bestand (floptool + cbmimage) hinaus.

## 7. Zwei Baum-Befunde aus der Gegenprobe (beide gemessen)

### 7.1 `docs/ORACLES.md` führt den floptool-gepaddeten Wert als Messung

Gegenprobe am Korpus-D64 (`tests/corpus_free/vice_c1541_35trk.d64`):
Rohkette gelesen — Directory-Eintrag `UFT MARKER` startet 17/0, der
**letzte Sektor trägt Link `(0, 0x80)`**, nach CBM-Regel also
**127 Nutzbytes**, sha1 `a9fb8f28e89edfbf69cdcb97a0adb50fea01c130`.
Zwei unabhängige Hände bestätigen das: cbmimage `showfile` (127 B,
gleiche sha1) und DiskToolC64 `-g` („size=254 | real size=127";
`FileSystemCBM.java:352` implementiert `(nextSector&0xff)-1` korrekt).

`docs/ORACLES.md` (§Differenzlauf-Standard) und `docs/PLAN_v4.1.7.md:117`
sagen dagegen „`UFT MARKER`, 254 Byte, sha1 `56fea729…`" — das ist die
sha1 der **vollen 254-B-Sektorkapazität**: floptool `flophashes`
ignoriert das Längenbyte des letzten Sektors und paddet (Gegenprobe an
frischem Abbild: 502-B-Datei → floptool meldet 508 = 2×254). Der Wert
ist als floptool-Ausgabe echt, als **Inhalts-Referenz** falsch — und er
steht ausgerechnet in der Zeile, die die „starke" Vergleichsform
definiert. Jeder künftige Differenzlauf UFT-vs-floptool auf CBM-Dateien
braucht diese Padding-Toleranz, sonst ist er rot ohne Fehler oder grün
ohne Beweis.

### 7.2 libcbmimage liest die 40-Spur-BAM aus dem Disknamen — wie UFT vor MF-649

`cbmimage open uft40_dolphin.d64 bam` meldet für Spuren 36-40 die
„freien Blöcke" **85, 68, 72, 52, 160** — das sind die Bytes
`'U','D','H','4',0xA0` aus „UFT DOLPHIN 40" ab `bam+0x90`; `checkbam`
meldet die Widersprüche brav. Ursache im Quelltext: **alle**
40-Spur-Typen (`TYPE_D64_40TRACK`, `_SPEEDDOS`, `_DOLPHIN`,
`_PROLOGIC`) teilen sich die 35-Spur-BAM-Beschreibung `i_d40_d64`
(`lib/d40_d64_d71.c:373-374`: Zähler ab 0x04, Schrittweite 4, ohne
Ende bei Spur 35; geteilt über die switch-Fälle `:443-446` und die
open-Funktionen `:601-668`). Nach 0xAC/0xC0 greift **nichts** (grep
über `lib/`: einziger 0xAC-Treffer ist ein `border_sector` in anderem
Kontext, `:332`). Die Typnamen existieren, die Layouts nicht.

Folge für SCOUT-55 (OpenCBM-Gutachten): die dort empfohlene
Registrierung „cbmimage `bam`/`checkbam` als BAM-Oracle für A5" ist in
diesem Punkt **nicht haltbar** — cbmimage hat auf den erweiterten
Spuren denselben Fehler, den MF-649 bei uns behoben hat.
Directory-/Datei-Ebene von cbmimage bleibt davon unberührt und ist hier
mehrfach als korrekt gemessen.

## 8. Einhängepunkte (im Baum auffindbar)

* `docs/plans/UMSETZUNGSLISTE.md` §A5 (erledigt, aber die
  Sondierungs-Logik lief nie gegen ein realistisches Abbild fremder
  Herkunft) und §A2 (D64-Verzeichnis ausleiten — braucht die
  Padding-Toleranz aus §7.1).
* `docs/PLAN_v4.1.7.md` §Korpus/Phase-1-Tabelle (d64 T1b) und §„Oracle
  und Korpus-Erzeuger dürfen nicht dieselbe Hand sein" (:127ff).
* `docs/ORACLES.md` §Differenzlauf-Standard (die 254-B-Zeile) und
  §floptool (Fallstrick-Liste).

## 9. OPEN_ITEMS-Vorschläge (3 von max. 5 — Nummern vorläufig)

### SCOUT-82 — 40-Spur-D64-Fixtures (Dolphin + Speed) aus mkd64 in den Korpus

**Kennzahl: ungeprüfte Formate runter** (sichert die d64-Hebung ab:
A5-Sondierung erstmals gegen fremd-erzeugte 40-Spur-Abbilder mit
belegten Extra-Spuren; heute prüft `tests/test_d64_bam_40track.c` nur
selbstgebaute Bytes). Drei Abbilder liegen verifiziert in
`tools/uft-scout/work/cbm_fixtures/` (SHA256SUMS.txt): 35-Spur mit 2
Dateien, 40-Spur Dolphin, 40-Spur Speed — Erzeuger mkd64 1.4b
(`59009db7`, deterministisch, Rezept §3.3), Inhalte durch libcbmimage
byteidentisch gegengelesen, Offsets durch VICE-Doku gedeckt.
Übernahme nach `tests/corpus_free/` + Manifest ist Stufe-4-Arbeit;
Manifest nennt Erzeuger-Hand Zirias und **schließt lib1541img als
Gegenstelle aus** (MF-644). Aufwand **S**.

### SCOUT-83 — floptool-Padding: Referenzwert in ORACLES.md/PLAN_v4.1.7 richtigstellen

**Kennzahl: ungeprüfte Formate runter** (hält die Messlatte der
T1b→T1-Hebungen ehrlich; ein Differenzlauf gegen den gepaddeten Wert
urteilt falsch — in beide Richtungen). Gemessen §7.1: wahre
Datei-Inhalte `UFT MARKER` = **127 B**, sha1 `a9fb8f28…` (Rohkette,
Link-Byte 0x80; zwei unabhängige Hände einig); floptool `flophashes`
paddet auf Sektorkapazität (254/508 B gemessen). Fix: beide Stellen als
„floptool-gepaddet" kennzeichnen, wahren Wert daneben, Toleranzregel in
die floptool-Fallstrick-Liste. Aufwand **S**.

### SCOUT-84 — SCOUT-55 einschränken: cbmimage ist kein Extended-BAM-Oracle

**Kennzahl: ungeprüfte Formate runter** (verhindert eine
Schein-Hebung: ein A5-Differenzlauf gegen cbmimage-`bam` wäre auf
Spuren 36-40 der Vergleich zweier Implementierungen desselben Fehlers —
vor MF-649 beidseitig grün gewesen). Gemessen §7.2 (Werkzeug + Quelle
`lib/d40_d64_d71.c:373-374`, Commit `1e2673ff`). Fix: falls/wenn
cbmimage nach `docs/ORACLES.md` registriert wird, Geltungsbereich
ausdrücklich „Directory + Datei-Inhalte, KEINE erweiterten
BAM-Zählbytes"; Referenz für die Offsets bleibt VICE-Doku
($AC-$BF/$C0-$D3) + Selbstbestätigungs-Kriterium des Fixes.
Aufwand **S**.

## 10. Fundus (keine Kennzahl bewegt — notiert, nicht eingeplant)

* floppydiskimagetool als G64-Zweitmeinung (Rundlauf 35-Spur
  byteidentisch, §4) und als SCP/KryoFlux/P64-Leser für CBM-GCR —
  ROT-Zone, geschlossene DLL, nur Blackbox.
* DiskToolC64 als dritte unabhängige Hand für 35-Spur-Dateiinhalte
  (127-B-Probe bestanden) — jar ohne Lizenz, Java-Abhängigkeit.
* mkd64 `-M MAPFILE` (Start-T/S je Datei) als maschinenlesbares
  Erwartungs-Nebenprodukt für Korpus-Tests.
* mkd64-Modul-API (Interleave je Datei, feste Startsektoren,
  Directory-only-Abbilder) für gezielte Randfall-Fixtures
  (z. B. Interleave-Tests für A2).
* Merkwürdigkeit am Rande: DiskToolC64 allokiert ab Spur 1 statt um
  Spur 18 — ergäbe Fixtures mit unüblicher, aber gültiger Belegung.

## 11. Beschaffungsliste

| Was | Wofür | liegt schon? (gegen `inv["korpus"]` geprüft) |
|---|---|---|
| 40-Spur-D64 Dolphin/Speed + 35-Spur-Mehrdatei | SCOUT-82 | **ja, neu erzeugt** — `work/cbm_fixtures/`, Hashes im SHA256SUMS.txt; im Korpus selbst: nein (einziges D64 ist 35-Spur) |
| 40/42-Spur-Abbild **historischer** Herkunft (echtes SpeedDOS-formatiertes Material) | Härtetest jenseits synthetischer Erzeuger | nein — bleibt offen wie im OpenCBM-Gutachten (§10 dort), niedrige Priorität |
| Nichts weiter | — | Prologic-Variante bewusst nicht angefordert: kein Erzeuger in diesem Zyklus schreibt sie |

## 12. Differenzlauf-Plan (für die „besser/richtig"-Aussagen oben)

Für SCOUT-82-Nutzung in Stufe 4: **UFT-A5-Leser vs. Rohbyte-Erwartung**
auf `uft40_dolphin.d64`/`uft40_speed.d64` — Metrik: `free_blocks`
gesamt (erwartet 723 gemäß mkd64-Lauf, §3.3) und Zählbyte je Spur
36-40 (erwartet 0/10/17/17/17); Toleranz: keine. Gegenstelle für die
Datei-Ebene: cbmimage `showfile` (byteweise). floptool nur für den
35-Spur-Fall (Toleranz: Längen-Padding auf Sektorkapazität, §7.1).
Beide Binaries liegen (`cbmimage.exe` aus MF-650-Bau, floptool-Hash
`6973d1b5…` = Registry-Eintrag).

## 13. UNGEKLÄRT

1. **mkd64-Eigenlizenz** — BSD-artig, aber unbenannt: Zonierung ist
   Eigentümer-Sache, **nur relevant, falls je Code portiert würde**
   (derzeit nicht vorgeschlagen).
2. **floppydiskimagetool 40-Spur-Mastering** — ob ein anderer
   Template-/Parameterweg (`no-tracks 84` steht im Template-Text) die
   Spuren 36-40 doch mastert, wurde nach dem gemessenen stillen
   Verlust (§4) nicht weiter verfolgt.
3. **`Mount-FloppyDiskImage`-Dateiextraktion** (floppydiskimagetool) —
   nicht gemessen; für die Fundus-Einordnung ohne Belang.
4. **Ob libcbmimage-Upstream den §7.2-Befund kennt** — nicht
   recherchiert; eine Upstream-Meldung wäre Eigentümer-Entscheidung
   (fremdes Projekt).
5. **DiskToolC64 42-Spur-Verhalten** — `DISK_SIZE = 174848` ist
   hartkodiert (`FileSystemCBM.java:67`); Verhalten auf über-großen
   Abbildern nicht gemessen.

---
*Scout-Zyklus CBM-Erzeuger, 2026-08-29. Alle Läufe auf dieser Maschine;
Artefakte: `work/cbm_fixtures/` (gitignored), Messungen
`work/{mkd64,DiskToolC64,floppydiskimagetool}.messung.json`. Keine
Hardware berührt (MF-310). Kein Code geschrieben, nichts nach `src/`,
`include/`, `tests/`.*
