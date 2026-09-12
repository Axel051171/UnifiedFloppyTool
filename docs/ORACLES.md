# Verzeichnis der Referenz-Werkzeuge (Oracles)

> Was `CAPABILITIES.md` für Controller ist, ist diese Datei für die
> Werkzeuge, die **urteilen**.

Ein Oracle entscheidet eine Behauptung, die UFT über sich selbst nicht
entscheiden kann. Die Einfrier-Regel (MF-498) verlangt für neuen Code im
Format- oder Decoder-Layer eine **benannte** Referenz — hier stehen die
Namen.

**Die Registry ist die SSOT, nicht diese Datei.**
`tests/differential/oracles.py` hält Auflösung, Version und Lizenz
maschinenlesbar; hier steht, was ein Mensch darüber wissen muss. Weichen
beide ab, gilt die Registry, und diese Datei ist gedriftet.

## Was ein Eintrag braucht

Vier Fragen, alle vier beantwortet oder kein Eintrag:

1. **Wofür ist es Referenz?** Nicht „was kann es", sondern welche
   Behauptung es entscheidet.
2. **Wie wird es gefunden?** Umgebungsvariable, dann `PATH`. Kein Raten
   in Installationsverzeichnissen.
3. **Wie wird die Herkunft gepinnt?** Versionsabfrage — oder, wenn das
   Werkzeug keine hat, `version_is_unaskable` plus SHA-256 der Datei
   (MF-623). Der Hash ist der **stärkere** Anker: eine Version gibt es in
   vielen Übersetzungen, den Hash einmal.
4. **Was ist mit der Lizenz?** Verglichen wird ausschließlich die
   **Ausgabe** eines fremden Programms. Es wandert kein Code ein — das
   ist der Unterschied, der die Lizenzfrage bei Oracles entschärft, und
   er steht bei jedem Eintrag ausdrücklich da.

**Kein Oracle auf Zusicherung.** Ein Werkzeug, das nicht gebaut und
ausgeführt wurde, ist kein Eintrag. Der Selbsttest
(`python tests/differential/oracles.py`) sagt, welche vorhanden sind —
und meldet **nicht** Erfolg, wenn es keines ist.

### Fünfte Frage: dieselbe Hand? (MF-644)

> **Wer den Korpus erzeugt hat, darf ihn nicht allein prüfen.**

Ein Oracle, das dasselbe Werkzeug ist wie der Korpus-Erzeuger, prüft
seine eigene Selbstkonsistenz — nicht UFT. Der T1b-Eintrag wird damit
**zirkulär**, und das fällt niemandem auf, weil alles grün ist.

**Ein fremdes Projekt hat dieselbe Falle unabhängig gefunden (MF-682).**
gwnbds `hardware-notes.md` dokumentiert selbstkritisch, dass frühe
„validated"-Behauptungen in Wahrheit nur den eigenen Write-Back-Cache
bestätigten: gelesen wurde, was kurz zuvor geschrieben worden war, und
die Diskette kam nie vor. Ein anderes Team, ein anderes Land, eine
andere Sprache, dieselbe Zirkularität.

Wenn zwei Gruppen unabhängig in dieselbe Grube fallen, ist es keine
Marotte dieses Projekts, sondern eine Eigenschaft des Feldes. Eine Regel
mit unabhängiger Bestätigung verteidigt man nicht noch einmal — man
wendet sie an.

Der Fall ist im eigenen Baum zweimal gemessen worden:

* **ADF:** `unadf` und AdfOpus teilen sich **ADFlib**.

  > **Berichtigt MF-682.** Hier stand, ADFlib sei „dieselbe Bibliothek,
  > die über `xdftool` auch unser Korpus-Abbild erzeugt hat". Das ist
  > **gemessen falsch**: die installierte `amitools` enthält **0**
  > ADFlib — weder als Datei noch als Nennung in irgendeinem ihrer
  > Python-Module. Zweimal unabhängig gemessen (Scout-Zyklus
  > `adf_zweitmeinung`, danach von mir nachgeprüft).
  >
  > Der Ausschluss von AdfOpus bleibt trotzdem richtig, nur mit anderem
  > Grund: `unadf` und AdfOpus teilen sich ADFlib **untereinander** —
  > als Paar sind sie keine zwei Hände. Die Verbindung zum
  > Korpus-Erzeuger gab es nie.
  >
  > Der Unterschied ist nicht akademisch. Nach der falschen Fassung war
  > jedes ADFlib-Werkzeug als Oracle verbrannt; nach der gemessenen ist
  > **eines** davon zulässig, solange das Korpus-Abbild nicht von ihm
  > stammt. Eine Zirkularitäts-Regel, die zu viel ausschließt, kostet
  > genauso viel wie eine, die zu wenig ausschließt — sie fällt nur
  > niemandem auf.
