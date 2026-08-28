# FORMAT-CLASSIFICATION — Kopierschutz- & Disk-Fehler-Repräsentation

_Momentaufnahme vom 2026-07-05 aus der Katalogtabelle (`data_layer`-Feld),
161 Format-IDs. Die Tabelle liegt seit MF-624 in
[`FORMAT_CATALOG.md`](FORMAT_CATALOG.md); ihre fruehere C-Datei
`src/formats/uft_format_registry_v2.c` ist geloescht._

> ⚠ **MF-620 — zwei Einschränkungen, die hier fehlten.**
>
> **Erstens: es gibt keinen Erzeuger.** Der Satz oben sagte „Generiert",
> aber im Baum liegt kein Skript, das diese Datei erzeugt (nachgemessen:
> `grep -rl "FORMAT-CLASSIFICATION" scripts/` ist leer). Sie ist eine
> von Hand erstellte Momentaufnahme und altert wie jede andere. Wer sie
> auffrischen will, muss den Weg erst schreiben.
>
> **Zweitens: die Quelle hatte keinen Aufrufer.**
> `uft_format_registry_v2.c` stand in `docs/orphan_baseline.txt`; seine
> sechs exportierten Funktionen rief niemand. Seit MF-624 ist die Datei
> geloescht und die Tabelle steht als Dokument in
> [`FORMAT_CATALOG.md`](FORMAT_CATALOG.md). Registriert wird über
> `src/formats/format_registry/uft_format_registry.c:434`
> (`uft_register_all_formats()`, gerufen von `src/main.cpp:43`), und
> dessen `all_plugins[]` speist sich aus Gruppen, nicht aus dieser
> Tabelle.
>
> Die 161 beschreiben also einen **Katalog**, nicht das Verhalten des
> Werkzeugs — dieselbe Unterscheidung wie bei den „55+
> Kopierschutz-Schemes" (P0-2). Die Zahl, die das Projekt sonst führt,
> kommt aus der SSOT `scripts/gen_format_list.py`: **88 Plugins**
> ausgeschrieben, 137 mit dem DSK-Makro.

> **Nenner-Klärung (357 vs. 161):** Die Ziel-Vorlage nennt „~357 Formate" — das ist
> die Anzahl `.c`-Quelldateien im Format-Layer, von denen viele Duplikate, Helfer oder
> toter Copy-Paste sind (siehe `docs/KNOWN_ISSUES.md` FMT-1..4, Memory
> `format-layer-fabrication`). Die **klassifizierungsrelevante** Einheit ist die
> distinkte **Format-ID** im Katalog ([`FORMAT_CATALOG.md`](FORMAT_CATALOG.md)): **161**.
> (MF-620: hier stand „in der SSOT-Registry". Das ist sie nicht —
> die Datei hat keinen Aufrufer, und die SSOT des Projekts ist
> `scripts/gen_format_list.py` mit 88 Plugins.) Diese Tabelle
> klassifiziert alle 161. Nicht-registrierte `.c`-Module sind per Definition kein
> exportierbares Format und tragen keine eigene Repräsentationsklasse.


## Zwei orthogonale Achsen

**Achse A — Repräsentationsklasse** (kann das Format Kopierschutz-Artefakte abbilden?):

- **Klasse 1 — Flux** (`data_layer 0`): speichert Roh-Flux-Timings → Weak/Fuzzy-Bits
  und Timing-Anomalien **nativ**. Multi-Revolution möglich. Perfektionierung =
  verlustfreier Flux-Pass-Through + Round-Trip-Test.
- **Klasse 2 — Bitstream** (`data_layer 1`): dekodierter Single-Revolution-Bitstream
  (GCR/MFM). Kann protection-relevante Regionen als **Snapshot** abbilden (Weak-Bit-
  Marker wo die Spec das vorsieht), aber **keine** Multi-Rev-Nichtdeterminismus-Sim.
- **Klasse 3 — Sektor/File** (`data_layer 2/3`): dekodierte Sektordaten. Kann
  Kopierschutz-Artefakte **strukturell nicht** abbilden. Perfektionierung =
  Erkennung + Nutzerwarnung beim verlustbehafteten Export.

**Achse B — Disk-Fehler-Marking** (orthogonal): Hat die Spec native Fehler-Kennzeichnung
(CRC-Fehler, Deleted-Address-Mark, Bad-Sector-Flag)? Manche Klasse-3-Sektor-Formate
(IMD, TD0, D88, EDSK, FDI, D64+Errormap) markieren **Fehler** nativ, obwohl sie
Kopierschutz nicht abbilden. Diese sind „error-aware Klasse 3".

