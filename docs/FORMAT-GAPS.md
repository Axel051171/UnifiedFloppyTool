# FORMAT-GAPS — verifizierte Format-Lücken (Format-Erweiterung Goal, Phase 1)

_Web-recherchiert 2026-07-05, abgeglichen mit der code-generierten Formatliste
(`scripts/gen_format_list.py`, 84 Plugins / 161 Katalog-IDs). Spalten: Format,
Quelle, Klassifizierung (Flux/Bitstream/Sektor), Aufwand, Praxisrelevanz.
Praxisrelevanz + Priorisierung sind **Entscheidungsvorlage** — die Auswahl
welche Kategorie umgesetzt wird, trifft der Maintainer (Phase 2, keine
autonome Implementierung vorab)._

Legende Aufwand: S = klein (<80 LOC, Geometrie-Probe), M = mittel (Probe +
Filesystem/Layout), L = groß (Flux/variable-Speed, neues Subsystem).

## Sofort gemeldete Scope-Befunde (bereits abgedeckt — KEINE Lücke)

- **DEC RX50** — **bereits implementiert**: eigener Handler `src/formats/dec/
  uft_rx50.c` (80 Tracks, 10 Sektoren, 512 B, RX50-Interleave 1,3,5,7,9,2,4,6,8,10),
  Registry-ID `{"RX50","img,dsk",...}`. RX01/RX02 in `src/formats/pdp/uft_pdp.c`.
  Die Ziel-Vorlage bat, das „zuerst zu klären" — geklärt: DEC ist vollständig.
- **Ensoniq Mirage/ASR** — **teilweise abgedeckt**: `src/formats/edk/uft_edk.c`
  (EDK = Ensoniq EPS/ASR/TS, 80×2×spt×512, gebaut). Read-Pfad vorhanden.
- **Roland D-20** — ~~bereits als Katalog-ID abgedeckt~~ **BERICHTIGT
  (MF-887): das trifft nicht zu.** Hier stand, D-20 sei „bereits als
  Katalog-ID `{"RolandD20","d20",...}` (`rolandd20.h`)" abgedeckt.
  Gemessen ist `include/uft/formats/rolandd20.h` **vier Zeilen lang** und
  besteht vollständig aus:

  ```c
  #ifndef UFT_ROLANDD20_H
  #define UFT_ROLANDD20_H
  #include <stdint.h>
  #endif
  ```

  Keine Struktur, keine Geometrie, keine Konstante — und **keine
  Katalog-ID**: der Eintrag `{"RolandD20", ...}`, den dieser Absatz zitierte,
  steht **nicht** in der Registry (`src/formats/format_registry/`, gemessen:
  null Treffer für `Roland`).

  Im ganzen Baum gibt es drei weitere Nennungen, und sie sind zweierlei:

  | Fundstelle | was es ist |
  |---|---|
  | `src/parsers/lexy_experimental/scp_parser_complete.hpp:61,418` | Enum-Tag + Anzeigename in einem Verzeichnis, das **in keinem Build steht** |
  | `include/uft/uft_scp_format.h:100` | `UFT_SCP_DISK_ROLAND_D20 = 0x60` — **legitim**: der Disk-Typ-Bytewert aus der SCP-Dateispezifikation, keine Zusage über Formatunterstützung. Gemessen ohne Verbraucher (nur definiert) |

  Ein leerer Header ist keine Katalog-ID, sondern ein Etikett; ein
  SCP-Typbyte ist eine Spezifikationskonstante. Beides zusammen ergibt keine
  Formatunterstützung. Die S-Serie-Sampler unten sind davon getrennt und
  weiterhin richtig eingeordnet.

## KERN-SCOPE-BEFUND (Phase-2/3, 2026-07-05) — Größen-Kollisionen

Die Größen-Verifikation ergab, dass fast alle Sampler-„Formate" **Standard-
Disk-Geometrien wiederverwenden** und sich nur im (out-of-scope) Dateisystem
unterscheiden:

| Format | Native Größe | Kollidiert größengleich mit |
|---|---|---|
| Akai S900/S950 | 819.200 (800K DD) | D81, Korg DSS-1 |
| Korg DSS-1 | 819.200 | D81, Akai |
| Roland S-50 | 737.280 (720K) | Atari ST 720K, IMG-720K |
| NeXT 2.88M | 2.949.120 | Atari ST ED (`ST_SIZE_DS_ED`) |

**Konsequenz:** Auf Basis-Niveau (Disk-Geometrie) sind das keine neuen Formate.
Ohne dokumentiertes Magic sind sie inhaltlich nicht vom bestehenden Format
unterscheidbar; ein neues Geometrie-Plugin würde die Auto-Erkennung brechen
oder nichts hinzufügen (= „Formatanzahl nachbauen", vom Ziel abgelehnt). Der
reale Mehrwert wäre der **proprietäre Dateisystem-Layer** — laut Ziel
ausgeklammert.

**Ausnahme mit echtem Nutzwert (implementiert):** Korg DSS-1 hat eine
**strukturell distinkte Geometrie** (5×1024 pro Track/Seite vs. D81 40×256) bei
gleicher Gesamtgröße. Ein D81-Reader liest ein Korg-Image als CBM-Sektoren falsch;
das neue Korg-Plugin liest es korrekt. Kollision ehrlich behandelt: Probe-
Confidence 40 < D81 (80) → Auto-Detect bleibt D81, Korg via `.dss`/manuell.
**Implementiert + getestet (MF-347)** — siehe unten.

## Verifizierte echte Lücken

| Format | Quelle | Klasse | Geometrie | Aufwand | Praxisrelevanz |
|---|---|---|---|---|---|
| **Akai S900 / S950** | [chickensys akais9x](http://www.chickensys.com/translator/documentation/floppyimageinfo/akais9x.html), [justsolve Akai](http://justsolve.archiveteam.org/wiki/Akai_Disk_Format) | Sektor | S900=3.5" DD, S950=HD, MFM; proprietäres Akai-FS in IMG | M (Geometrie-Probe + FS-Layout) | **Hoch** — sehr verbreitete Retro-Audio-Sampler, aktive Preservation-Community (akaiutil) |
| **Akai S1000 / S3000** | [fmjsoft akai](https://www.fmjsoft.com/fmt/akai.htm) | Sektor | S1000 proprietär; S2000/S3000 = MS-DOS-formatiert (via IMG lesbar) | S–M | Mittel — S3000 oft schon als IMG lesbar |
| **Korg DSS-1 / DSM-1** | [chickensys korg](http://www.chickensys.com/translator/documentation/floppyimageinfo/korg.html), [Wikipedia DSS-1](https://en.wikipedia.org/wiki/Korg_DSS-1) | Sektor | 80×2×5×**1024** = 819.200 B (DSDD, gleiche Medien wie Mirage/früher Mac) | S–M (1024-B-Sektor-Geometrie) | Mittel — Kult-Sampler, Community-Support (Gotek/HxC) |
| **Roland S-50 / S-550 / S-330** | [llamamusic s50](https://llamamusic.com/s50s550/sinfo.html) | Sektor | 800K DD Floppy | M | Mittel |
| **Roland S-770 / S-750 / S-760** | [llamamusic s-7xx](https://llamamusic.com/s50s550/s-7xx_disk_info.html) | Sektor | HD-Floppy (+ MO-Drive, out-of-scope) | M | Mittel |
| **Apple Lisa Twiggy / FileWare** | [brouhaha twiggy](http://www.brouhaha.com/~eric/retrocomputing/lisa/twiggy.html), [archiveteam Twiggy](http://fileformats.archiveteam.org/wiki/Twiggy_floppy) | **Flux** (variable-Speed GCR, 218–320 RPM, 62.5 TPI) | 5.25" DS, 1702 Sekt.×512 (+20 Tag-B) = 871.424 B | **L** (variable-Speed nur über Flux verlustfrei; dekodiert = Sektor) | Nieder-praktisch/Hoch-Preservation — extrem seltene HW, aber forensisch bedeutsam; nur MOOF-Erwähnung im Code, kein Handler |
| **NeXT 2.88M Floppy** | [Wikipedia Floppy](https://en.wikipedia.org/wiki/Floppy_disk) | Sektor | 3.5" DSED 2.88 MB (80×2×36×512 = 2.949.120), perpendicular MFM | **S** (Standard-2.88M-Geometrie) | **Niedrig** — NeXT-Floppys sind Standard-2.88M-Images, bereits als IMG/generische Geometrie lesbar; nur der NeXT-UFS-Dateisystem-Layer wäre neu |
| **ABB IRB2000 / IRB-Serie** | keine öffentliche Spec gefunden | ? | ? | ? (spec-blockiert) | Nische — Industrie/CNC; **keine byte-genaue Spec öffentlich auffindbar** → nicht implementierbar ohne Reverse-Engineering + Ground-Truth-Disks |

## Phase-3 Dispositionen (Freigabe „alle", Stand 2026-07-05)

| Format | Disposition | Grund |
|---|---|---|
| **Korg DSS-1** | ✅ **implementiert + getestet** (MF-347) | Geometrie 5×1024 verifiziert + strukturell distinkt von D81 → realer Nutzwert |
| **Akai S900/S950** | ✅ **implementiert + getestet** (MF-348) via akaiutil-Primärquelle (1024-Byte-Blöcke, DD 5×1024 / HD 10×1024; nur Geometrie-Fakten, kein Code kopiert). DD-Kollision wie Korg (Probe 40<80), HD eindeutig (70). ~~Ursprünglich blockiert:~~ | DD 819.200 kann 5×1024 **oder** 10×512 sein, HD 1.638.400 — exakte Sektoren/Track **nicht** öffentlich verifizierbar. Auf Inferenz zu bauen wäre Fabrikation. Braucht FlashFloppy-Disk-Def (`interface=akai-s950`) oder Referenz-Image |
| **Roland S-50/S-550** | ⛔ nicht implementiert | 737.280 = Standard-720K (9×512), keine verifiziert-distinkte Geometrie → kein Disk-Level-Neuwert (bereits als ST/IMG lesbar); Wert läge im FS (out-of-scope) |
| **NeXT 2.88M** | ⛔ nicht implementiert | 2.949.120 = identische Geometrie zu Atari ST ED (36×512) → kein Neuwert; nur NeXT-UFS-FS wäre neu (out-of-scope) |
| **Apple Lisa Twiggy** | ✅ **implementiert + getestet** (MF-349, dekodierte Sektor-Ebene) — verifizierte Zonen-Tabelle (46 Tracks/Seite, 22..15 Sekt./Zone, 1702×512 = 871.424). Flux-Klasse-1: variable-Speed-Timing NICHT im dekodierten Image erhalten (Feature-Matrix flaggt es); volle Flux-Erhaltung braucht Flux-Pfad + Referenz-Image. ~~Ursprünglich aufwands-blockiert:~~ | Einzige größen-eindeutige (871.424), aber **Flux-Klasse-1**: variable-Speed GCR + zoned per-Track-Sektoren; die Zonen-Verteilung ist nicht verifiziert (Fabrikations-Risiko) und Timing-Erhalt braucht das Flux-Subsystem (L). Braucht Twiggy-Flux-Referenz-Image |
| **ABB IRB2000/IRB** | ⛔ nicht implementiert (spec-blockiert) | Keine öffentliche Byte-Spec auffindbar; ohne Reverse-Engineering + Ground-Truth-Disks nicht seriös |

**Ehrliches Ergebnis:** von den freigegebenen Kandidaten war **1 sauber
verifizierbar** und wurde implementiert (Korg). Die übrigen sind entweder
geometrie-kollidierend ohne Disk-Level-Neuwert (Roland/NeXT — Wert im FS,
out-of-scope), geometrie-unverifizierbar (Akai — Fabrikations-Risiko) oder
aufwands-/spec-blockiert (Twiggy Flux, ABB Spec). Das ist gemäß Ziel ein
zulässiges Ergebnis, dokumentiert statt erfunden.

## FluxEngine-Abgleich: 15 Formate ohne UFT-Entsprechung (MF-887)

_Quelle: `davidgiven/fluxengine`, 34 Formatprofile unter
`src/formats/*.textpb`. Die Profile nennen ihre eigenen Primärquellen und
tragen eine **eigene Reifekennzeichnung** (`read_support_status`) — die ist
hier mitgeführt, weil `DINOSAUR` FluxEngines **niedrigste** Stufe ist und
nicht als „verifiziert" gelesen werden darf._

> **GESPERRT bis Moratoriumsende.** `docs/VERIFICATION_PLAN.md:234` sagt:
> *„Das **Moratorium bleibt in Kraft**, bis NFD-r0 auf T1/T1b ist."*
> Gemessen in `docs/VERIFICATION_TIERS.md:67` steht `nfd` auf **T2**. Danach
> gilt **1:2** — ein neues Format kostet zwei Hebungen. Diese Tabelle ist
> **Entscheidungsvorlage**, keine Aufgabenliste; sie steht hier, damit die
> Entscheidung vorbereitet ist, wenn die Sperre fällt.

| FluxEngine | Beschreibung | Aufwand | Anmerkung |
|---|---|---|---|
| `agat` | 840 kB, sowjetischer Apple-II-Klon | M | `soviet/` enthält nur `uft_bk0010.c` |
| `mx` | DVK, sowjetischer PDP-11-Klon, FM | M | Profil dokumentiert das Spurlayout vollständig (8×0x0000-Sync, 0x00F3-Kennung, 11×128 Wörter + 16-bit-Summe, little-endian); Primärquellen im Profil zitiert |
| `tartu` | „Palivere", TTL-Logik | M | — |
| `juku` | CP/M, estnischer Rechner | S–M | `cpm/` deckt nur das Dateisystem, nicht diese Geometrie |
| `n88basic` | NEC PC-8800/PC-98, 77 Spuren × 26 Sekt. HD | M | `nec/` enthält nur `uft_pce.c` (andere Plattform) |
| `tids990` | TI-990 Minicomputer, 8″ DSSD | M | `minicomputer/` hat nur DG Nova und Prime |
| `f85` | proprietäre 4-in-5-GCR | L | eigener Kodierer nötig |
| `fb100` | FM, eigenes Record-Format | M–L | — |
| `aeslanier` | 77 Spuren, hartsektoriert | M | `hardsector/` kennt dieses Profil nicht |
| `ampro`, `eco1`, `epsonpf10`, `icl30`, `tiki` | fünf CP/M-Geometrien | je S | `cpm/` deckt nur das Dateisystem |
| `ms2000` | Microdisk Development System | M | — |
| `psos` | 800 kB DSDD, PHILE-Dateisystem | M | — |
| `smaky6` | Schweizer Rechner, experimenteller FS-Read | M | — |

### Der Abkürzungsweg für Roland D-20 existiert nicht (MF-887)

Die Zulieferung schlug vor, D-20 über den vorhandenen Brother-Decoder zu
bauen: FluxEngines Profil sagt *„it seems to use precisely the same format as
the Brother word processors: a thoroughly non-IBM-compatible custom GCR
system"*, und UFT habe „bereits eine echte, funktionierende Implementierung
genau dieser GCR-Kodierung".

**Gemessen trägt das nicht.** `src/formats/brother/brother.c` (107 Zeilen;
die Zulieferung nennt `uft_brother.c`, so heißt die Datei nicht) ist **kein
GCR-Decoder**, sondern ein Sektor-Offset-Leser:

```c
size_t offset = (t * dev->sectors + s) * dev->sectorSize;
fseek(f, offset, SEEK_SET); fread(buf, 1, dev->sectorSize, f);
```

Eine GCR-Tabelle gibt es — `BROTHER_GCR_ENCODE[32]` und die daraus in
`init_decode_table()` gebaute Umkehrtabelle `g_brother_decode[256]`. Sie wird
**nie gelesen**: kommentarfrei über `git ls-files` gezählt kommt
`g_brother_decode` **dreimal** vor, alle drei in derselben Datei —
Deklaration, `memset`, und die Schleife, die sie befüllt. Kein Dekodierpfad
benutzt sie.

Dazu: die gesamte Brother-API (`brother_probe`/`_open`/`_read_sector`/
`_write_sector`) hat **null Aufrufer** außerhalb ihrer eigenen zwei Dateien.
`src/formats/brother/brother.c` steht folgerichtig seit MF-509 in
`docs/orphan_baseline.txt` (Zeile 89) — unabhängig von dieser Messung.

**Es gibt also nichts wiederzuverwenden.** Wer D-20 nach Moratoriumsende
angeht, schreibt den GCR-Dekoder neu — und dann für Brother gleich mit, denn
der fehlt dort ebenso. Das ist kein Argument gegen den Fund: der Hinweis
„gleiches Format wie Brother" bleibt wertvoll, weil er **zwei** Formate an
denselben Dekoder bindet. Er senkt nur den Aufwand nicht, wie die Zulieferung
annahm — er verdoppelt den Nutzen des einen, der noch zu bauen ist.

**Was D-20 zusätzlich blockiert:** im Korpus liegt kein D-20-Abbild. Die
Geometrie 78/1/256/12/Skew 5 stammt aus FluxEngines Profil (dort
`read_support_status: UNICORN`, die höchste Stufe) — das ist eine **benannte
Referenz**, aber keine Messung. Die EINFRIER-REGEL verlangt beides.

## Klassifizierungs-Hinweise (Basis-Niveau, wie im Ziel gefordert)

- Die Sampler-Formate (Akai/Korg/Roland-S) sind **Sektor-Image (Klasse 3)** —
  MFM-Standardspuren mit spezifischer Geometrie + proprietärem Dateisystem. Der
  Disk-Layer ist als IMG-artige Geometrie lesbar; der Mehrwert ist
  Geometrie-Erkennung + optional der Filesystem-Layer.
- **Lisa Twiggy** ist das einzige echte **Flux-Format (Klasse 1)** hier
  (variable Motordrehzahl → Timing nur über Flux erhaltbar), damit L-Aufwand.
- **NeXT** ist faktisch Standard-2.88M → geringster technischer Neuwert.
- **ABB IRB** ist ohne öffentliche Spec + Ground-Truth-Disks nicht seriös
  umsetzbar (Autonomiegrenze: keine erfundene Verifikation).

## Nicht byte-verifizierbar ohne Ground-Truth-Korpus (Autonomiegrenze)

Für alle neuen Formate gilt: die Byte-genaue Read-Korrektheit gegen reale
Disks ist nur mit Referenz-Images verifizierbar. Legitime öffentliche Quellen:
- HxC-Sample-Archiv + chickensys Translator-Doku (Geometrie-Referenz)
- akaiutil (Akai-FS-Referenz-Implementierung, Open Source)
- Applesauce/Lisa-Preservation-Community (Twiggy-Flux-Referenz)
Ohne solche Images bleibt die Struktur-Ebene (Header/Geometrie-Probe) das
verifizierbare Niveau; die Dateisystem-/Sample-Extraktion braucht echte Disks.
