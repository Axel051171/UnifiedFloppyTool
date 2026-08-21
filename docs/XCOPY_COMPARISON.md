# X-Copy — Quellenvergleich, Nachtrag 2026-08-21

**Dieses Dokument ist ein Delta.** Zwei ältere Dateien decken das Thema bereits
ab und werden hier nicht wiederholt:

| Dokument | Inhalt | Stand |
|---|---|---|
| [`XCOPY_ALGORITHM_MIGRATION.md`](XCOPY_ALGORITHM_MIGRATION.md) | 7 portierbare Algorithmen (Single-Pass-Decode, Multi-Pattern-Sync, GAP-Histogramm, `getracklen`, `clockbits`, syncpos-Tabelle, 2-Rev-Index-Capture) | 2026-04-24 |
| [`XCOPY_INTEGRATION_TODO.md`](XCOPY_INTEGRATION_TODO.md) | T1–T8 Integrationsplan; Statusabgleich MF-421: alles erledigt außer T3-Rest | 2026-04-23 / 2026-08-18 |

Bei einem erneuten Durchgang durch `xcopy_src/xcop.s`, `xio.s`, `xcopy.i` und
die `xvs.library`-Autodocs kamen **drei Fehler in unserem eigenen Code** heraus,
die in keinem der beiden Dokumente stehen. Sie sind unten belegt.

---

## Lizenzlage — offener Punkt aus T1

`XCOPY_INTEGRATION_TODO.md` T1 hält fest: Virus-Signatur-Schema steht mit 48
Einträgen, **die Byte-Pattern selbst sind PENDING und bräuchten eine
xvs.library-Extraktion.**

Dazu aus der erneuten Durchsicht: `xvslibrary/xvs/Developer/C/clib/xvs_protos.h`
trägt

```
Copyright © 2001 Georg Hörmann and Dirk Stöcker
All Rights Reserved
```

und im gesamten Archiv liegt kein Lizenztext, der eine Weiterverwendung
erlaubt. Auch `xcop.s:4049 virustab` — 110 `(Offset, 32-Bit-Wert)`-Paare — ist
eine fremde Datenbank ohne Lizenz.

**Konsequenz für T1: die Pattern-Extraktion ist nicht durchführbar,** solange
die Herkunft nicht belegt ist. Dieselbe Regel wie bei `src/switch/` (MF-441).
Das Schema kann bleiben; die Einträge müssen aus einer Quelle kommen, deren
Lizenz das hergibt, oder leer bleiben und ehrlich als leer gemeldet werden.

Fachlich kommt hinzu: ein einzelner 32-Bit-Vergleich an festem Offset erzeugt
über 110 Signaturen eine messbare Falsch-Positiv-Rate. X-Copy weiß das selbst —
`WHATS_NEW.DOC` begründet den CONTINUE-Knopf mit *„only implemented for the
case that XCopy detects a harmless bootblock as a virus"*.

---

## Fund 1 — `0xF8BC` ist kein Sync, und wir suchen danach

`xcopy_src/xcopy.i`:

```
INDEXCOPY = $F8BC
```

`xcop.s:2110-2113` benutzt den Wert als **Modus-Sentinel**:

```asm
    move.w  sync,D1
    cmp.w   #INDEXCOPY,D1
    beq.s   stdsync         ; -> NUR nach $4489 suchen, index-synchron kopieren
```

`$F8BC` bedeutet damit ausdrücklich *„kein Custom-Sync — nimm den Standard und
synchronisiere auf den Index"*. Der Wert steht **nie auf einer Diskette**.

UFT führt ihn als Sync-Muster:

| Ort | |
|---|---|
| `src/analysis/uft_track_analysis.c:21` | in `AMIGA_SYNCS[]` |
| `src/analysis/uft_track_analysis.c:854` | zweite Kopie derselben Liste |
| `src/analysis/uft_track_analysis.c:977` | `case 0xF8BC:` → meldet `"Index Copy Protection"` |
| `src/formats/amiga/uft_amiga_protection.h:42` | `AMIGA_SYNC_INDEX 0xF8BC` |

