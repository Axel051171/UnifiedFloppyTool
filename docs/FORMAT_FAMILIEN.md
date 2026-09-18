# Format-Familien — wer erzeugt was

> **ERZEUGT von `scripts/gen_familien.py`. Nicht von Hand aendern.**
>
> Eine Familie ist hier die Menge der Formate, die DASSELBE gemessene
> Werkzeug herstellen kann — nicht eine Plattform. Quelle ist allein
> `docs/erzeuger_kanaele.json`, das je Format den Erzeuger fuehrt, der
> wirklich gelaufen ist.

## Die Familien

| Quelle | Formate | beruehrt | welche |
|---|---:|---|---|
| `hxcfe` | 6 | **ja** | `d77` `jv3` `trd` `v9t9` `vdk` `xdm86` |
| `dsktrans` | 5 | **ja** | `apridisk` `cfi` `cqm` `dc42` `td0` |
| `dskform` | 1 | — | `cpm` |
| `epstool` | 1 | — | `edk` |
| `floptool` | 1 | — | `tan` |

## Der Fortschritt, nach Quellen gezaehlt

| | |
|---|---|
| Plugins mit Variantentafel | **9** |
| davon mit gemessenem Erzeuger | **2** (td0 trd) |
| beruehrte Quellen | **2 von 5** |
| das entspricht Formaten | **11 von 14** mit Erzeuger |
| Zensus-Eintraege ohne Erzeuger | 16 — gemessene Abwesenheit, siehe unten |
| Formate OHNE Zensus-Eintrag | 107 von 137 — **nicht gemessen**, nicht "keine Familie" |

## Gemessen: kein Erzeuger unter den geprueften Werkzeugen

Fuer diese Formate hat der Zensus gesucht und **nichts gefunden**. Das ist
eine Aussage ueber die probierten Werkzeuge, nicht ueber die Welt.

* `adf_ext` — probiert: hxcfe -conv:AMIGA_EXTADF / -conv:AMIGA_OLDEXTADF
* `akai_s900` — probiert: hxcfe -uselayout:AKAIS950_DD_800KB -conv:HXC_HFE, zurueck ueber -conv:RAW_LOADER
* `cas` — probiert: floptool flopconvert auto cas; hxcfe -modulelist/-rawlist
* `dim` — probiert: hxcfe -conv:X68000_DIM; floptool flopconvert auto dim
* `dms` — probiert: hxcfe -conv:AMIGA_DMS
* `do` — probiert: hxcfe -conv:APPLE2_DO
* `fdi_pc98` — probiert: SAMdisk `fdi` / floptool `pc98_fdi`
* `fds` — probiert: floptool flopconvert auto fds; hxcfe -modulelist/-rawlist
* `korg_dss1` — probiert: hxcfe -uselayout:KORGDSS1_DD_800KB
* `lisa_twiggy` — probiert: floptool flopconvert auto twiggy/lisa; MAME ap_dsk35.cpp; fluxfox moof.rs
* `mgt` — probiert: dsktrans -itype imd -otype raw -format mgt800
* `nfd` — probiert: gemessen MF-1082: sechs Leser, zwei Schreiber, beide nur r1
* `po` — probiert: hxcfe -conv:APPLE2_PO
* `pro` — probiert: a8rawconv (Quelle in src/a8rawconv/)
* `syn` — probiert: floptool flopconvert auto syn/synclavier; hxcfe -modulelist/-rawlist
* `udi` — probiert: SAMdisk (Quelle in src/samdisk/ und neue-ideen/samdisk_plus.zip)

## Widerspruch: „kein Erzeuger" gegen Stufe T1/T1b

Diese Formate stehen auf einer Stufe, die eine **fremde Hand VERLANGT** —
und tragen zugleich `kanal: keiner`. Beides kann nicht stimmen; der
Zensus kennt den Erzeuger noch nicht, mit dem die Hebung gelungen ist.

| Format | Stufe | der Zensus probierte |
|---|---|---|
| `adf_ext` | **T1b** | hxcfe -conv:AMIGA_EXTADF / -conv:AMIGA_OLDEXTADF |
| `cas` | **T1b** | floptool flopconvert auto cas; hxcfe -modulelist/-rawlist |
| `dms` | **T1b** | hxcfe -conv:AMIGA_DMS |
| `do` | **T1b** | hxcfe -conv:APPLE2_DO |
| `fdi_pc98` | **T1b** | SAMdisk `fdi` / floptool `pc98_fdi` |
| `fds` | **T1b** | floptool flopconvert auto fds; hxcfe -modulelist/-rawlist |
| `mgt` | **T1** | dsktrans -itype imd -otype raw -format mgt800 |
| `po` | **T1b** | hxcfe -conv:APPLE2_PO |

**8 von 16** Eintraegen ohne Erzeuger sind damit nachweislich veraltet.

## Beinahe-Treffer — hier entscheidet ein Mensch

Diese Paare haette ein Abgleich ueber Namensanfaenge verwechselt. Sie sind **nicht** gezaehlt:

* Plugin `adf` gegen Zensus-Schluessel `adf_ext` — verschiedene Formate (vgl. MF-1222: `dim` gegen `dim_atari`).

