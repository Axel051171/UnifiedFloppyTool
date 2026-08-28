<!-- uebernommen: MF-650 -->
<!-- stufe: 3 -->
# Gutachten: jfdelnero/HxCFloppyEmulator

Stand: 2026-08-28 · Scout-Zyklus (SCOUT-49…52)
Messung: `work/HxCFloppyEmulator.messung.json`
(HEAD `05b53aa`, letzter Commit 2026-08-22, shallow clone; die 276
`.o`-Dateien im Sprachhistogramm stammen aus dem eigenen Oracle-Build
dieses Zyklus, nicht aus dem Repo)
· Inventar: UFT `548f204b`, erzeugt 2026-08-28T21:18Z, SSOT ok,
22 Korpus-Abbilder

**Anlass (Regel 6, Neubesuch-Klausel):** kein Neubesuch — das Repo war
nie selbst begutachtet, nur vier Nachbarn (HXCFE_file_selector,
TRS80_HxC, HxCFloppyImageConverter, AdfOpus_HxC). Der Auftrag kommt aus
zwei benannten Stellen im Baum: `docs/OPEN_ITEMS.md:2356` benennt dieses
Repo als **Folgeziel** (libhxcfe-Quelltext, AmigaDOS-Encoder), und
`docs/plans/FLUXENGINE.md:57-61` macht die drei HxC-Attributionen zur
**Vorbedingung** der GPLv3-Türfrage (`LIZ-1`).

---

## TL;DR

1. **`LIZ-1`-Kernfrage beantwortet: die Tür bleibt offen.** HxC-eigener
   Code ist **GPL-2.0-or-later** (Datei-Header, `COPYING_FULL`, Ausgabe
   von `hxcfe -license`); **kein** GPL-2.0-only, **kein** GPL-3.0-only
   im Erstautor-Code. Und die drei Attributionen in unserem Baum sind
   nach Idiom-Prüfung in beide Richtungen **eigenständige
   Implementierungen**, keine Ports — selbst im schlimmsten Fall hätte
   GPLv2+ die GPLv3-Tür nicht geschlossen.
2. **`hxcfe` ist als Oracle gebaut und gemessen** (nicht zugesichert):
   v2.16.15.2, MinGW64, zwei Shim-Zeilen nötig. ADF→HFE→ADF
   byte-identisch; liest unsere ADF/HFE/SCP/ATR/FDI-Korpusdateien;
   liest unser G64 **nicht**; D64 nur strukturell.
3. **Der AmigaDOS-MFM-Encoder liegt vor, Zone GRÜN, portierbar** —
   `tg_addAmigaSectorToTrack()` schließt die MF-539-Lücke (ADF→HFE
   abgelehnt), mit dem gebauten `hxcfe` als Rotbeweis-Oracle.
4. **B4 ist ohne Lizenzfrage lösbar:** aus unserem eigenen Korpus-ADF
   erzeugt `hxcfe` ein HFE mit `track_encoding=0x01` /
   `interface_mode=0x04` — gemessen, Rezept unten.

Kategorien: **Oracle** (hxcfe CLI) + **Verbesserung** (Encoder-Port für
abgelehnten Wandlungspfad) + **Daten** (Fixture-Rezept) + Lizenzbefund.

---

## Teil A — Der `LIZ-1`-Auftrag

### A.1 Welche Lizenz trägt HxCFloppyEmulator wirklich?

Je Unterverzeichnis geprüft (Vendoring-Regel), Wortlaut zitiert:

| Quelle | Befund (Wortlaut) |
|---|---|
| `libhxcfe/COPYING`, `HxCFloppyEmulator_cmdline/COPYING`, `HxCFloppyEmulator_software/COPYING`, `libhxcadaptor/COPYING`, `libusbhxcfe/COPYING` | jeweils GPL-Volltext „GNU GENERAL PUBLIC LICENSE **Version 3, 29 June 2007**" (674 Zeilen, Z. 1-2) |
| `libhxcfe/COPYING_FULL` Z. 18-21 (identisch in allen 5 Unterverz.) | „HxCFloppyEmulator is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; **either version 2 of the License, or (at your option) any later version.**" |
| Datei-Header, z. B. `libhxcfe/sources/floppy_loader.c:14-15`, `libhxcfe/sources/tracks/track_formats/amiga_mfm_track.c:14`, `HxCFloppyEmulator_cmdline/sources/hxcfe.c` | „either version 2 of the License, or (at your option) any later version" |
| Ausgabe `hxcfe.exe -license` (selbst gebaut, gemessen) | „either version 2 of the License, or (at your option) any later version" |

