# Erzeuger-Zensus — wer SCHREIBT welches Format

> **Erzeugt von `scripts/gen_erzeuger_zensus.py`. Nicht von Hand pflegen.**


Die Stufe **T1b** hat genau eine mechanische Bedingung: ein Manifest-Eintrag mit `origin: "cross-tool"`, dessen `test` in der Testliste des Plugins steht. Daraus folgt die einzige Frage, die eine Hebung entscheidet — **gibt es ein fremdes Werkzeug, das dieses Format SCHREIBT?**


**Die `R/W`-Spalte allein genuegt dafuer nicht.** Sie ist eine Zusage des Werkzeugs ueber sich selbst und luegt in beide Richtungen; gemessen:


```
TI994A_V9T9;RW   aus RAW  ->  184 320 Byte, 100 % 0xF6   (MF-1021)
TI994A_V9T9;RW   aus IMD  ->  720/720 Sektoren richtig   (MF-1060)
APPLE2_DO;RW     aus IMD  ->  0 Byte                     (MF-1061)
```

Die Spalte **Kanal** wird deshalb AUSGEFUEHRT, nicht gelesen. Sie steht in `docs/erzeuger_kanaele.json`; ein Format, das dort fehlt, ist **nicht gemessen** — nicht unmoeglich.


## Stand


| | |
|---|---|
| Plugins gesamt | 89 |
| davon auf T2/T3 (offen) | 9 |
| davon mit **gemessenem** Erzeuger-Kanal | **0** |
| davon mit Werkzeug-Zusage, Kanal ungemessen | 0 |
| hxcfe-Module mit `RW` | 38 |
| libdsk-Typen (alle les- und schreibbar) | 26 |
| floptool-Module gesamt | 151 |
| davon schreibfaehig (`rw`/`-w`) | 122 |

## Die offenen Formate


| Format | Stufe | hxcfe (RW) | libdsk | floptool (w) | Kanal | Klasse |
|---|---|---|---|---|---|---|
| `a2r` | T2 | — | — | — | nicht gemessen | — (hat bereits ein Fremdabbild) |
| `akai_s900` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `dim` | T2 | ATARIST_DIM (?) | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `korg_dss1` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `lisa_twiggy` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `nfd` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `pro` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `syn` | T3 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `udi` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |

`(?)` hinter einem Werkzeugnamen heisst: die Zuordnung laeuft ueber eine Endung, die sich **mehrere** Plugins teilen. `.dsk` tragen `apridisk`, `cpm`, `do`, `jv1` und `tan` gemeinsam, und `AMSTRADCPC_DSK` schreibt keines davon. Ein solcher Treffer ist ein Verdacht, kein Kandidat.



## Was bei jedem Lauf herauskam


MF-1082: dieser Abschnitt hat gefehlt. Das Feld `docs/erzeuger_kanaele.json` wurde eingelesen, in die Zeile gelegt und **nie ausgegeben** — siebzehn Formate trugen dort eine ausfuehrliche Messung, die niemand zu sehen bekam, in einer Datei, deren Kopf sagt, sie halte die Laeufe fest. Dieselbe Klasse wie ein Leser ohne Tuer (MF-930), nur an den Messdaten.


**`adf_ext`** (Kanal: keiner)

MF-1097, gemessen an LAEUFEN statt an der Modulliste. hxcfe fuehrt ZWEI Lader fuer die erweiterte ADF, und beide sagen beim Ausfuehren woertlich ab: `No export support in AMIGA_EXTADF!` bzw. `... in AMIGA_OLDEXTADF!`. Die Eingabe wird dabei richtig geladen und kodiert (880 kB, 80 Spuren, 2 Seiten, 11 Sektoren) — es fehlt allein der Schreiber. floptools `adf` ist das FLACHE Amiga-ADF: MAMEs `adf_format::identify()` nimmt ausschliesslich 901 120 / 912 384 / 1 802 240 Byte und kennt die Kennung `UAE-1ADF` nicht (0 Treffer in `ami_dsk.cpp`). libdsk fuehrt kein ADF. Der kanonische Erzeuger ist WinUAE selbst; im Baum liegt davon nur `FloppyControl/Docs/disk.cpp` — Dokumentation, Kanal *Spec*, und genau die Quelle, die UFTs Leser nennt.

**`akai_s900`** (Kanal: keiner)