## Scope-Verfeinerungen (gegenüber der Ziel-Vorlage, sofort gemeldet)

1. **HFEv3 → Klasse 2, nicht Klasse 1.** HFE (inkl. v3) ist ein **Bitstream**-Format
   (MFM-Track-Daten), kein Roh-Flux. v3 fügt nur variable Bitrate hinzu (partielles
   Timing), nicht die volle Flux-Timing-Repräsentation. Es steht daher in Klasse 2
   mit dokumentierter Timing-Teilfähigkeit.
2. **TAP** (Commodore, `data_layer 1`) ist ein **Tape-Puls**-Format, kein Disk-
   Bitstream — Rand der Klasse 2, eigene Semantik.
3. **EDD** (`data_layer 0`) ist streng genommen bit-cell-Level (Apple II), zwischen
   Flux und Bitstream; hier als Klasse 1 geführt (nächstliegend, Weak-Bit-fähig).


## Klasse 1 — Flux (12 Formate)

| Format | Ext | Plattform | Kopierschutz-Repräsentation | Disk-Fehler-Marking |
|---|---|---|---|---|
| **A2R** | .a2r | Apple | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **CFI** | .cfi | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **CWTool** | .cwt | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **DFI** | .dfi | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **EDD** | .edd | Preservation | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **GWRAW** | .raw | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **IPF** | .ipf | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **KFRAW** | .raw | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **MFI** | .mfi | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **PFI** | .pfi | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **SCP** | .scp | Flux | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |
| **WOZ** | .woz | Apple | nativ (Flux: Weak/Fuzzy/Timing) | nativ (vollständig) |

## Klasse 2 — Bitstream (21 Formate)

| Format | Ext | Plattform | Kopierschutz-Repräsentation | Disk-Fehler-Marking |
|---|---|---|---|---|
| **86F** | .86f | Flux | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **ATX** | .atx | Atari | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **Aeslanier** | .aes | Aeslanier | Snapshot (bitstream, kein Timing) | partiell |
| **Brother** | .br | Brother | Snapshot (bitstream, kein Timing) | partiell |
| **DMK** | .dmk | TRS-80 | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **DTI** | .dti | Multi | Snapshot (bitstream, kein Timing) | partiell |
| **EDSK** | .dsk | Amstrad | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **G64** | .g64 | Commodore | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **G71** | .g71 | Commodore | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **HFE** | .hfe | Flux | Snapshot (bitstream, kein Timing) | partiell |
| **MFM** | .mfm | Multi | Snapshot (bitstream, kein Timing) | partiell |
| **NBZ** | .nbz | Apple | Snapshot (bitstream, kein Timing) | partiell |
| **NIB** | .nib | Apple | Snapshot (bitstream, kein Timing) | partiell |
| **P64** | .p64 | Commodore | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **PRI** | .pri | Flux | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **PRO** | .pro | Atari | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **STT** | .stt | Atari ST | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **STX** | .stx | Atari ST | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **TAP** | .tap | Commodore | Snapshot (bitstream, kein Timing) | partiell |
| **UDI** | .udi | Spectrum | Snapshot (Weak-Bit-Regionen) | nativ/partiell |
| **Victor9K** | .v9k | Victor | Snapshot (bitstream, kein Timing) | partiell |

## Klasse 3 — Sektor (115 Formate)