* **ATR:** `atrcopy 10.1` hat `atrcopy_dos2sd.atr` **erzeugt**
  (Manifest wörtlich). Als alleiniges Oracle wäre der Eintrag zirkulär.

**Aufgelöst wird es durch eine zweite, unabhängige Hand** — nicht durch
eine zweite Fassung derselben Bibliothek. Für ATR sind es zwei:

| zweite Hand | Lizenz | Unabhängigkeit belegt durch |
|---|---|---|
| `lsatr` (dmsc/mkatr) | GPL-2.0 | eigene C-Codebasis, eigener DOS-2- und SpartaDOS-Leser, keine atrcopy-Verwandtschaft |
| `a8rawconv` | GPL-2.0-or-later | anderer Autor, anderer Ansatz (Flux-Ebene); `ATR→XFD` byteidentisch zum Korpus-XFD |

**Registrierungsregel:** jeder neue Eintrag nennt, **wer den Korpus
erzeugt hat, gegen den er prüfen soll**. Ist es dasselbe Werkzeug, wird
er nur zusammen mit einer zweiten Hand eingetragen — oder gar nicht.

## Der Differenzlauf-Standard (seit MF-629)

Verglichen wird **Inhalt, byteweise nachgerechnet**, nicht die
Verzeichnisdarstellung:

| Stufe | Form | Beispiel |
|---|---|---|
| schwach | Verzeichnis listen | `floptool flopdir d64 cbmdos <datei>` |
| **stark** | **Hash je Datei** | `floptool flophashes d64 cbmdos <datei>` |
| stärker | Bytes herausholen und selbst hashen | `floptool flopread … <pfad> <ziel>` |

Gemessen am Korpus-D64: `UFT MARKER`.

> **Berichtigt MF-684.** Hier stand „254 Byte, sha1 `56fea729…`" als
> Messung. Das ist der Wert **nach floptool-Polsterung**: `flophashes`
> füllt den Dateiinhalt auf die Sektorkapazität auf (254 B bei einem
> Block). Der wahre Inhalt ist **127 Byte**, sha1 `a9fb8f28…` — zwei
> unabhängige Hände einig, und seit MF-683 vom eigenen
> Verzeichnisleser gestützt: `uft_cbmdos_read_directory` meldet für
> diese Datei **1 Block**, und 127 Byte passen in einen.
>
> Der Unterschied ist nicht kosmetisch. Ein Differenzlauf gegen den
> gepolsterten Wert misst **die Polsterung**, nicht das Format: er wird
> grün, sobald unser Leser auf 254 auffüllt — also gerade dann, wenn er
> etwas erfindet. Ein Oracle-Wert, der Erfindung belohnt, ist schlimmer
> als keiner.
>
> **Toleranzregel:** `flophashes` taugt für Datei-**Namen**, -**Zahl**
> und -**Reihenfolge**; für Inhalte nur mit ausdrücklicher
> Padding-Toleranz. Wer Inhalte byteweise vergleichen will, nimmt
> `flopread` und hasht selbst — die Zeile darüber nennt das ohnehin den
> stärkeren Weg.

Wo ein Werkzeug Spur- statt Dateiebene beurteilt (`nibscan`s BAM-/DIR-
und Full-CRC), ergänzt das die Datei-Ebene, es ersetzt sie nicht.

## Pflichtfeld: Längensemantik (MF-685)

**Jedes registrierte Oracle nennt, ob es rohe oder auf Blockgrenzen
gepolsterte Längen liefert** — gemessen, mit Datum und Kalibrierdatei.
Ohne dieses Feld ist ein Eintrag unvollständig.

Der Anlass steht eine Überschrift höher: floptool meldete 254 Byte für
eine 127-Byte-Datei, und dieser Wert stand hier als Messung. Es gibt
keinen Grund anzunehmen, dass floptool das einzige Werkzeug mit dieser
Eigenschaft ist — also wird gefragt, statt gehofft.

**Kalibrierung:** eine Datei mit krummer, bekannter Länge durchreichen
und die gemeldete Zahl ansehen. Das Hausmaß ist **127 Byte** — die
Länge des UFT-Markers, der in beiden Korpus-Abbildern steckt (D64 und
ADF) und dessen wahrer Wert dreifach belegt ist.

Warum krumm: eine Datei mit glatter Länge kann beide Antworten geben,
ohne dass man den Unterschied sieht. 127 liegt unter jeder üblichen
Blockgröße und über null — eine gepolsterte Antwort ist sofort als
solche erkennbar (254 bei CBM-DOS, 488 bei Amiga-OFS, 512 bei FAT).