Ein 16-Bit-Muster trifft in einem MFM-Bitstream im Mittel alle 65536 Bit — auf
einer ~100.000-Bit-Spur also ein- bis zweimal zufällig. Der Befund ist Rauschen
mit einem Namen davor.

---

## Fund 2 — Das 16-Byte-Sektor-Label wird gelesen und verworfen

`src/flux/uft_flux_decoder.c:1103-1106`:

```c
amiga_read_field(bits, bit_count, &pos, 16, label, &hdr_csum);
(void)label;  /* OS-recovery info — preserved on disk, unused here */
```

`flux_decoded_sector_t` hat kein Feld dafür. Das Label liegt im
Header-Prüfsummenbereich, wird also gelesen und geht in die Prüfsumme ein —
danach ist es weg.

Bei einem Werkzeug mit dem Grundsatz „Kein Bit verloren" ist das stiller
Datenverlust auf dem Hauptpfad. AmigaDOS legt dort Recovery-Informationen ab,
und mehrere Schutzverfahren benutzen das Feld als Ablage.

---

## Fund 3 — Der Flux-Decoder kann keine Amiga-HD und keine 82 Zylinder

`src/flux/uft_flux_decoder.c:1111`:

```c
if (track > 159 || sec > 10) return FLUX_ERR_NO_SYNC;
```

- `sec > 10` — richtig für DD (11 Sektoren, 0–10), **verwirft HD** (22
  Sektoren, 0–21).
- `track > 159` — richtig für 80 Zylinder, **verwirft Zylinder 80 und 81**.

Innerhalb von UFT widersprüchlich: `src/formats/adf/uft_adf_plugin.c:96` führt
`"HD variant (1760 KB)"` als `UFT_FEATURE_SUPPORTED`, `uft_adf_parser_v2.c:39`
kennt `ADF_SIZE_HD`. Der Sektorpfad kann HD, der Fluxpfad nicht — SCP→ADF einer
HD-Diskette liefert nichts.

Zum Zylinderlimit: X-Copy erlaubt `endtrack DC.W 79 ; 0 - 81` (`xio.s:210`) und
steppt in `track0` bis 83 (`xcop.s:1836`). Zylinder 80/81 sind genau der
Bereich, in dem Zusatzkapazität und Kopierschutz liegen.

---

## Fund 4 — Drei Amiga-Sync-Tabellen, die sich widersprechen → ✓ BEHOBEN (MF-453, FMT-17)

Ergänzung zu `XCOPY_ALGORITHM_MIGRATION.md` §2, das die Multi-Pattern-Suche
vorschlägt: die Muster **liegen bereits dreimal im Baum**, in drei Fassungen.

| Datei | Inhalt |
|---|---|
| `src/analysis/uft_track_analysis.h:41-44` | `0x4489`, `0x9521`, `0xA245` „Ocean/Imagine", `0xA89A` |
| `src/formats/amiga/uft_amiga_protection.h:35-42` | dieselben, aber `0xA245` = **„Beyond the Ice Palace"**, plus `0x448A`, `0xF8BC` |
| `src/protection/uft_amiga_protection.c:23,52` | kennt keine davon, dafür `0x8a91` und `0x8914` |

Und der Decoder (`uft_flux_decoder.c:1169`) benutzt keine davon — er sucht nur
`MFM_SYNC_PATTERN` = `0x4489`. Die Multi-Pattern-Suche aus dem Migrationsbericht
ist damit nicht „noch zu bauen", sondern „dreimal gebaut und nirgends
angeschlossen".

Dasselbe Muster wie ARCH-9 / ARCH-10 / ARCH-14: ein Fakt, mehrfach
implementiert, und welche Fassung gilt, entscheidet der Aufrufweg.

---

## Was X-Copy an Verfahren hat, das keines der drei Dokumente abdeckt

**Gemessene statt nomineller Spurkapazität, pro Laufwerk.**
`xcop.s:1929 SpeedCheck` löscht die Spur mit `$AAAA`, liest zwei Umdrehungen und
bestimmt über `getracklen` die tatsächliche Kapazität; `drilen DS.W 4`
(`xcop.s:2085`) hält je Laufwerk ein Ergebnis, `mestrack` setzt die
Schreiblänge individuell.