**Zählung (Methode: `grep -rl "either version 2"` bzw. `"either
version 3"` über `libhxcfe/sources --include=*.c --include=*.h`,
Nenner `find … -name '*.c' -o -name '*.h'`):** 459 von 842 Dateien
tragen den GPLv2+-Grant, **0** Dateien einen Version-3-Grant. Die 383
ohne Grant sind: vendorte Fremdbibliotheken
(`thirdpartylibs/`: zlib, expat, lz4, xdms-1.3.2, FATIOlib, adflib,
libspng, libsap), **generierte** Header (`xml_disk/DiskLayouts/*.h`,
`licensetxt.h`, `init_script.h`), kopf­lose interne Header (`types.h`,
`plugins_id.h`, …) und `tracks/std_crc32.c` (Gary S. Brown, permissiv,
Kopfzeile 2).

**Zone nach `playbook/lizenzmatrix.md`: formal PRÜFEN** (Zeile
„Datei-Header ≠ Repo-Lizenz": COPYING trägt GPLv3-Text, die Grants
sagen v2+). **Konsequenz:** die operative Lizenzaussage — COPYING_FULL,
jeder lizenzierte Datei-Header, die Laufzeitausgabe des Binaries — ist
einheitlich **GPL-2.0-or-later**, also Matrix-Zeile GRÜN
(„GPL-2.0(-only/or-later) ✅ mit Attribution-Header"). Für einen
Code-Port gilt trotzdem die PRÜFEN-Eskalation: **Eigentümer-Vorlage**
mit genau diesem Befund, keine eigene Auslegung (siehe SCOUT-51).
Vendoring-Ausnahmen je portierter Datei erneut prüfen.

**Für die Türfrage aus `docs/plans/FLUXENGINE.md:57-61` heißt das,
doppelt abgesichert:**

* Es gibt im HxC-Erstautor-Code **kein GPL-2.0-only** (0 Treffer,
  Methode oben). Selbst ein echter Port hätte die GPLv3-Tür nicht
  geschlossen — GPLv2+ ist mit v2 **und** v3 verträglich.
* Und es ist kein Port (A.2/A.3).

### A.2 Die drei Attributionen in unserem Baum

Suchmethode: `grep -rn -i -E "(based on|adapted from|derived from|port
of|…).*hxc" src/ include/` plus Abgleich mit der Attributionsliste von
`scripts/audit_spdx_policy.py` (Zeilen 50/54/55 des Laufs vom
2026-08-28). Es sind **drei Attributionszeilen in zwei Sachverhalten**:

| # | Datei:Zeile | Wortlaut |
|---|---|---|
| 1 | `src/rawformatdialog.h:6` | „Based on HxCFloppyEmulator's RAW format configuration." |
| 2 | `src/visualdiskdialog.cpp:6` | „Based on HxCFloppyEmulator's Visual Floppy Disk view." |
| 3 | `src/visualdiskdialog.h:6` | „Based on HxCFloppyEmulator's Visual Floppy Disk view." |

Alle drei Dateien sind gebaut und verdrahtet (`.pro:251-252, 301-302,
377-378`; Aufrufer `src/toolstab.cpp`), im Baum seit v4.1.0
(`git log --diff-filter=A`: Commit `4d622192`, 2026-02-08; `@date` im
Kopf: 2026-01-12). Die Vorrangregel aus `QUARANTINE_PROCESS.md` §1
greift nicht — beide haben Aufrufer, also **Herkunftsaudit**.

Zusätzlich existiert ein vierter, unkritischer Fall:
`include/uft/formats/flux/uft_hxcstream.h:9` „Reference: HxC Floppy
Emulator project by Jean-Francois DEL NERO" — Klasse C (Spec-Verweis),
vom Zensus korrekt nicht als Ableitung geführt.

### A.3 Ableitung oder Spec-Verweis? (Idiom-Methode, beide Richtungen)

Mutmaßliche Quellen im HxC-Baum:

* für `visualdiskdialog`: `libhxcfe/sources/tracks/display_track.c`
  (4188 Z., die einzige Disk-/Track-Visualisierung; GUI-Fenster
  `cb_floppy_infos_window.cxx` ruft sie nur auf)
* für `rawformatdialog`:
  `HxCFloppyEmulator_software/sources/gui/rawfile_loader_window.*` +
  `cb_rawfile_loader_window.cxx`

**Richtung 1 — HxC-Idiome in unseren Dateien (Ergebnis: 0 Treffer).**
Gesucht in `src/visualdiskdialog.{cpp,h}`, `src/rawformatdialog.{cpp,h}`:
`pushTrackCode`, `sectorconfig`, `trackencoding`, `HXCFE_TD`,
`intersidesectornumbering`, `reversesides`, `fillvalue`, `chk_`,
`numin_` → **alle 0**. Auch kein Treffer für `hxcfe`, `td_`,
`putchar8x8`, `angle_step`, `layer`.

**Richtung 2 — unsere Idiome im HxC-Baum (Ergebnis: 0 Treffer).**
Gesucht in `libhxcfe/sources`, `HxCFloppyEmulator_software/sources`,
`HxCFloppyEmulator_cmdline/sources`: `generateSampleData`,
`SectorAlternate`, `statusColor`, `Center hole`, `interSideNumbering`,
`sidesGrouped`, `layoutPreset`, `Draw tracks from outside` → **alle 0**.

**Strukturvergleich `visualdiskdialog` vs. `display_track.c`:**
grundverschiedene Verfahren. HxC rendert einen eigenen
Software-Framebuffer bitcell-genau aus Flux-/Timing-Daten
(`plot`/`line_fast`/`putchar8x8`/`draw_circle` mit
`td->angle_step = 0.001`, `display_track.c:266/306/125/3023/866`);
unser Widget malt Qt-`QPainterPath`-Tortensegmente pro
Sektor-**Status** (`visualdiskdialog.cpp:106-118`, `arcTo`), eine ganz
andere Datengranularität. Das String-Vorkommen „ISO MFM" ist beidseits
generische Encoding-Nomenklatur (`display_track.c:3519` als
Tabelleneintrag, bei uns als Label-Text) — Fakt, kein Idiom
(`QUARANTINE_PROCESS.md` §4).

**Strukturvergleich `rawformatdialog` vs. `rawfile_loader_window`:**
Die **Optionsmenge** ist erkennbar HxC nachempfunden —
`interSideNumbering`/`reverseSide`/`autoGap3`/`preGapLength`/
`interleave`/`skew`/`formatValue` (`rawformatdialog.h:59-72`) spiegeln
`intersidesectornumbering`/`reversesides`/`autogap3`/`pregap`/
`interleave`/`skew`/`formatvalue` (`rawfile_loader_window.h:22-44`).
Das ist Feature-Imitation einer Dialog-Oberfläche, keine Code-Ähnlichkeit:
FLTK-Callbacks vs. Qt-Signals/Slots, keinerlei gemeinsame Bezeichner
(Richtung 1/2). **Der stärkste Einzelbeleg für Eigenständigkeit ist
eine divergierende Fehldeutung:** HxCs `formatvalue` ist das
**Füllbyte** der formatierten Sektoren
(`cb_rawfile_loader_window.cxx:104`:
`rfc->fillvalue=(unsigned char)rlw->numin_formatvalue->value()`);
unser `calculateFormatValue()` erfindet stattdessen eine gepackte
Geometrie-Zahl `(tracks<<16)|(sides<<8)|sectors`
(`rawformatdialog.cpp:368-377`). Ein Port hätte die
Füllbyte-Semantik mitgeschleppt; eine eigenständige Implementierung,
die nur die Oberfläche gesehen hat, rät — und hat hier falsch geraten.

### A.4 Verdikt je Datei (nach `docs/QUARANTINE_PROCESS.md`)

| Datei | Audit-Stand | Begründung |
|---|---|---|
| `src/visualdiskdialog.cpp` + `.h` | **eigenständig** (freigesprochen) | 0/9 und 0/8 Idiom-Treffer in beiden Richtungen; anderes Datenmodell, anderes Toolkit, keine Kommentar-Echos |
| `src/rawformatdialog.h` (+ `.cpp`) | **eigenständig** (freigesprochen) | Optionsmenge nachempfunden (zulässig), 0 gemeinsame Bezeichner, divergierende `formatValue`-Semantik als positiver Eigenständigkeitsbeleg |

**Keine Quarantäne.** Empfohlene Folge ist eine Kopf-Berichtigung nach
dem MF-636-Grundsatz („eine Attribution ist eine rechtliche Aussage"):
aus „Based on X" wird „Verhalten nach dem Vorbild von HxCFloppyEmulator
(GPL-2.0-or-later), **eigenständige Implementierung**" — siehe
Vorschlag SCOUT-52. Attribution **nicht** löschen (`OPEN_ITEMS.md`,
MF-636: „Eine Attribution zu löschen macht aus einer offenen Frage eine
verschwiegene").

**Getrennt und deutlich, wie beauftragt: `LIZ-1` ist für den
HxC-Anteil auflösbar, Ergebnis entlastend.** Drei der 48 offenen
Attributionen können mit „eigenständig, Quelle GPL-2.0-or-later"
geschlossen werden. Die FLUXENGINE-Türfrage ist für HxC negativ
beantwortet: keine GPL-2.0-only-Bindung, weder aus der Quelle noch aus
einer Ableitung.

---

## Teil B — Was HxC kann, was UFT fehlt

### B.1 `hxcfe` als externes Oracle — gebaut und gemessen

**Build (Windows, Qt-MinGW 13.1.0, TARGET=mingw64):**

```
export PATH=/c/Qt/Tools/mingw1310_64/bin:$PATH
cd work/HxCFloppyEmulator/build
mingw32-make TARGET=mingw64 AR=x86_64-w64-mingw32-gcc-ar RESC=windres \
    HxCFloppyEmulator_cmdline
```

Zwei Anpassungen im Klon nötig (dokumentiert, für die
ORACLES-Registrierung zu wiederholen): der Build-Helfer
`libhxcfe/sources/xml_disk/converttools/bmptob8/bmptoh.c:20`
inkludiert `<sysexits.h>`, das MinGW nicht hat → lokaler Shim-Header
(8 `EX_*`-Defines) + Include auf `"sysexits.h"` geändert. Danach baut
alles durch: `hxcfe.exe`, `libhxcfe.dll`, `libusbhxcfe.dll`.

**Pinnung:** Version „HxC Floppy Emulator : Floppy image file converter
v2.16.15.2 / libhxcfe version : 2.16.15.2"; Quelle HEAD `05b53aa`
(2026-08-22); SHA-256 `hxcfe.exe`
`a3ed7a4157d39e010fb658f464004403b16596db21fc576bc39f6b050ba7e8ac`,
`libhxcfe.dll`
`3fcc2b3a17e005d0daafd0f70c4b4fa898e6a2f54a0834240f60fefb496170fd`
(lokaler Build, Hash nur für diese Sitzung bindend — die Registry pinnt
ihren eigenen Build).

**Messläufe gegen UNSEREN Korpus** (nicht gegen Namenslisten —
AGENT.md Regel 2):

| Korpusdatei | Ergebnis |
|---|---|
| `tests/corpus_free/xdftool_dd_ofs.adf` → HFE → ADF | **byte-identisch**, SHA-256 `9af68fcc…` beidseits |
| `tests/corpus_free/gw_amigados.hfe` | gelesen: 80 Spuren, 2 Seiten, **1760 Sektoren, 0 bad** — trotz `track_encoding=0xFF` |
| `gw_amigados.hfe` → ADF | **byte-identisch** zur Quelle `xdftool_dd_ofs.adf` (`9af68fcc…`) — Drei-Hände-Dreieck: xdftool schrieb das ADF, greaseweazle das HFE, hxcfe dekodiert zurück |
| `tests/corpus/gw_amigados.scp` | gelesen: 80 Spuren, 1760 Sektoren, 0 bad (Flux-Dekodierung) |
| `tests/corpus/zxart_spectrofon01.fdi` | gelesen: 83 Spuren, 2652 Sektoren, 0 bad |
| `tests/corpus_free/atrcopy_dos2sd.atr` | gelesen: 40 Spuren, 720 Sektoren |
| `tests/corpus_free/vice_c1541_35trk.d64` | Loader gefunden, aber **0 Sektoren/0 Bytes** in `-infos` — als Sektor-Oracle für D64 unbrauchbar, nur Struktur |
| `tests/corpus_free/vice_c1541_35trk.g64` | **„No loader support the file"** — kein G64-Oracle |

Modulbestand (Methode: `hxcfe -modulelist`, Zeilenmuster
`^[A-Z0-9_]+;(R |RW);`): **181 Module, davon 38 RW**. Das ist
Namensabdeckung, keine Fähigkeitsaussage — als Oracle zählt nur der
Lauf gegen die konkrete Datei (oben).

**Fünfte ORACLES-Frage („dieselbe Hand?"):** libhxcfe ist unabhängig
von greaseweazle, VICE, atrcopy und unseren Parsern — als Gegen-Hand
für ADF/HFE/SCP tauglich. **Ausnahme DMS:** der `AMIGA_DMS`-Loader ist
vendortes xdms-1.3.2 — dieselbe Hand wie unser `uft_dms.c`-Port. Für
C6 (`dms` T3→T1b) beweist ein Differenzlauf gegen hxcfe daher nur
**Port-Treue** („unser 1940-Zeilen-Port verhält sich wie das
Original"), nicht Korrektheit gegen die Realität. Genau diese
Port-Treue ist aber die offene Behauptung aus `OPEN_ITEMS.md:2361-2368`.

### B.2 Die MF-539-Lücke: der AmigaDOS-MFM-Encoder liegt vor, Zone GRÜN

UFT lehnt ADF→HFE ausdrücklich ab
(`src/formats/uft_format_convert_bitstream.c:778`: „ADF->HFE requires
an AmigaDOS MFM encoder; this tree has only an IBM System-34
encoder…"). Die benannte Quelle:

* `libhxcfe/sources/tracks/track_formats/amiga_mfm_track.c:356-485`,
  `tg_addAmigaSectorToTrack()` — vollständiger AmigaDOS-Sektor-Encoder:
  Odd/Even-Split über `LUT_Byte2OddBits`/`LUT_Byte2EvenBits`
  (Z. 393-397), Header-XOR-Checksumme (Z. 399-403),
  Daten-XOR-Checksumme über die kodierten Longwords (Z. 422-436),
  Gap-/Sync-Aufbau aus der `isoibm_config`-LUT (Z. 369-384).
* Datei-Header: GPLv2+-Grant (Z. 14) — Matrix-Zeile GRÜN, portierbar
  „mit Attribution-Header (samdisk-Muster: Quelle+Commit im Header)".
* Wegen COPYING(v3-Text)≠Header(v2+) gilt für den Port die
  PRÜFEN-Eskalation: **Eigentümer-Vorlage vor dem Port**, mit dem
  Befund aus A.1 als Entscheidungsgrundlage.

**Rotbeweis-Weg für Stufe 4 (Pflichtteil des Gutachtens):** benannte
Referenz = `amiga_mfm_track.c` + gebautes `hxcfe` als Oracle. Erst der
rote Test: UFT ADF→HFE liefert heute `UFT_ERR_NOT_IMPLEMENTED`
(gemessen in `uft_format_convert_bitstream.c:776-784`); der Test
fordert Bit-Identität des dekodierten Rückwegs. Dann Port/Neubau mit
Referenz im Header. Abnahme = Differenzlauf (unten). Einfrier-Regel:
das ist **kein neues Format**, sondern ein Encoder für zwei bestehende
T1b-Formate (`adf`, `hfe`) und öffnet einen registrierten, bisher
abgelehnten Wandlungspfad — Bugfix/Hebungs-Kategorie, kein
Moratoriumsfall; die 1:2-Frage entfällt.

### B.3 B4-Fixture ohne Lizenzfrage: selbst erzeugen (Rezept, gemessen)

`docs/plans/UMSETZUNGSLISTE.md:138` (B4) wartet auf ein HFE mit echter
Encoding-Kennung und nennt als Ausweg „selbst erzeugen mit `hxcfe`".
Gemessen am 2026-08-28:

```
hxcfe -finput:tests/corpus_free/xdftool_dd_ofs.adf \
      -conv:HXC_HFE -foutput:hxc_amiga.hfe
```

Ergebnis-Header (xxd, Bytes 0-19):
`48584350494346 45 00 54 02 01 fd00 0000 04 01 0100` →
`track_encoding = 0x01` (AMIGA_MFM), `interface_mode = 0x04`
(AMIGA_DD), 84 Spuren, Bitrate 253 kbit/s. Damit läuft der tote Zweig
`case HFE_ENC_AMIGA_MFM:` (`src/formats/hfe/uft_hfe.c:137`) erstmals
mit einer echten Datei. **Quelle des Inhalts ist unser eigenes
Korpus-ADF** — die Zone-PRÜFEN-Frage des HxC-Distributionsabbilds aus
dem `hxcfe_amiga_copy_utility`-Gutachten entfällt vollständig.
Rückweg gemessen: dieselbe Datei dekodiert `hxcfe` byte-identisch zum
Quell-ADF. (Das HFE ist eine **erzeugte** Wandlung, kein Original —
als Parser-Fixture tauglich, als Forensik-Referenz nicht.)

### B.4 Fundus (bewegt keine Kennzahl — nicht vorgeschlagen)

* 101 Loader-Verzeichnisse / 181 Module als **Verhaltens-Spec-Quelle**
  für künftige Tier-Hebungen (Header-Layouts mit GPLv2+-Quelltext als
  benannter Referenz) — je Format erst bei anstehender Hebung ziehen.
* `libhxcfe/sources/tracks/track_formats/`: dokumentierte Encoder für
  exotische Verfahren (Arburg, Centurion, Northstar, Heathkit,
  Micraln, DEC RX02, E-mu Emulator FM) — neue Formate sind
  Einfrier-Regel-Fundus.
* `xml_disk/DiskLayouts/` (86 XML-Layouts): kuratierte Sammlung —
  sui-generis-Vorsicht der Matrix (Laufzeit-Parser-Muster, nicht
  kopieren). Nur relevant, wenn je ein Layout-Katalog ansteht.
* HxC-eigene Testdaten (`tests/data/disks_images.zip`, 16 Abbilder,
  u. a. `ADOS_880kB.adf`+`.hfe`-Paar, TD0, 86F, VDK, Apridisk):
  Lizenz = Repo-Zone, als Fixtures nicht nötig, solange wir selbst
  erzeugen können (B.3).
* `hxcfe`-Skript-Engine (`-script:`) und FS-Zugriff
  (`-putfile`/`-getfile`, FAT/AmigaDOS/…): für spätere
  Dateisystem-Differenzläufe interessant; heute ohne Einhängepunkt.

---

## Pflichtfelder

**Kategorie:** Oracle + Verbesserung + Daten (+ Lizenzbefund Teil A).

**Lizenzzone mit Konsequenz:** formal PRÜFEN (COPYING v3-Text vs.
Header v2+), operativ GPL-2.0-or-later; Oracle-Nutzung uneingeschränkt
zulässig (nur Ausgaben werden verglichen, kein Code wandert); Code-Port
nur nach Eigentümer-Vorlage (SCOUT-51). Vendorte Fremdanteile je Datei
gesondert; `capslib/` enthält **nur** die CapsAPI-Header + Runtime-Lader
(`capslibloader.c`), keinen SPS-Code — IPF liest das gebaute Binary nur
mit extern vorhandener CAPS-DLL (hier nicht installiert, nicht
gemessen).

**Attribution der Quelle:** HxCFloppyEmulator, Jean-François DEL NERO /
HxC2001, **GPL-2.0-or-later** (Beleg A.1).

**Bewegte Kennzahlen:** je Vorschlag unten benannt.

**Inventar (Abfragen zitiert, `work/inv.json`, UFT `548f204b`):**

* `hfe` → `{"vorhanden": true, "tier": "T1b",
  "plugin_liste_vollstaendig": true}`
* `adf` → `{"vorhanden": true, "tier": "T1b", "treffer": ["adf",
  "adf-copy", "adf_adl", …]}`
* `amiga` → `{"vorhanden": true, "treffer": ["amiga", "amiga_ext",
  "uft_amiga_syncs"]}` — Decoder ja; der **Encoder** ist eine
  Fähigkeit, die der Index nicht führt; von Hand nachgesehen:
  `uft_format_convert_bitstream.c:764-769` bestätigt „kein
  AmigaDOS-Encoder im Baum" (`grep -rln 0x4489` findet nur Decoder und
  Schutz-Erkennung)
* `dms` → `{"vorhanden": true, "tier": "T3"}`
* `scp` → T1b, `fdi` → T1 (bereits gehoben, kein Hebungsbedarf)
* Korpus: 22 Einträge; `xdftool_dd_ofs.adf`, `gw_amigados.hfe`,
  `gw_amigados.scp` liegen — **Beschaffungsliste ist leer**, alles
  Benötigte ist vorhanden oder selbst erzeugbar

**Einhängepunkte (im Baum auffindbar):**

* `docs/ORACLES.md` + `tests/differential/oracles.py` (Registry) —
  Muster B3 (`docs/plans/UMSETZUNGSLISTE.md:129-136`)
* `docs/plans/UMSETZUNGSLISTE.md:138` (B4)
* `src/formats/uft_format_convert_bitstream.c:778` (MF-539-Ablehnung)
  + `src/core/uft_roundtrip.c` (Matrix-Eintrag bei Erfolg)
* `docs/OPEN_ITEMS.md:1966` (`LIZ-1`) + `docs/plans/FLUXENGINE.md:57-61`
  (Türfrage) + `docs/plans/UMSETZUNGSLISTE.md` C1

**Oracle-Kandidat:** `hxcfe` v2.16.15.2 (gebaut, gemessen, Pinnung
oben). **Beschaffungsliste:** leer (gegen `inv["korpus"]` geprüft).

**Aufwandsklassen:** SCOUT-49 **S** (Registry-Eintrag + Build-Rezept),
SCOUT-50 **S** (ein Fixture + ein Test), SCOUT-51 **M**
(Eigentümer-Vorlage, dann Port ~130 Z. + LUTs + Rotbeweis + Matrix),
SCOUT-52 **S** (drei Kopfzeilen + Zensus-Lauf).

**Differenzlauf-Plan (Pflicht für SCOUT-51, da Fähigkeitsbehauptung):**

* Binaries: UFT-Konverter (nach Port) vs. `hxcfe` v2.16.15.2 (Pinnung
  oben).
* Korpus: `tests/corpus_free/xdftool_dd_ofs.adf` (liegt) + mindestens
  ein weiteres, mit `xdftool` erzeugtes FFS-ADF (selbst erzeugbar,
  keine Beschaffung).
* Lauf: beide erzeugen HFE aus demselben ADF; beide dekodieren das
  jeweils **fremde** HFE zurück zu ADF.
* Metrik: Rück-ADF byte-identisch zur Quelle (SHA-256); HFE-Header-
  Felder `track_encoding`/`interface_mode` identisch.
* Toleranzliste: Spurzahl (HxC schreibt 84, Quelle hat 80 — die 4
  Zusatzspuren sind leer/formatiert; zulässig, solange der Rückweg
  identisch bleibt); Bitraten-Feld ±2 kbit/s; Gap-Byte-Längen dürfen
  abweichen, solange jeder Leser (hxcfe UND UFT) alle 1760 Sektoren
  mit 0 bad findet.

## UNGEKLÄRT

1. **Lizenzstand der Quelle zum Attributionszeitpunkt (2026-01)** —
   Shallow-Clone, Historie nicht messbar. Durch das
   „eigenständig"-Verdikt ohne Folge; bei einem künftigen Port zählt
   ohnehin der heutige Stand.
2. **`hxcfe`-D64-Verhalten** — Loader meldet sich, extrahiert aber 0
   Sektoren aus unserem VICE-D64. Ob Konfigurationsfrage oder
   grundsätzliche Lücke: nicht weiter verfolgt; als D64/G64-Oracle
   **nicht** verwenden (dafür liegt VICE c1541 im Oracle-Bestand).
3. **IPF via CAPS-DLL** — nicht gemessen (DLL nicht installiert);
   ohnehin durch `LIZ-2`/IPF-Quarantänelage blockiert.
4. **FATIOlib-Doppellizenz** („GPL" ohne Version + kommerzielles
   Angebot, `thirdpartylibs/FATIOlib/COPYRIGHT.txt` = GPLv2-Volltext) —
   nur relevant, falls je FS-Code aus diesem Teilbaum betrachtet wird;
   für alle vier Vorschläge ohne Berührung.
5. **`hxcfe -infos` Sektorzahlen** beruhen auf HxCs eigenem
   Sektor-Extraktor; für Nicht-Amiga-Formate wurde die Zahl (z. B.
   FDI: 2652) nicht gegen eine dritte Hand geprüft — vor einer
   FDI-Nutzung als Oracle einmal gegen UFTs T1-Befund abgleichen.

---

## Vorschlagsblock (max. 5 — es sind 4)

> Übernahme nach `docs/OPEN_ITEMS.md` durch einen Menschen; ein
> Vorschlag ist kein Eintrag.

**SCOUT-49 — `hxcfe` v2.16.15.2 als externes Oracle registrieren
(ADF/HFE/SCP-Amiga-Pfad).** Gebaut und gemessen 2026-08-28 (MinGW64,
zwei Shim-Zeilen für `sysexits.h`, Rezept im Gutachten): ADF→HFE→ADF
byte-identisch (SHA `9af68fcc…`), dekodiert `gw_amigados.hfe` und
`gw_amigados.scp` zu 1760/1760 Sektoren, unabhängige Hand zu
greaseweazle/xdftool/UFT. Eintrag nach ORACLES-Muster (4+1 Fragen
beantwortet; DMS-Sonderfall: nur Port-Treue-Oracle, xdms-1.3.2 =
dieselbe Hand wie `uft_dms.c`). **Kennzahl: ungeprüfte Formate runter**
(stützt Hebungen; unmittelbar C6/`dms` und B4/`hfe`).
Quelle: `tools/uft-scout/out/HxCFloppyEmulator.gutachten.md` §B.1.

**SCOUT-50 — B4 schließen: HFE-Fixture mit echter Encoding-Kennung
selbst erzeugen.** `hxcfe -finput:xdftool_dd_ofs.adf -conv:HXC_HFE`
liefert gemessen `track_encoding=0x01`/`interface_mode=0x04`/84 Spuren
aus unserem eigenen Korpus-ADF — der Zweig `HFE_ENC_AMIGA_MFM`
(`src/formats/hfe/uft_hfe.c:137`) läuft damit erstmals; die
Lizenzfrage des HxC-Distributions-Fixtures entfällt. Ein Test, der die
Kennungen und den dekodierten Inhalt prüft. **Kennzahl: ungeprüfte
Formate runter (`hfe`).**
Quelle: Gutachten §B.3; Plan-Anker `docs/plans/UMSETZUNGSLISTE.md:138`.

**SCOUT-51 — Eigentümer-Vorlage: AmigaDOS-MFM-Encoder aus libhxcfe
portieren und ADF→HFE anbieten.** Quelle
`amiga_mfm_track.c:356-485` (`tg_addAmigaSectorToTrack`,
GPLv2+-Header Z. 14; formaler PRÜFEN-Fall COPYING≠Header → Vorlage
statt Auslegung). Rotbeweis-Weg und Differenzlauf-Plan liegen im
Gutachten; Oracle ist mit SCOUT-49 gebaut. Kein Moratoriumsfall
(Encoder für zwei bestehende T1b-Formate, öffnet einen registrierten,
heute abgelehnten Pfad — MF-539). **Kennzahl: angebotene
Wandlungspfade rauf** (ADF→HFE, perspektivisch als 5. verlustfreier
Pfad mit Messung).
Quelle: Gutachten §B.2.

**SCOUT-52 — `LIZ-1`-Teilauflösung: die drei HxC-Attributionen
schließen.** Idiom-Audit beide Richtungen je 0 Treffer, Verdikt
**eigenständig** für `src/rawformatdialog.h:6`,
`src/visualdiskdialog.cpp:6`, `src/visualdiskdialog.h:6`; stärkster
Beleg die divergierende `formatValue`-Semantik (Füllbyte vs. gepackte
Geometrie). Köpfe nach MF-636 berichtigen („Verhalten nach dem Vorbild
von HxCFloppyEmulator (GPL-2.0-or-later), eigenständige
Implementierung"), Zensus neu laufen lassen. Die FLUXENGINE-Türfrage
ist für HxC entlastend beantwortet: kein GPL-2.0-only, keine
Ableitung. **Kennzahl: die begründete fünfte** — Dateien mit
ungeklärter Herkunft, Verdachtsstufe 48→45.
Quelle: Gutachten Teil A.