| Format | Ext | Plattform | Kopierschutz-Repräsentation | Disk-Fehler-Marking |
|---|---|---|---|---|
| **2MG** | .2mg,2img | Apple | keine (strukturelle Grenze) | keine |
| **ABC800** | .dsk,img | Nordic | keine (strukturelle Grenze) | keine |
| **ADF** | .adf | Amiga | keine (strukturelle Grenze) | keine |
| **ADF_ADL** | .adf,adl | Acorn | keine (strukturelle Grenze) | keine |
| **ADL** | .adl | BBC | keine (strukturelle Grenze) | keine |
| **ADZ** | .adz | Amiga | keine (strukturelle Grenze) | keine |
| **ATR** | .atr | Atari | keine (strukturelle Grenze) | keine |
| **Agat** | .agat | Agat | keine (strukturelle Grenze) | keine |
| **Applix** | .apx,img | Applix | keine (strukturelle Grenze) | keine |
| **BK0010** | .bkd,img | Soviet | keine (strukturelle Grenze) | keine |
| **CPM** | .cpm | CP/M | keine (strukturelle Grenze) | keine |
| **CQM** | .cqm | PC | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **Calcomp** | .cal,img | Calcomp | keine (strukturelle Grenze) | keine |
| **Cromemco** | .cro,img | Cromemco | keine (strukturelle Grenze) | keine |
| **D13** | .d13,dsk | Apple | keine (strukturelle Grenze) | keine |
| **D1M** | .d1m | Commodore | keine (strukturelle Grenze) | keine |
| **D2M** | .d2m | Commodore | keine (strukturelle Grenze) | keine |
| **D4M** | .d4m | Commodore | keine (strukturelle Grenze) | keine |
| **D64** | .d64 | Commodore | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **D67** | .d67 | Commodore | keine (strukturelle Grenze) | keine |
| **D71** | .d71 | Commodore | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **D77** | .d77 | PC-98 | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **D80** | .d80 | Commodore | keine (strukturelle Grenze) | keine |
| **D82** | .d82 | Commodore | keine (strukturelle Grenze) | keine |
| **D88** | .d88,d77,d68 | PC-98 | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **D90** | .d90 | Commodore | keine (strukturelle Grenze) | keine |
| **DART** | .dart | Mac | keine (strukturelle Grenze) | keine |
| **DC42** | .dc42,image | Mac | keine (strukturelle Grenze) | keine |
| **DCM** | .dcm | Atari | keine (strukturelle Grenze) | keine |
| **DCP** | .dcp | Mac | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **DCU** | .dcu | Mac | keine (strukturelle Grenze) | keine |
| **DGNova** | .dg,img | DG | keine (strukturelle Grenze) | keine |
| **DHD** | .dhd | Multi | keine (strukturelle Grenze) | keine |
| **DIM** | .dim | PC-98 | keine (strukturelle Grenze) | keine |
| **DMF** | .dmf | PC | keine (strukturelle Grenze) | keine |
| **DMF_MSX** | .dsk | MSX | keine (strukturelle Grenze) | keine |
| **DMS** | .dms | Amiga | keine (strukturelle Grenze) | keine |
| **DNP** | .dnp | Commodore | keine (strukturelle Grenze) | keine |
| **DNP2** | .dnp2 | Commodore | keine (strukturelle Grenze) | keine |
| **DO** | .do,dsk | Apple | keine (strukturelle Grenze) | keine |
| **DS2** | .ds2 | Multi | keine (strukturelle Grenze) | keine |
| **DSC** | .dsc | Multi | keine (strukturelle Grenze) | keine |
| **DSD** | .dsd | BBC | keine (strukturelle Grenze) | keine |
| **DSK** | .dsk | Amstrad | keine (strukturelle Grenze) | keine |
| **FB100** | .fb | FB | keine (strukturelle Grenze) | keine |
| **FDD** | .fdd | PC-98 | keine (strukturelle Grenze) | keine |
| **FDI** | .fdi | Multi | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **FDS** | .fds | NES | keine (strukturelle Grenze) | keine |
| **FDX** | .fdx | PC-98 | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **FLEX** | .dsk | 6809 | keine (strukturelle Grenze) | keine |
| **HDM** | .hdm | PC-98 | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **Heathkit** | .h8d,h17,img | Heathkit | keine (strukturelle Grenze) | keine |
| **HitachiS1** | .s1,img | Hitachi | keine (strukturelle Grenze) | keine |
| **IMA** | .ima | PC | keine (strukturelle Grenze) | keine |
| **IMD** | .imd | PC | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **IMG** | .img,ima,flp | PC | keine (strukturelle Grenze) | keine |
| **IMZ** | .imz | PC | keine (strukturelle Grenze) | keine |
| **JV1** | .jv1 | TRS-80 | keine (strukturelle Grenze) | keine |
| **JV3** | .jv3 | TRS-80 | keine (strukturelle Grenze) | keine |
| **JVC** | .jvc,dsk | TRS-80 | keine (strukturelle Grenze) | keine |
| **LDBS** | .ldbs | Multi | keine (strukturelle Grenze) | keine |
| **LIF** | .lif | HP | keine (strukturelle Grenze) | keine |
| **MAC_DSK** | .image,img | Apple | keine (strukturelle Grenze) | keine |
| **MBD** | .mbd | Multi | keine (strukturelle Grenze) | keine |
| **MGT** | .mgt | SAM | keine (strukturelle Grenze) | keine |
| **MSA** | .msa | Atari ST | keine (strukturelle Grenze) | keine |
| **MS_DMF** | .dmf | PC | keine (strukturelle Grenze) | keine |
| **Meritum** | .mrt,img | Meritum | keine (strukturelle Grenze) | keine |
| **Micropolis** | .mpo | Micropolis | keine (strukturelle Grenze) | keine |
| **NFD** | .nfd | PC-98 | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **NorthStar** | .nsi | NorthStar | keine (strukturelle Grenze) | keine |
| **OPD** | .opd,opu | Spectrum | keine (strukturelle Grenze) | keine |
| **ORIC_DSK** | .dsk | Oric | keine (strukturelle Grenze) | keine |
| **OSD** | .osd | OS-9 | keine (strukturelle Grenze) | keine |
| **PC99** | .dsk | TI-99 | keine (strukturelle Grenze) | keine |
| **PDI** | .pdi | Multi | keine (strukturelle Grenze) | keine |
| **PMC** | .pmc,img | PMC | keine (strukturelle Grenze) | keine |
| **PO** | .po | Apple | keine (strukturelle Grenze) | keine |
| **PSI** | .psi | Flux | keine (strukturelle Grenze) | keine |
| **Pravetz** | .prv,img | Pravetz | keine (strukturelle Grenze) | keine |
| **Prime** | .prm,img | Prime | keine (strukturelle Grenze) | keine |
| **Pyldin** | .pyl,img | Pyldin | keine (strukturelle Grenze) | keine |
| **QDOS** | .ql,mdv | QL | keine (strukturelle Grenze) | keine |
| **RAW** | .raw,bin | Multi | keine (strukturelle Grenze) | keine |
| **RC759** | .rc7,img | RC759 | keine (strukturelle Grenze) | keine |
| **RX50** | .img,dsk | DEC | keine (strukturelle Grenze) | keine |
| **Robotron** | .kcc,img | Robotron | keine (strukturelle Grenze) | keine |
| **RolandD20** | .d20 | Roland | keine (strukturelle Grenze) | keine |
| **S24** | .s24 | Multi | keine (strukturelle Grenze) | keine |
| **SAD** | .sad | SAM | keine (strukturelle Grenze) | keine |
| **SAP** | .sap | Thomson | keine (strukturelle Grenze) | keine |
| **SBT** | .sbt | Multi | keine (strukturelle Grenze) | keine |
| **SDF** | .sdf | SAM | keine (strukturelle Grenze) | keine |
| **SSD** | .ssd | BBC | keine (strukturelle Grenze) | keine |
| **ST** | .st | Atari ST | keine (strukturelle Grenze) | keine |
| **STZ** | .stz | Atari ST | keine (strukturelle Grenze) | keine |
| **SanyoMBC** | .mbc,img | Sanyo | keine (strukturelle Grenze) | keine |
| **SharpX1** | .2d,img | SharpX1 | keine (strukturelle Grenze) | keine |
| **Smaky6** | .smk | Smaky | keine (strukturelle Grenze) | keine |
| **TD0** | .td0 | PC | keine (Sektor) — aber native Fehler-Marks | nativ (CRC/Deleted/Bad-Sector) |
| **TIDS990** | .ti | TI | keine (strukturelle Grenze) | keine |
| **TRD** | .trd | Spectrum | keine (strukturelle Grenze) | keine |
| **Tartu** | .tar | Tartu | keine (strukturelle Grenze) | keine |
| **Trinity** | .trin | Spectrum | keine (strukturelle Grenze) | keine |
| **UNIFLEX** | .dsk | 6809 | keine (strukturelle Grenze) | keine |
| **V9T9** | .dsk | TI-99 | keine (strukturelle Grenze) | keine |
| **VDK** | .vdk | TRS-80 | keine (strukturelle Grenze) | keine |
| **VFD** | .vfd | PC | keine (strukturelle Grenze) | keine |
| **VersaDOS** | .vdo,img | Motorola | keine (strukturelle Grenze) | keine |
| **X64** | .x64 | Commodore | keine (strukturelle Grenze) | keine |
| **X71** | .x71 | Commodore | keine (strukturelle Grenze) | keine |
| **X81** | .x81 | Commodore | keine (strukturelle Grenze) | keine |
| **XDF** | .xdf | Atari | keine (strukturelle Grenze) | keine |
| **XFD** | .xfd | Atari | keine (strukturelle Grenze) | keine |
| **ZilogMCZ** | .mcz | Zilog | keine (strukturelle Grenze) | keine |

