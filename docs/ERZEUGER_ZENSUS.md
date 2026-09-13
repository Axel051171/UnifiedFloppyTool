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
| Plugins gesamt | 88 |
| davon auf T2/T3 (offen) | 16 |
| davon mit **gemessenem** Erzeuger-Kanal | **0** |
| davon mit Werkzeug-Zusage, Kanal ungemessen | 0 |
| hxcfe-Module mit `RW` | 38 |
| libdsk-Typen (alle les- und schreibbar) | 26 |
| floptool-Module gesamt | 151 |
| davon schreibfaehig (`rw`/`-w`) | 122 |

## Die offenen Formate


| Format | Stufe | hxcfe (RW) | libdsk | floptool (w) | Kanal | Klasse |
|---|---|---|---|---|---|---|
| `adf_ext` | T2 | AMIGA_ADF (?) | — | adf (?), adfs_n (?), adfs_o (?) | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `akai_s900` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `cas` | T2 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `cpm` | T2 | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) | a2_16sect_dos (?), a2_16sect_prodos (?), abc800i (?), abc_fd2 (?), adam (?), atom (?), bw12 (?), bw2 (?), c8280 (?), cgenie (?), cpis (?), cpm, flex (?), guab (?), itt3030 (?), jv1 (?), jv3 (?), jvc (?), kaypro2 (?), kaypro2x (?), m5 (?), mdos (?), mgt (?), mm1 (?), mm2 (?), msx (?), nabupc (?), nascom (?), oric_dsk (?), oric_jasmin (?), os9 (?), pc (?), pc98 (?), ql (?), smx, svi (?), tandy2k (?), tdf (?), ti99 (?), tiki100 (?), tvc (?), uniflex (?), vtech_dsk (?) | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `dim` | T2 | ATARIST_DIM (?) | — | — | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `dms` | T3 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `edk` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `fdi_pc98` | T2 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `fds` | T2 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `korg_dss1` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `lisa_twiggy` | T2 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `nfd` | T2 | — | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `pro` | T2 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `syn` | T3 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `tan` | T2 | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) | a2_16sect_dos (?), a2_16sect_prodos (?), abc800i (?), abc_fd2 (?), adam (?), atom (?), bw12 (?), bw2 (?), c8280 (?), cgenie (?), cpis (?), flex (?), guab (?), itt3030 (?), jv1 (?), jv3 (?), jvc (?), kaypro2 (?), kaypro2x (?), m5 (?), mdos (?), mgt (?), mm1 (?), mm2 (?), msx (?), nabupc (?), nascom (?), oric_dsk (?), oric_jasmin (?), os9 (?), pc (?), pc98 (?), ql (?), svi (?), tandy2k (?), tdf (?), ti99 (?), tiki100 (?), tvc (?), uniflex (?), vtech_dsk (?) | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `udi` | T2 | — | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |

`(?)` hinter einem Werkzeugnamen heisst: die Zuordnung laeuft ueber eine Endung, die sich **mehrere** Plugins teilen. `.dsk` tragen `apridisk`, `cpm`, `do`, `jv1` und `tan` gemeinsam, und `AMSTRADCPC_DSK` schreibt keines davon. Ein solcher Treffer ist ein Verdacht, kein Kandidat.



## Was bei jedem Lauf herauskam


MF-1082: dieser Abschnitt hat gefehlt. Das Feld `docs/erzeuger_kanaele.json` wurde eingelesen, in die Zeile gelegt und **nie ausgegeben** — siebzehn Formate trugen dort eine ausfuehrliche Messung, die niemand zu sehen bekam, in einer Datei, deren Kopf sagt, sie halte die Laeufe fest. Dieselbe Klasse wie ein Leser ohne Tuer (MF-930), nur an den Messdaten.


**`akai_s900`** (Kanal: keiner)

MF-1061, gemessen und VERWORFEN. hxcfe zerlegt ein flaches 819 200-Byte-Abbild mit dieser Anordnung richtig — 800 von 800 Sektoren tragen ihren eigenen Namen, und die Gegenprobe mit ENSONIQ_DD_800KB (gleiche Groesse, 10 x 512 statt 5 x 1024) trifft nur 320 von 1600. Die ZERLEGUNG ist also eine echte zweite Hand auf die Geometrie. Aber das Zurueckschreiben ins flache Format ist LAYOUT-UNABHAENGIG byteidentisch — auch ueber die Ensoniq-Anordnung kommt dieselbe Datei heraus. Ein so erzeugtes Fixture waere eine Tautologie, kein Beleg.

