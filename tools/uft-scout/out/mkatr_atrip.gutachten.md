<!-- uebernommen: MF-650 -->
# Gutachten: dmsc/mkatr (lsatr) + robmcmullen/atrip

Zyklus „Atari-Zweithand", 2026-08-28 · Eigentümer-Auftrag (beide Repos
stehen als Bedingung in `docs/plans/UMSETZUNGSLISTE.md` B1/B2) ·
Messungen: `work/mkatr.messung.json`, `work/atrip.messung.json` ·
Repo-Stände: mkatr `97f6278f` (2026-01-18, „Release new version 1.4"),
atrip `170ab327` (2019-07-05) · Inventar: `work/inv.json` (in dieser
Sitzung neu gebaut, `rc=0`, „88 Plugins (SSOT ok), … 22 Korpus-Abbilder,
HEAD 548f204b").

> **Hinweis Werkzeugkette:** `gutachten.py` hat den mechanischen Entwurf
> verweigert — „RATENBREMSE: bereits 5 Gutachten ohne Uebernahme-Marke"
> (FloppyTools, dfsimage, lib1541img, sector-cpc, superdiskindex; drei
> davon aus parallel laufenden Zyklen). Dieses Gutachten ist deshalb von
> Hand geschrieben (Präzedenz: a8rawconv, MF-643); die Vorschläge unten
> stehen **hinter** der Abarbeitung der offenen Marken an.
>
> **Nummern vorläufig:** SCOUT-64…66 (höchste vergebene Marke war
> SCOUT-48; drei Zyklen laufen parallel, Auftrag nennt Start bei 64).

---

## Teil 1 — dmsc/mkatr (`lsatr` + `mkatr`)

### Kategorie

**Oracle (Hauptertrag) + Verbesserung (Spec-Referenz für zwei
ATR-Randfälle) + Daten (SpartaFS-Generator, Fundus).**

### Lizenzzone mit Konsequenz

**GRÜN — GPL-2.0-or-later. Portierbar in ein GPL-2.0-Projekt; für den
Oracle-Einsatz wandert ohnehin kein Code ein.**

Beleg aus den Dateien, nicht aus dem README:

* `LICENSE` (339 Zeilen): wörtlicher FSF-Text „GNU GENERAL PUBLIC
  LICENSE Version 2, June 1991". `vermessen.py` meldete „GPL-2.0 + LGPL
  (mehrdeutig)" — **Fehlalarm des Scanners**, bekannt aus dem
  a8rawconv-Gutachten: die beiden „Lesser"-Treffer (LICENSE:18,
  LICENSE:338) sind Bestandteil des unveränderten GPLv2-Standardtexts.
  Kein Doppel-Lizenz-Fall.
* **26 von 26** Dateien unter `src/` (13 `.c` + 13 `.h`) sowie das
  `Makefile` tragen den Kopf wörtlich: „either version 2 of the License,
  or (at your option) any later version". Methode: Schleife über
  `src/*.c src/*.h`, `grep` auf die Kopfzeilen 1–8; 26/26 Treffer, 0
  Abweichler. Beispiele: `src/atr.c:5-7`, `src/lsatr.c:5-7`,
  `Makefile:4-7`.
* Laufzeit-Banner (`lsatr -v`): „mkatr version 1.4, Copyright (C)
  2016-2026 Daniel Serpell … free software under the GNU GENERAL PUBLIC
  LICENSE."

**Kein GPL-2.0-only-Risiko**: die or-later-Klausel steht in jeder Datei
(relevant wegen `docs/plans/FLUXENGINE.md` — only würde den Baum
dauerhaft verengen; hier nicht der Fall).

### Attribution der Quelle

mkatr 1.4, Daniel Serpell (dmsc), 2016–2026, GPL-2.0-or-later.
3 124 Zeilen C (Methode: `wc -l src/*.c src/*.h`, Summe der Ausgabe).
Einziger Autor laut `git log --format=%an | sort -u`: Daniel Serpell.

### Bau-Beleg (Auftragspunkt 1)

Gebaut auf dieser Maschine (Windows, MinGW g++ 13.1.0 aus
`/c/Qt/Tools/mingw1310_64/bin`):

```
cd tools/uft-scout/work/mkatr
PATH=/c/Qt/Tools/mingw1310_64/bin:$PATH mingw32-make CC=gcc
→ rc=0, keine Warnungen (CFLAGS -O2 -Wall)
lsatr.exe  106 419 B  sha256 dc7f90046d833b7b122865f8d9347bb90526d05bda02e145ae663fea043ed1d5
mkatr.exe   95 840 B  sha256 bc48511cbd5009e7d0b6dd6bb6b0aead154e3083fb7c3fd5dffb59ad2fa08e13
```

Windows-Tauglichkeit ist im Quelltext angelegt (`src/compat.c:8-13`,
`_WIN32`-Zweig für `mkdir`).

### Lauf-Beleg gegen das Korpus-Paar

```
$ ./lsatr.exe ../../../../tests/corpus_free/atrcopy_dos2sd.atr
…atrcopy_dos2sd.atr: 720 sectors of 128 bytes, DOS 2.0s, 707 sectors free of 707 total.
rc=0

$ ./lsatr.exe ../../../../tests/corpus_free/atrcopy_dos2sd.xfd
…atrcopy_dos2sd.xfd: 720 sectors of 128 bytes, DOS 2.0s, 707 sectors free of 707 total.
rc=0

$ ./lsatr.exe -a …atrcopy_dos2sd.atr        # Atari-Listing
Image size: 720 sectors of 128 bytes / DOS size: 707 sectors free of 707 total
Volume: DOS 2.0s / Directory of /           # LEER
rc=0

$ ./lsatr.exe -X <ziel> …atrcopy_dos2sd.atr # Extraktion
rc=0, 0 Dateien extrahiert
```

**Feststellung mit Folge:** unser einziges ATR-Korpus-Abbild ist
**leer** (707 von 707 Sektoren frei, leeres Verzeichnis; deckungsgleich
mit dem Manifest „empty VTOC"). Der B1-Differenzlauf auf diesem Abbild
entscheidet also nur Geometrie + VTOC + Leerverzeichnis, **keine
Datei-Ebene**. → SCOUT-65.

### Die drei Oracle-Fragen (Auftragspunkt 2)

**(a) Konsole ohne GUI? JA.** Reines CLI (`lsatr.c` main, Ausgabe über
stdio; einzige Abhängigkeiten libc). `-h`/`-v` mit `rc=0` gemessen.

**(b) Inhalte je Datei, nicht nur Listing? JA.** `-x`/`-X pfad`
extrahiert Dateiinhalte. Gemessen (mkatr→lsatr-Rundlauf): 36-Byte-Probe
eingelegt, extrahiert, **SHA-256 identisch**
(`35bb15338ab857faf109293a1973e4d27c700a0104dcdc90bd9891645e675084`
vor wie nach). Zweiter Beleg an Fremd-Abbildern: A4096.DAT aus drei
atrip-Samples (SD/ED/DD) extrahiert, alle drei
`d4d4073c36126053bee5a2399b5ecc3509b6b000c7805a1b1221b9cf03c1516d`.
Eigene Hashes gibt lsatr nicht aus — Hashen macht der Aufrufer nach der
Extraktion (Stufe „stärker" des Differenzlauf-Standards, ORACLES.md).

**(c) Unabhängig von der Hand des Korpus-Erzeugers? JA.** Korpus stammt
von atrcopy (Rob McMullen). mkatr: einziger Autor Daniel Serpell
(git log); eigene C-Codebasis; `grep -rni "atrcopy|robmcmullen|omnivore|python" mkatr/`
→ **rc=1, 0 Treffer**. Keine geteilte Bibliothek, keine Ableitung.

**Herkunfts-Anker für die Registry:** `lsatr -v` antwortet („mkatr
version 1.4"), zusätzlich Binär-SHA-256 oben. `version_is_unaskable`
ist nicht nötig.

### Fähigkeitsumfang (für den Registry-Text, aus dem Quelltext)

* Dateisysteme lesend: DOS 1, DOS 2.0s/2.0d, DOS 2.5, MyDOS,
  SpartaDOS/BW-DOS, LiteDOS, Bibo-DOS-Erkennung, Howfen-DOS, K-Boot,
  BAS2BOOT (`README.md:81-89`; Signaturlogik `src/lsdos.c:326-406`,
  `src/lssfs.c`, `src/lshowfen.c`).
* Container: ATR (Magic `0x96,0x02` = 0x0296, `src/atr.c:44`), roh/XFD
  nur bei exakt 720×128 oder 1040×128 Bytes (`src/atr.c:46-66`);
  Sektorgrößen 128/256, **512 wird abgelehnt** (`src/atr.c:68-73` —
  enger als UFT, `uft_atr.c:58-62` akzeptiert 512 für SDX-Large-ATR).
* Standard-Größentabelle SD/ED/DD/HD bis 16M: `src/disksizes.h:23-33`.

### Spec-Befund 1: DD-ATR mit voll gespeicherten Boot-Sektoren

`src/atr.c:76-90` behandelt **beide** in freier Wildbahn belegten
Ablagen der ersten 3 Sektoren eines 256-Byte-ATR („Some images store
full size fo the first 3 sectors, others store 128 bytes"): ist die
Bildgröße durch 256 teilbar → Voll-Ablage, sonst 128-Byte-Ablage; dazu
Heuristik + Nullprüfung der oberen Hälften (`atr.c:104-125`).

**UFT kennt nur die 128-Byte-Ablage:** `atr_sector_offset()`
(`src/formats/atr/uft_atr.c:19-25`) rechnet fest
`3×128 + (n−4)×sector_size`; Kommentar `uft_atr.c:59` („The first 3
boot sectors stay 128 B regardless"). Nach Quelltextlage liest UFT die
Voll-Ablage-Variante **um 384 Bytes versetzt** — nicht ausgeführt, der
Rotbeweis ist Stufe-4-Arbeit (→ SCOUT-66).

**Fixture-Rezept gemessen:** aus `atrip/samples/dos_dd_test1.atr`
(183 952 B = 16 + 3×128 + 717×256) die Boot-Sektoren auf 256 B
genullt-aufgefüllt, Header-Paragraphenfeld auf 184 320/16 gesetzt →
184 336 B. `lsatr` liest die synthetisierte Voll-Ablage-Datei mit
identischem Listing und **identischem Datei-Hash** (A4096.DAT wieder
`d4d4073c…`), rc=0. Rezept-Skript im Gutachten-Anhang unten.

### Spec-Befund 2 (Beleg für Plan-Baustein A3, kein eigener Vorschlag)

Unabhängige Bestätigung der dritten Hand, dass **XFD = ATR minus
16-Byte-Kopf**: (1) `lsatr` liest `atrcopy_dos2sd.atr` und `….xfd` zu
identischem Dateisystem-Zustand (Ausgaben oben, beide rc=0);
(2) im Quelltext akzeptiert `atr.c:46-66` kopflose Abbilder als rohe
SS/SD- bzw. SD/ED-Sektorfolge. Zusammen mit a8rawconv (`ATR→XFD`
byteidentisch, `docs/ORACLES.md`) sind das **zwei** vom Korpus-Erzeuger
unabhängige Bestätigungen für den 13. Wandlungspfad.

---

## Teil 2 — robmcmullen/atrip

### Kategorie

**Als Oracle: AUSGESCHLOSSEN (zirkulär + auf dieser Maschine nicht
lauffähig). Als Daten-Quelle: brauchbar (samples/).**

### Zirkularität (Auftragspunkt 2c, scharf)

atrip ist **dieselbe Hand** wie der Erzeuger unseres ATR-Korpus:

* `README.rst:6`: „The successor to atrcopy, this is under heavy
  development and is still in a beta state."
* `README.rst:219`: „atrcopy … Precursor to ``ATRip``".
* Einziger Autor laut `git log --format=%an | sort -u`: Rob McMullen —
  derselbe Autor wie atrcopy, das laut Korpus-Manifest
  `atrcopy_dos2sd.atr` erzeugt hat („atrcopy 10.1 template dos2sd.atr",
  `tests/corpus_manifest/manifest.json`, images[2].tool).

Nach der fünften Registrierungsfrage (`docs/ORACLES.md`, MF-644) darf
atrip den ATR-T1b-Eintrag damit **nicht** stützen. Das ist ein
**Ausschlussgrund, kein Makel** — ob atrip Code von atrcopy erbt oder
eine Neuschrift ist, ist dafür unerheblich: gleiche Hand reicht.

### Lauffähigkeit (Fessel-Prüfung aus dem Auftrag)

Gemessen, nicht vermutet — atrip hat **zwei** Fesseln, atrcopy hatte
eine:

* `python -c "import atrip"` (Python 3.13.14) scheitert sofort:
  `atrip/container.py:4 → import pkg_resources →
  ModuleNotFoundError` (pkg_resources liegt auf modernen
  setuptools-Ständen nicht mehr bei). Plugin-Auflösung hängt zusätzlich
  an pkg_resources-Entry-Points (`setup.py:30-40`; 8 Kernmodule
  importieren pkg_resources).
* `np.fromstring` (unter NumPy ≥1.22/2.x entfernt; hier liegt 2.5.1)
  9× in `atrip/machines/atari8bit/jumpman/parser.py:442-709`.
* Stand: Version 0.5.0 (`atrip/_version.py`), Beta laut README, letzter
  Commit **2019-07-05** — seit sieben Jahren ruhend.

### Lizenz

* `LICENSE` (339 Zeilen): GPLv2-Standardtext — gleicher
  Scanner-Fehlalarm wie oben.
* **Keine Datei-Köpfe:** `grep -rn "GNU|License|Copyright" atrip/*.py`
  → 0 Treffer; `setup.py:84` sagt nur „GPL" ohne Version. Ohne
  or-later-Erklärung konservativ als **GPL-2.0-only** zu behandeln
  (Zone GRÜN, aber verengend) — praktisch folgenlos, da kein Port
  vorgeschlagen wird.
* `LICENSE.unlzw` (zlib-artig, Mark Adler/Brandon Owen) deckt
  `atrip/compressors/unix_compress.py` — Vendoring-Fall, sauber
  deklariert.

### Wert trotz Ausschlusses: `samples/` als Fixture-Quelle

52 Beispieldateien, darunter `dos_{sd,ed,dd}_test{1..5}.atr`. Gemessen
mit dem frisch gebauten `lsatr`:

| Abbild | lsatr-Urteil | Inhalt | rc |
|---|---|---|---|
| `dos_sd_test1.atr` (SHA-256 `c34df1c5…6315f9`) | 720×128, DOS 2.0s, 655/707 frei | 5 Dateien A128/256/512/1024/4096.DAT | 0 |
| `dos_ed_test1.atr` (`4c8b6f23…e9870c`) | 1040×128, DOS 2.5, 655/1010 frei | dieselben 5 | 0 |
| `dos_dd_test1.atr` (`567573c6…3def4e`) | 720×256, DOS 2.0D, 679/707 frei | dieselben 5 | 0 |

Extrahierte Datei-Hashes (Erwartungswerte für einen Differenzlauf),
alle drei Dichten liefern **byteidentische** Nutzdaten:

```
A128.DAT  d3e53394fe16aad0509a6c59b3321f09996ca4c396e4b3f404582542a371f515
A256.DAT  2c4a0471b729b15844c480ab5611059cbbde24ea6116f4f3e32200acabbcc828
A512.DAT  43d815e7b9e12914e30eb45e9ce40eaf6d7ef923211e169cdb171f2f20e2786a
A1024.DAT 4f9d1cb16b2f8547a4e10554d0b4ab897fab7fd5e6603e619e70c48d1e02c4aa
A4096.DAT d4d4073c36126053bee5a2399b5ecc3509b6b000c7805a1b1221b9cf03c1516d
```

Inhalt ist **generierte Musterdatei-Füllung** („A128   \x00A128
\x01…", per Hexdump geprüft; Erzeugungsskript
`samples/create_binary.py:11-14` beschreibt exakt dieses Muster) — kein
DOS.SYS, keine urheberrechtlich relevanten Fremddateien auf den drei
geprüften Abbildern (Verzeichnislisting oben). Herkunft dieser Fixtures
ist damit dieselbe Hand wie der Korpus — **als Eingabe unbedenklich**,
weil das Urteil beim unabhängigen Oracle (`lsatr`) liegt, nicht beim
Erzeuger.

---

## Was das Inventar dazu sagt (Abfragen zitiert)

```
"atr":  { "vorhanden": true,  "tier": "T1b", "plugin_liste_vollstaendig": true }
"xfd":  { "vorhanden": true,  "tier": "T1b", "plugin_liste_vollstaendig": true }
"atari dos": { "vorhanden": false, "schwache_treffer": ["atari"],
               "hinweis": "nur Teilwort-Treffer … von Hand pruefen" }
"spartados" / "mydos" / "dos 2.5": { "abgedeckt": false,
               "hinweis": "INVENTAR DECKT DAS NICHT AB … Von Hand im Baum pruefen" }
```

Handprüfung zu den `abgedeckt:false`-Begriffen (Regel 4): der Baum
**hat** Atari-DOS-Code — zwei parallele Fassungen,
`src/formats/atari/atari_{atr,check,dos2,sparta,util}.c` (2 869 Zeilen)
und `src/formats/atari/uft_atari_dos.c` (510 Zeilen), zusammen 3 379 +
337 (`uft_atari8_disk.c`) Zeilen; Methode `wc -l` über die genannten
Dateien. Genau die B1-Lage (0 Aufrufer, 0 Tests laut Plan). Es wird
also **kein** Dateisystem-Neubau vorgeschlagen, sondern Oracle + Korpus
für die bereits geplante Entscheidung.

Korpus-Abgleich der Beschaffungsliste: `inv["korpus"]` führt 22
Abbilder; für ATR/XFD nur das leere atrcopy-Paar. Die drei
DOS-2-Samples liegen dort **nicht** → Beschaffung ist keine Dublette.

## Einhängepunkte (im Baum auffindbar)

* `docs/plans/UMSETZUNGSLISTE.md` **B1** (Differenzlauf über die zwei
  Atari-DOS-Fassungen; „Davor: lsatr bauen" — erledigt, Beleg oben),
  **B2** (lsatr + a8rawconv registrieren), **A3** (ATR↔XFD-Pfad).
* `docs/ORACLES.md` §„Vorgemerkt, noch nicht registriert" (lsatr-Zeile
  wartet auf den Registry-Eintrag) und §„Was ausdrücklich kein Oracle
  ist" (atrip nachtragen).
* `tests/differential/oracles.py` (Registry, 6 Einträge, kein
  ATR-Werkzeug — Selbsttest in dieser Sitzung gelaufen).

## Oracle-Kandidat

`lsatr` (mkatr 1.4). Gebaut, gelaufen, gehasht — Registry-Eintrag kann
ohne weitere Beschaffung erfolgen. atrip: ausgeschlossen (oben).

## Beschaffungsliste

Nichts extern zu beschaffen. Alles liegt bereits auf der Maschine:

* Klone: `tools/uft-scout/work/mkatr` (`97f6278f`),
  `tools/uft-scout/work/atrip` (`170ab327`)
* Gebaute Binaries + Hashes: oben (work/mkatr/)
* Fixtures für SCOUT-65: 3 Dateien aus `work/atrip/samples/` (Hashes
  oben) → nach `tests/corpus_free/` bzw. `tests/corpus/` + Manifest
* Fixture für SCOUT-66: Rezept unten, deterministisch aus
  `dos_dd_test1.atr` ableitbar (Ergebnis-SHA-256
  `2306031c6d972cad64ed2757b7b8aa3a51c4029d7cce9b8d8baf5d8b0f3ac5ca`)

## Aufwandsklasse

SCOUT-64: **S** · SCOUT-65: **S** · SCOUT-66: **S** (Spec+Fixture;
der eigentliche Fix ist Stufe-4-Arbeit mit Rotbeweis)

## Differenzlauf-Plan (für B1, präzisiert durch diesen Zyklus)

* **Binaries:** UFT-Testtreiber über beide In-Tree-Fassungen
  (`atari_dos2.c`-Gruppe und `uft_atari_dos.c`) einerseits, `lsatr.exe`
  (`dc7f9004…`) andererseits.
* **Korpus:** `atrcopy_dos2sd.atr` (leer — prüft Geometrie/VTOC) **+
  die drei DOS-2-Samples** (prüfen Datei-Ebene; ohne sie ist der Lauf
  auf der Datei-Ebene leer, siehe Feststellung oben).
* **Metrik:** je Abbild: erkanntes DOS-Format, Sektoren frei/gesamt,
  Verzeichnisliste (Name, Größe), SHA-256 je extrahierter Datei.
* **Toleranzen:** Namensdarstellung (8.3 vs. Unix-Form), Reihenfolge
  der Einträge; **keine** Toleranz bei Datei-Bytes und Frei-Sektoren.

## UNGEKLÄRT

* Ob UFT die Voll-Ablage-Variante (SCOUT-66) tatsächlich versetzt
  liest: Quelltextlage eindeutig (`uft_atr.c:19-25`), aber **nicht
  ausgeführt** — der Rotbeweis gehört in Stufe 4.
* atrip-Lizenzfassung (GPL-2.0-only vs. §9-Versionswahl mangels
  Datei-Köpfen): folgenlos, solange kein Code portiert wird; bei
  Übernahme der drei Sample-Abbilder als Fixtures ist die Einstufung
  „triviale generierte Musterdaten, GPL-Repo-Testdaten" eine
  **Eigentümer-Entscheidung** (Regel 8; Manifest-Eintrag mit Herkunft
  genügt nach bestehender Korpus-Praxis).
* `lsatr`-Verhalten auf beschädigten/abgeschnittenen Abbildern: nicht
  gemessen (für den Oracle-Einsatz auf intakten Fixtures nicht nötig).
* Ob atrip Code-Ableitung oder Neuschrift von atrcopy ist: für den
  Ausschluss unerheblich (gleiche Hand), daher nicht weiter verfolgt.

## Fundus (bewegt keine Kennzahl — nicht vorgeschlagen)

* Lese-Wissen für MyDOS, SpartaDOS/BW-DOS, LiteDOS, Bibo-DOS,
  Howfen-DOS, K-Boot in `lsdos.c`/`lssfs.c`/`lsextra.c`/`lshowfen.c` —
  neue Dateisysteme fallen unter das Moratorium (EINFRIER-REGEL).
* `mkatr` als hardwarefreier **SpartaFS-Korpus-Generator** (Rundlauf
  oben belegt) — sinnvoll erst, falls B1 die SpartaFS-Seite
  (`atari_sparta.c`) am Leben lässt.
* Größen-/Signaturtabellen (`disksizes.h:23-33`, MyDOS-Signaturlogik
  `lsdos.c:342-406`) als Prüfwissen für ATR-Geometrie-Heuristiken.
* atrip-Architekturidee „Container→Media→Filesystem als getrennte
  Schichten mit Signatur-Registry" — Konzept, kein konkreter Baustein.

## Anhang: Fixture-Rezept SCOUT-66 (deterministisch)

```python
src = open('atrip/samples/dos_dd_test1.atr','rb').read()   # 183 952 B
hdr = bytearray(src[:16]); body = src[16:]                 # 3*128 + 717*256
boots = [body[i*128:(i+1)*128] + b'\x00'*128 for i in range(3)]
new = b''.join(boots) + body[384:]                         # 184 320 B
par = 184320 // 16
hdr[2], hdr[3], hdr[6] = par & 0xff, (par >> 8) & 0xff, (par >> 16) & 0xff
open('dd_fullboot.atr','wb').write(bytes(hdr) + new)       # 184 336 B
# lsatr: identisches Listing, A4096.DAT unverändert d4d4073c…, rc=0
```