MF-1061, gemessen und VERWORFEN. hxcfe zerlegt ein flaches 819 200-Byte-Abbild mit dieser Anordnung richtig — 800 von 800 Sektoren tragen ihren eigenen Namen, und die Gegenprobe mit ENSONIQ_DD_800KB (gleiche Groesse, 10 x 512 statt 5 x 1024) trifft nur 320 von 1600. Die ZERLEGUNG ist also eine echte zweite Hand auf die Geometrie. Aber das Zurueckschreiben ins flache Format ist LAYOUT-UNABHAENGIG byteidentisch — auch ueber die Ensoniq-Anordnung kommt dieselbe Datei heraus. Ein so erzeugtes Fixture waere eine Tautologie, kein Beleg. **NACHTRAG MF-1149, und er schliesst die Frage kategorisch statt nur fuer diesen Aufruf:** der ZELLSTROM-Umweg, der `opus` (MF-1084), `d13` (MF-1085) und `edk` (MF-1103) gehoben hat, traegt hier nachweislich nicht. Gemessen mit einer selbstbenennenden 819 200-Byte-Eingabe: `-uselayout:AKAIS950_DD_800KB -conv:HXC_HFE` ergibt einen 2 008 064 Byte grossen MFM-Zellstrom (Schnittstellenmodus S950_DD_FLOPPYMODE, Sektoren 1..5 zu 1024 B), `-uselayout:ENSONIQ_DD_800KB` einen ebenso grossen mit Sektoren 0..9 zu 512 B - die beiden Stroeme unterscheiden sich in **1 790 325 von 2 008 064 Byte**. Zurueck ueber `RAW_LOADER` liefern BEIDE dieselbe Datei: **0** abweichende Byte gegen die Eingabe, identische SHA-256. Das fremde Werkzeug modelliert die Geometrie also wirklich - aber die Auskunft steht im ZWISCHENSTUECK und kann das ZIEL nicht erreichen. **Ein flaches Abbild mit gleichfoermiger Sektorgroesse und linearer Abbildung IST die Bytefolge und hat keinen Platz fuer die Aussage:** jedes Tripel (Zylinder, Koepfe, Sektoren), dessen Produkt mit der Sektorgroesse die Dateigroesse trifft, liest dieselben Bytes in derselben Reihenfolge und beschriftet sie nur anders. Pruefung (3) aus `_pruefung` faellt damit nicht am Werkzeug, sondern am FORMAT; kein Erzeuger kann das aendern. **Nicht in dieser Klasse sind zonierte oder nichtlineare flache Abbilder** - bei `victor9k` (zwei Zonentafeln), `v9t9` (Kopf 1 rueckwaerts) und `nanowasp` (Skew) zeigt die Bytelage die Anordnung, und genau darum trugen die ihre Hebung. Ob eine Stufe hier trotzdem ruhen darf (Gestalt `tan`/P3-365), ist als **P3-403** eine Eigentuemer-Entscheidung.

**`apridisk`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`cas`** (Kanal: keiner)

MF-1097, misst nach, was MF-1040 benannt hatte. Gemessen im Lauf: `Error: Format 'cas' unknown`. Der Grund ist kategorial: CAS ist ein KASSETTEN-Strom, floptool und hxcfe sind Disketten-Werkzeuge. MAMEs `fmsx_cas.cpp` wandelt in WAV-Abtastwerte und gibt keine Bloecke zurueck — es ist eine Spec-Quelle, kein Erzeuger. Die fuenf Korpus-`.cas` sind `derived`. Ein Erzeuger muesste von aussen kommen (openMSX, castools).

**`cfi`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`cpm`** (Kanal: behaelter:libdsk-dskform-pcw720 + cpmtools-cf2dd)