## Klasse 3 — File/Archiv (13 Formate)

| Format | Ext | Plattform | Kopierschutz-Repräsentation | Disk-Fehler-Marking |
|---|---|---|---|---|
| **CRT** | .crt | Commodore | keine (strukturelle Grenze) | keine |
| **FIAD** | .tfi | TI-99 | keine (strukturelle Grenze) | keine |
| **LNX** | .lnx | Lynx | keine (strukturelle Grenze) | keine |
| **M2I** | .m2i | Commodore | keine (strukturelle Grenze) | keine |
| **P00** | .p00,p01,p02 | Commodore | keine (strukturelle Grenze) | keine |
| **PRG** | .prg | Commodore | keine (strukturelle Grenze) | keine |
| **SCL** | .scl | Spectrum | keine (strukturelle Grenze) | keine |
| **T64** | .t64 | Commodore | keine (strukturelle Grenze) | keine |
| **TAP** | .tap | Multi | keine (strukturelle Grenze) | keine |
| **TIFILES** | .tifiles | TI-99 | keine (strukturelle Grenze) | keine |
| **TZX** | .tzx | Spectrum | keine (strukturelle Grenze) | keine |
| **UEF** | .uef | BBC | keine (strukturelle Grenze) | keine |
| **XEX** | .xex | Atari | keine (strukturelle Grenze) | keine |

