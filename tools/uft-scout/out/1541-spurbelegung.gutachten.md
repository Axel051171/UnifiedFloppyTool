# Gutachten: Physische Sektorreihenfolge einer 1541-Spur (P3-111)

Datum: 2026-09-04 · Agent: uft-scout · Zyklus-Anlass: gezielter Auftrag
des Eigentümers zu `docs/OPEN_ITEMS.md` P3-111 (MF-859)

## TL;DR — die Antwort

**Ein Commodore 1541 legt die Sektoren einer Spur beim Formatieren
physisch AUFSTEIGEND ab: 0, 1, 2, …, n−1. Es gibt keinen physischen
Interleave.** Der Versatz 10 (Directory: 3) lebt ausschließlich in der
**Blockvergabe** des DOS (`FNDNXT`), genau wie der Eigentümer vermutet
hat. UFTs `d64_write_track_gcr()` interleavt damit die falsche Ebene —
ein damit erzeugtes Spurbild hat eine Anordnung, die kein 1541 je
geschrieben hat.

Belegt mit **fünf unabhängigen Code-Händen plus einer eigenen Messung**
an einem fremd erzeugten G64-Abbild. Die stärkste Hand ist die
1541-ROM-Formatier-Routine selbst.

Die **zweite Frage** aus P3-111 ist ebenfalls beantwortet: die echte
1541-DOS-Vergaberegel ist die **„beim Überlauf zusätzlich minus
eins"-Regel** (lib1541img-Vorgabe `freeSectorOnTrack`), **nicht** die
modulare. Für 21 Sektoren: `0,10,20,8,18,6,…`. UFTs vorhandene Regel
(modular) entspricht `CFF_SIMPLEINTERLEAVE` und damit nicht dem DOS —
wobei sie für das physische Spurbild ohnehin an der falschen Stelle
steht.

Kategorie: **Verbesserung** (Berichtigung einer belegten Falschaussage
im eigenen Baum; Verifikationsarbeit an einem vorhandenen Format —
EINFRIER-REGEL-konform, kein neues Plugin).

---

## 1. Frage 1: physische Spurbelegung — AUFSTEIGEND

### Hand 1 (autoritativ): die 1541-ROM-Formatier-Routine

Quelle: kommentierte Disassemblierung des 1541-ROM **901229-01**,
digitalisiert nach „Inside Commodore DOS" (Richard Immers / Gerald G.
Neufeld, 1984), `https://g3sl.github.io/c1541rom.html`, abgerufen
2026-09-04 **wörtlich per curl** (1 485 832 Bytes), nicht über eine
zusammenfassende Zwischenschicht.

Die Formatier-Routine (Job-Code $F0) baut erst alle Sektor-Header
sequenziell in einen Puffer und schreibt sie dann in Puffer-Reihenfolge
auf die Spur:

| ROM-Adresse | Code | Kommentar des Listings (wörtlich) |
|---|---|---|
| $FC36–$FC38 | `LDA #$00 / STA $0628` | „Set sector counter SECT ($0628) to $00." |
| $FC3F (MAK10) | `LDA $39 / STA $0300,Y` … | „Loop to create sector header images in buffer ($0300+)" |
| $FC46–$FC49 | `LDA $0628 / STA $0300,Y` | Sektornummer = SECT in den Header |
| $FC7A–$FC82 | `INC $0628 / LDA $0628 / CMP $43 / BCC $FC3F` | „Increment SECT … compare it to number of sectors on track SECTR" |
| $FCAA–$FCAC | `LDA #$00 / STA $32` | „Set the pointer to the header GCR image HDRPNT ($32) to $00 so it points to the start of the first header image." |
| $FCB1 (WRTSYN) | Sync, dann 10 Header-Bytes ab `$0300,Y` mit `Y = HDRPNT` | Schreibschleife je Sektor |
| $FD12–$FD17 | `LDA $32 / CLC / ADC #$0A / STA $32` | „Advance the header pointer HDRPNT by 10 so it points to the start of the next header image." |
| $FD19–$FD1C | `DEC $0628 / BNE $FCB1` | nächster Sektor, bis SECT = 0 |

Puffer-Reihenfolge = 0,1,2,…,SECTR−1; die Schreibschleife rückt den
Header-Zeiger **linear** um 10 weiter. **Physische Reihenfolge =
aufsteigend.** Kein Interleave-Code existiert in diesem Pfad.