MF-1085/MF-1087, P3-363. **Der Treffer ist eine NAMENSGLEICHHEIT, kein Kanal.** Der Zensus ordnet floptools Modul `cpm` diesem Plugin zu, weil beide so heissen - gemessen ist floptools `cpm` aber "Poly CP/M disk image" (ein neuseelaendischer Poly-1-Rechner) und hat mit UFTs libdsk-staemmigem `cpm` (55 diskdefs, Amstrad/PCW/Spectrum+3/...) nichts zu tun. Dasselbe gilt fuer den zweiten Treffer `smx` ("Specialist MX/Orion/B2M"), der nur deshalb als EINDEUTIG gilt, weil er die Endung `.cpm` fuehrt und `.cpm` im Baum nur dieses eine Plugin traegt. Ein gleicher Name ist kein Kanal, und eine eindeutige Endung auch nicht. **DRITTE Kollision, gemessen MF-1093:** auch **SAMdisk** fuehrt einen Eintrag `cpm` — und sein `src/types/cpm.cpp` sagt in der ersten Zeile, was es ist: "Basic support for SAM Coupe Pro-DOS images", 720K, erkannt an der Endung `.cpm`. Damit ist `.cpm` die Endung, ueber die sich in diesem Baum **drei verschiedene Formate** zuordnen lassen (Poly CP/M, Specialist MX, SAM Coupe Pro-DOS) — und keines davon ist UFTs libdsk-staemmiges CP/M. **NACHTRAG MF-1149, P3-383 erledigt: der Kanal ist da, und er lag nicht bei floptool.** Er heisst **cpmtools**, liegt seit MF-1085 gebaut im Baum und stand in keinem Register — `docs/ORACLES.md` fuehrte nur `cpmls`, den LESER. `mkfs.cpm` legt kein Abbild um, es legt ein DATEISYSTEM an, und `cpmcp` legt Dateien hinein; wo die Bytes landen, entscheidet die Definition. Das ist genau der Unterschied zu `dsktrans`, mit dem MF-1039 T1b verneint hat („eine Gleichheit ohne Aussage"). **Gemessen: der Kanal braucht ZWEI Werkzeuge.** `mkfs.cpm -f ibm-3740` erzeugt **9984** Byte statt 256 256 — nur Systemspuren und Verzeichnis —, und selbst eine voll beschriebene ibm-3740 kaeme auf 255 488, weil die letzten 768 Byte hinter dem letzten vollen 1024-Byte-Block liegen; `uft_cpm_detect_diskdef()` verlangt aber die exakte Gesamtgroesse. Den vollen Behaelter legt libdsks `dskform -type raw -format pcw720` an (737 280 Byte, 737 270 davon 0xE5), und **den Bruecken-Namen nennt cpmtools selbst**: seine `diskdefs`-Zeile fuer `cf2dd` traegt `libdsk:format pcw720`. Pruefung (3) ist bestanden statt behauptet: dieselben 20 Nutzdateien unter `cpm86-720` weichen in **474 521**, unter `altdsdd` in **473 722** von 737 280 Byte ab. UFT liest daraus **960 von 960 Nutzsektoren an ihrer eigenen Stelle**, dazu 9 Systemsektoren, 16 Verzeichnissektoren mit 256 Eintraegen und 455 freie — Summe 1440. Fixture `tests/corpus_free/cpmtools_cf2dd_720k.cpm`, Test `test_cpm_gegen_cpmtools`. Lizenzen jetzt gemessen (vorher LIZ-1 „nicht gemessen"): cpmtools 2.21 **GPL-3**, libdsk 1.5.12 **LGPL-2+**, beide AUSGEFUEHRT.

**`cqm`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`d77`** (Kanal: imd)

MF-1061. 348 848 B, voller D88-Container: 80 von 164 Spurzeigern, Sektorkoepfe C/H/R/N, Groessenfeld bei 0x1C trifft die Dateilaenge (Beleg am Objekt). Ueberhang 21 168 = 32 + 656 + 80 x 16 x 16.

**`dc42`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`dim`** (Kanal: keiner)

MF-1097. **Zwei Werkzeuge fuehren ein DIM, und das schreibende meint ein anderes Format** — die Gestalt von `fdi_pc98` (MF-1093) und `cpm` (MF-1085). hxcfes Liste hat `X68000_DIM` UND `ATARIST_DIM`; letzteres ist UFTs `dim_atari`, seit MF-690 auf T1b. Gemessen im Lauf: hxcfe LAEDT die Korpus-Datei richtig (`File loader found : X68000_DIM`) und sagt dann `No export support in X68000_DIM!`; floptool antwortet `Error: Saving to format 'dim' unsupported`. Fuer das X68000-DIM gibt es damit keinen Erzeuger. Die fremde ZERLEGUNG aus MF-1037 bleibt davon unberuehrt (hxcfe liest DIM und schreibt IMD) — sie belegt die Auslegung, nicht die Herkunft.

**`dms`** (Kanal: keiner)