---

## Disk-Fehler-Audit — verifizierter Ist-Zustand (2026-07-05)

Die generische Spalte „Disk-Fehler-Marking" oben leitet sich aus der Formatklasse
ab. Der tatsächliche **Code-Audit** (jedes error-aware Format gegen die publizierte
Spec verifiziert, mit Test) ergab folgenden konkreten Stand — maßgeblich ist diese
Tabelle, Details in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) FMT-9:

| Format | Fehler-Marks | Audit-Ergebnis | Test |
|---|---|---|---|
| **IMD** | CRC, Deleted | read+represent+**preserve-on-write** | `test_imd_error_marks` |
| **EDSK** | uPD765 ST1/ST2 (CRC, Deleted) | read+represent+**preserve** | `test_edsk_error_marks` |
| **D88** | DDAM, FDC-Status | read+represent+**preserve** — **Offset-Bug gefixt** (+0D→+07/+08) | `test_d88_error_marks` |
| **D64** | Errormap (1541-Codes) | read+represent — **Drop-Bug gefixt** (Trailer wurde verworfen); Write-Erhalt offen (L) | `test_d64_errormap` |
| **TD0** | CRC, Deleted (dtype) | read+represent (read-only) — **Scan-Off-by-one gefixt** | `test_td0_error_marks` |
| **STX** | FDC-Status (CRC, Deleted) | read+represent (read-only) — **Descriptor-Offset-Bug gefixt** (+08/+0C→+0A/+0E) | `test_stx_error_marks` |
| **ATX** | FDC-Status + Weak-Bits | auditiert **spec-korrekt**, kein Bug | (verifiziert) |
| **DMK** | Deleted (DAM 0xF8) | Deleted read+represent; CRC = kein explizites Flag → Berechnung nötig (deferred) | — |
| **FDI (generisch)** | `flags`-Byte unklar | **Fabrikations-Verdacht** — Format matcht keine reale Spec, Flag-Semantik unverifizierbar; nicht gemappt | — |
| **FDI (Anex86)** | keine | roher C/H/S-Dump = strukturelle Grenze (korrekt) | — |
| **NFD R0/R1** | DDAM, Status, ST0-2 | **Reader strukturell falsch** vs. pc98.org-Spec (per-Track statt per-Sektor, erfundene Offsets) → L-Rewrite + Korpus nötig; nicht blind gefixt | — |

**Audit-Bilanz:** 4 echte forensische Bugs gefunden+gefixt (D64/TD0/STX/D88, alle
spec-web-verifiziert + getestet), 2 tiefere Fabrikations-Struktur-Defekte gefunden
+ ehrlich dokumentiert (FDI/NFD — bewusst **nicht** blind gefixt, da Byte-Exaktheit
ein Ground-Truth-Korpus braucht). Muster: **Test-Bau = Audit**; Offset/Descriptor-
Reader müssen gegen die publizierte Spec verifiziert werden (deckt sich mit den
FMT-1..4-Fabrikations-Funden).

---

## Phase-0 — Geometrie-Scan (Voraussetzungsfeld)

