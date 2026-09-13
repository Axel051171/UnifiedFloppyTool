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
| davon auf T2/T3 (offen) | 26 |
| davon mit **gemessenem** Erzeuger-Kanal | **0** |
| davon mit Werkzeug-Zusage, Kanal ungemessen | 1 |
| hxcfe-Module mit `RW` | 38 |
| libdsk-Typen (alle les- und schreibbar) | 26 |

## Die offenen Formate


| Format | Stufe | hxcfe (RW) | libdsk | Kanal | Klasse |
|---|---|---|---|---|---|
| `2img` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `adf_ext` | T2 | AMIGA_ADF (?) | — | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `akai_s900` | T2 | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `cas` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `cpm` | T2 | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `d13` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `dim` | T2 | ATARIST_DIM (?) | — | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `dms` | T3 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `edk` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `fdi_pc98` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `fds` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `hardsector` | T3 | RAW_LOADER (?) | — | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `ipf` | T3 | SPS_IPF | — | nicht gemessen | — (hat bereits ein Fremdabbild) |
| `jv1` | T2 | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `korg_dss1` | T2 | — | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `lisa_twiggy` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `mgt` | T2 | RAW_LOADER (?) | — | keiner | **C** — gemessen: dieser Weg traegt nicht |
| `nfd` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `opus` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `pro` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `rcpmfs` | T3 | — | rcpmfs | nicht gemessen | **A?** — Werkzeug sagt RW, Kanal UNGEMESSEN |
| `scl` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `syn` | T3 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `tan` | T2 | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) | nicht gemessen | **?** — nur ueber eine GETEILTE Endung zugeordnet |
| `udi` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |
| `victor9k` | T2 | — | — | nicht gemessen | **B/C** — kein Werkzeug im Baum, das schreibt |

`(?)` hinter einem Werkzeugnamen heisst: die Zuordnung laeuft ueber eine Endung, die sich **mehrere** Plugins teilen. `.dsk` tragen `apridisk`, `cpm`, `do`, `jv1` und `tan` gemeinsam, und `AMSTRADCPC_DSK` schreibt keines davon. Ein solcher Treffer ist ein Verdacht, kein Kandidat.


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
| `86f` | T1b | — | — |
| `adf` | T1b | AMIGA_ADF (?) | — |
| `adf_arc` | T1b | AMIGA_ADF (?) | — |
| `adl` | T1b | AMIGA_ADF (?) | — |
| `apridisk` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | apridisk, dsk (?) |
| `atr` | T1b | — | — |
| `atx` | T1 | — | — |
| `cfi` | T1b | — | cfi |
| `cqm` | T1b | — | — |
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
| `hfe` | T1 | HXC_HFE, HXC_HFEV3, HXC_STREAMHFE | — |
| `imd` | T1 | IMD_IMG | imd |
| `img` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), RAW_LOADER (?) | dsk (?) |
| `jv3` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TRS80_JV3 | dsk (?), jv3 |
| `jvc` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `kfx` | T1b | KRYOFLUXSTREAM (?) | raw (?) |
| `logical` | T1b | — | logical |
| `mfi` | T1b | MAME_MFI | — |
| `micropolis` | T1b | — | — |
| `msa` | T1b | ATARIST_MSA | — |
| `msx_disk` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), RAW_LOADER (?) | dsk (?) |
| `myz80` | T1b | — | myz80 |
| `nanowasp` | T1b | — | nanowasp |
| `nib` | T1b | — | — |
| `northstar` | T1b | NORTHSTAR | — |
| `pdp` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `po` | T1b | AMSTRADCPC_DSK (?), APPLE2_PO, ORIC_DSK (?) | dsk (?) |
| `posix` | T1b | AMSTRADCPC_DSK (?), KRYOFLUXSTREAM (?), ORIC_DSK (?), RAW_LOADER (?) | dsk (?), raw (?) |
| `pri` | T1b | — | — |
| `qrst` | T1b | — | qrst |
| `sad` | T1b | — | — |
| `sam` | T1b | — | — |
| `sap_thomson` | T1b | — | sap |
| `scp` | T1b | SCP_FLUX_STREAM | — |
| `ssd` | T1b | — | — |
| `st` | T1b | ATARIST_ST | — |
| `stx` | T1b | ATARIST_STX | — |
| `t1k` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?) | dsk (?) |
| `td0` | T1b | — | — |
| `trd` | T1b | ZXSPECTRUM_TRD | — |
| `v9t9` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TI994A_V9T9 (?) | dsk (?) |
| `vdk` | T1b | AMSTRADCPC_DSK (?), DRAGON3264_VDK, ORIC_DSK (?) | dsk (?) |
| `woz` | T1b | — | — |
| `xdm86` | T1b | AMSTRADCPC_DSK (?), ORIC_DSK (?), TI994A_V9T9 (?) | dsk (?) |
| `xfd` | T1b | — | — |