MF-1097. **Das Orakel ist da, das Objekt fehlt — und damit ist SCOUT-11 schaerfer als bisher.** xDMS 1.3.2 liegt ZWEIMAL als Quelle im Baum (`HxCFloppyEmulator/.../thirdpartylibs/xdms/xdms-1.3.2/src/` und `amigadx/lib/xdmslib/`), und hxcfe fuehrt `AMIGA_DMS`. Gemessen im Lauf: `No export support in AMIGA_DMS!`. Das ist kein Versaeumnis, sondern die Bauart — beide Quellen tragen ausschliesslich ENTPACKER (`u_deep`, `u_heavy`, `u_medium`, `u_quick`, `u_rle`, `u_init`), keinen Packer; DMS-Dateien entstehen mit dem Original-DMS auf dem Amiga. Und im ganzen Baum liegt **keine einzige `.dms`** (`find -iname '*.dms'` = 0 Treffer). Der Differenzlauf aus SCOUT-11 scheitert also nicht am Werkzeug, sondern am KORPUS.

**`do`** (Kanal: keiner)

MF-1061, P3-348. rc = 0 und eine Datei von 0 BYTE. Apple-II-Disketten sind GCR-kodiert und ihre Sektorreihenfolge folgt einer Verschraenkung, die aus einem IMD-Satz nicht folgt — das Modul erwartet Fluss oder ein bereits Apple-geordnetes Abbild. Keine Aussage ueber andere Kanaele (to_woz2 steht aus).

**`edk`** (Kanal: layout:ENSONIQ_DD_800KB)

MF-1103, und der Eintrag berichtigt meinen eigenen von MF-1097. Dort stand `kanal: keiner` — gemessen gegen **ein** Werkzeug (floptools `esq16`, P3-364), waehrend im Baum **59 Klone** liegen und ich drei befragt hatte. Das ist woertlich die Lehre aus MF-1033/MF-1024: "das Werkzeug kann es nicht" war eine Aussage ueber den benutzten Aufruf. **Der Kanal traegt, in vier Schritten:** `epstool mkhfe --os` legt die EPS-Diskette an (1600 Bloecke, FAT ab Block 5, Wurzelverzeichnis 3); `epstool <img> import` schreibt 1200 selbstbenennende Bloecke durch epstools eigenen Dateisystemcode; `hxcfe -uselayout:ENSONIQ_DD_800KB -conv:HXC_HFE` kodiert in einen **2 008 064 Byte** grossen MFM-Zellstrom; `epstool hfe2img` dekodiert zurueck — eine ZWEITE fremde Hand — byteidentisch, und epstools Dateisystemleser findet beide Dateien darin wieder. Gemessen liest UFT **1600 von 1600 Bloecken an ihrer Stelle, 0 abweichend**; davon tragen **1379 Inhalt** und **1200 die Marke `UFT-EDK #NNNN`**. **Der erste Versuch wurde verworfen und das gehoert dazu:** dieselbe Kette ohne importierte Nutzlast ergab 1421 Nullbloecke und nur 158 verschiedene — der Abgleich haette zu 89 % Fuellung gegen Fuellung gehalten (MF-1021), und die Gegenprobe "um einen Block versetzt" fand dort noch 2 von 9 Uebereinstimmungen statt 0. **Was floptool angeht, bleibt P3-364 stehen:** sein `esq16` verschiebt im eigenen Rundlauf 1440 von 1600 Sektoren um eine Stelle. Nicht jedes Werkzeug, das die Groesse kennt, ist ein Erzeuger. Offen bleibt die **HD**-Spielart (1 638 400 Byte) — dafuer gibt es weiterhin kein Abbild von fremder Hand.

**`fdi_pc98`** (Kanal: keiner)

MF-1093. Zwei Werkzeuge fuehren ein `fdi`, und **keines meint dieses Format**: floptools `pc98_fdi` ist `r-`, also nur lesend (gemessen MF-1083), und SAMdisks `fdi` ist laut Kopfzeile seiner `src/types/fdi.cpp` das **Spectrum**-FDI ("Full Disk Image for Spectrum", worldofspectrum.org) — das ist UFTs `fdi`, das bereits auf T1 steht, nicht `fdi_pc98`. Ein Schreiber fuer das PC-98-FDI ist im Baum nicht vorhanden.

**`fds`** (Kanal: keiner)