| Semantik | heißt | taugt für |
|---|---|---|
| **roh** | Länge aus dem Dateikopf | Inhalte byteweise |
| **gepolstert** | belegte Blockkapazität | Namen, Zahl, Reihenfolge — Inhalte nur mit ausdrücklicher Toleranz |
| **ungemessen** | niemand hat nachgesehen | nichts. Ein Oracle ohne Kalibrierung ist eine Zusicherung |

Die Kalibrierung passiert **einmal je Oracle**, nicht einmal je
Schreck.

### Stand der Kalibrierung

| Werkzeug | Semantik | gemessen | womit |
|---|---|---|---|
| `floptool` (`flophashes`) | **gepolstert** (254 B bei einem CBM-Block) | 2026-08-29, MF-684 | Korpus-D64, `UFT MARKER` |
| `xdftool` (amitools, seit MF-693 registriert) | **roh** | 2026-08-29 MF-685, nachgemessen 2026-08-30 | Korpus-ADF, `marker.txt` — der eigene Leser meldet 127, nicht 488 |
| `adfrescue` | **roh** | Scout-Zyklus `adf_zweitmeinung` | 127 B, byteidentisch zur xdftool-Extraktion |
| `to_woz2` | **nicht anwendbar** (erzeugt Bitstrom, listet keine Dateien) | 2026-08-30, MF-712 | Ausgabe-SHA fuer eine benannte Eingabe, im Eich-Test gepinnt |
| `fluxtoimd` (FM) | **nicht anwendbar** (dekodiert Kanalbits, listet keine Dateien) | 2026-09-04, MF-864 | vier Abnahmen an der erzeugten FM-Spur, alle uebereinstimmend |
| `lsatr`, `a8rawconv`, `gw`, `cpmls`, `hxcfe`, `samdisk`, `dtc` | **ungemessen** | — | offen |

Die sieben ungemessenen sind kein Vorwurf, sondern eine Liste: keiner
von ihnen war bisher an einem Inhalts-Differenzlauf beteiligt. Wer den
ersten fährt, kalibriert vorher.

## Registrierte Oracles (10)

Stand `tests/differential/oracles.py`, 2026-08-30 (MF-693).