**`apridisk`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`cfi`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`cpm`** (Kanal: keiner)

MF-1085/MF-1087, P3-363. **Der Treffer ist eine NAMENSGLEICHHEIT, kein Kanal.** Der Zensus ordnet floptools Modul `cpm` diesem Plugin zu, weil beide so heissen - gemessen ist floptools `cpm` aber "Poly CP/M disk image" (ein neuseelaendischer Poly-1-Rechner) und hat mit UFTs libdsk-staemmigem `cpm` (55 diskdefs, Amstrad/PCW/Spectrum+3/...) nichts zu tun. Dasselbe gilt fuer den zweiten Treffer `smx` ("Specialist MX/Orion/B2M"), der nur deshalb als EINDEUTIG gilt, weil er die Endung `.cpm` fuehrt und `.cpm` im Baum nur dieses eine Plugin traegt. Ein gleicher Name ist kein Kanal, und eine eindeutige Endung auch nicht.

**`cqm`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`d77`** (Kanal: imd)

MF-1061. 348 848 B, voller D88-Container: 80 von 164 Spurzeigern, Sektorkoepfe C/H/R/N, Groessenfeld bei 0x1C trifft die Dateilaenge (Beleg am Objekt). Ueberhang 21 168 = 32 + 656 + 80 x 16 x 16.

**`dc42`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`do`** (Kanal: keiner)

MF-1061, P3-348. rc = 0 und eine Datei von 0 BYTE. Apple-II-Disketten sind GCR-kodiert und ihre Sektorreihenfolge folgt einer Verschraenkung, die aus einem IMD-Satz nicht folgt — das Modul erwartet Fluss oder ein bereits Apple-geordnetes Abbild. Keine Aussage ueber andere Kanaele (to_woz2 steht aus).

**`edk`** (Kanal: keiner)

MF-1085, P3-364. floptools `esq16` (Ensoniq VFX-SD/SD-1/EPS-16) nimmt genau die 819 200 Byte, die UFTs `edk` als DD-Fassung fuehrt - aber SEIN EIGENER RUNDLAUF VERSCHIEBT DIE DISKETTE. `esqimg_format::load()` setzt `sectors[i].sector_id = i` (0..9), `save()` sammelt ueber `get_track_data_mfm_pc()` die Nummern 1..10 ein. Gemessen an 80 x 2 x 10 mit Ortsmarken je Sektor: 1440 von 1600 Sektoren kommen um GENAU EINE STELLE verschoben zurueck, 160 genullt (einer je Spur), 83 360 Byte abweichend, 0 an ihrer Stelle. Ein Werkzeug, dessen eigener Rundlauf eine Diskette verschiebt, ist kein Erzeuger.

**`jv3`** (Kanal: imd)

MF-1061, am Rand gemessen. 111 104 B = 102 400 Nutzlast + 8704, und 8704 ist 0x2200 — genau die Kopfgroesse, die MF-1017 von 0x2300 berichtigt hat, hier von fremder Hand bestaetigt. 400/400 Mustertreffer.

**`korg_dss1`** (Kanal: keiner)

MF-1061, wie `akai_s900`. Die Zerlegung stimmt (800/800, keine Verschraenkung im Gegensatz zu Akai), das Zurueckschreiben ist tautologisch.

**`mgt`** (Kanal: keiner)

MF-1064, gemessen und VERWORFEN. libdsk fuehrt `mgt800` in seiner Formatliste, und der Lauf liefert 819 200 Byte mit 1600/1600 Mustertreffern — sieht nach einem Fixture aus. Die Gegenprobe entscheidet dagegen: DIESELBE Datei kommt heraus, wenn man `-format` ganz weglaesst, und auch mit `-format pcw800`. Alle drei sha256-identisch. libdsk reicht die IMD-Sektoren also in DATEIREIHENFOLGE durch; der Schalter aendert beim Schreiben eines FLACHEN Ziels nichts. Ein so erzeugtes Abbild waere eine Tautologie wie bei `akai_s900` (MF-1061). **Der Unterschied zu MF-1032/1033, wo derselbe Schalter trug:** dort war die RAW-Seite die EINGABE und brauchte eine Geometrie, das Ziel war ein Container mit eigener Anordnung. Beim Schreiben nach raw bestimmt die Eingabe die Reihenfolge. Der Kanal taugt also fuer Container, nicht fuer flache Ziele.