### Hand 2: OpenCBM `cbmformat` (läuft auf echter 1541-Hardware)

`tools/uft-scout/work/OpenCBM/opencbm/cbmformat/cbmformat.a65`:

* Z. 293–294: `ldy #$00 / sty SECTOR2` — „start with sector 0"
* Z. 296–331 (`PrepSec`): Header sequenziell in BUFFER0, `lda SECTOR2 /
  sta BUFFER0+2,y`, dann `inc SECTOR2 / cmp SECTR / bcc PrepSec`
* Z. 343–384 (`Restart`/`NxtSec`): `HDRPNT` startet bei 0, je Sektor 10
  Header-Bytes ab `BUFFER0,y`, `sty HDRPNT` rückt linear weiter,
  `dec SECTOR2 / bne NxtSec`

Gleiche Struktur wie das ROM, auf realer Hardware im Einsatz:
**aufsteigend**.

### Hand 3: VICE

`vice/src/diskimage/fsimage-dxx.c`, Funktion
`fsimage_read_dxx_image()` (Zählung der gefetchten Datei von
`raw.githubusercontent.com/VICE-Team/svn-mirror/main`, 2026-09-04):

* Z. 262: `for (sector = 0; sector < max_sector; sector++)`
* Z. 278–279: `header.sector = sector; gcr_convert_sector_to_GCR(...)`
* Z. 283: `ptr += SECTOR_GCR_SIZE_WITH_HEADER + headergap + gap + (synclen * 2);`

Aufsteigend, Zielzeiger linear. Der anschließende Block (Z. 291 ff.)
wendet ausdrücklich nur **Spur-zu-Spur-Skew** an (Rotationsversatz der
ganzen Spur beim Stepping) — der Kommentar benennt das als das einzige
physische Reihenfolge-Phänomen echter Disketten und grenzt es vom
Spur-Innern ab.

### Hand 4: nibtools

`tools/uft-scout/work/nibtools/fileio.c::read_d64` (Z. 693):

* Z. 760: `for (sector = 0; sector < sector_map[track]; sector++)`
* Z. 778: `convert_sector_to_GCR(buffer, gcrdata + (sector * (SECTOR_SIZE + sector_gap_length[track])), track, sector, id, error);`

Zielposition ist eine **lineare Funktion der Sektornummer** —
aufsteigend. nibtools ist das Referenzwerkzeug der
C64-Preservation-Szene; seine D64→GCR-Rekonstruktion ist gegen
Nibbler-Dumps echter Disketten gebaut.

### Hand 5: 1541 Ultimate (die Anlaufstelle des Eigentümers)

Der eigentliche Quelltext liegt in `GideonZ/1541ultimate` (nicht in den
beiden genannten Adressen, siehe §5).
`software/drive/disk_image.cc::convert_track_bin2gcr()` (gefetcht von
`raw.githubusercontent.com/GideonZ/1541ultimate/master`, 2026-09-04):

* Z. 251: `for(uint8_t s=0;s<sectors_per_track[region];s++) {`
* Z. 267: `header[2] = (uint8_t)s;` (Sektornummer in den Header)
* GCR-Zielzeiger `gcr` läuft über die ganze Schleife linear weiter
  (Z. 287–293, 329–331)

Die FPGA-Nachbildung, die D64-Abbilder für ein emuliertes Laufwerk in
GCR wandeln **muss**, wählt: **aufsteigend**.

### Messung: fremd erzeugtes G64-Abbild

`tools/uft-scout/work/cbm_fixtures/fdit_uft35.g64`
(SHA-256 `5d01ddf3…` laut `SHA256SUMS.txt` daneben). Eigener
GCR-Header-Parser (bitgenau: Sync = ≥10 Eins-Bits, 5:4-GCR-Decode,
Blockkennung $08, Sektornummer = Header-Byte 2; Skript im Scratchpad,
Wegwerf-Messung):

* **alle 35 Spuren: 0,1,2,…,n−1** mit n = 21/19/18/17 je Zone —
  vollständig aufsteigend, kein Interleave.

Methode der Zahl „35 Spuren aufsteigend": Menge 1 = dekodierte
Header-Sektornummern je Spur in Fund-Reihenfolge; Menge 2 = die Folge
0…n−1; Treffer = exakte Folgengleichheit (der doppelte End-0-Eintrag
ist der Umlauf des verdoppelten Bitstrings). 35/35 Spuren identisch.