MF-1097, bestaetigt P3-339. Gemessen im Lauf: floptool antwortet `Error: Format 'fds' unknown` — es ist ein Disketten-Werkzeug, und MAMEs `nes_dsk.cpp` hat keine `save`-Funktion (0 Treffer fuer `::save`/`supports_save`). hxcfes 207 Module und 95 Diskettenanordnungen fuehren kein FDS; libdsks 26 Treiber ebenfalls nicht. Die drei Korpus-`.fds` sind `derived` (nach den nesdev-Seiten gebaut), also kein Fremderzeugnis. Ein echtes FDS-Abbild waere ein Spiel-Abzug — das ist eine KORPUS- und Rechtefrage, keine Werkzeugfrage.

**`jv3`** (Kanal: imd)

MF-1061, am Rand gemessen. 111 104 B = 102 400 Nutzlast + 8704, und 8704 ist 0x2200 — genau die Kopfgroesse, die MF-1017 von 0x2300 berichtigt hat, hier von fremder Hand bestaetigt. 400/400 Mustertreffer.

**`korg_dss1`** (Kanal: keiner)

MF-1061, wie `akai_s900`. Die Zerlegung stimmt (800/800, keine Verschraenkung im Gegensatz zu Akai), das Zurueckschreiben ist tautologisch. **NACHTRAG MF-1149, hier eigens gemessen und nicht von `akai_s900` uebernommen:** dieselbe selbstbenennende 819 200-Byte-Eingabe durch DREI Anordnungen - `KORGDSS1_DD_800KB`, `ENSONIQ_DD_800KB` und `AKAIS950_DD_800KB` - ergibt drei VERSCHIEDENE Zellstroeme (KORGDSS1 gegen AKAIS950 weichen in **1 746 925 von 2 008 064 Byte** ab, obwohl beide 5 x 1024 sind: die Schnittstellenmodi und damit die Zwischenraeume unterscheiden sich). Zurueck ueber `RAW_LOADER` liefern **alle drei** dieselbe Datei, **0** abweichende Byte gegen die Eingabe. Der Grund liegt im FORMAT und nicht im Werkzeug; Begruendung und Abgrenzung bei `akai_s900`, Eigentuemer-Entscheidung als **P3-403**.

**`lisa_twiggy`** (Kanal: keiner)

MF-1097. **Zwei Werkzeuge kennen Twiggy, und keines erzeugt ein rohes Twiggy-Abbild.** MAMEs `ap_dsk35.cpp` nennt `871424 // Apple Twiggy 851KiB` — aber im **DC42**-Lader, und direkt danach legt derselbe Lader die 3,5-Zoll-Zonentafel an (`for track<80`, `ns = 12 - track/16`) statt der Twiggy-Tafel (46 Spuren je Seite, 22..15 Sektoren). fluxfox kennt Twiggy in `file_parsers/as/moof.rs`, also im MOOF-Behaelter. Beide reden ueber einen ANDEREN Behaelter als UFTs rohes `lisa_twiggy`. Gemessen im Lauf: floptool antwortet fuer `twiggy` wie fuer `lisa` `Error: Format ... unknown`; hxcfes 95 Anordnungen fuehren keine Lisa.

**`mgt`** (Kanal: keiner)

MF-1064, gemessen und VERWORFEN. libdsk fuehrt `mgt800` in seiner Formatliste, und der Lauf liefert 819 200 Byte mit 1600/1600 Mustertreffern — sieht nach einem Fixture aus. Die Gegenprobe entscheidet dagegen: DIESELBE Datei kommt heraus, wenn man `-format` ganz weglaesst, und auch mit `-format pcw800`. Alle drei sha256-identisch. libdsk reicht die IMD-Sektoren also in DATEIREIHENFOLGE durch; der Schalter aendert beim Schreiben eines FLACHEN Ziels nichts. Ein so erzeugtes Abbild waere eine Tautologie wie bei `akai_s900` (MF-1061). **Der Unterschied zu MF-1032/1033, wo derselbe Schalter trug:** dort war die RAW-Seite die EINGABE und brauchte eine Geometrie, das Ziel war ein Container mit eigener Anordnung. Beim Schreiben nach raw bestimmt die Eingabe die Reihenfolge. Der Kanal taugt also fuer Container, nicht fuer flache Ziele.

**`nfd`** (Kanal: keiner)