**`nfd`** (Kanal: keiner)

MF-1082, Erzeuger-Zensus der NFD-Familie. **Leser (gemessen): sechs.** Greaseweazle `src/greaseweazle/image/nfd.py` (Keir Fraser, PUBLIC DOMAIN) - traegt `read_only = True`, liest NUR r0 (`T98FDDIMAGE.R0`) und wirft bei r1 ausdruecklich; in `tools/util.py:303` als `'.nfd': 'NFD'` REGISTRIERT. MAMEs `nfd_dsk.cpp` (BSD-3) - liest r0 UND r1, kein `save`. FluxEngine `nfdimagereader.cc`. `d88split`/`nfd2mhlt.pl` (tomari, Public Domain) - liest r0 und r1 (r1 ungetestet), schreibt NUR Mahalito. FIVEC. UFT selbst. **Schreiber (gemessen): zwei, und BEIDE nur r1.** FIVEC speichert laut pc98.org alle NFD als r1, weil r0 deutlich anders und veraltet ist - reine Software. `NFDMAKE.EXE` aus dem T98-NEXT TOOL ist der urspruengliche Erzeuger, dokumentiert sind heute `-r1` und `-r1d`; ein `-r0` findet sich in keiner der gesichteten Quellen, und es laeuft auf ECHTER PC-9801-Hardware (MF-310). Dasselbe gilt fuer T98-Nexts eigenes 'FD zu Abbild wandeln' - es braucht ein echtes Laufwerk. **Fuer r0 ist damit kein reiner Software-Schreiber gefunden**, und das ist eine Messung ueber sechs Werkzeuge statt ueber eines. **Was dabei frei ist:** die Formatbeschreibungen beider Fassungen stammen vom Urheber und tragen eine ausdrueckliche Freigabe - woertlich 'Freely used for data analysis, tool development, etc.', 2001/01/22 LED (pc98.org/project/doc/nfdr0.html und nfdr1.html). Kanal *Spec*, unbeschraenkt.

**`po`** (Kanal: keiner)

MF-1061, P3-348. Wie `do`: rc = 0, Datei 0 Byte.

**`td0`** (Kanal: imd)

MF-1063. Ein IMD-Traeger, fuenf Ziele. Sektor 1 der Spur 0/0 traegt einen echten PC-720K-Bootsektor, damit `cfi`s strukturelle Sonde (sie verlangt eine gueltige BPB) ueberhaupt ansprechen kann; die uebrigen 1439 benennen sich selbst. Kein Fuellbyte.

**`trd`** (Kanal: imd)

MF-1061, am Rand gemessen. 655 360 B ohne Ueberhang, 2560/2560 Mustertreffer.

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
| `adl` | T1b | AMIGA_ADF (?) | — |
| `apridisk` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | apridisk, dsk (?) |
| `atr` | T1b | — | — |
| `atx` | T1 | — | — |
| `cfi` | T1b | — | cfi |
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
| `do` | T1b | AMSTRADCPC_DSK (?), APPLE2_DO, ORIC_DSK (?) | dsk (?) |
| `dsk_cpc` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `edsk` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?), edsk |
| `fdi` | T1 | — | — |
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
| `td0` | T1b | — | — |
| `trd` | T1b | ZXSPECTRUM_TRD | — |
| `v9t9` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TI994A_V9T9 (?) | dsk (?) |
| `vdk` | T1b | AMSTRADCPC_DSK (?), DRAGON3264_VDK, ORIC_DSK (?) | dsk (?) |
| `victor9k` | T1b | — | — |
| `woz` | T1b | — | — |
| `xdm86` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TI994A_V9T9 (?) | dsk (?) |
| `xfd` | T1b | — | — |

