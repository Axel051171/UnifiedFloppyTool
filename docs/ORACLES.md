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

## Registrierte Oracles — diese Tafel ist eine AUSWAHL (MF-1149)

**Hier stand „Registrierte Oracles (10)", und drei Zahlen widersprachen
sich.** Gemessen am 2026-09-15: `tests/differential/oracles.py` fuehrt
**25** Eintraege (`len(REGISTRY)`), diese Tafel hatte **7** Zeilen, und
die Ueberschrift sagte **10**. Das ist der Zahlendrift-Fall, den dieser
Baum mehrfach gesehen hat (MF-541, MF-601, MF-626) — eine von Hand
gepflegte Zahl neben einer abgeleiteten Quelle.

Die Ueberschrift nennt deshalb keine Zahl mehr. **Die Registry ist die
Quelle**, und ihr eigenes Tor `oracle_registry` haelt sie vollstaendig —
es prueft je Eintrag Herkunft, Zweck, Abstammung und Fassungsanker und
faellt, wenn einer fehlt (gemessen: meine beiden neuen Eintraege fielen
beim ersten Lauf mit „keine Versionsabfrage — dann ist seine Aussage
nicht zitierfaehig"). Diese Tafel erklaert die Eintraege, die eine
Erklaerung brauchen. Wer alle sehen will: `ctest -R oracle_registry`
listet sie samt Verfuegbarkeit. Eine ABGELEITETE Tafel waere besser und
ist nicht gebaut — als **P3-404** eingetragen statt still gelassen.

Stand der erklaerten Auswahl: 2026-09-15 (MF-1149; vorher 2026-08-30,
MF-693).

| Kurzname | Variable | Lizenz | Herkunfts-Anker | entscheidet |
|---|---|---|---|---|
| `gw` | `GW` | Unlicense | `--version` | Flux-Aufnahme und -Wandlung am Greaseweazle; Bezug für die gw-vs-UFT-Differenztests (P3.2). **Berichtigt MF-1082:** MF-795 hielt fest, `gw` kenne PC-98 nur als Rohgeometrie (`pc98.2d/2dd/2hd/2hs`). Gemessen traegt es ein eigenes **NFD**-Modul (`src/greaseweazle/image/nfd.py`), in `tools/util.py:303` als `'.nfd': 'NFD'` registriert. Es ist `read_only = True` und liest **nur r0** — der Blocker faellt dadurch nicht, aber die Begruendung stand auf einer falschen Praemisse |
| `cpmls` | `CPMLS` | GPL-3.0 | `-h` | CP/M-Verzeichnislesung gegen eine `diskdefs`-Definition. Liest `cpmls` ein Abbild und UFT nicht gleich, liegt es an UFT |
| `hxcfe` | `HXCFE` | GPL-2.0 | `-help` | Format-Wandlung über viele Container (HFE, IMG, DSK, …); Bezug für T1b-Eingaben |
| `samdisk` | `SAMDISK` | MIT | `--version` | Container-Formate und ihre Randfälle. Die **Quelle** liegt zusätzlich im Baum (`src/samdisk/`) und dient als Spec-Referenz |
| `dtc` | `DTC` | proprietär, nur Ausführung | `-h` | KryoFlux-Rohstrom-Aufnahme; Bezug für den KryoFlux-Lesepfad |
| `epstool` | `EPSTOOL` | **keine** — nur Ausführung | SHA-256 des Binärs (keine Versionsabfrage; `version`/`--version` drucken nur den Kopf) | Ensoniq EPS/EPS-16+/ASR: legt Diskettenabbilder an (`mkhfe`), schreibt Dateien durch seinen eigenen Dateisystemcode hinein (`import`) und wandelt HFE↔roh (`hfe2img`). Seit MF-1103 der Erzeuger für `edk`. **Die Lizenzlage gehört dazu:** sein README sagt *„provided for educational and archival purposes"* — das ist **keine Rechteeinräumung**. Damit ist es ein Oracle wie `dtc`: ausführen ja, weitergeben nein. Der Quelltext bleibt unter `tools/uft-scout/work/epstool/` und wird nicht übernommen. **Und eine Grenze ist gemessen, nicht vermutet:** `mkhfe --os` bettet ein **83 KB großes EPS-1-Betriebssystem** ein — Ensoniqs Code. Belege für `tests/corpus_free/` werden deshalb **ohne** `--os` gebaut, und `test_edk_gegen_epstool` hält das fest (0 von 4 verbotenen Zeichenketten) |
| `mkfs.cpm` (+ `cpmcp`) | `MKFS_CPM` | **GPL-3.0** (cpmtools 2.21, Michael Haardt — gemessen MF-1149 an `COPYING` und `configure.in`) | **keine** Versionsabfrage (gemessen: der Aufruf ohne Argumente druckt nur „Usage: mkfs.cpm …", `-V`/`--version` gibt es nicht); gepinnt ueber SHA-256 `f5ad7261…` bzw. `76f3a12e…` | **Der CP/M-ERZEUGER, seit MF-1149** — und er beantwortet P3-383. `mkfs.cpm` legt kein Abbild um, es legt ein **Dateisystem** an, und `cpmcp` legt Dateien hinein; wo die Bytes landen, entscheidet die Definition. Genau darin liegt der Unterschied zu `dsktrans`, mit dem MF-1039 T1b verneint hat („eine Gleichheit ohne Aussage"). **Gemessen:** dieselben 20 Nutzdateien weichen unter `cpm86-720` in **474 521** und unter `altdsdd` in **473 722** von 737 280 Byte ab. `fsck.cpm` prueft das Erzeugnis fehlerfrei, `cpmls -l` listet es. **Die Grenze gehoert dazu:** allein reicht es nicht — `mkfs.cpm -f ibm-3740` schreibt **9984** statt 256 256 Byte, den vollen Behaelter legt `dskform` |
| `dskform` | `DSKFORM` | **LGPL-2.0-or-later** (libdsk 1.5.12, John Elliott) | **keine** eigene Versionsabfrage (sie sitzt in `dsktrans -version`, MF-1032); gepinnt ueber SHA-256 `73586b31a80b0cb141dfa3c82ece6556af3b0a52a015e0863f27bdd2c5fe70d2` | **Behaelter-Erzeuger, seit MF-1149**: `dskform -type raw -format <name>` legt ein Rohabbild in der VOLLEN Groesse einer benannten Geometrie an — fuer `pcw720` gemessen 737 280 Byte, 737 270 davon 0xE5. Gebraucht, weil cpmtools nur Systemspuren und Verzeichnis schreibt, waehrend `uft_cpm_detect_diskdef()` die exakte Gesamtgroesse verlangt. **Den Bruecken-Namen nennt cpmtools selbst:** seine `diskdefs`-Zeile fuer `cf2dd` traegt `libdsk:format pcw720` |
| `floptool` | `FLOPTOOL` | **BSD-3-Clause** (MAME) | `version` **und** SHA-256 | Verzeichnis **und Hashes** bei ausdrücklich genanntem Container + Dateisystem; seit MF-1083 auch **Erzeuger**: `flopconvert` schreibt **122 von 151** Formaten. **Zwei Berichtigungen MF-1083:** hier stand *keine Versionsabfrage* — `floptool version` gibt es, und sie meldet die Bauzeichenkette; und *GPL-2.0-or-later* — MAMEs `floptool.cpp` und die Formatschicht tragen `// license:BSD-3-Clause` |

### Kein Abbild-Oracle, aber ein gebautes Messwerkzeug: `hdlen` (MF-1089)

**Es steht bewusst NICHT in der Tafel oben und nicht in
`tests/differential/oracles.py`**, und der Grund ist der Zweck: `hdlen`
erzeugt kein Abbild und kann in keinem Manifest-Eintrag als `oracle`
stehen. Es entscheidet eine Eigenschaft von **Polynomen**, nicht von
Disketten. Ein Eintrag in der Tafel waere eine Zusage, die es nicht
einloesen kann.

| | |
|---|---|
| Was | Hamming-Distanz-Profil eines CRC-Polynoms: bis zu welcher **Datenwortlaenge** eine HD garantiert ist |
| Urheber | Philip Koopman, Carnegie Mellon University |
| Lizenz | **CC BY-SA 4.0** — Ausfuehren und Lesen frei; ein PORT waere eine Eigentuemer-Entscheidung (siehe unten) |
| Quelle | `neue-ideen/hdlen.tar.gz` und `neue-ideen/ChecksumCRC_BookCode.zip` — **gemessen identisch**, 60 Dateien, 0 Unterschiede |
| Ort | `tools/uft-scout/work/hdlen/` (gitignoriert wie alle Orakel-Klone) |
| Quell-SHA-256 | `fast_hdlen.cpp` `ce5bdd7afdbe534125759b3ad61698180fe85733dc1bd37a4062b63a703dfba1` |
| Binaer-SHA-256 | `2fc7cd30c11322a9fef2b007a1f06f0b008a7dc65b76c3b42872f20b1f0aa8fd` |

**Bau** — das mitgelieferte `build.sh` schaltet rund vierzig Warnungen
scharf (`-Werror -Weffc++ -Wpadded -Wunsafe-loop-optimizations`); mit
gcc 13 genuegt der schlichte Aufruf:

```bash
export PATH=/c/Qt/Tools/mingw1310_64/bin:$PATH
g++ -O2 -std=c++11 fast_hdlen.cpp -o hdlen.exe
```

**Eichung — und sie ist die des Urhebers, nicht meine.** Das Archiv
bringt 24 Paare aus Eingabe und `.gold`-Sollausgabe mit. Gemessen:
**24 von 24 getroffen**.

**Der erste Lauf meldete 0 von 24.** Die Ausgabe des Windows-Baus
traegt CRLF, die `.gold`-Dateien LF — Inhalt Zeichen fuer Zeichen
gleich. Das ist in dieser Sitzung das **dritte** Mal, dass ein
Byte-Vergleich Zeilenenden als Unterschied gemeldet hat (nach dem
`hardsector_tool`-Abgleich in P3-368 und dem `.gitattributes`-Befund
in MF-1085). Wer Rohbytes hasht, hasst die Zeilenenden mit.

**Wozu es hier taugt** — die Notation ist der halbe Nutzen:
`hdlen` erwartet **Koopman-Notation** (impliziter +1-Term, hoechstes
Bit weggelassen, links ausgerichtet). `0x1021` heisst dort `0x8810`.
Wer den normalen Wert einsetzt, misst ein anderes Polynom, ohne dass
irgendetwas warnt:

```
./hdlen.exe 0x8810       ->  0x8810 {32751,32751}        CCITT-16
./hdlen.exe 0xc002       ->  0xc002 {32751,32751}        CRC-16-IBM/ARC
./hdlen.exe 0x82608edb   ->  {4294967263,91607,2974,...} CRC-32
```

**Was ein Port kosten wuerde:** UFT ist GPL-2.0-**or-later**. CC BY-SA
4.0 ist einseitig nach GPL-3 vertraeglich — ein uebernommener Rumpf
zwingt das Gesamtwerk also auf GPL-3 und nimmt dem Projekt die
GPL-2-Option. Dieselbe Lage wie bei CRC RevEng (P3-370) und bei
`hardsector_tool` (P3-368). Ausfuehren bindet nichts; deshalb wird
ausgefuehrt und nicht uebernommen.

> **NACHTRAG MF-1195 — einmal ist CC-BY-SA-Material doch uebernommen
> worden, und das gehoert hier vermerkt, damit der Baum sich nicht selbst
> widerspricht.**
>
> Seit MF-1195 liegen **113 Referenzsaetze** aus dem Wikipedia-Artikel
> „List of floppy disk formats" (Schnappschuss 2026-09-16, CC BY-SA 4.0)
> als Tafel im Baum: `src/formats/reference/uft_floppy_reference*.inc`,
> Namensnennung als `UFT_FLOPPY_REFERENCE_SOURCE_URL` im Header. Das ist
> **kein Widerspruch zur Regel darueber**, aber es ist eine Ausnahme mit
> drei Gruenden, und alle drei sind benannt statt vorausgesetzt:
>
> 1. **Eine Eigentuemer-Entscheidung liegt vor** (2026-09-16, woertlich
>    „mach A-005 ungestopt"); sie hebt die Sperre **S3** fuer genau diese
>    Ernte auf. `P3-445` fuehrt sie samt dem, was sie NICHT aufhebt.
> 2. **Die Kosten sind schon bezahlt.** Der Satz oben nennt als Preis, dem
>    Projekt die GPL-2-Option zu nehmen — und genau das hat **MF-698**
>    bereits entschieden: die drei AIR-Dateien sind `GPL-3.0-only`, und
>    die verteilbare Fassung des Gesamtwerks ist seitdem an GPL-3
>    gebunden. Der Preis faellt hier also nicht ein zweites Mal an.
> 3. **Es sind Daten, kein Rumpf.** Uebernommen sind Zahlen und Namen,
>    keine Zeile fremder Logik — der Kanal *Nachbau* nach MF-695.
>
> **Und was die Entscheidung ausdruecklich nicht aendert:** eine
> Sekundaerquelle hebt **keine Tier-Stufe**. Die Tafel taugt zur
> Einordnung, nicht zum Nachweis; `UFT_FLOPPY_REF_WRITE_SAFE` wird fuer
> keinen ihrer Saetze gesetzt, und die Meldung im Oeffnungspfad nennt sich
> selbst „Einordnung aus einer Sekundaerquelle, kein Nachweis". Wer den
> naechsten CC-BY-SA-Fund einbauen will, braucht wieder eine
> Entscheidung — die drei Gruende oben gelten fuer **diese** Ernte, nicht
> als neue Hausregel.

### Baurezept `floptool` (MF-1083)

Aufgeschrieben, weil es **nicht** der dokumentierte Weg ist — genau
wie bei libdsk (MF-1028). Der gelieferte Schnappschuss
`neue-ideen/exsource/mame-master.zip` (SHA-256
`e7955339ded6a27357bedd195e8ee27bf8310a9e5f87f9df0ca389c90c0832c8`)
enthaelt **nur** `src/lib/` — 845 Eintraege, 7,4 MB, ohne `tools`,
ohne `osd`, ohne `3rdparty`. `floptool.cpp` selbst fehlt darin.

Gebaut unter `tools/uft-scout/work/mame-master/` (gitignored):

1. Archiv entpacken (`src/lib/formats` 221 `.cpp`, `src/lib/util` 46).
2. Von `mamedev/mame@master` nachgeholt, weil im Archiv nicht
   enthalten: `src/tools/floptool.cpp`, `src/tools/image_handler.{h,cpp}`,
   `src/osd/{osdcomm,osdcore,osdfile,eminline,eigcc*,eivc*,strconv}.h`,
   `src/osd/osdcore.cpp`, `src/osd/strconv.cpp`,
   `src/osd/modules/file/win*.cpp`, `src/osd/modules/lib/osdlib_win32.cpp`,
   `src/osd/windows/winut*.{h,cpp}`; dazu **utf8proc** (MIT) aus
   `JuliaStrings/utf8proc`.
3. Uebersetzt mit `g++ -std=c++20 -DCRLF=3 -DUNICODE -D_UNICODE`
   (MAME-master ist C++20 und erwartet `UNICODE`), gelinkt gegen
   `-lz -lws2_32 -luser32 -lshlwapi`. **267 Objektdateien.**

**Drei Stellen brauchten eine Ergaenzung, und alle drei stehen
ausserhalb der Versionskontrolle:**

* `rotr_32`/`rotl_32`/`rotr_64`/`rotl_64` — das Archiv ist **aelter**
  als master (sein `ap_dsk35.cpp` ruft `rotr_32`, master benutzt
  `std::rotr`), und die von master geholten osd-Koepfe kennen sie nicht
  mehr. Uebernommen wurde **MAMEs eigene Definition** aus `mame0250`,
  woertlich, nicht nachgebaut.
* `src/emu/logmacro.h` — ein dreizeiliger, wirkungsloser Ersatz fuer
  `ti99_dsk.cpp`. Dieselbe Bauform wie der `<binary-io.h>`-Ersatz beim
  Bau von `a2nibblize`.
* `src/osd/uft_nicht_gebaut.cpp` — CHD (braucht libFLAC), ZIP/7z
  (LZMA) und XML (expat) sind ohne `3rdparty` nicht baubar. Sie sind
  **nicht weggelassen**, sondern als Stellen definiert, die **laut
  abbrechen**. Ein floptool, das eine CHD stillschweigend falsch
  behandelt, waere schlimmer als eines, das sagt: dieser Teil ist nicht
  gebaut (UFT-A02).

**Nicht gebaut und benannt:** `flacfile.cpp` (FLAC-Kassetten),
`h17d_dsk.cpp` und `hti_tap.cpp` — die letzten beiden fehlen **im
Archiv selbst**, obwohl `all.cpp` sie nennt. `has_formats.h` wird
deshalb aus den **tatsaechlich uebersetzten** Objektdateien erzeugt
(184 von 186), nicht aus einer Wunschliste.

**Eichung (MF-1083), am Objekt:**

| Datei | floptool sagt |
|---|---|
| `tests/corpus_free/vice_c1541_35trk.d64` | `..++. d64` (Hoechstwertung) |
| `tests/corpus_free/gw_amigados.hfe` | `.+.+. hfe` (Kennung getroffen) |
| `tests/corpus_free/hxcfe_pc160.imd` | `.+.+. imd` |
| `flopconvert d64 g64` | 278 164 Byte, als `g64` wiedererkannt |
| SHA-256 des Binaers | `bbf893dd41a3e58bacb5a0c773b8728cddd2efe5d3e7a668ed532b059fdb1d2a` |
| `adf2dms` | — (Python, im Baum geklont) | **MIT** (LICENSE.txt woertlich: „Copyright (c) 2022 Darsey Litzenberger") | **Quellstand `8adfe6acfdc9f18f3627e04eb1d1f798c112f3da`** + Paketfassung `0.0.1` (`setup.cfg:10`), Pfad `tools/uft-scout/work/adf2dms`; zusaetzlich gemessen: deterministisch (zwei Laeufe, gleiche Ausgabe-SHA-256) | **AmigaDOS-ADF → DMS. Der fehlende ERZEUGER.** P3-347 und P3-373 hatten gemessen, dass KEIN Werkzeug im Baum eine DMS erzeugt: hxcfe, xdms und amigadx lesen nur, der originale Packer laeuft auf dem Amiga und ist proprietaer. Jeder DMS-Test war damit ein geschlossener Kreis (Gestalt `apridisk`/MF-1009, `qrst`/MF-1028). **Seine Schreiber-Linie ist unabhaengig von xDMS** — und aus xDMS stammen sowohl UFTs Leser als auch der von hxcfe; genau deshalb zaehlt er. Die fremde Hand fuer **`dms`**, seit MF-1135 Stufe **T1b**. Belegt sind die Betriebsarten **NOCOMP** (cmode 0) und **RLE** (cmode 1), in allen 80 Spursaetzen am Kopffeld nachgezaehlt; die RLE-Fassung ist 40 376 Byte aus 901 120, also keine Durchreichung. **Nicht belegt:** quick/medium/deep/heavy — vier Betriebsarten ohne fremde Gegenprobe. Zweite Hand ist `hxcfe`, das beide Erzeugnisse zu 0 von 901 120 Byte abweichend zurueckliest |
| `fluxtoimd` | — (Python, im Baum geklont) | GPL-3.0-**only** | Pfad `tools/uft-scout/work/fluxtoimd` | FM- und M2FM-Modulation. Seit MF-864 die zweite Hand fuer die FM-Pruefspur: Adressmarken, `FM.decode()`, CRC-Parameter. **Wird ausgefuehrt, nicht portiert** — GPL-3-only vertraegt sich mit dem Baum, aber der Kanal ist ausdruecklich „Oracle" |
| `lsatr` | `LSATR` | GPL-2.0-or-later | `-v` → „mkatr version 1.4" | Atari-DOS in ATR **und** XFD: Geometrie, DOS-Variante, freie Sektoren; Inhalte je Datei über `-x`/`-X`. Die **unabhängige** Hand gegen den atrcopy-erzeugten Korpus |
| `xdftool` | `XDFTOOL` | GPL-2.0-or-later | **Paketversion** (`importlib.metadata.version("amitools")`) — das Werkzeug gibt selbst keine aus; die SHA-256 der aufgeloesten Datei steht im Manifest daneben | AmigaDOS-Verzeichnis und Dateiinhalte in ADF. Zugleich der **Erzeuger** von `xdftool_dd_ofs.adf` — beantwortet damit die Provenienz-, nicht die Richtigkeitsfrage. Laengensemantik **roh** (127, nicht 488), Unabhaengigkeit gegen `adfrescue` gemessen (MF-693) |
| `libdsk` | — (im Baum geklont und dort gebaut) | LGPL-2+ (John Elliott) | Pfad `tools/uft-scout/work/libdsk` + **Fassung 1.5.12** (`dsktrans -version`, berichtigt MF-1032 — hier stand „das Werkzeug hat keine Versionsabfrage“, und das trifft nicht: `dsktrans -version` antwortet „libdsk version 1.5.12“, und `config.h` fuehrt `PACKAGE_VERSION "1.5.12"`) plus **SHA-256** der gebauten Werkzeuge (`dsktrans.exe` d0d826c2fc212d26725192418f53da1bec50a758d79f65f0500a7f086e3136df, `dskid.exe` 14abc39bc4ffebf4b2d8690d2e3be9a17955de0ef498337b367a2e77afd56e29) | **Fünfundzwanzig** Container-Formate, darunter `qrst`, `myz80`, `nanowasp`, `logical`, `rcpmfs`, `cfi`, `jv3`, `sap`, `imd`, `apridisk`, `dc22`/`dc42`, `dsk`/`edsk`, `copyqm`, `tele`, `ydsk`, `simh`, `ldbs` — und, was hier den Unterschied macht, **Formatbeschreibungen** in `doc/` (`qrst.html`, `cfi.html`, `apridisk.html`, `libdsk.txt` mit 121 KB). MF-1028 hat `qrst` daraus gehoben. Registriert mit dem Vorbehalt aus dem Abschnitt darunter: **der Bau geht nur an den Autotools vorbei** |
| `to_woz2` | `TO_WOZ2` | GPL-3.0 (Zone GELB) | **Quellstand + Baurezept + Ausgabe-SHA** — nicht der Binaerhash (siehe unten) | Apple-II-Sektorabbild → WOZ 2.0 mit **synthetisiertem** GCR-Strom (6-and-2 / 5-and-3). Die fremde Hand fuer `do`, `po`, `d13` — Stufe **T1b** (Fremdwerkzeug-Abbild), nicht T2 |
| `a2nibblize` | — (im Baum geklont, dort gebaut) | GPL-3.0 (Zone GELB) | **Quellstand + Baurezept + Ausgabe-SHA**, wie bei `to_woz2` und aus demselben Grund: Quellstand `639dc1c3281f`, Rezept unten, Ausgabe-SHA-256 `c09155a2662fe3bb85afc49f65223381df628b47319d111af9a0bacc919b372e` für die benannte Eingabe `tests/corpus_free/uftk_dos33_35trk.do` | **Apple-II-Sektorabbild (`.do`) → NIB.** Seine eigene Hilfe: „*Converts an Apple ][ floppy disk image from .do format to .nib nibble format*". Die fremde Hand für **`nib`** — seit MF-1050 Stufe **T1b** (Fremdwerkzeug-Abbild). **Es lag im selben Klon wie `to_woz2`, seit derselben Sichtung, und wurde nur nie bemerkt:** der Eintrag zu nibtools weiter unten sagt völlig richtig, über `nibconv` führe kein Weg zu T1b — das ist **Commodore**-NIB. Die Frage, ob das **Apple**-Paket einen Erzeuger enthält, hat niemand gestellt. **Lehre: ein geklontes Paket ist mehr als das eine Werkzeug, wegen dessen man es geklont hat.** Und die Spurlänge ist dort nicht behauptet, sondern **gerechnet** — `a2nibblize.c:78` baut 6656 aus der Feldanordnung auf (6 + 3 + 8 + 3 + 3 + 343 + 3 + 27 = 396 je Sektor, ×16, + 0x30 + 0x110), also genau die Konstante, die `uft_nib.c` führt |
| `atrip` | — (im Baum geklont, als Bibliothek benutzt) | GPL-2 (Zone GELB, mit UFTs GPL-2-or-later vertraeglich — hier trotzdem **nur ausgefuehrt**, keine Zeile uebernommen) | **Quellstand + Baurezept + Ausgabe-SHA.** Klon `tools/uft-scout/work/atrip/`, Ausgabe-SHA-256 `6f25703ef425943918261d6c7b6a8ff4a4fca16c98cdc829c29c6dbcd237ba35` (SD) und `f9d3a5a3f172c79772fb318481aa6d2ca06d949b951b7717b59e8f856f94152d` (DD) fuer die im Rezept benannten Eingaben | **Atari-Sektorabbild → DCM.** `atrip/compressors/dcm.py` ist der einzige vollstaendige DCM-Kodec, der diesem Baum offensteht; sein **Kodierer** `calc_packed_data` hat die beiden Pruefdateien erzeugt, und sein Dekoder liest die drei echten, historischen DCMs aus `atrip/samples/`. Die fremde Hand fuer **`dcm`** — seit MF-1053 Stufe **T1b**. **Und er ist ein Orakel mit gemessenen Grenzen:** fuer Doppeldichte ist sein Pfad defekt (Rezept unten), `atrcopy/dcm.py` ist trotz gleichen Dateinamens **kein** Dekoder (MF-1052), und seine flache Sektoranordnung steht im Widerspruch zu seiner eigenen ATR-Konvention — entschieden wurde das nicht durch das Orakel, sondern am Objekt (gueltige VTOC und ein lesbarer Dateiname bei Sektor 360/361) |
| `fluxfox` | — (im Baum geklont, **nicht** gebaut) | **MIT** (Daniel Balsom, 2024) | Quellstand `1d72ff1b329c` (2026-08-11), Pfad `tools/uft-scout/work/fluxfox` + **SHA-256 je benutzter Datei** im Manifest | **Datenquelle, kein ausgefuehrtes Werkzeug** — und das ist der Unterschied zu jedem anderen Eintrag dieser Tafel. fluxfox traegt in `tests/images/sector_test/` **dieselbe 360K-Diskette in vierzehn Formaten** (`.86f`, `.hfe`, `.imd`, `.img`, `.imz`, `.mfi`, `.mfm`, `.pfi`, `.pri`, `.psi`, `.scp`, `.tc`, `.td0` + KryoFlux-ZIP). Drei davon liegen seit MF-1071 im Korpus und dienen einander als Gegenprobe: die `.img` ist linear durchnummeriert und damit die Bruecke, gegen die `.86f` und `.pri` gehalten werden. Die fremde Hand fuer **`86f`** und **`pri`** — beide seit MF-1071 auf **T1b**. **Zwei Grenzen gehoeren dazu:** fuer `86f` bleibt der Vorbehalt aus MF-961 in praeziserer Form bestehen — die Datei ist belegt von fluxfox, nicht vom kanonischen 86Box; und die zweite Sammlung desselben Klons, `tests/images/transylvania/`, ist **nicht** benutzbar: ihre `LICENSE.txt` erlaubt das Kopieren nur, solange „you do not charge any money“ — unvereinbar mit GPL-2-or-later |

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

### atrip — ein Kodierer, drei Umgehungen und zwei eigene Fehler (MF-1053)

`atrip` (Rob McMullen, GPL-2) liegt als Klon unter
`tools/uft-scout/work/atrip/`. Benutzt wird **nur** die Klasse
`atrip.compressors.dcm.DCMCompressor`, und zwar ausgefuehrt — UFTs
Entpacker ist aus dem beobachteten Verhalten eigenstaendig geschrieben
(Muster MF-614).

Drei Handgriffe sind noetig, und **keiner davon gehoert in den Klon**;
sie leben im eigenen Skript:

1. **Ein `pkg_resources`-Ersatz.** `atrip/container.py:4` importiert es,
   um ueber `iter_entry_points` installierte Plugins zu finden. setuptools
   fehlt in dieser Umgebung, und wer die Klasse direkt benutzt, braucht
   die Plugin-Suche nicht:

```python
# pkg_resources.py — nur fuer den Oracle-Lauf, im Scratchpad
def iter_entry_points(group, name=None):
    return iter(())

class DistributionNotFound(Exception):
    pass

def get_distribution(name):
    raise DistributionNotFound(name)
```

2. **Ein `media`-Huellenobjekt.** Der Packer fragt genau vier Dinge ab:

```python
class Medium:
    def __init__(self, daten, sektoren, sgr):
        self.daten = np.frombuffer(daten, dtype=np.uint8)
        self.sector_size = sgr
        self.num_sectors = sektoren
    def get_index_of_sector(self, n):
        return ((n - 1) * self.sector_size, self.sector_size)
    def __getitem__(self, s):
        return self.daten[s]
```

3. **Zwei numpy-Ersetzungen fuer den DEKODER** (nur fuer die Gegenprobe;
   der Kodierer braucht sie nicht). Unter numpy >= 2 rechnen drei Stellen
   in `uint8` und laufen ueber — `get_current_sector` (`hi * 256`,
   `dcm.py:93`) sowie die Indexzaehler in `decode_41`/`decode_44`, die
   bei 256-Byte-Sektoren die 256 nie erreichen. Dieselbe Rechnung in
   Python-Ganzzahlen behebt es.

**Der Lauf:**

```python
c = DCMCompressor()
dcm = bytes(c.calc_packed_data(np.frombuffer(roh, dtype=np.uint8),
                               Medium(roh, 720, 128)))
```

Die **Eingaben** sind UFT-eigen, selbstbenennend und rechtefrei; sie
werden im Test aus derselben Regel neu gerechnet statt als Blob
mitgefuehrt:

* **SD**, 720 x 128: sechs Zonen zu je 120 Sektoren, eine je Blocktyp —
  Kopf `"UFT-K Snnn "` + Lauf `0xAA` · alle gleich (`"UFT-K ZONE1 "` +
  `0x5A`) · 128 x `0x33` mit vier wechselnden Bytes bei 124..127 ·
  `0xC3` mit Kopf am Anfang · `0x7E` mit Kopf am Ende · alles
  verschieden. Gemessen loest das **alle sechs** Blocktypen aus:
  `0x41` x238, `0x42` x1, `0x43` x4, `0x44` x357, `0x46` x119,
  `0x47` x1.
* **DD**, 720 x 256: Sektor 1..360 ein Fuellbyte `(s*11+37) & 0xFF` bis
  zum Sektorende mit Kopf davor, Sektor 361..720 die feste Grundlage
  `(i*37+91) & 0xFF` mit Kopf am Ende. Gemessen `0x43` x360,
  `0x44` x359, `0x47` x1.

**Warum die DD-Datei nicht ebenfalls alle sechs traegt — gemessen, nicht
vermutet.** atrips Doppeldichte-Pfad ist an drei Stellen defekt:

* sein `encode_43` legt `rle_start = 256` in einen `uint8`-Puffer und
  bricht mit `OverflowError` ab (`dcm.py:196`), sobald ein **woertlicher**
  Lauf bis zum Sektorende reicht;
* sein Packer waehlt fuer 256-Byte-Sektoren `0x42`, das nur die Bytes
  0..127 festlegt — sein **eigener** Dekoder liest das dann falsch, der
  Rundlauf ist nicht mehr identisch;
* `decode_41`/`decode_44` zaehlen den Index als `uint8`.

Eine Pruefdatei, die um einen Orakel-Defekt herumgebaut ist, waere keine.
Deshalb deckt die DD-Datei die **Anordnung** und die **Dichte** ab, nicht
die Befehlsvielfalt — und die eine Regel, die dabei ungeprueft bliebe
(`ende == 0` heisst 256 in der **woertlichen** Phase), ist an den **drei
echten** DCM-Dateien nachgemessen: mit entfernter Regel antwortet `open`
dort `SCHEITERT`.

**Die staerkste Abnahme liegt ausserhalb des Korpus.** `atrip/samples/`
enthaelt drei echte, historische DCM-Dateien (`mydos_dd_bm301318.dcm`,
`mydos_dd_bm301419.dcm`, `mydos_sd_mydos4534.dcm`), die niemand in
diesem Baum geschrieben hat. UFTs Entpacker und atrips Dekoder liefern
fuer alle drei **720 von 720 Sektoren byteidentisch**. Sie koennen nicht
in den Korpus — ihre Weitergabe ist nicht geklaert —, deshalb steht
das Ergebnis im Commit-Text und nicht in einer Zusage.


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

#### Gebaut MF-1124 — das Rezept, weil es nicht das dokumentierte ist

Auf Eigentümer-Anweisung („fdc_bitstream bauen … eine offene Schuld
seit MF-626"). Klon unter `tools/uft-scout/work/fdc_bitstream`
(`--depth 1`, 2026-09-14).

**Lizenz: MIT** (`LICENSE.md`, Copyright 2022 Yasunori Shimura).
`tools/uft-scout/data/auftraege.json` nannte keine — damit ist auch der
**Port**-Kanal offen, nicht nur Oracle. Es bleibt trotzdem extern, aus
den drei Gründen oben.

**Der Bau bricht mit MinGW/GCC 13.1.0 mit 93 Fehlern aus EINER
Ursache.** Sechs Header benutzen `uint8_t` ohne `#include <cstdint>`:
`bit_array.h`, `image_fdx.h`, `image_hfe.h`, `image_mfm.h`,
`image_rdd.h`, `mfm_codec.h`. MSVC zieht `<cstdint>` transitiv herein,
libstdc++ seit GCC 13 nicht. Sechs eingefügte Zeilen **im Klon** →
0 Fehler. Das ist ein Befund **über** das Werkzeug, keine Übernahme aus
ihm; nichts davon kommt in den Baum.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build          # -> bin/: analyzer, create_mfm_image,
                             #    fdc_test, image_converter,
                             #    pauline2raw, testCompare,
                             #    testFormat, testRaw2d77
```

**Was es kann, gemessen:**

* `create_mfm_image` erzeugt ein **8 410 112 Byte** großes
  MFM-Bitstromabbild (84 Spuren) — ein **fremder Erzeuger auf
  Bitstromebene**, und die hat dieser Baum sonst nicht.
* `image_converter -i … -o …` liest `mfm, raw, d77, hfe, fdx` und
  schreibt zusätzlich `rdd`, mit **sieben wählbaren VFO-Varianten**
  (`-vfo 0…9`) und `-gain low high`. **Zwei Eingabeformate liest UFT
  auch:** `hfe` (T1) und `d77` (T1b).
* Ausgeführt belegt: `image_converter -i
  tests/corpus_free/gw_amigados.hfe -o x.raw` — 2 049 024 Byte hinein,
  **8 533 880** heraus.

**Was es NICHT kann, und das widerlegt eine Annahme aus P3-389:**
**kein FM.** Klammerfest über den ganzen Baum gemessen
(`[^m]fm_(encode|decode|codec|write|read)`, `single_density`,
`FM_MODE`, `is_fm`): **null Treffer**; `mfm_codec.h` führt nur
`mfm_encoder`/`mfm_write_byte`/`mfm_read_byte`. Die Vermutung, eine
MB8877/µPD765-Emulation decke „FM und MFM", stammte von mir und war
falsch — frühere `\bfm\b`-Treffer kamen allein daher, dass „mfm" die
Zeichen „fm" enthält. **Der FM-Encoder-Blocker aus P3-218/MF-864 bleibt
offen.**

**Auflage — und sie reicht weiter als im Register vermerkt.** Dort
stand, `test_data/*.raw` seien kein KryoFlux-Strom. Das trifft auch auf
das **Ausgabeformat** zu: an der eigenen Ausgabe gemessen beginnt sie
mit `**BIT_RATE 506000`, `**TRACK_RANGE 0 159`, `**MEDIA_TYPE 2D`. Wer
eine solche `.raw` UFTs `kfx`-Leser vorwirft, liest falsch. Der Kopf ist
dafür selbstbeschreibend und taugt als Vergleichsziel.

**Noch nicht registriert, und der Grund ist benannt:** ein Oracle
braucht einen Test, der es befragt. Der schärfere Vergleich — Bitstrom
→ **Sektoren** über `d77` — geht am einzigen **freien** HFE nicht, weil
`gw_amigados.hfe` **Amiga**-MFM ist und `d77` IBM-Sektorstruktur
erwartet. Der reine Bitstromvergleich braucht es nicht und ist der
erste Schritt.

**BERICHTIGT MF-1125 — hier stand „`tests/corpus/` ist auf dieser
Maschine leer", und das war zweifach falsch.** Gemessen hat das
Verzeichnis **25** Einträge, und darunter liegt genau die Datei, die
der Satz daneben als fehlend erklärte:
`tests/corpus/kor_c/OUT-THINK - KAMASOFT - OUT-THINK FOR CPM.hfe`,
1 004 544 Byte, `track_encoding = 0x00` = **ISOIBM_MFM** — ein
IBM-MFM-HFE, 40 Spuren × 2 Seiten, Kaypro DSDD 10×512. Und daneben
liegt dieselbe Diskette als **IMD mit 800 Sektoren**, also eine
unabhängige Wahrheit auf Sektorebene.

Damit ist der schärfere Weg **nicht** von einer Beschaffung abhängig,
sondern nur noch Arbeit: `image_converter -i <kor_c.hfe> -o x.d77`,
dann die Sektoren gegen die IMD halten. Das ist der Vergleich, den
P3-389 Punkt (2) verlangt, und er ist gegen die Tail-Defekte des
HFE-Lesers robust, weil Sektordaten in den vollen Blöcken liegen und
nicht im Polster. Nicht gemacht — benannt, damit „später" nicht „nie"
heißt. **Der Satz war die Klasse „Aufzählung statt Messung": ich hatte
nicht nachgesehen.**

**NACHTRAG MF-1125 — der erste Schritt ist gegangen, und er schliesst
den HFE-Weg AUS: sein HFE-Leser ist gemessen falsch.** Nicht als
Oracle für HFE registrieren. Zwei Befunde in
`disk_image/image_hfe.cpp`:

1. **Er liest hinter seinen eigenen Puffer.** Die Schleife lautet
   `for (blk_id = 0; blk_id <= num_blocks; blk_id++)` — eine Runde zu
   viel; `buf` ist `num_blocks * 0x200` Byte gross, und der letzte
   Durchgang greift bei `buf.data() + num_blocks * 0x200` zu.
2. **Er nimmt dem letzten Block volle 256 Byte je Seite** (Z. 86,
   `size = (blk_id * hfe_blk_size < track_len) ? hfe_blk_size / 2 :
   fraction / 2`) und liest damit das Polster als Fluss.

Aufgerechnet an Spur 0/0 von `gw_amigados.hfe`, restlos: UFT nach
MF-1125 **50 526** Übergänge, fdc_bitstream meldet **50 855**,
dazwischen **264** aus 132 Byte Polster `0x88` und **65** aus
124 Byte hinter dem Puffer. 50 526 + 264 + 65 = 50 855.

**Warum das hier steht und nicht nur in P3-389:** ein Test, der 50 855
festgenagelt hätte, hätte einen Defekt festgenagelt — und wer beim
Auseinanderlaufen „nach oben" korrigiert, baut den Pufferüberlauf des
Orakels in den eigenen Leser ein. Für den Spurschluss einer HFE gibt es
im Feld **keinen** brauchbaren fremden Zeugen; vier Umsetzungen geben
vier Antworten (P3-391). Als Bitstrom-Zeuge bleibt fdc_bitstream
brauchbar — aber über `.mfm` aus seinem eigenen `create_mfm_image`,
nicht über HFE, und dafür fehlt UFT das `.mfm`-Plugin (P3-349).

### Laufwerks-Firmware — eine Quellengattung, kein Oracle (MF-1179)

Fast alles in diesem Verzeichnis ist ein **Leser**: ein Werkzeug, das ein
fremdes Abbild aufmacht und deutet. Eine Laufwerks-Firmware ist der
**Schreiber** — sie hat die Disketten hergestellt, um die es geht. Das
macht sie zur staerksten Spec-Quelle, die es fuer ein Format geben kann,
und zugleich **ausdruecklich zu keinem Oracle**: sie wird nicht
ausgefuehrt, sie wird gelesen. Kanal *Spec* (MF-695).

Registriert ist bisher **eine**:

| Firmware | Stand | Lizenz | Verankerung | was sie entscheidet |
|---|---|---|---|---|
| **Atari 1050 Turbo v3.5** | Atasm-Quelltext `SOFTTRB35`, ausgeliefertes ROM `T1050_2B.8KB` (1988) | (c) 1986-88 Bernhard Engl, Atasm-Fassung (c) 2004 Matthias Reichl — **keine Rechteeinraeumung** | **Gegen ihr eigenes Erzeugnis geprueft:** das aus dem Quelltext gebaute `turbo1050-35.rom` und das ausgelieferte ROM haben denselben md5 `35be2c58f1e0b04ab5a1f2459e5515bd`, **0 abweichende Byte** (MF-1164) | Sektoren/Spur je Dichte · Interleave je Dichte UND SIO-Geschwindigkeit · die drei Zwischenraum-Paare je Dichte · das FM-Fuellbyte `$00` · die Feldlage des 12-Byte-Percom-Blocks · die Kommandokodes `$4E`/`$4F` · das Fehlen eines Index-Address-Marks |

**Die Verankerung ist der Punkt.** Eine Spec-Quelle ohne Anker ist eine
Behauptung; hier ist der Anker ein Byte-fuer-Byte-Vergleich zwischen dem
Quelltext und dem Chip, der 1988 ausgeliefert wurde. Damit stammen die
Konstanten vom Schreiber der Disketten und nicht von einem Deuter — und
das ist eine andere Aussage als „ein zweites Werkzeug liest es auch so".

**Was daraus NICHT folgt.** Diese Firmware ist ein **Nachruest-ROM**,
nicht das Original-ROM der Atari 1050. Wo sie von einer anderen Quelle
abweicht, ist damit nicht entschieden, was auf einer Diskette von Atari
steht — die Abweichung wird **festgenagelt, nicht aufgeloest** (P3-434).
Eine Quellengattung sagt, wie stark eine Aussage ist, nicht dass sie die
einzige waere.

**Und der Assembler bleibt draussen.** Uebernommen sind Zahlen mit
Fundstelle — `tests/test_1050_firmware_als_quelle.c` laeuft ohne das
Archiv —, nicht Zeilen. Das Archiv wird nicht mitgeliefert. „ED hat 26
Sektoren" ist eine Tatsache ueber ein Format; der 6502-Code dahinter ist
fremdes Eigentum.

**Naechste Kandidaten derselben Gattung**, jeder mit demselben
Ankerbedarf: das Original-ROM der Atari 1050, das XF551-ROM, die
Percom-RFD-Firmware. Gepruefter und **abgelehnter** Kandidat: `FLOFOR`
(Peter Putnik 2006, Freeware mit Quelltext `FO98.S`) — es ist ein
Formatierer fuer den **Atari ST**, also 512-Byte-MFM auf einer anderen
Maschine, und traegt zur Atari-8-Bit-Geometrie nichts bei; es stand in
einem Auftrag neben der 1050-Firmware und gehoert gemessen in eine
andere Familie. Seine Weitergabebedingung („must be in this ZIP archive,
with all files included") verbietet ohnehin jede Teiluebernahme.

## Was ausdrücklich **kein** Oracle ist

| Werkzeug | Grund |
|---|---|
| `unadf`, AdfOpus | teilen sich **ADFlib** — untereinander eine Hand, also als *Paar* keine Zweitmeinung. **Nicht** dieselbe Hand wie unser Korpus-Abbild; das stand hier bis MF-682 und war gemessen falsch |
| `amigadx` | vendorte ADFlib 0.7.10 **und** Total-Commander-Plugin ohne Konsolen-Einstieg |
| `ADFDiskBox` | ruft nur `cmd.exe /C gw …`; **keine** eigene ADF-Ebene (0 Treffer auf `ReadAllBytes\|FileStream\|adflib\|RootBlock`) |
| `FloppyControl` selbst | WinForms-GUI, nicht skriptbar. Nur sein `dskx` ist ein Konsolenprogramm |
| `WinUAE` | ADFlib-unabhängig und damit inhaltlich interessant, aber GUI **und** ohne Lizenzdatei im Repo — Referenz ja, Oracle nein |
| `atrip` | **dieselbe Hand wie der Korpus.** `README.rst:6` nennt es wörtlich „The successor to atrcopy", gleicher Autor — fünfte Registrierungsfrage, ausgeschlossen. Unabhängig davon auf dieser Maschine nicht lauffähig: `pkg_resources` fehlt unter Python 3.13, und `np.fromstring` steht neunmal im Code (tot unter NumPy 2.5.1) **NACHTRAG MF-1053 — beide Haelften dieser Absage sind nachgemessen, und eine faellt.** *Nicht lauffaehig* trifft nicht mehr zu: `pkg_resources` braucht einen **dreizeiligen** Ersatz (leeres `iter_entry_points`), und `np.fromstring` liegt auf einem Pfad, den der DCM-Kodierer nie betritt — er laeuft. *Dieselbe Hand wie der Korpus* dagegen **stimmt und bleibt stehen**: fuer `atr`/`xfd` stammen die Korpus-Abbilder von `atrcopy`, und atrip ist dessen Nachfolger vom selben Autor; als Zweitmeinung ueber diese Formate ist es weiterhin ausgeschlossen. Fuer **`dcm`** greift der Einwand nicht: dort gibt es kein atrcopy-Abbild und kann keines geben — `atrcopy/dcm.py` ist **kein** Dekoder und erst recht kein Packer (MF-1052). Verglichen wird UFTs eigenstaendig geschriebener Entpacker gegen ein Abbild fremder Hand; das ist genau, was T1b verlangt. Registriert ist atrip deshalb **nur fuer DCM** — siehe Haupttafel oben. |

| Atari-1050-Turbo-Firmware | **kein Oracle, weil nicht ausgefuehrt — und das ist keine Schwaeche, sondern die Gattung (MF-1179).** Sie ist eine *Spec*-Quelle, verankert an ihrem eigenen ausgelieferten ROM (md5-identisch, 0 abweichende Byte). Ein Oracle waere sie erst in einem Emulator, der eine Diskette wirklich formatiert; dann waere zu belegen, dass der Emulator die FDC-Zeitlagen trifft — und genau das ist die Wette, die dieser Baum bei `86f` abgelehnt hat (MF-961). Siehe den Abschnitt „Laufwerks-Firmware — eine Quellengattung" oben |
| `drimg` 0.86 | **kein Oracle, weil es sich hier nicht bauen und auch gebaut nicht fahren lässt (MF-1180).** P. Putnik 2006, GPL-2.0-or-later. `configure.in` fordert KDE 3 + Qt 3 (`KDE_USE_QT(3.2.0)`); auf dieser Maschine liegt nur Qt 6.10.2, und `moc-qt3`/`kde-config` gibt es nicht. **Entscheidend ist aber der zweite Grund, denn er gilt auch auf einer KDE-3-Maschine:** es hat keinen nicht-grafischen Einstieg — `main.cpp` zeigt das Fenster und ruft `app.exec()`, die einzige Kommandozeilenoption ist auskommentiert, und die geparsten Argumente werden verworfen (`args->clear()`, daneben ein `/// @todo`). Ein Oracle muss sich skripten lassen; dieses Werkzeug wartet auf einen Mausklick. Dazu gibt es keine Beschreibung: der docbook ist die unbearbeitete KDE-Vorlage. Vollständige Messung samt Portabsage: **P3-436** |
| Daniel B. Sedory, „MSWIN4.1 FD Boot Record" | **kein Oracle, sondern eine *Spec*-Quelle — und eine mit einer ausdrücklichen Weitergabeschranke (MF-1181).** `https://daniel.sedory.com/asm/mbr/WIN98FDB.htm`, Revised 29.04.2003, Update 27.04.2005, Updated 05.04.2009. „Web Presentation and Text are Copyright © 2001-2005 by Daniel B. Sedory (**NOT to be reproduced in any form without Permission of the Author!**)" — also GELESEN, Bytelagen zitiert, **kein Textabsatz und kein Bootcode übernommen**; dieselbe Behandlung wie die 1050-Turbo-Firmware. Ein Oracle wäre sie ohnehin nicht: sie ist eine Beschreibung, kein ausführbares Werkzeug. **Was sie entscheidet:** die Bytelagen des MSWIN4.1-Bootrecords (OEM bei 0x003, Code 0x03E..0x17E, Unterprogramm 0x1F1..0x1FB, `IO      SYS` bei 0x1D8, `MSDOS   SYS` bei 0x1E3), die BPB-Werte einer 1440-K-Startdiskette, und — unabhängig nachgerechnet — die **2847** nutzbaren Sektoren, die `fat_analyze_boot_sector()` ebenfalls liefert. **Und sie legt den Geltungsbereich fest:** derselbe Bootcode liegt auf Windows 98, 98 SE, ME **und** den XP-Startdisketten — „MSWIN4.1" ist keine Aussage über die Windows-Fassung. Umsetzung: `src/formats/fat/uft_win98_fdb.c`, abgenommen von `tests/test_win98_fdb.c` (6 Zusagen). Offene Punkte aus derselben Quelle: **P3-438** |
| `atr2imd` / `imd2atr` aus `jhallen/atari-tools` | **Gebaut, gelaufen und trotzdem verworfen — weil sein Zwischenformat nur es selbst lesen kann (MF-1178).** Commit `835d5a6fc1258921949fe92400adb789c398b9c3` (2021-10-22), GPL v1 oder später, © 2011 Joseph H. Allen; beide Werkzeuge übersetzen mit einem einzigen `gcc -O2` und liefen auf dieser Maschine. Zwei unabhängige Abweichungen vom Format, das sie zu schreiben behaupten, **keine davon dokumentiert**: **(1)** der Kopf trägt keine `IMD `-Kennung, sondern `ATR2IMD 1.0: <Datum>` — eine echte IMD im Korpus beginnt mit `IMD 1.17: …`, und UFTs Leser verlangt die Kennung (`uft_imd_plugin.c:14,34,89`), antwortete auf die erzeugte Datei also gemessen mit `UFT_ERROR_FORMAT_INVALID`. **(2)** jedes Datenbyte ist **komplementiert** (`atr2imd.c:284,290,297` schreibt `~atr->data[…]`), und nur der eigene Partner dreht es zurück (`imd2atr.c:311`, `buf[y] ^= 0xFF`). Gemessen: der Rundlauf des Paars ist byteidentisch — **0 von 92 176** Byte abweichend —, die Zwischendatei aber für jeden anderen IMD-Leser unbrauchbar; aus `UFT-ATR S0001` wird `AA B9 AB D2 …`. Dieselbe Entscheidung wie bei floptools `esq16` (MF-1085, P3-364), nur aus einem anderen Grund: dort verschob der eigene Rundlauf die Diskette, hier ist er in Ordnung und die **Zwischendatei** ist es nicht. **Was das Paket trotzdem beiträgt, über den Kanal *Spec*:** seine `readme.md` nennt die drei ATR-Größen **mit Begründung** (92 176 / 133 136 / 183 952, letzteres „− 384 because first three sectors are short") und zwei Erkennungsschwellen (131 088, 183 952) — festgenagelt in `tests/test_atr_groessen_gegen_jhallen.c`, alle drei von UFT richtig gelesen. Dazu drei **Interleave-Tafeln** (`atr2imd.c:56-65`), eine Aussage, die UFT sonst nirgends hat: P3-432 |

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