| Kurzname | Variable | Lizenz | Herkunfts-Anker | entscheidet |
|---|---|---|---|---|
| `gw` | `GW` | Unlicense | `--version` | Flux-Aufnahme und -Wandlung am Greaseweazle; Bezug für die gw-vs-UFT-Differenztests (P3.2) |
| `cpmls` | `CPMLS` | GPL-3.0 | `-h` | CP/M-Verzeichnislesung gegen eine `diskdefs`-Definition. Liest `cpmls` ein Abbild und UFT nicht gleich, liegt es an UFT |
| `hxcfe` | `HXCFE` | GPL-2.0 | `-help` | Format-Wandlung über viele Container (HFE, IMG, DSK, …); Bezug für T1b-Eingaben |
| `samdisk` | `SAMDISK` | MIT | `--version` | Container-Formate und ihre Randfälle. Die **Quelle** liegt zusätzlich im Baum (`src/samdisk/`) und dient als Spec-Referenz |
| `dtc` | `DTC` | proprietär, nur Ausführung | `-h` | KryoFlux-Rohstrom-Aufnahme; Bezug für den KryoFlux-Lesepfad |
| `floptool` | `FLOPTOOL` | GPL-2.0-or-later (MAME) | **SHA-256** (keine Versionsabfrage) | Verzeichnis **und Hashes** bei ausdrücklich genanntem Container + Dateisystem |
| `fluxtoimd` | — (Python, im Baum geklont) | GPL-3.0-**only** | Pfad `tools/uft-scout/work/fluxtoimd` | FM- und M2FM-Modulation. Seit MF-864 die zweite Hand fuer die FM-Pruefspur: Adressmarken, `FM.decode()`, CRC-Parameter. **Wird ausgefuehrt, nicht portiert** — GPL-3-only vertraegt sich mit dem Baum, aber der Kanal ist ausdruecklich „Oracle" |
| `lsatr` | `LSATR` | GPL-2.0-or-later | `-v` → „mkatr version 1.4" | Atari-DOS in ATR **und** XFD: Geometrie, DOS-Variante, freie Sektoren; Inhalte je Datei über `-x`/`-X`. Die **unabhängige** Hand gegen den atrcopy-erzeugten Korpus |
| `xdftool` | `XDFTOOL` | GPL-2.0-or-later | **Paketversion** (`importlib.metadata.version("amitools")`) — das Werkzeug gibt selbst keine aus; die SHA-256 der aufgeloesten Datei steht im Manifest daneben | AmigaDOS-Verzeichnis und Dateiinhalte in ADF. Zugleich der **Erzeuger** von `xdftool_dd_ofs.adf` — beantwortet damit die Provenienz-, nicht die Richtigkeitsfrage. Laengensemantik **roh** (127, nicht 488), Unabhaengigkeit gegen `adfrescue` gemessen (MF-693) |
| `libdsk` | — (im Baum geklont und dort gebaut) | LGPL-2+ (John Elliott) | Pfad `tools/uft-scout/work/libdsk` + **Fassung 1.5.12** (`dsktrans -version`, berichtigt MF-1032 — hier stand „das Werkzeug hat keine Versionsabfrage“, und das trifft nicht: `dsktrans -version` antwortet „libdsk version 1.5.12“, und `config.h` fuehrt `PACKAGE_VERSION "1.5.12"`) plus **SHA-256** der gebauten Werkzeuge (`dsktrans.exe` d0d826c2fc212d26725192418f53da1bec50a758d79f65f0500a7f086e3136df, `dskid.exe` 14abc39bc4ffebf4b2d8690d2e3be9a17955de0ef498337b367a2e77afd56e29) | **Fünfundzwanzig** Container-Formate, darunter `qrst`, `myz80`, `nanowasp`, `logical`, `rcpmfs`, `cfi`, `jv3`, `sap`, `imd`, `apridisk`, `dc22`/`dc42`, `dsk`/`edsk`, `copyqm`, `tele`, `ydsk`, `simh`, `ldbs` — und, was hier den Unterschied macht, **Formatbeschreibungen** in `doc/` (`qrst.html`, `cfi.html`, `apridisk.html`, `libdsk.txt` mit 121 KB). MF-1028 hat `qrst` daraus gehoben. Registriert mit dem Vorbehalt aus dem Abschnitt darunter: **der Bau geht nur an den Autotools vorbei** |
| `to_woz2` | `TO_WOZ2` | GPL-3.0 (Zone GELB) | **Quellstand + Baurezept + Ausgabe-SHA** — nicht der Binaerhash (siehe unten) | Apple-II-Sektorabbild → WOZ 2.0 mit **synthetisiertem** GCR-Strom (6-and-2 / 5-and-3). Die fremde Hand fuer `do`, `po`, `d13` — Stufe **T1b** (Fremdwerkzeug-Abbild), nicht T2 |
| `a2nibblize` | — (im Baum geklont, dort gebaut) | GPL-3.0 (Zone GELB) | **Quellstand + Baurezept + Ausgabe-SHA**, wie bei `to_woz2` und aus demselben Grund: Quellstand `639dc1c3281f`, Rezept unten, Ausgabe-SHA-256 `c09155a2662fe3bb85afc49f65223381df628b47319d111af9a0bacc919b372e` für die benannte Eingabe `tests/corpus_free/uftk_dos33_35trk.do` | **Apple-II-Sektorabbild (`.do`) → NIB.** Seine eigene Hilfe: „*Converts an Apple ][ floppy disk image from .do format to .nib nibble format*". Die fremde Hand für **`nib`** — seit MF-1050 Stufe **T1b** (Fremdwerkzeug-Abbild). **Es lag im selben Klon wie `to_woz2`, seit derselben Sichtung, und wurde nur nie bemerkt:** der Eintrag zu nibtools weiter unten sagt völlig richtig, über `nibconv` führe kein Weg zu T1b — das ist **Commodore**-NIB. Die Frage, ob das **Apple**-Paket einen Erzeuger enthält, hat niemand gestellt. **Lehre: ein geklontes Paket ist mehr als das eine Werkzeug, wegen dessen man es geklont hat.** Und die Spurlänge ist dort nicht behauptet, sondern **gerechnet** — `a2nibblize.c:78` baut 6656 aus der Feldanordnung auf (6 + 3 + 8 + 3 + 3 + 343 + 3 + 27 = 396 je Sektor, ×16, + 0x30 + 0x110), also genau die Konstante, die `uft_nib.c` führt |

### libdsk — der Bau geht nur an den Autotools vorbei (MF-1028)

libdsk liegt seit der Scout-Welle als Klon unter
`tools/uft-scout/work/libdsk` (gitignored). Bis MF-1028 galt es als
nicht baubar; gemessen ist es baubar, und der Weg dorthin gehört
aufgeschrieben, weil er nicht der dokumentierte ist.

**Erstens: `make` gibt es auf dieser Maschine.** Es heißt
`mingw32-make.exe` und liegt in der Qt-Toolchain
(`/c/Qt/Tools/mingw1310_64/bin/`, GNU Make 4.2.1) — nur nicht unter dem
Namen `make` und nicht im `PATH`. Wer `which make` fragt, bekommt nichts
und schließt falsch.

**Zweitens: `./configure` läuft durch, `make` nicht.** Die erzeugten
Makefiles setzen `SHELL` auf `C:/Program Files/Git/usr/bin/sh.exe` —
**mit Leerzeichen, unquotiert**. Jeder libtool-/depcomp-Aufruf scheitert
mit `/usr/bin/sh: line 1: C:/Program: No such file or directory`. Ein
`SHELL=`-Override auf der Kommandozeile hilft nicht, weil der Pfad in
weitere Variablen expandiert ist.

**Drittens: direkt übersetzen geht.** libdsk ist reines C:

```sh
export PATH=/c/Qt/Tools/mingw1310_64/bin:$PATH
cd tools/uft-scout/work/libdsk
./configure --disable-shared          # nur fuer config.h
gcc -c -O1 -w -I include -I . -DHAVE_CONFIG_H -DNOTWINDLL lib/*.c
ar rcs libdsk_static.a *.o
for t in dskid dskform dsktrans; do
  gcc -O1 -w -I include -I . -DHAVE_CONFIG_H -DNOTWINDLL -o $t.exe \
      tools/$t.c tools/crc16.c tools/utilopts.c tools/formname.c \
      tools/bootsec.c libdsk_static.a -lz
done
```

**70 von 70** Bibliotheksdateien übersetzen fehlerfrei. Zwei Fallen dabei:
`tools/dskutil.c` darf **nicht** mitgelinkt werden (es hat ein eigenes
`main`), und `-lz` ist nötig — zlib liegt im Qt-MinGW-Toolchain
(`libz.a` und `zlib.h` sind beide da; siehe P3-329, wo genau diese Frage
als Eigentümer-Entscheidung offensteht).

**Was libdsk damit entscheidet und was nicht.** Es **liest** die
Formate seiner Treibertafel und beschreibt einige davon in `doc/`; damit
ist es Referenz für Aufbau und Inhalt. Es **erzeugt** sie auch — aber
für ein kopfloses Eingabeabbild bekommt sein `raw`-Treiber die Geometrie
nicht (er nimmt 85 Zylinder / 2 Köpfe / 70 Sektoren an), und `dsktrans`
hat kein `-format`. Damit sind Fixtures **fremder Erzeugung** heute
nicht herstellbar; die Formate bleiben auf T2 statt T1b. Der Blocker ist
als **P3-333** gefasst, mit dem, was ihn öffnen würde.

### floptool — lag einmal auf dieser Maschine (MF-720: nicht mehr)

Beschafft aus `mame0289b`; die SHA-256 der Distribution wurde gegen die
offizielle `SHA256SUMS` geprüft. Werkzeug-Hash `6973d1b5…20ac21`.

> **Berichtigung MF-720.** Die Überschrift sagte „der einzige, der auf
> dieser Maschine liegt“. Gemessen (2026-08-30) stimmt das nicht mehr:
> `shutil.which('floptool')` leer, `FLOPTOOL` ungesetzt, Downloads,
> Temp und Repo durchsucht — das Binary ist fort. Damit sind **0 von 9**
> registrierten Oracles verfügbar, und jede Eichung, die floptool
> braucht, überspringt sich still.
>
> Das ist Doku-Drift derselben Art wie die drei T3-Zahlen in der README
> (MF-716): eine Aussage über den **Maschinenzustand**, die in einer
> Datei steht und dort nicht altern kann. Wer sie wieder beschafft,
> pinnt sie über den bereits notierten Werkzeug-Hash und legt sie an
> einen Ort, den `FLOPTOOL=` benennt.

**Gemessen am freien Korpus (MF-623/629):**

* echte Auflistung: `.d64`, `.d71`, `.g64` — dazu `pc_fat` vollständig
  (gegen ein selbstgebautes 720K-FAT12 geprüft)
* **Fallstrick:** floptool prüft den **Container**, nicht das
  **Dateisystem**. `flopdir adf cbmdos` auf einem AmigaDOS-Abbild endet
  mit `rc=0`, leerem Volumenamen und leerer Liste. Eine leere Auflistung
  ist deshalb **„kein Ergebnis"**, nie „leere Diskette" — sonst
  bestätigt das Oracle einen Lesefehler von UFT, statt ihn aufzudecken.
* Zufallsbytes und ein falscher Container fliegen dagegen laut heraus.
* `.d80` und `.d82` **hängen** (>9 min, abgebrochen). Jeder Aufruf
  braucht ein Zeitlimit.
* Gegen die fünf Phase-1-Ziele: **1 von 5** (nur D64). Für ADF und ATR
  bringt floptool kein Dateisystem mit.

### to_woz2 — warum der Binaerhash hier NICHT der Anker ist

`floptool` kommt als **heruntergeladene Distribution**: sein Binaerhash
ist stabil und pinnt das Werkzeug. `to_woz2` wird aus Quellen **gebaut**
— und da gilt das nicht.

**Gemessen (MF-711/712):** zwei unabhaengige Baue aus demselben
Quellstand (`639dc1c`) ergaben

| | Binaer-SHA-256 | Ausgabe 16-Sektor | Ausgabe 13-Sektor |
|---|---|---|---|
| Bau A (Scout) | `434cfbda…` | `0015aa1e2024…` | `a5ff575f7f82…` |
| Bau B (Gegenbau) | `4dbb8def…` | `0015aa1e2024…` | `a5ff575f7f82…` |

**Verschiedene Binaries, byteidentische Ausgabe.** Wer den Binaerhash
als Identitaet liest, hat ein Oracle, das nach jedem Neubau ein anderes
zu sein scheint — und ein T1b-Manifest, das ohne Not veraltet.

Zitierfaehig ist deshalb: **Quellstand + Baurezept + Ausgabe-SHA fuer
eine benannte Eingabe.** Der Hash im Manifest sagt weiterhin, WELCHER
BAU lief; das ist eine Bau-Angabe, keine Werkzeug-Identitaet.

**Baurezept** (Autotools werden nicht gebraucht; das `ctest`-Submodul
haengt an einem toten `git://` und wird so umgangen), aus `src/`:

```
gcc -O2 -o to_woz2 to_woz2.c nibblize_4_4.c nibblize_5_3.c \
    nibblize_5_3_alt.c nibblize_5_3_common.c nibblize_6_2.c \
    ctest/ctest.c -I.
```

### a2nibblize — dasselbe Paket, zwei Handgriffe mehr (MF-1050)

`a2nibblize` liegt neben `to_woz2` im selben Klon und erzeugt aus einem
`.do`-Sektorabbild eine **NIB**. Damit hat `nib` seit MF-1050 ein Abbild
von fremder Hand — Stufe **T1b**.

Der Bau braucht zwei Dinge mehr als `to_woz2`, beide wegen der
Autotools, die hier nicht laufen (dieselbe Lage wie bei libdsk):

1. **`-DPACKAGE_STRING=…`** — `a2nibblize_opt.c:51` druckt es in
   `--version` und bekommt es sonst aus `config.h`.
2. **Ein Ersatz für gnulibs `<binary-io.h>`.** `a2nibblize.c` ruft
   `SET_BINARY(1)`, bevor es die Nibbles nach stdout schreibt. Ohne den
   Schalter wandelt die Windows-C-Bibliothek beim `putchar` jedes `\n`
   in `\r\n` — die erzeugte NIB wäre um **jedes 0x0A-Nibble** verfälscht.
   Der Ersatz ist drei Zeilen und gehört ins Rezept, nicht in den Baum:

```c
/* src/binary-io.h — nur fuer den Oracle-Bau, liegt in
   tools/uft-scout/work/ (gitignored) */
#if defined(_WIN32) || defined(__CYGWIN__)
#  include <io.h>
#  include <fcntl.h>
#  define SET_BINARY(fd)  ((void)_setmode((fd), _O_BINARY))
#else
#  define SET_BINARY(fd)  ((void)0)
#endif
```

```
gcc -O2 -DPACKAGE_STRING='"Apple-II-Disk-Tools (UFT oracle build)"' \
    -o a2nibblize a2nibblize.c a2nibblize_opt.c \
    nibblize_4_4.c nibblize_5_3.c nibblize_5_3_alt.c \
    nibblize_5_3_common.c nibblize_6_2.c ctest/ctest.c -I.

./a2nibblize < uftk_dos33_35trk.do > a2nibblize_uftk_35trk.nib
```

**Die Spurlänge ist dort nicht behauptet, sondern gerechnet**
(`a2nibblize.c:78`): 6 Byte Vorspann + 3 Adress-Prolog + 8 (Spur,
Sektor, Band in 4-and-4) + 3 Adress-Epilog + 3 Daten-Prolog + 343
6-and-2-Nutzlast + 3 Daten-Epilog + 27 Lücke = **396** je Sektor; mal 16,
plus 0x30 und 0x110 = **6656**; mal 35 Spuren = **232 960**. Genau die
Konstanten, die `src/formats/nib/uft_nib.c` führt — bis MF-1050 ohne
genannte Quelle.

gcc 13.1.0 (MinGW), rc=0, 0 Warnungen.

**Die Eichung** steht in `tests/differential/test_oracles.py`
(`test_to_woz2_reproduces_its_pinned_output_when_present`): sie erzeugt
eine deterministische 143 360-Byte-Eingabe, ruft das Werkzeug und
vergleicht die SHA-256 der Ausgabe gegen
`cb4269d51f73b070bcf086eb032846973a558253302d618adf942d0a50c86107`.
Ohne Werkzeug ueberspringt sie sich und **behauptet nichts**. Der Anker
ist rotbeweis-geprueft: mit verfaelschtem Pin wird die Pruefung rot.

**FALLSTRICK — Benutzungsregel.** Mit einem **absoluten** Pfad bricht
`to_woz2` mit `0xC0000374` (STATUS_HEAP_CORRUPTION) ab. Das ist der
1-Byte-Ueberlauf in `parse_filename` (`to_woz2.c:367-369`), und er ist
nicht theoretisch — die erste Fassung des Eich-Tests hat ihn
ausgeloest. **Immer aus dem Arbeitsverzeichnis mit relativen Namen
rufen.** Wer das Werkzeug mit fremden Dateinamen fuettert, fuettert
einen Ueberlauf.

**Fuenfte Frage (MF-644), ehrlich:** eine zweite, unabhaengige Hand fuer
WOZ 2.0 gibt es hier **nicht**. Der Abgleich stuetzt sich auf die
veroeffentlichte WOZ-2.0-Spezifikation (applesaucefdc.com) — eine Spec,
kein zweites Werkzeug. Ein Fehler, den `to_woz2` aus der Spec
uebernommen haette, faellt damit nicht auf.

**Lizenz:** GPL-3.0 (`COPYING`, woertlicher Text), Zone **GELB** — kein
Port. Verglichen wird ausschliesslich die Ausgabe. Nachgelagert traegt
`nibblize` die Erklaerung „Based on code by Andy McFadden"
(CiderPress, BSD-3): eine zweistufige Kette, die nur bei einem Port zu
klaeren waere. Das Repo ist vom Urheber als **DEPRECATED**
gekennzeichnet — fuer ein Oracle folgenlos, fuer eine Abhaengigkeit
waere es eines.

## Vorgemerkt, noch nicht registriert

Diese Werkzeuge sind gemessen oder gebaut, haben aber **keinen**
Registry-Eintrag. Sie zählen deshalb für kein T1b-Manifest.

| Werkzeug | Stand | offen |
|---|---|---|
| `nibconv`, `nibscan` (nibtools) | **hier gebaut und gelaufen (MF-1005)**, hardwarefrei mit MinGW ohne OpenCBM: `gcc -O2 -std=c99 -I include/WINDOWS -D WIN32 -o nibscan nibscan.c gcr.c prot.c fileio.c crc.c md5.c lz.c`. Gegen `tests/corpus_free/vice_c1541_35trk.d64` gelaufen: Header-/Cosmetic-Disk-ID und je Spur Länge und Dichte (7692/Dichte 3 für Spur 1–17, 7142/Dichte 2 ab 18). **Lizenz: für ein Oracle ohne Auflage — die Fassung selbst ist aber offen (P3-316).** Eigentümer-Feststellung 2026-09-10: frei. Welche freie Lizenz, ist seit MF-1007 **widersprüchlich gemessen**: der Scout las bei Commit `a549c18` (2025-01-30) einen **GPL-3.0-Volltext**, das am selben Tag beigesteuerte `neue-ideen/nibtools-extra.zip` trägt eine 229-Byte-`LICENSE`, die **Apache-2.0** nennt. Für diesen Eintrag ist das folgenlos — ein Oracle wird ausgeführt, nichts übernommen —, für einen **Port** wäre es entscheidend: Apache-2.0 ist mit GPL-**2** unvereinbar, mit GPL-3 einseitig vereinbar | **Kein Eintrag — und der Grund ist nicht die Lizenz, sondern dass es nichts entscheidet.** Gemessen: nibtools bedient `g64` (**T1**), `d64`, `d71`, `g71` (**T1b**) — jedes davon steht schon auf der höchsten erreichbaren Stufe. Und es ist **nicht** das Oracle für UFTs `nib`: das ist **Apple-II**-NIB (`uft_nib.c:3`), nibtools' NIB ist **Commodore**. Zwei Formate, dieselbe Endung. `nibconv` schreibt ausserdem nur D64/G64, kann also kein NIB **erzeugen** — kein Weg zu T1b. Ein Eintrag müsste die Spalte „entscheidet" leer lassen und trüge dafür die Pflege (SHA-Bindung, Eichung nach MF-693). Bleibt vorgemerkt, bis ein Commodore-NIB oder eine Schutzspur-Frage im Korpus liegt — `SCOUT-20`. <br>**Herkunfts-Anker, falls es doch dazu kommt:** NICHT der Binärhash. Das Binary trägt den Bauzeitstempel in sich („Built Sep 10 2026 16:03:19"), sein SHA-256 ist also nicht reproduzierbar — es braucht das `to_woz2`-Muster: Quellstand + Baurezept + Ausgabe-SHA |
| `dskx` (FloppyControl) | Quelle gelesen, CLI belegt (`list`/`extract`/`--deleted`) | **nicht gebaut, nicht gelaufen** — vor jedem Eintrag bauen. Eng auf gelöschte FAT12-Einträge und Bad-Cluster zu schneiden, weil floptool den normalen Inhalt bereits liest — `SCOUT-F1` |
| `a8rawconv` | im Baum vendort (`src/a8rawconv/`), heute gebaut; `ATR→XFD` byteidentisch zum Korpus-XFD | Eintrag ausstehend (`SCOUT-33`). Zugleich die In-Tree-Referenz für den FM-Pfad |
| `atrcopy` | erzeugt unseren ATR-Korpus; `crc`-Unterbefehl liefert CRC32 je Datei über den Inhalt | **Auflage:** braucht `numpy<1.23` — unter NumPy 2.5.1 drei gemessene Abstürze bei jedem Abbild-Open. Nur zusammen mit `lsatr` eintragen (Zirkularität) |
| **fdc_bitstream** (yas-sim) | **extern**, nicht im Baum — die vendorte Kopie ist mit MF-626 gelöscht | als **Upstream-Oracle** bauen und eintragen, **nicht** zurückholen (siehe unten) |

### Warum fdc_bitstream extern bleibt (MF-644)

Der Wunsch dahinter ist richtig: ein **zweiter, unabhängiger
MFM-Decoder als Schiedsrichter** ist genau, was Tier-Hebung und
Rettungskette später brauchen.

Der Weg dorthin ist aber nicht `git revert` auf MF-626. Ein
zurückgeholtes Vendoring bringt 2795 Zeilen in den Baum, die

* die Verwaisten-Regel bei **jedem** Durchgang erneut anfassen muss,
* eine Baulast in beiden Build-Systemen tragen,
* und einen Anker brauchen, der nur existiert, um sie zu rechtfertigen.

Als **externes Oracle** entfällt all das: Upstream im Prüfstand bauen,
hier registrieren, fertig. Kein Code im Baum, keine Lizenzfrage, keine
Baulast — und derselbe Schiedsrichter.

Der Anker gehört deshalb **hierher**, nicht in einen Plan: ein Oracle
ist kein Baustein, den man später verdrahtet, sondern ein Werkzeug, das
urteilt.

## Was ausdrücklich **kein** Oracle ist

| Werkzeug | Grund |
|---|---|
| `unadf`, AdfOpus | teilen sich **ADFlib** — untereinander eine Hand, also als *Paar* keine Zweitmeinung. **Nicht** dieselbe Hand wie unser Korpus-Abbild; das stand hier bis MF-682 und war gemessen falsch |
| `amigadx` | vendorte ADFlib 0.7.10 **und** Total-Commander-Plugin ohne Konsolen-Einstieg |
| `ADFDiskBox` | ruft nur `cmd.exe /C gw …`; **keine** eigene ADF-Ebene (0 Treffer auf `ReadAllBytes\|FileStream\|adflib\|RootBlock`) |
| `FloppyControl` selbst | WinForms-GUI, nicht skriptbar. Nur sein `dskx` ist ein Konsolenprogramm |
| `WinUAE` | ADFlib-unabhängig und damit inhaltlich interessant, aber GUI **und** ohne Lizenzdatei im Repo — Referenz ja, Oracle nein |
| `atrip` | **dieselbe Hand wie der Korpus.** `README.rst:6` nennt es wörtlich „The successor to atrcopy", gleicher Autor — fünfte Registrierungsfrage, ausgeschlossen. Unabhängig davon auf dieser Maschine nicht lauffähig: `pkg_resources` fehlt unter Python 3.13, und `np.fromstring` steht neunmal im Code (tot unter NumPy 2.5.1) |

**Offene Lücke:** eine **ADFlib-unabhängige, skriptbare** Zweitmeinung
für ADF fehlt weiterhin. Drei Zyklen haben sie gesucht und nicht
gefunden. Phase 1 Nr. 2 (AmigaDOS) hängt daran.

## Pflege

* Der **Scout** trägt neue Kandidaten hier ein — er baut sie ohnehin —
  und nennt dabei Lizenz, Bau-Rezept und was das Werkzeug entscheidet.
* Der **MF-Workflow** trägt den Registry-Eintrag nach, sobald das
  Werkzeug gebaut und gelaufen ist.
* `tests/differential/test_oracles.py` prüft die Registry auf sich
  selbst (14 Prüfungen, läuft in ctest als `oracle_registry`). Fehlt ein
  Werkzeug auf der Maschine, überspringen sich die Tests, die es
  brauchen — sauber und sichtbar, nie stillschweigend grün.