Protection-/Fehler-Klassifizierung ist nur zuverlässig, wenn die Geometrie
(Tracks, Sides, Sektoren/Track, Sektorgröße) korrekt erkannt wird — ein falscher
Sektor-Count korrumpiert jede nachgelagerte Mark. Der Audit ergab **drei
Geometrie-Typen**:

| Typ | Sektoren/Track | Sektorgröße | Beispiele | `uft_geometry_t.sectors` |
|---|---|---|---|---|
| **Uniform** | konstant | konstant | IMG, ADF, ATR, DO/PO, DC42 | exakt |
| **Zoned (GCR)** | variabel pro Zone | konstant | D64/D71 (21/19/18/17), G64/G71 (Speed-Zones), D81 | = **Maximum** (Summary) |
| **Per-Track variabel** | variabel pro Track | ggf. variabel | IMD, EDSK, TD0, D88, CQM | = **Maximum** (Summary) |

**Struktur-Grenze (getrackt, FMT-9):** `uft_geometry_t` (`include/uft/uft_types.h`)
ist **uniform** — ein einzelnes `sectors` + `sector_size`. Für Zoned- und
Per-Track-variable-Formate meldet es das **Maximum** als Summary, kann das echte
per-Track-Layout aber nicht ausdrücken.

**Wichtig — die READS sind trotzdem korrekt:** zoned/variable Plugins lesen die
echte per-Track-Sektorzahl aus ihren internen Tabellen (D64 `d64_spt[]`, IMD/EDSK
per-Track-Header, G64 Speed-Zone-Map), nicht aus `geometry.sectors`. Verifiziert:
`test_d64_geometry_zones` beweist D64 liest 21/19/18/17 pro Zone (Total 683) trotz
uniform `geometry.sectors = 21`. Die uniforme Struct ist also eine **Summary-
Vereinfachung**, kein Read-Bug.

**Offen (FMT-9, L-Aufwand + ABI):** ein zoned/non-uniform Geometrie-Typ
(per-Track-Sektor-Count + per-Track-Größe) in `uft_geometry_t`. Public-Struct-
Änderung → ABI-relevant, viele Consumer. Bis dahin bleibt `geometry.sectors` das
Maximum-Summary; per-Track-Wahrheit liegt in den Plugin-Tabellen.

---

## Phase-0 — Geometrie-Typ pro Format (skaliert auf alle 161 IDs)

Familien-basierte Heuristik (Phase-0), verifiziert am D64-Zoned-Beispiel (`test_d64_geometry_zones`). Verteilung: **Uniform 108 · Per-Track-variabel 32 · Zoned-GCR 9 · Flux-N/A 12**.

> **Scope-Korrektur (2026-07-05, sofort gemeldet):** Apple-II-5.25"-Formate (DO/PO/NIB/D13/2MG/Agat) sind **Uniform** (16 Sektoren DOS 3.3, 13 bei D13), NICHT Zoned — nur CBM-GCR und Mac-3.5"-GCR haben echte Speed-Zonen. D1M/D2M/D4M (CMD FD, MFM) und D90 (HD) sind ebenfalls nicht GCR-zoned.

**Zoned-GCR (9)** — variable Sektoren pro Speed-Zone, konstante Sektorgröße; Read via Zonen-Tabelle (CBM 21/19/18/17, Mac 3.5", Victor):
  D64, D67, D71, D80, D82, DC42, G64, G71, Victor9K

**Per-Track-variabel (32)** — per-Track-Deskriptoren (Sektorzahl/-größe variiert pro Track); Read via per-Track-Header:
  86F, ATX, Aeslanier, Brother, CQM, D77, D88, DCP, DIM, DMK, DTI, EDSK, FDI, FDX, HDM, HFE, IMD, JV3, MFM, NBZ, NFD, NIB, P64, PRI, PRO, SAP, STT, STX, TAP, TAP, TD0, UDI

**Flux-N/A (12)** — Roh-Flux, keine Sektor-Geometrie bis zum Decode:
  A2R, CFI, CWTool, DFI, EDD, GWRAW, IPF, KFRAW, MFI, PFI, SCP, WOZ

**Uniform (108)** — konstante Sektorzahl + -größe; `uft_geometry_t` ist hier exakt. Umfasst IMG/ADF/ATR/DO/PO/NIB (Apple-II 5.25")/CP-M-Familie u.a.

_Grundlage für Phase 4: Format-Varianten unterscheiden sich oft in der Geometrie (D64 35↔40 Track, ADF DD↔HD, D88 2D↔2DD↔2HD) — diese Typ-Spalte ist die technische Basis der Varianten-Recherche._