MF-1082, Erzeuger-Zensus der NFD-Familie. **Leser (gemessen): sechs.** Greaseweazle `src/greaseweazle/image/nfd.py` (Keir Fraser, PUBLIC DOMAIN) - traegt `read_only = True`, liest NUR r0 (`T98FDDIMAGE.R0`) und wirft bei r1 ausdruecklich; in `tools/util.py:303` als `'.nfd': 'NFD'` REGISTRIERT. MAMEs `nfd_dsk.cpp` (BSD-3) - liest r0 UND r1, kein `save`. FluxEngine `nfdimagereader.cc`. `d88split`/`nfd2mhlt.pl` (tomari, Public Domain) - liest r0 und r1 (r1 ungetestet), schreibt NUR Mahalito. FIVEC. UFT selbst. **Schreiber (gemessen): zwei, und BEIDE nur r1.** FIVEC speichert laut pc98.org alle NFD als r1, weil r0 deutlich anders und veraltet ist - reine Software. `NFDMAKE.EXE` aus dem T98-NEXT TOOL ist der urspruengliche Erzeuger, dokumentiert sind heute `-r1` und `-r1d`; ein `-r0` findet sich in keiner der gesichteten Quellen, und es laeuft auf ECHTER PC-9801-Hardware (MF-310). Dasselbe gilt fuer T98-Nexts eigenes 'FD zu Abbild wandeln' - es braucht ein echtes Laufwerk. **Fuer r0 ist damit kein reiner Software-Schreiber gefunden**, und das ist eine Messung ueber sechs Werkzeuge statt ueber eines. **Was dabei frei ist:** die Formatbeschreibungen beider Fassungen stammen vom Urheber und tragen eine ausdrueckliche Freigabe - woertlich 'Freely used for data analysis, tool development, etc.', 2001/01/22 LED (pc98.org/project/doc/nfdr0.html und nfdr1.html). Kanal *Spec*, unbeschraenkt.

**`po`** (Kanal: keiner)

MF-1061, P3-348. Wie `do`: rc = 0, Datei 0 Byte.

**`pro`** (Kanal: keiner)

MF-1093. `pro` ist APE ProSystem (Atari 8-bit) — ein Kopierschutz-Format mit 12-Byte-Sektorkopf und Phantomsektoren. Das naheliegende Werkzeug im Baum ist `a8rawconv`; gemessen ueber seine Formatmodule kennt es **a2, adf, atr, atx, vfd, xfd** und **kein PRO**. Der Erzeuger waere APE selbst (proprietaer, Windows) — eine Beschaffungsfrage.

**`syn`** (Kanal: keiner)

MF-1097, schliesst die Erzeuger-Frage zu P3-340. Gemessen im Lauf: floptool antwortet `Error: Format 'syn' unknown` und ebenso fuer `synclavier`; hxcfes 207 Module und 95 Diskettenanordnungen fuehren keine Synclavier-Zeile, libdsks 26 Treiber auch nicht. **Der Blocker ist hier doppelt, und beide Haelften sind benannt:** es fehlt ein Erzeuger UND eine nachlesbare Beschreibung — der einzige Fund aus MF-1085 ist ein KryoFlux-Forumsfaden, der mit 403 antwortet. Ohne Beschreibung waere selbst ein Abbild nicht auswertbar.

**`tan`** (Kanal: artefaktgleich:jv1)

MF-1097, und die Zeile ist bewusst KEIN Negativ. Gemessen im Lauf: floptool kennt weder `tan` noch `trs80` als Zielnamen (`Error: Format ... unknown`) — ein eigener TAN-Schreiber existiert also nicht. **Aber der Kanal existiert trotzdem:** TAN ist ein kopfloser Sektorabzug in JV1-Anordnung (10 x 256, EINE Seite; belegt gegen MAMEs `jv1_format::formats[]` und Tim Mann), und damit ist `tests/corpus_free/floptool_jv1_80spuren.jv1` byteweise zugleich ein gueltiges TAN-Abbild — dieselben 204 800 Byte, andere Endung. Die **Eigentuemer-Entscheidung ist am 2026-09-13 gefallen** (P3-365, Weg a; MF-1104): die Stufe ruht darauf, und der Test benennt ausdruecklich, was sie NICHT sagt. Vorher galt: sie truege sonst eine Aussage, die der Beleg nicht deckt (dass die im Feld kursierenden `.tan` wirklich diese Anordnung tragen). Drei Wege mit Kosten und Kennzahl stehen in **P3-365**.

**`td0`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`trd`** (Kanal: imd)

MF-1061, am Rand gemessen. 655 360 B ohne Ueberhang, 2560/2560 Mustertreffer.

