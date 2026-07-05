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
- **Roland D-20** (Synthesizer, kein Sampler) — bereits als Katalog-ID
  `{"RolandD20","d20",...}` (`rolandd20.h`). Die S-Serie-Sampler unten sind
  davon getrennt.

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
