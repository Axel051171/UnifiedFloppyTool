# Gutachten: zwei Download-Sammlungen (`exsource`, `copy`) — gezielt gegen zwei Blockaden

**Scout-Zyklus:** 2026-09-02 · **Auftrag:** Blockade 1 (FAT12 ohne fremdes
Abbild) und Blockade 2 (Container mit expliziter Geometrie) beantworten,
**keine** Vollsichtung · **Nur Dokument, kein Code.**

**Untersuchter Stand:** `C:\Users\Axel\Downloads\exsource` (24 Dateien) und
`C:\Users\Axel\Downloads\copy` (57 Einträge auf oberster Ebene; die
Auftrags-Zahl „176 Dateien" ist rekursiv und wurde nicht nachgezählt).
Arbeitskopien ausschließlich unter dem Sitzungs-Scratchpad
(`…\scratchpad\w\`), nichts im UFT-Baum. HEAD des Baums beim Lauf:
`337fefbb` (MF-785).

**Inventar (Schritt 1, Pflicht):** `inventar.py build . -o inv.json` →
`OK: 88 Plugins (SSOT ok), 232 Format-Dirs, 88 Tier-Zeilen, 35 Korpus-Abbilder,
HEAD 337fefbb`, rc=0. Abfragen zitiert:

| Begriff | Antwort (wörtlich) | Folge |
|---|---|---|
| `fat12` | `vorhanden: true, treffer: [uft_fat12], plugin_liste_vollstaendig: true` | vorhanden — es geht um **Belegung**, nicht um ein Plugin |
| `mtools`, `fatfs` | `abgedeckt: false` (Fähigkeitsfrage) | von Hand geprüft: `git grep -il "ChaN\|FatFs - Generic" src include tests` → **0** Treffer in `src/`; `tests/test_fatfs.c` heißt so, ist aber UFTs eigener Test gegen `uft_fat12_detect()` — kein FatFs im Baum |
| `st` | `vorhanden: true, tier: T2` | siehe EmptyFlops unten |
| `fdi` | `vorhanden: true` (`fdi`, `fdi_pc98`), `fdi_pc98` steht auf **T3** | siehe Nebenfund PC-98 |
| `xdf`, `imz`, `kc85`, `pcw` | `vorhanden: true` | `kc85`: **nur ein Header** (`git ls-files` → `include/uft/formats/uft_kc85.h`, keine `.c`) |
| `anex86`, `atari_st` | `vorhanden: false`, kein schwacher Treffer | `anex86` ist der Name des FDI-Containers, den `fdi_pc98` liest — kein Fehlbestand |

`data/known_negatives.json`: `FDOS/format` (bewertet, Plan liegt:
`docs/plans/FDOS_FORMAT.md`), `RobSmithDev/DiskFlashback` (bewertet, Plan
liegt), `simonowen/samdisk`, `jfdelnero/HxCFloppyEmulator` — keiner der
übrigen hier untersuchten Kandidaten steht darin.

---

## Blockade 1 — FAT12 hat kein fremdes Abbild

### Ausgangslage im Baum (gemessen)

* `docs/VERIFICATION_TIERS_FS.md:29`: `uft_fat12 | FS-T1 | test_fatfs | alle
  Tests bauen ihre Eingabe selbst — geprueft gegen den eigenen Erzeuger`.
  Bestätigt: `tests/test_fatfs.c:35` `build_fat12_boot()` setzt den
  Bootsektor von Hand (`EB 3C 90`, „MSDOS5.0", 1.44 MB, `0xAA55`).
* Das einzige FAT12-tragende Korpus-Abbild ist **auch eigene Hand**:
  `tests/corpus_manifest/manifest.json` → `gw_msx_2dd.img`, `source`:
  „Quellabbild eigen erzeugt (BPB nach FAT12-Beschreibung, OEM "UFTMSX",
  Media-Descriptor 0xF9, sonst Nullen); dann zweimal gw". Greaseweazle
  reicht Sektoren durch — FAT, Verzeichnis und BPB stammen von UFT.
  `tests/test_corpus_msx.c:41` sagt das selbst.
* Der geplante Weg (blueMSX + Nextor, P3-13) ist eine Klick-Sitzung und
  liefert ein Abbild mit **154 Byte Nextor-Bootcode** (MF-781).
* API-Umfang, den ein fremdes Abbild prüfen kann: `src/fs/uft_fat12.c`
  exportiert neben `uft_fat12_detect()` auch `uft_fat_open`,
  `uft_fat_read_dir`, `uft_fat_read_dir_path`, `uft_fat_extract_to_file`,
  `uft_fat_inject`, `uft_fat_mkdir` (Zeilen 106–812). Ein fremdes Abbild
  **mit Dateien** prüft also Verzeichnis und Clusterketten, nicht nur den BPB.

### Fund 1 — GNU mtools 4.0.49 (`copy/mtools-4.0.49.tar.gz`)

**Kategorie:** Oracle + Daten/Fixture · **Kanal (MF-695):** Oracle —
Ausführen ist keine Ableitung; **kein Port** (Lizenz, s. u.) ·
**Bewegte Kennzahl (MF-640):** *ungeprüfte Formate (T3) runter* — indirekt
über jedes FAT12-tragende T3-Plugin, das ein mformat-Abbild als Korpus
bekommt (Kandidat gemessen: `fdi_pc98`, T3, „—" in der Testspalte); direkt:
`uft_fat12` FS-T1 verliert den Vermerk „gegen den eigenen Erzeuger".
**Einhängepunkt (im Baum auffindbar):** `docs/OPEN_ITEMS.md` **P3-13**
(dort ist mtools als Erzeuger von `nextor.dsk` bereits genannt) und
`docs/PLAN_v4.1.7.md:229` (Zeile 4 der FS-Tabelle: „FAT12 (IMG/DSK) |
`mtools` (`mdir`) | … kein Korpus-Eintrag → braucht erst einen").

**Lizenz, wörtlich (Bericht, kein Urteil — MF-679):** `COPYING` Zeile 1–2:
„GNU GENERAL PUBLIC LICENSE / Version 3, 29 June 2007". `mformat.c:1–8`:
„Copyright 1986-1992 Emmet P. Gray. Copyright 1994,1996-2009 Alain Knaff.
… under the terms of the GNU General Public License … either version 3 of
the License, or (at your option) any later version." → **GPL-3.0-or-later**,
Matrix-Zone **GELB**: kein Port, Verhaltens-Spec und Oracle-Binary erlaubt.
Keine abweichenden Lizenzen in Unterverzeichnissen gesucht (nur `debian/`
vorhanden; nicht geprüft → UNGEKLÄRT, für den Oracle-Kanal ohne Folge).

**Baut es unter MinGW?** Ja, mit zwei Abweichungen — gemessen mit
`/c/Qt/Tools/mingw1310_64` (gcc 13.1.0, `mingw32-make`):

1. `./configure --host=x86_64-w64-mingw32 --build=x86_64-w64-mingw32
   --disable-floppyd --without-x` → rc=0, „Host OS=mingw32". Der
   Cross-Build ist dokumentiert: `INSTALL:57–65`.
2. Erster `make`: **Abbruch** `charsetConv.c:348: fatal error: langinfo.h`
   — `configure` fand `iconv.h` (Qt-MinGW liefert eines) und setzte
   `HAVE_ICONV_H 1` (`config.h:65`); `charsetConv.c:346–348` zieht dann
   `<langinfo.h>` nach, das MinGW nicht hat. Abhilfe hier: `HAVE_ICONV_H`
   in `config.h` ausgetragen, `codepages.o`/`charsetConv.o` gelöscht,
   erneut gebaut → rc=0, `mtools.exe` 1 001 312 Byte, SHA-256
   `29f3cc04b26e3fb17dea005e9aa6a99a458d10705f271ed1912d73fb8bb7270b`.
   Sauberer wäre vermutlich `ac_cv_header_iconv_h=no` beim configure —
   **nicht gemessen**.
3. Ein Multi-Call-Binary: Aufruf als `mtools.exe -c mformat …`
   (`mtools.c:117–129`) oder als Kopie `mformat.exe` (`mt_stripexe`,
   `mtools.c:107`). Beides funktioniert, Letzteres gemessen.
4. `tty.c:59: warning: Cannot use raw terminal code (disabled)` — harmlos
   für mformat/mcopy/mdir.

**Windows-Defender-Befund (gemessen, Eigentümer-Vorlage):** Die Kopien
`mformat.exe`, `mcopy.exe`, `mdir.exe` und später `mtools.exe` selbst wurden
während der Messung unter Quarantäne gestellt — `Get-MpThreatDetection`:
14:53:28 und 14:53:50, `Trojan:Win32/Bearfoos.A!ml`, ActionSuccess=True.
Das ist eine ML-Heuristik auf ein frisch gebautes, unsigniertes Binary,
das rohe Bootsektor-Bytes trägt; die Messungen A–F unten waren vorher
fertig, Messung G (Vorlage mit Sprung+Signatur) **fiel deshalb aus**.
Für Stufe 4 heißt das: entweder Ausnahme für das Werkzeugverzeichnis
(Eigentümer-Entscheidung) oder ein **signiertes Paket** — `pacman -S mtools`
in `C:\msys64` (pacman vorhanden, Paket **nicht** installiert; nicht
gemessen, braucht Netz) oder `apt install mtools` in WSL Ubuntu (WSL
vorhanden, `apt-cache policy mtools` → „Installed: (none)"). SHA-Pinnung
im Register wie bei `dtc`.

**Was ist `mtools14_msdos.zip`?** **Nicht GNU mtools.** `MTOOLS14.ZIP/
mtools.txt:1–13`: „MTOOLS Version 1.4, Autor: Frank Dachselt, Dresden …
Programmpaket zur Rechnerkopplung zwischen dem KC 85 und einem PC" (seriell,
Null-Modem; KC-seitig MicroDOS/ML-DOS). Beigelegt `ADF_150.ZIP` = „ADF v1.50
serial port FOSSIL driver" (`FILE_ID.DIZ`). DOS-Binaries von 1988–2002,
keine Lizenzdatei gesehen → Zone ROT. Für FAT12 irrelevant; für KC85 Fundus.

**Exakte Befehlszeile** (mit Kopie `mformat.exe`; über das Multi-Call-Binary
lautet der Anfang `mtools.exe -c mformat`):

```
mformat -C -i uft_fat12_720.img -t 80 -h 2 -n 9 -N 12345678 -v UFTFAT12 -B tpl.bin ::
mcopy   -i uft_fat12_720.img payload.bin ::PAYLOAD.BIN
mdir    -i uft_fat12_720.img ::
```

`-C` legt die Datei an (`mformat.c:1267–1278`), `-t/-h/-n` sind Zylinder/
Köpfe/Sektoren **explizit** (`-f 1440` u. ä. schlägt sie aus `old_dos.c`
nach), `-N` pinnt die Seriennummer (`mformat.c:1394–1396`, sonst
`lrand48()`), `-v` das Label, `-B` liest eine 512-Byte-Bootsektor-Vorlage
und setzt `keepBoot=1` (`mformat.c:1281–1296`). `-S n` wählt die
Sektorgröße als FDC-Code (`-S 3` = 1024 — **nicht gemessen**, `mformat.c:
1018–1026`; damit wären PC-98-Geometrien 77/2/8×1024 erreichbar).

**Schreibt es Bootcode?** **Ja — standardmäßig, und abschaltbar.** Gemessen
an fünf Abbildern (Bytes ≠ 0 im Bereich 0x3E–0x1FD, also hinter dem
erweiterten BPB und vor der Signatur):

| Lauf | Aufruf | Sprung | OEM | Signatur | Bytes ≠ 0 in 0x3E–0x1FD | FAT[0] |
|---|---|---|---|---|---|---|
| A | `-f 1440` | `EB 3C 90` | `MTOO4049` | `55 AA` | **44** (0x3E–0x1CB) | `F0 FF FF` |
| B | `-t 80 -h 2 -n 9` | `EB 3C 90` | `MTOO4049` | `55 AA` | **44** | `F9 FF FF` |
| C | wie B, `-B zero.bin` (512 × 0x00) | `00 00 00` | 8 × 0x00 | `00 00` | **0** | `F9 FF FF` |
| D | wie B, `-k` auf frischem `-C` | `00 00 00` | 8 × 0x00 | `00 00` | **0** | `F9 FF FF` |
| F | `-f 360` | `EB 3C 90` | `MTOO4049` | `55 AA` | **44** | `FD FF FF` |

Die 44 Bytes zerfallen in zwei Quellen, beide im Quelltext benannt:
**36** davon sind das 47-Byte-Bootprogramm `bootprog[]`
(`mformat.c:150–155`, Realmode: `cli; xor ax,ax; mov ds/es; … int 13h;
int 19h`), installiert bei Offset 0x3E durch `inst_boot_prg()`
(`mformat.c:157–172`, Aufruf 1424–1425, nur `if(!keepBoot)`); **8** sind ein
Schein-Partitionseintrag bei 0x1BE (`80 00 01 00 01 01 12 4F … 40 0B`,
„install fake partition table pointing to itself", `mformat.c:1333–1336`,
ebenfalls nur ohne `keepBoot`). Die Signatur `0xAA55` wird ebenfalls nur ohne
`keepBoot` gesetzt (`mformat.c:1307–1308`), der OEM-Name „MTOO4049" nur ohne
`keepBoot` (`mformat.c:136–140`, `patchlevel.c:27`).

**Folge für den Fremdcode-Befund:** Mit `-B` **enthält das Abbild keinen
mtools-Code** — Lauf C und D: 0 Bytes ≠ 0 hinter dem BPB. Der Preis: Sprung
und Signatur fehlen dann auch (Lauf C: `00 00 00`, `00 00`), und
`uft_fat12_detect()` verlangt beides (`tests/test_fatfs.c:15–16`). Die
Vorlage muss deshalb selbst `EB 3C 90` bei 0 und `55 AA` bei 0x1FE tragen —
fünf Bytes, die keine Schöpfung sind (Lauf G, **nicht gemessen**, s. o.
Defender; der Codepfad ist eindeutig: `keepBoot` lässt die Vorlage
unangetastet außer BPB-Feldern, Zeilen 1298, 1307, 1386).
Manifest-Feld `fremdcode`: **„nein — 0 Bytes ≠ 0 in 0x3E–0x1FD gemessen
(mformat -B, Vorlage eigen: Sprung+Signatur)"**. Ohne `-B` lautet er
**„ja (36 B Bootprogramm 0x3E–0x6C + 8 B Partitionseintrag 0x1BE–0x1CD)"**.

**Reproduzierbarkeit (gemessen):** Lauf C und D — zwei getrennte Aufrufe —
liefern **identische** SHA-256 (`86ab24e8216b8ebb…`). Die Seriennummer war
in allen Läufen `0000-7C98` (`lrand48()` auf diesem Build offenbar ohne
Zeitsaat; **nicht** darauf verlassen, `-N` setzen). Restliches Abbild nach
FATs und Wurzelverzeichnis: 0 Bytes ≠ 0 (kein Füllmuster). `mcopy` und
`mdir` funktionieren gegen die Abbilddatei (Lauf E: `HELLO TXT 10 …
729 088 bytes free`); `minfo` gibt die Geometrie zurück und nennt die
mformat-Zeile, mit der man das Abbild reproduziert.

**Oracle-Register-Entwurf** (`tests/differential/oracles.py`, Muster
`bluemsx`, Zeilen 226–264): `name="mformat"`, `exes=("mformat",
"mformat.exe", "mtools", "mtools.exe")`, `version_args=("-V",)`,
`reference_for="ERZEUGT KORPUS fuer uft_fat12 (FS-T1, bisher eigener
Erzeuger) und jedes FAT12-tragende Plugin; PRUEFER fuer das Verzeichnis
ueber mdir"`, `licence="GPL-3.0-or-later"`, `abstammung="UNABHAENGIG —
git grep -i mtools src include: 0 Treffer (gemessen 2026-09-02)"`.
Achtung: `nextor.dsk` aus dem Nextor-Release ist laut MF-781 **auch**
mformat-erzeugt — ein Differenzlauf UFT-vs-Nextor-Abbild prüft in der
FAT-Schicht dann dieselbe Hand wie dieser Fund. Das ist kein Nachteil des
Funds, sondern ein Grund für Fund 2.

### Fund 2 — die zweite FAT-Hand: FatFs R0.16 über Flopgen (`copy/fatfs-master.zip`, `copy/Flopgen-main.zip`)

`docs/PLAN_v4.1.7.md:233–238` verlangt bei gleichem Erzeuger und Prüfer
eine Zweitmeinung als **Pflicht**. Drei Kandidaten in der Sammlung, gemessen:

| Hand | Lizenz (Datei) | Windows | Bootcode | Geometrie explizit | Urteil |
|---|---|---|---|---|---|
| **dosfstools** `mkfs.fat` (4.2+git, `configure.ac:17`) | `COPYING`: GPL-3.0; `mkfs.fat.c:13–14` „version 3 … or later" → GELB | **kein** MinGW/Win32-Pfad (`grep -ri mingw\|_WIN32\|cygwin src configure.ac` → 0); nur WSL, dort nicht installiert | **immer**: `dummy_boot_jump`+`dummy_boot_code` 448 B (`mkfs.fat.c:200–205, 135, 774–790`), nur die Meldung ist per `-m` tauschbar; Atari-Zweig (`--variant=atari`) schreibt `60 1C` und keinen x86-Code, aber auch keine Signatur (Zeile 793–794) | ja: `-C`, `-g heads/spt`, `-F 12`, `-S`, `-s`, `-r`, `-M`, `-R` (`usage()`) | brauchbar als **Prüfer** (`fsck.fat`), als Erzeuger nur mit `fremdcode: ja (448 B)` |
| **FatFs R0.16** roh (`source/ff.c:2`) | `LICENSE.txt`: „one of the BSD-style licenses … equivalent to the 1-clause BSD license" — **steht nicht in der Matrix → PRÜFEN** (Regel 3) | Bibliothek, kein Programm | keiner: `EB FE 90` + „MSDOS5.0", Rest `memset 0` (`ff.c` „Create FAT VBR") | **nein**: Media `0xF8`, SecPerTrk **63**, Heads **255** fest verdrahtet — ein Festplatten-BPB, für Disketten falsch | nur über einen Wrapper |
| **Flopgen** (maksgraczyk) | `LICENSE`: GPL-3.0 → GELB; vendort FatFs (PRÜFEN) und CLI11 (`cli/LICENSE`: BSD-3-Klausel-Text) | **ja**, gemessen: gcc 13.1 baut mit `mingw32-make CXX=g++ CC=g++ LDFLAGS=` (Original-Makefile setzt `CC=$(CXX)` und `-lstdc++fs`; beides musste angepasst werden — `stdc++fs` gibt es in GCC 13 nicht mehr, `CC=gcc` bricht die C++-Bindung von `file_disk_*`) | keiner — gemessen | teilweise: `-s 360/720/1200/1440/2880` (`image.cpp:40–72` setzt Media, SPT, Köpfe je Größe; der Autor hat `MKFS_PARM` um `n_heads`, `sec_per_track`, `mdt` erweitert, `ff.h:275`, `ff.c:6226–6227`) — keine freie Geometrie | **zweite Hand, mit einem gemessenen Makel** |

**Flopgen, gemessen** (`flopgen.exe -o fg1 -s 720 payload.bin`, zweimal):
737 280 Byte, SHA-256 beide Läufe identisch
(`7ee6f68365ab188cea18fdd10c4c192df6e7af6309ae65a017003988324e74d3`).
Bootsektor: Sprung `EB FE 90`, OEM „MSDOS5.0", BPB **bytegleich mit dem
mtools-BPB** für 720 K (`00 02 02 01 00 02 70 00 A0 05 F9 03 00 09 00 02 00
00 00`), Signatur `55 AA`, **0 Bytes ≠ 0 in 0x3E–0x1FD**, Wurzeleintrag
`PAYLOAD BIN` mit Datum 2026-09-02. **Makel:** `FAT[0]` = `F8 FF FF`, obwohl
der BPB-Media-Descriptor `F9` sagt — Flopgen patcht den BPB, nicht die
FAT-Kennung. Das ist für UFT ein **Prüffall**, kein Ausschluss: ein Leser,
der beide vergleicht, muss den Widerspruch melden und nicht stillschweigend
eine Seite nehmen. Seriennummer im BPB = `00 00 22 5D` = das FAT-Datum;
die Bytegleichheit über zwei Läufe gilt also **am selben Tag**, über Tage
hinweg **nicht belegt**.

**Fremdcode-Befund Flopgen:** „nein — Sprung `EB FE 90` (3 Byte), sonst 0
gemessen". Vorgefertigte Binaries laut README auf der Releases-Seite —
nicht geholt, nicht gemessen.

### Was Stufe 4 daraus baut (Weg, nicht Code)

1. **Rotbeweis zuerst:** `test_corpus_fat12` (neu, im MF-Workflow) öffnet
   das mformat-Abbild über `uft_fat_open()` und vergleicht
   `uft_fat_read_dir()` gegen die `mdir`-Ausgabe (Name, Größe, Datum) und
   den extrahierten `PAYLOAD.BIN` byteweise gegen die Quelle. Vor dem
   ersten Lauf muss die Erwartung **aus mdir** stammen, nicht aus UFT.
2. **Zweite Hand:** dasselbe Abbild durch `fsck.fat -n` (WSL) — Prüfer,
   nicht Erzeuger; plus das Flopgen-Abbild als zweiter Erzeuger mit dem
   F8/F9-Widerspruch als benanntem Fall.
3. **Manifest:** zwei Einträge in `tests/corpus_manifest/manifest.json`,
   `origin: cross-tool`, `oracle: mformat` bzw. `flopgen`, `tool` mit
   Version **und** SHA-256 des Binaries, `source` = die Befehlszeile oben,
   `fremdcode` wie gemessen. Beide Werkzeuge brauchen vorher den
   Registereintrag (`scripts/audit_korpus_herkunft.py` weist sonst ab).
4. **Header-Referenz:** `src/fs/uft_fat12.c` nennt bereits „Microsoft FAT
   Specification (hardwhitepaper, Aug 2005)" (Zeile 14–15) — der
   Korpus-Bezug kommt in den Testkopf, nicht in den Leser.
5. Damit wird die blueMSX-Klick-Sitzung (P3-13) für den **FAT12-Zweck**
   entbehrlich; für den **MSX-Zweck** (Nextor-Bootsektor, MSX-DOS-Eigenheiten)
   bleibt sie bestehen. Das ist eine Plan-Änderung nach „Messung vor Plan".

---

## Blockade 2 — Container mit expliziter Geometrie

**Ergebnis: kein Fund.** Keiner der sieben Kandidaten schreibt einen
Container **mit Kopf** und nimmt dabei eine explizite Geometrie entgegen.
Je Kandidat, kurz:

| Kandidat | Was es erzeugt | Geometrie explizit? | Schreibt (Formate) | Lizenz (Datei) | Urteil |
|---|---|---|---|---|---|
| **TotalImage** (C#/.NET 8, WinForms) | Rohabbilder mit FAT12 | ja, aus fester Liste (`Docs/status.md`: PC-Formate mit/ohne BPB, DMF, 8" 250k/1232k, Siemens PC-D, Alphatronic, Eagle, Tandy 2000; Acorn 800k „writes a broken bootsector") | **nur Raw** — Container VHD/NHD/IMZ/Anex86-FDI/PCjs: „Create: No" (`status.md`, Tabelle Image containers) | `LICENSE`: MIT → GRÜN | GUI ohne CLI; `TotalImage.IO` ist eine Bibliothek und ließe sich skripten (dotnet vorhanden) — für Rohabbilder mit exotischer Geometrie ein **Fundus**, für Blockade 2 nein |
| **Flopgen** | Rohabbilder FAT12, 5 Größen | nein (nur `-s`) | Raw | GPL-3.0 | siehe Fund 2; für Blockade 2 nein |
| **CreateXDF** (C#, Claunia) | IBM-XDF-Rohabbild 3,5"/5,25" | nein, fest (`Program.cs:98–110`, 23/19 SPT) | Raw XDF | **keine Lizenzdatei** (Dateiliste: `.gitignore`, `.csproj`, `Program.cs`, `AssemblyInfo.cs`, `.sln`) → ROT | Zusätzlich: der eingebettete Bootsektor trägt wörtlich „XDF v1.1a (c) 1993, 1994 -- Backup Technologies, Inc., Tampa FL … All rights reserved worldwide. Duplication prohibited without permission" (`Program.cs:9–40`) — ein damit erzeugtes Abbild ist in **512 Bytes proprietär**. Nicht verwenden |
| **DiskFormatID** (Python 2, PyQt4, Linux) | nichts — GUI-Frontstück für KryoFlux DTC (`README.md:5–11`) | — | — | keine Lizenzdatei gefunden (`find -iname LICENSE*` → 0) | irrelevant |
| **format** (FreeDOS FORMAT 0.92) | formatiert **Laufwerke** unter DOS, keine Abbilddateien | ja (`/T:`, `/N:`, `/F:`) | — | `doc/license.txt`: GPL-2.0; `floppy.c:12`: „GNU GPL, Version 2" → GRÜN | bereits `bewertet` (`known_negatives.json`), Plan `docs/plans/FDOS_FORMAT.md`; nur in einem DOS-Emulator nutzbar — nicht in dieser Runde |
| **EmptyFlops** | zwei fertige Atari-ST-Leerabbilder, kein Werkzeug | — | — | **keine Datei außer den zwei `.st`** → Herkunft unbekannt, ROT/PRÜFEN | siehe Nebenfund unten |
| **DiskFlashback** | Windows-Laufwerksmounter; „Create blank disk images", „Format floppy disks" über GUI (`README.md:18,22`) | nein | ADF/ST/IMG per vendorter ADFlib/FatFs/PFS3lib | `GPL2.txt` + `MPL2.txt` + BSD-4 (PFS3lib) → Mischlage, bereits als ORANGE-Muster in der Matrix | `bewertet`, Plan liegt; kein CLI — nein |

**Folge:** P3-14 bleibt, wie es steht — der Weg ist die **explizite
Geometrieangabe an SAMdisk/hxcfe** (dort als Option benannt), nicht ein
weiteres Werkzeug. Dieses Gutachten trägt dazu nur den Negativbeleg bei.

---

## Nebenfunde (Fundus, keine Aufträge — jeweils ohne bewegte Kennzahl oder ohne Beleg)

* **EmptyFlops** (`720PCF.st` 737 280 B, SHA-256 `fd250521…`; `800ST.st`
  819 200 B, SHA-256 `b5dd394a…`, Dateidatum 2007-01-24): TOS-formatierte
  Leerabbilder 80/2/9 und 80/2/10 (BPB gelesen: `0x13`=1440 bzw. 1600,
  `0x18`=9 bzw. 10, Media `F9`, Füllwert `E5`). `800ST.st` beginnt mit
  `4E 71 60 3C` (m68k NOP + BRA.S) und trägt 392 Bytes ≠ 0/≠ E5 im
  Bootsektor — **ausführbarer Inhalt, Herkunft unbekannt**. `st` steht auf
  T2 mit dem Vermerk „NICHT verifiziert: Verhalten an einem realen
  ST-Abbild (keines im Korpus)" (`VERIFICATION_TIERS.md:66`). Die zwei
  Dateien wären das erste reale ST-Paar — **nur** wenn der Eigentümer die
  Herkunft nennen kann; ohne sie ist `origin: real` nicht belegbar.
  Bewegt keine T3-Zahl (st ist T2) → Fundus, mit Rückfrage.
* **`fdi_pc98`** (T3, keine Tests): TotalImage liest Anex86-FDI/HDI
  (`TotalImage.IO/Containers/Anex86/Anex86Header.cs`, MIT) — eine zweite,
  lizenzgrüne Lesart des Kopfes (Offset 0x1C Zylinder usw.) neben dem
  Header-Kommentar in `src/formats/fdi_pc98/uft_fdi_pc98.c`. Das ist eine
  **Spec-Quelle** für eine Variantenprüfung, kein Erzeuger; ein Erzeuger
  fehlt weiterhin (mformat könnte das Rohabbild 77/2/8×1024 liefern, `-S 3`
  nicht gemessen; der 4096-Byte-Kopf müsste eigen gesetzt werden).
* **`uft_*`-Archive:** `uft_todo_boostpack.zip` geöffnet — eigenes,
  chat-erzeugtes Material (`docs/README.md:1–4`: „Das ist Source Code, den
  du direkt in dein Projekt ziehen kannst"; CMake-Projekt
  `uft_todo_boostpack`, `src/format/dc42.c`, `adfs.c`, `pipeline/smoke.c`).
  Keine Fremdsichtung; die übrigen 22 `uft_*`-Archive nicht geöffnet.
  Hinweis an den Eigentümer: falls je etwas davon eingezogen wird, gilt die
  EINFRIER-REGEL in voller Schärfe (ungeprüfter Format-Code ohne benannte
  Referenz), und `dc42` existiert im Baum bereits (`src/formats/dc42/`).
* **`xcopypro_source_2011`:** nicht angefasst (Brandmauer).

## Dokumente (billiger Nebenweg, je ein Satz)

* `22disk_def_kc85.txt` — eine 22DISK-Definition „KC85 MikroDos DSDD 96 tpi
  5,25": 80 Zyl., 2 Seiten, 5 × 1024 B, Skew 2, `BSH 4 BLM 15 EXM 0 DSM 389
  DRM 127 AL0 0C0H OFS 4`; im Baum gibt es zu KC85 **nur einen Header ohne
  Quelldatei** und keinen Eintrag in `uft_cpm_diskdefs.c` (grep 0) — sie
  belegt also nichts Bestehendes und dürfte unter der EINFRIER-REGEL nur
  als CP/M-`diskdef`-Datum in die vorhandene Tabelle, nicht als Plugin.
* `KC85FileFormats.txt` (mrhill/susowa 2008) — Datei-, nicht Diskformate
  (KCC/KCB/SSS/TTT-Kopfaufbau); belegt nichts im Format-Layer, könnte einen
  KC85-**Tape**-Leser stützen (Skeleton-Audit nennt `kc85`, `kc_turbo` unter
  `include/uft/tape/`), Fundus.
* `formatting floppy.txt` — **kein Dokument**, sondern HTML-eskapierter
  C-Quelltext eines generischen DOS-FORMAT („format.c 1.0", `sccsid`), mit
  eingebettetem `bootcode[]` (Zeile 206) und ohne Lizenz/Autor (grep 0) →
  ROT; als Verhaltensbeschreibung der BPB-Ableitung (`struct BPB`,
  Zeilen 38–50) lesbar, mehr nicht.
* `DDI-1 FD-1 Service Manual.en.rar` (PDF, 4,2 MB) und `Amstrad PCW
  8256-8512 Service Manual.en.rar` (PDF, 14,9 MB) — **nicht gelesen**;
  könnten die Zeile `{"DSK_PCW", "Amstrad PCW", 80, 1, 9, 512, 368640}`
  (`src/formats/dsk_generic/uft_dsk_generic.c:34`) und die CPC-3"-Geometrie
  in `dsk_cpc`/`edsk` an einer Herstellerquelle belegen oder widerlegen —
  das ist der sicherste Kanal (Spec), Aufwand S, Kennzahl nur, falls dabei
  ein T3-Plugin (`edsk` ist T3) eine benannte Referenz bekommt.

---

## OPEN_ITEMS-Vorschläge (2 von höchstens 5 — der Rest bewegt keine Zahl)

**V1 — mformat als FAT12-Erzeuger und `mdir` als Prüfer ins Register; erstes
fremdes FAT12-Abbild.** Kennzahl: T3 runter (Vorbedingung für jedes
FAT12-tragende T3-Plugin, gemessen `fdi_pc98`) und FS-T1 ohne Selbstbezug.
Einhängepunkt: P3-13 (Zusatz), `PLAN_v4.1.7.md:229`. Kanal: Oracle,
GPL-3.0-or-later (GELB — kein Port). Beschaffung: signiertes mtools
(msys2/WSL) **oder** Defender-Ausnahme für den Eigenbau, SHA-gepinnt;
Vorlage `tpl.bin` (5 eigene Bytes); Befehlszeile oben. Aufwand **S**.
UNGEKLÄRT: Lauf G (Vorlage mit Sprung+Signatur) ist Codepfad-belegt, nicht
gemessen.

**V2 — Zweitmeinung Pflicht: Flopgen/FatFs als zweiter Erzeuger, `fsck.fat`
als zweiter Prüfer.** Kennzahl: dieselbe wie V1 — ohne V2 ist V1 nach
`PLAN_v4.1.7.md:233–238` kein unabhängiger Beleg. Lizenz-Vorlage für den
Eigentümer: FatFs-Lizenz steht nicht in der Matrix (PRÜFEN), betrifft aber
nur das Oracle-Binary, nicht UFT-Code. Aufwand **S**; der F8/F9-Widerspruch
wird als benannter Prüffall mitgeführt.

**Nicht vorgeschlagen (Fundus):** EmptyFlops (Herkunft), TotalImage.IO als
Geometrie-Skript, PC-98-Erzeugerweg, KC85-diskdef, Service-Handbücher.
**Blockade 2:** kein Fund — gültiges Ergebnis.

## UNGEKLÄRT

1. mtools: Lauf G (Vorlage mit `EB 3C 90` + `55 AA`) — durch
   Defender-Quarantäne nicht gemessen; `-S 3` (1024-Byte-Sektoren) nicht
   gemessen; `ac_cv_header_iconv_h=no` als sauberer configure-Weg nicht
   gemessen; `debian/`-Unterverzeichnis auf abweichende Lizenz nicht geprüft.
2. Flopgen: Bytegleichheit über **Tage** (Seriennummer = FAT-Datum) nicht
   belegt; Releases-Binary nicht geholt; FatFs-Lizenz-Zone ist
   Eigentümer-Vorlage.
3. dosfstools in WSL: Installation und `fsck.fat`-Lauf nicht gemessen
   (kein Netz/sudo in dieser Sitzung).
4. EmptyFlops: Herkunft der zwei ST-Abbilder.
5. Die Service-Handbücher wurden nicht gelesen; die PCW-Geometrie-Zeile im
   Baum ist damit weder bestätigt noch widerlegt.
6. Die 22 übrigen `uft_*`-Archive: nicht geöffnet, Einordnung „eigenes
   Material" gilt gemessen nur für `uft_todo_boostpack.zip`.