**`udi`** (Kanal: keiner)

MF-1093. SAMdisk ist das naheliegende Werkzeug — MF-1015 hat UFTs UDI-Leser gegen `src/samdisk/udi.cpp` gemessen und es dabei in der Pruefsumme **begruendet ueberstimmt**. Es kann UDI aber nur LESEN: seine Formattafel (`include/types.h`) fuehrt `ADD_IMAGE_RO(UDI)`, nicht `ADD_IMAGE_RW`. Ein Bau von SAMdisk wuerde fuer `udi` also nichts oeffnen. Von den **28** schreibbaren SAMdisk-Formaten trifft keines eines der offenen UFT-Formate; der einzige Namenstreffer ist `cpm`, und der ist eine Kollision (siehe dort).

**`v9t9`** (Kanal: imd)

MF-1060. DSSD 184 320 B: 720/720 Mustertreffer, Gegenprobe LINEAR nur 18 — unterscheidet. SSSD 92 160 B unterscheidet NICHT (einseitig fallen beide Formeln zusammen), DSDD 368 640 B ist hxcfe misslungen (50,2 % 0xF6, 9 statt 18 Sektoren je Spur).

**`vdk`** (Kanal: imd)

MF-1061. Zweiseitig 368 652 B = 12 Kopf + 40 x 2 x 18 x 256; hxcfe setzt den Kopf selbst. Drei Anordnungs-Hypothesen: zylinder-verschraenkt 1440/1440, kopf-dur 36, kopf-dur rueckwaerts 36 — unterscheidet. EINSEITIG waere es wertlos.

**`xdm86`** (Kanal: imd)

MF-1060. Dasselbe Abbild wie `v9t9` — beide Plugins lesen 184 320 Byte als 40 x 2 x 9 x 256.


## Der blinde Fleck dieses Zensus


Die Zuordnung Werkzeugmodul -> Plugin laeuft ueber die **Dateiendung** — abgeleitet, nicht gepflegt (MF-636). Das hat eine Grenze, und sie gehoert benannt statt verschwiegen: Werkzeuge, deren interner Typname nichts mit einer Endung zu tun hat, fallen durch. libdsks `copyqm` und `tele` sind genau das — sie schreiben die Formate, die UFT `cqm` und `td0` nennt, und dieser Zensus sieht es nicht.


**Was hier steht, ist also eine Untergrenze.** Die unzugeordneten Namen unten sind der Rueckstand, aus dem die naechsten Kandidaten kommen — jeder von Hand aufzuloesen und dann als gemessener Kanal einzutragen, nicht als Namenstafel.


**hxcfe-`RW`-Module, deren Endung KEIN Plugin traegt (13)** — ein Werkzeug im Baum schreibt sie, UFT liest sie nicht. Das ist die **Lückenliste**, und jeder Eintrag käme als **T1b** auf die Welt statt als T3, weil der Erzeuger vom ersten Tag an da ist (Preis der 1:2-Regel damit gedeckt):


* `AMIGA_ADZ` — `*.adz`
* `ARBURG` — `*.arburgfd`
* `ATARIST_STW` — `*.stw`
* `FDX68_FDX` — `*.fdx`
* `GENERIC_XML` — `*.xml`
* `H17_HEATHKIT` — `*.h17`
* `HEATHKIT` — `*.h8d`
* `HXCMFM_IMG` — `*.mfm`
* `HXC_AFI` — `*.afi`
* `HXC_QD` — `*.qd`
* `SPECCYDOS_SDD` — `*.sdd`
* `THOMSON_FD` — `*.fd`
* `VTR_IMG` — `*.vtr`

**hxcfe-`RW`-Module, deren Endung ein Plugin traegt, die aber trotzdem nicht zugeordnet wurden (0):** keine — hier fehlt die ZUORDNUNG, nicht das Format.


**libdsk-Typen ohne Zuordnung (12):** `copyqm`, `floppy`, `gotek`, `gotek72`, `ldbs`, `ntwdm`, `rawob`, `rawoo`, `remote`, `simh`, `tele`, `ydsk`


Bei libdsk lässt sich das nicht trennen: seine Typen tragen **keine Dateiendung**, nur einen internen Namen. `copyqm` und `tele` standen genau deshalb hier, obwohl UFT sie als `cqm` und `td0` längst liest — MF-1063 hat sie von Hand aufgelöst und gehoben. Der Rest dieser Liste ist ungeprüft und kann beides sein.