**Einschränkung:** die Herkunft von `fdit_uft35.g64` ist in
`SHA256SUMS.txt` nicht notiert (vermutlich ein früherer
DiskImageTool-Zubringer, „fdit"). Die Messung belegt daher, *was ein
weiteres fremdes Werkzeug erzeugt*, nicht Referenzhardware — sie ist
die sechste Hand, nicht die erste.

### Zwischenfazit Frage 1

Fünf unabhängige Codebasen (ROM, OpenCBM, VICE, nibtools,
1541ultimate) und ein gemessenes Fremd-Artefakt sagen dasselbe. Die
Zwei-Hände-Anforderung des Auftrags ist übererfüllt; die Hände sind
nicht voneinander abgeschrieben (6502-Assembler ≠ C ≠ C++, drei davon
bauen Spuren für unterschiedliche Zwecke).

---

## 2. Frage 2: welche Vergaberegel hat das echte DOS?

ROM-Routine `FNDNXT` ($F173), dieselbe Quelle wie Hand 1 — das ist die
Routine hinter dem `tstfnd.src::fndnxt`-Kommentar des Eigentümers:

| ROM-Adresse | Code | Wirkung |
|---|---|---|
| $F173–$F178 | `LDA $81 / CLC / ADC $69 / STA $81` | Sektor += SECINC |
| $F17A–$F187 | `JSR $F24B (MAXSEC)` … `CMP $81 / BCS $F195` | kein Überlauf → fertig |
| $F189–$F18F | `SEC / LDA $81 / SBC $024E / STA $81` | Überlauf: Sektorzahl abziehen |
| $F191–$F193 | `BEQ $F195 / DEC $81` | **und wenn ≠ 0: zusätzlich −1** |

SECINC-Werte, ebenfalls aus dem ROM:

* $EBCD–$EBCF (Reset/DSKINT): `LDA #$0A / STA $69` — „the normal next
  sector increment" = **10**
* $D497–$D499 (NXDRBK, die `nxdrbk`-Stelle des Eigentümers):
  `LDA #$03 / STA $69` — „the increment used for the directory track"
  = **3**, danach wieder zurückgestellt ($D49E–$D49F)

Das ist **byte-für-byte die lib1541img-Vorgabe**
(`tools/uft-scout/work/lib1541img/src/lib/1541img/cbmdosfs.c:126–134`):

```c
nextsect = sectno + interlv;
if (nextsect >= sectors)
{
    nextsect -= sectors;
    if (nextsect) --nextsect;
}
```

Damit zwei unabhängige Hände auch für Frage 2 (ROM + lib1541img).
Ergebnis für 21 Sektoren: `0,10,20,8,18,6,…`. Die modulare Regel
(`CFF_SIMPLEINTERLEAVE`, und UFTs `uft_d64_sektor_an_position()` mit
`s = (s + versatz) % n`) weicht ab dem vierten vergebenen Block ab.

**Einordnung:** diese Regel gehört zur **Dateiblock-Vergabe** (BAM),
nicht zum Spurbild. Für `d64_write_track_gcr()` ist keine der beiden
Varianten richtig — dort gehört gar kein Interleave hin. Relevant wird
die Regel nur, falls UFT je CBM-DOS-Blockvergabe nachbildet (z. B. ein
D64-Dateisystem-Schreiber).

---

## 3. Was das für UFT heißt (Lagebild, gemessen)

* `src/formats/uft_d64_writer.c::d64_write_track_gcr()` (Z. 302) ordnet
  über `uft_d64_sektor_an_position()` (Z. 76) physisch interleavt an —
  **nach obiger Beweislage eine Anordnung, die kein 1541 erzeugt.**
* **Aber: der Erzeuger ist heute eine Tür ohne Leser.** Je Symbol über
  den Baum gegrept (2026-09-04): `d64_write_track_gcr` und
  `d64_writer_write` haben außerhalb des eigenen Moduls **nur** ihren
  Header-Prototyp; `uft_d64_sektor_an_position` hat als einzigen
  Aufrufer seinen eigenen Rotbeweis-Test
  `tests/test_d64_writer_sektorfolge.c`. Aus dem Modul erreichbar ist
  allein `d64_gcr_to_flux` (gerufen in
  `src/formats/uft_format_convert_bitstream.c:314`) — und dessen
  Eingabespuren kommen aus einer G64 (`g64_get_track`, Z. 301), nicht
  aus `d64_write_track_gcr`.
* **Der angebotene Wandlungspfad D64→G64 ist bereits korrekt:** er
  läuft über `src/formats/c64/uft_d64_g64.c::build_gcr_track()` mit
  `for (int s = 0; s < num_sectors; s++)` (Z. 920) — aufsteigend, wie
  die Referenz. Der von MF-532/533 gemessene Pfad ist von diesem
  Befund **nicht** betroffen.
* **Der eigentliche Schaden liegt im Test:**
  `tests/test_d64_writer_sektorfolge.c` (MF-859) schreibt die
  Vergabefolge als **physisches Soll** fest. Ein Rotbeweis, der das
  Falsche beweist, ist gefährlicher als gar keiner — die nächste Hand,
  die den Writer verdrahtet, hält die interleavte Ordnung für belegt.
* Nebenbefund: es gibt **zwei** GCR-Spur-Erzeuger im Baum
  (`build_gcr_track` korrekt, `d64_write_track_gcr` falsch) — dieselbe
  Doppelungsklasse, die MF-695/701 beim Speicher-/Dateiweg beseitigt
  hat.

## 4. Inventar-Abfrage (zitiert)

```json
"d64": { "vorhanden": true, "abgedeckt": true, "treffer": ["d64"],
         "tier": "T1b", "plugin_liste_vollstaendig": true }
"g64": { "vorhanden": true, "abgedeckt": true, "treffer": ["g64"],
         "tier": "T1",  "plugin_liste_vollstaendig": true }
```

Beide Formate vorhanden — dieses Gutachten schlägt nichts Neues vor,
es belegt die Referenz für Bestehendes (Verifikationsarbeit, von der
EINFRIER-REGEL ausdrücklich erlaubt).

## 5. Die Anlaufstellen des Eigentümers — Ergebnis wie angekündigt

* `GideonZ/ultimate_releases`: per GitHub-contents-API gelistet —
  **nur** ZIP-Binaries (`ultimate_3.x.zip`, `u64_*.zip`, …) und
  Changelog-Texte. Kein Quelltext. Der Erwartungsdämpfer traf zu.
* `ar.c64.org/wiki/1541uII-quickref.txt`: MediaWiki-Handbuchseite
  (Benutzer-Referenz); zur Spurbelegung wortlos.
* Die lokale ZIP beim Eigentümer wurde **nicht benötigt**: der
  1541ultimate-Quelltext in `GideonZ/1541ultimate` beantwortet die
  Frage (Hand 5).

## 6. Lizenzurteile (Bericht je Quelle; Einordnung beim Eigentümer, MF-679)

Alle Quellen wurden ausschließlich als **Verhaltens-Beleg** genutzt —
kein Code übernommen, keine Übernahme vorgeschlagen.

| Quelle | Lizenzbeleg (aus der Datei gelesen) | Zone / Konsequenz |
|---|---|---|
| 1541-ROM 901229-01 + „Inside Commodore DOS" (g3sl.github.io) | keine Lizenzdatei; Buch © 1984 Immers/Neufeld, ROM © Commodore | alle Rechte vorbehalten → **nur Zitat/Spec-Kanal** (hier genutzt) |
| OpenCBM (`work/OpenCBM`) | `opencbm/COPYING` = GPL-2.0; **aber** `cbmformat.a65` trägt einen eigenen BSD-artigen Header (Redistributions-Klauseln) | Datei-Header ≠ Repo-Lizenz → **Grenzfall, Vorlage** falls je mehr als Zitat gewollt |
| VICE `fsimage-dxx.c` | Datei-Header: GPL „either version 2 … or (at your option) any later version" | GPL-2.0-or-later; als einzige der fünf formal GPL-2.0-verträglich — für diesen Zweck irrelevant, nur Beleg |
| nibtools (`work/nibtools`) | `LICENSE` = GPL-3.0 | **SPERR** für Port ins GPL-2.0-Projekt; Verhaltens-Beleg zulässig |
| 1541ultimate | `LICENSE.txt` = GPL-3.0 | **SPERR** für Port; Verhaltens-Beleg zulässig |
| lib1541img (`work/lib1541img`) | `LICENSE.txt` = BSD-2-Clause (Copyright „Excess", 2 Klauseln, keine Namensklausel) | verträglich; bereits als Klon vorhanden |

## 7. Oracle-Kandidat und Beschaffung

* **Oracle für die physische Ordnung:** VICE `c1541`/Attach-Pfad oder
  nibtools `nibconv` (D64→G64) — beide erzeugen die Referenzordnung.
  nibtools liegt bereits als Klon; keine Beschaffung nötig.
* **Fixture:** `cbm_fixtures/fdit_uft35.g64` liegt und ist vermessen
  (aufsteigend, 35/35). Gegen `inv["korpus"]` geprüft: nichts weiter
  anzufordern. Wünschenswert, aber nicht blockierend: eine
  Herkunftszeile für `fdit_uft35.g64` in `SHA256SUMS.txt`.
* Ein G64 von einer **real formatierten** Diskette (Community, MF-310)
  bliebe die einzige Steigerung auf T1 — für die Beantwortung von
  P3-111 nicht mehr erforderlich.

## 8. OPEN_ITEMS-Vorschlag (1 von max. 5 — mehr braucht es nicht)

> **P3-111 SCHLIESSEN — beide Fragen sind beantwortet.**
> (1) Physische Spurbelegung: **aufsteigend 0…n−1**, belegt durch
> 1541-ROM $FC36–$FD1C (MAK10/WRTSYN), OpenCBM `cbmformat.a65`
> PrepSec/NxtSec, VICE `fsimage-dxx.c:262`, nibtools `fileio.c:760`,
> 1541ultimate `disk_image.cc:251`, plus Messung `fdit_uft35.g64`
> (35/35 Spuren aufsteigend). Der Versatz 10/3 ist **Blockvergabe**
> (ROM FNDNXT $F173, SECINC $EBCD=10 / NXDRBK $D497=3).
> (2) Vergaberegel: die echte DOS-Regel ist **„Überlauf: −Sektorzahl,
> dann −1 falls ≠0"** (ROM $F189–$F193 = lib1541img
> `cbmdosfs.c:126–134`), nicht die modulare.
> **Stufe-4-Arbeit daraus:** `d64_write_track_gcr()` auf aufsteigende
> physische Ordnung stellen und `tests/test_d64_writer_sektorfolge.c`
> umdrehen — der Test beweist derzeit eine Ordnung, die kein 1541
> erzeugt, als Soll. `uft_d64_sektor_an_position()` ist als
> Vergabe-Helfer umzuwidmen oder zu entfernen; falls er bleibt, gehört
> die Überlauf-Regel auf die DOS-Variante. Referenz in die Header
> (EINFRIER-REGEL (c)). Nebenbefund für den Innendienst: zweiter
> GCR-Erzeuger neben `build_gcr_track` (Doppelungsklasse MF-695).
> **Kennzahl:** keine der vier direkt — der angebotene Pfad D64→G64
> ist korrekt und `d64_write_track_gcr` hat heute keinen Aufrufer. Die
> Begründung ist dieselbe Klasse wie P3-20: eine belegte **stille
> Falschaussage** (fixierter Rotbeweis mit falschem Soll) in einem
> Modul, das auf Verdrahtung wartet. Aufwandsklasse **S** (eine Datei
> + ein Test; alle Referenzzitate liegen in diesem Gutachten).

## 9. UNGEKLÄRT

* **Herkunft von `fdit_uft35.g64`** — `SHA256SUMS.txt` trägt keine
  Herkunftszeile; die Messung zählt darum als sechste Hand, nicht als
  Referenz.
* **Original-DOS-Quelltext (`tstfnd.src`/`tst4.src`)**: der Klon von
  `mist64/cbmsrc` scheiterte am Windows-Checkout (Dateinamen); nicht
  weiterverfolgt, weil das ROM-Listing dieselben Routinen adressgenau
  abdeckt. Die Zuordnung Kommentar↔ROM ($D48D NXDRBK, $F173 FNDNXT)
  ist über die Label-Namen des Listings belegt, nicht über die
  .src-Dateien selbst.
* **Spur-zu-Spur-Skew**: real existent (VICE modelliert ihn,
  Kommentar Z. 291 ff. nennt ihn ausdrücklich „somewhat arbitrary")
  — für P3-111 ohne Belang, aber falls UFT je „hardware-echte" G64
  erzeugen will, ist Skew die einzige legitime physische
  Reihenfolge-Stellschraube, nicht Intra-Spur-Interleave.
* **Ob UFT irgendwo CBM-DOS-Blockvergabe nachbildet** (dann würde
  Frage 2 kodierend relevant): im Rahmen dieses Auftrags nicht
  vermessen.