`XCOPY_ALGORITHM_MIGRATION.md` §4 behandelt `getracklen` als
Puffer-Längenbestimmung — die **Kalibrierung** darüber, also „wie viel kann
*dieses* Laufwerk pro Umdrehung", steht dort nicht. UFT hat in
`src/hal/uft_drive.c:12-27` nur nominelle Werte (`.rpm = 300.0`); das ist die
Bezugsgröße für `uft_write_precomp.c` und für jede Aussage „lange Spur".

Braucht Hardware (UFT-008), deshalb hier nur festgehalten.

---

## Leads aus dem Umfeld (nicht übernommen, EINFRIER-REGEL)

**`keirf/disk-utilities` — libdisk** ist die eigentliche Fundgrube für das, was
X-Copys NIBBLE-Modus blind kopiert. Die Header-Kommentare der Format-Handler
sind halbe Specs. `libdisk/format/amiga/amigados.c` führt über den Standard
hinaus:

| Sync | Zuordnung |
|---|---|
| `0x4521` | Z Out, Track 1 |
| `0x4891` | Turbo Outrun |
| `0x4A84` | Future Tank |
| `0x448A`, `0x8912`, `0x4251`, `0x2149`, `0x2959`, `0x8524`, `0x2429`, `0x4428`, `0x4429`, `0x2849`, `0x4292`, `0x5429` | eigene Handler ohne Titel im Array |

Dazu `copylock.c` (23-Bit-LFSR, Taps 1 und 23), `pdos.c` (Sync `0x1448`,
Disk-Key-EOR-Ketten, Longtrack ~105.500 bit), `longtrack.c` (≥107.200 bit).

**Warum nicht jetzt:** Die EINFRIER-REGEL (MF-363) verlangt vor neuem
Formatcode eine reale Referenzdatei. Die richtige Reihenfolge ist deshalb, wie
in der Referenzliste vorgeschlagen:

1. libdisk als **Referenz-Orakel** behandeln, nicht als Codespender — pro
   Amiga-Trackformat, das UFT beansprucht, ein Cross-Check gegen `disk-analyse`
   (analog zur pyOTDR-Validierung).
2. Greaseweazle-/SCP-Dump einer echten Diskette pro Format, bevor ein Parser
   entsteht.
3. `xcop.s` als **Verhaltensspec** extrahieren, nicht als Quelle portieren.

Bemerkung zur Lizenz: libdisk ist quelloffen, was den Weg über das Orakel auch
rechtlich sauber macht — im Gegensatz zur X-Copy-Quelle und zur `xvs.library`.

---

## Quellenlage im Upload

Der Ordner enthält **zwei** X-Copy-Bäume. Geprüft (MF-454): die Unterschiede
liegen ausschließlich in der Bootblock-Prüfsumme (`xcop.s:4411-4677`, `addq/neg`
durch `not.l` ersetzt — arithmetisch identisch, da −(x+1) = ~x) und bei
`xio.s:2451`. Keine der hier zitierten Stellen ist betroffen. Alle Zitate
beziehen sich auf `xcopy_src/`.

---

## Reihenfolge

| # | Aufgabe | Status |
|---|---|---|
| 1 | `0xF8BC` aus den Sync-Tabellen | ✓ MF-452 |
| 2 | Sektor-Label durchreichen | ✓ MF-452 |
| 3 | HD- und Zylindergrenzen | ✓ MF-452 |
| 4 | Amiga-Sync-SSOT + Decoder anschließen | ✓ MF-453 |
| 5 | Fehlerklasse 5 (Header-Longword) | ✓ MF-454 |
| 6 | Laufwerkskalibrierung im HAL | HW-Bench nötig |
| 7 | libdisk als Referenz-Orakel (Cross-Check) | Lead, s.o. |

1–3 sind Fehlerbehebungen an Bestehendem und fallen nicht unter die
EINFRIER-REGEL (MF-363). 4 ist eine Zusammenführung vorhandener Tabellen, keine
neue Formatkenntnis.