## Bereits belegt


| Format | Stufe | hxcfe (RW) | libdsk |
|---|---|---|---|
| `2img` | T1b | — | — |
| `86f` | T1b | — | — |
| `adf` | T1b | AMIGA_ADF (?) | — |
| `adf_arc` | T1b | AMIGA_ADF (?) | — |
| `adf_ext` | T1b | AMIGA_ADF (?) | — |
| `adl` | T1b | AMIGA_ADF (?) | — |
| `apridisk` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | apridisk, dsk (?) |
| `atr` | T1b | — | — |
| `atx` | T1 | — | — |
| `cas` | T1b | — | — |
| `cfi` | T1b | — | cfi |
| `cpm` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `cqm` | T1b | — | — |
| `d13` | T1b | — | — |
| `d64` | T1b | — | — |
| `d67` | T1b | — | — |
| `d71` | T1b | — | — |
| `d77` | T1b | — | — |
| `d80` | T1b | — | — |
| `d81` | T1b | — | — |
| `d82` | T1b | — | — |
| `d88` | T1b | NEC_D88 | — |
| `dc42` | T1b | RAW_LOADER (?) | dc42 |
| `dcm` | T1b | — | — |
| `dim_atari` | T1b | ATARIST_DIM (?) | — |
| `dmk` | T1b | TRS80_DMK | — |
| `dms` | T1b | — | — |
| `do` | T1b | AMSTRADCPC_DSK (?), APPLE2_DO, ORIC_DSK (?) | dsk (?) |
| `dsk_cpc` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `edk` | T1b | — | — |
| `edsk` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?), edsk |
| `fdi` | T1 | — | — |
| `fdi_pc98` | T1b | — | — |
| `fds` | T1b | — | — |
| `g64` | T1 | — | — |
| `g71` | T1b | — | — |
| `hardsector` | ? | RAW_LOADER (?) | — |
| `hfe` | T1 | HXC_HFE, HXC_HFEV3, HXC_STREAMHFE | — |
| `imd` | T1 | IMD_IMG | imd |
| `img` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), RAW_LOADER (?) | dsk (?) |
| `ipf` | T1 | SPS_IPF | — |
| `jv1` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `jv3` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TRS80_JV3 | dsk (?), jv3 |
| `jvc` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `kfx` | T1b | KRYOFLUXSTREAM (?) | raw (?) |
| `logical` | T1b | — | logical |
| `mfi` | T1b | MAME_MFI | — |
| `mgt` | T1 | RAW_LOADER (?) | — |
| `micropolis` | T1b | — | — |
| `msa` | T1b | ATARIST_MSA | — |
| `msx_disk` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), RAW_LOADER (?) | dsk (?) |
| `myz80` | T1b | — | myz80 |
| `nanowasp` | T1b | — | nanowasp |
| `nib` | T1b | — | — |
| `northstar` | T1b | NORTHSTAR | — |
| `opus` | T1b | — | — |
| `pdp` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `po` | T1b | AMSTRADCPC_DSK (?), APPLE2_PO, ORIC_DSK (?) | dsk (?) |
| `posix` | T1b | AMSTRADCPC_DSK (?), KRYOFLUXSTREAM (?), ORIC_DSK (?), RAW_LOADER (?) | dsk (?), raw (?) |
| `pri` | T1b | — | — |
| `qrst` | T1b | — | qrst |
| `rcpmfs` | ? | — | rcpmfs |
| `sad` | T1b | — | — |
| `sam` | T1b | — | — |
| `sap_thomson` | T1b | — | sap |
| `scl` | T1 | — | — |
| `scp` | T1b | SCP_FLUX_STREAM | — |
| `ssd` | T1b | — | — |
| `st` | T1b | ATARIST_ST | — |
| `stx` | T1b | ATARIST_STX | — |
| `t1k` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `tan` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `td0` | T1b | — | — |
| `trd` | T1b | ZXSPECTRUM_TRD | — |
| `v9t9` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TI994A_V9T9 (?) | dsk (?) |
| `vdk` | T1b | AMSTRADCPC_DSK (?), DRAGON3264_VDK, ORIC_DSK (?) | dsk (?) |
| `victor9k` | T1b | — | — |
| `woz` | T1b | — | — |
| `xdm86` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TI994A_V9T9 (?) | dsk (?) |
| `xfd` | T1b | — | — |

