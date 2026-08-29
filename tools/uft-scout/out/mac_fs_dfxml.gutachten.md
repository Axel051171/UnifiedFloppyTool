# Gutachten: Macintosh-Dateisysteme und forensischer Export (Zyklus 2026-08-29)

**Repos:** `michaelengel/mfsreader` · `cul-it/hfs2dfxml` · `andete/disk-peek`
**Marken:** SCOUT-88, SCOUT-89 (vorläufig; höchste vergebene Marke war SCOUT-66)
**Inventar:** `tools/uft-scout/work/inv.json`, frisch gebaut auf HEAD `18fe2bcc`
(„OK: 88 Plugins (SSOT ok), 232 Format-Dirs, 88 Tier-Zeilen, 22 Korpus-Abbilder").
**Messungen:** `work/mfsreader.messung.json`, `work/hfs2dfxml.messung.json`,
`work/disk-peek.messung.json`.

---

## Lizenzurteil (alle drei, aus der Datei, nicht aus dem README)

| Repo | Lizenzdatei | Kennung | Zone | Datei-Header |
|---|---|---|---|---|
| mfsreader | `LICENSE` — „This is free and unencumbered software released into the public domain." | Unlicense/PD | **GRÜN** | `readmfs.c` trägt keinen Lizenz-/Copyright-Kopf |
| hfs2dfxml | `LICENSE.md` — „Copyright (c) 2015, Dianne Dietrich … Neither the name of hfs2dfxml nor the names of its contributors may be used to endorse…" (3 Klauseln) | BSD-3-Clause | **GRÜN** | keine Py-Datei trägt eigenen Lizenzkopf (grep über `hfs2dfxml/*.py`: 0 Treffer) |
| disk-peek | `LICENSE` — „MIT License / Copyright (c) 2026 Joost Yervante Damad" | MIT | **GRÜN** | keine JS-Datei trägt Lizenzkopf |

Der Auftrag verlangte, GPL-2.0-**only** vs. **-or-later** wörtlich zu
zitieren: **keines der drei Repos ist GPL-lizenziert**, die Frage stellt
sich hier nicht. Konsequenz Zone GRÜN: Code-Port mit Attribution-Header
zulässig, Konzept-Nachbau zulässig, Oracle-Nutzung zulässig.

**Vendoring geprüft:** hfs2dfxml führt ein Submodule `deps/dfxml`
(`.gitmodules`: `url = https://github.com/simsong/dfxml.git`, gepinnt auf
`e75ef197`, im Klon **nicht** ausgecheckt); `hfs2dfxml/dfxml.py` und
`hfs2dfxml/Objects.py` sind **Symlinks** dorthin, kein einkopierter
Fremdcode. Lizenz des Submodules nicht geprüft (→ UNGEKLÄRT 4; solange
niemand portiert, ohne Folge). disk-peek `js/fat12.js:4` erklärt „ported
from a proven Python FAT12 extractor used for disk preservation" **ohne
die Quelle zu nennen** — nach MF-636 wäre das bei einem Port ein
PRÜFEN-Fall; da hier nichts portiert wird, nur notiert.

---

## Frage A — MFS: die Lücke ist real (gemessen)

### Messung im UFT-Baum

* `grep -rniE "\bmfs\b" src/fs/ include/uft/` → **0 Treffer**. Ohne
  Wortgrenze trifft „mfs" nur Ro**mFS**/RCP**MFS** (Nintendo bzw.
  Remote-CP/M, `include/uft/formats/nintendo/uft_3ds.h`,
  `include/uft/formats/uft_rcpmfs.h`) — Namensgleichklang, kein Macintosh.
* `grep -rni "drSigWord\|0xD2D7" src/ include/` → **0 Treffer** (die
  MFS-Volumensignatur kommt im Baum nicht vor).
* `grep -rniE "\bhfs\b" src/ include/` → 9 Treffer, alle Kommentare/
  Partitionstabellen-Strings (`uft_diskcopy.c:247`, `uft_fat32_mbr.c:81`,
  samdisk-Vendor) — kein HFS- und erst recht kein MFS-Leser.
* `ls src/fs/` → 7 Dateien: AmigaDOS (5), FAT12 (1), Bootblock-Scanner (1).
* Die **Container** liegen dagegen vor: `dc42` ist registriertes Plugin
  auf **T2** (`docs/VERIFICATION_TIERS.md:43`, Referenz DiscFerret/
  Mini-vMac-Checksumme, MF-324), Mac-GCR-Codec-Enums existieren
  (`include/uft/core/uft_encoding.h:85-86`), MOOF-Parser liegt
  (`src/formats/apple/uft_moof_parser.c`).

**Inventar-Abfrage (zitiert):**

```
"mfs":  { "vorhanden": true, "treffer": ["rcpmfs", "uft_rcpmfs"], ... }
"hfs":  { "vorhanden": false, "abgedeckt": false, ... }
"dc42": { "vorhanden": true, "treffer": ["dc42", "dsk_dc42"], "tier": "T2", ... }
```

Der „starke" Treffer für `mfs` ist selbst ein Namensgleichklang
(`rcpmfs` = Remote CP/M File System) — derselbe Fehlertyp wie der
Rotbeweis `flux visualization`/MF-643. Die Handprüfung oben (drSigWord,
Wortgrenzen-grep) ist deshalb das Urteil, nicht die Abfrage:
**UFT hat kein MFS, und auch kein HFS.** Container ja, Dateisystem nein.

### Kann mfsreader die Referenz sein? Nein — aber ein Oracle-Kandidat mit Einschränkung

`readmfs.c` (221 Zeilen, ein File) liest Volumeninfo an Block 2,
12-Bit-Belegungskarte, flaches Verzeichnis, und extrahiert **Data- und
Resource-Fork** je Datei (`readmfs.c:35-42, 90-118`). Die autoritative
Quelle nennt nur das README: „Based on information in Inside Macintosh
volume II" (`README.md:3`) — das ist eine Spec-Attribution, kein
Code-Port, und **Inside Macintosh Vol. II** ist die Referenz, die eine
Stufe-4-Implementierung in den Header schreiben müsste. mfsreader selbst
taugt als PD-Strukturskizze, **nicht** als Referenz, denn:

1. **Belegter Bug in der Belegungskarte** (quelltextbelegt, nicht
   ausgeführt): `readmfs.c:207` rechnet
   `blkn2 = (*(bm+1) & 0x0F << 8) + *(bm+2)`. `<<` bindet stärker als
   `&`, also steht dort `*(bm+1) & 0xF00` — für ein `uint8_t` immer 0.
   **Jeder ungerade 12-Bit-Eintrag verliert sein High-Nibble**; Ketten
   auf Blöcke ≥ 256 werden falsch. Auf einer echten 400K-MFS-Diskette
   gibt es 391 Alloc-Blöcke (am `sample.img` gemessen: `drNmAlBlks=391`),
   der Bug trifft also reale Disketten.
2. Alloc-Blockgröße hartkodiert (`#define ALLOCBLOCKSIZE 1024`,
   `readmfs.c:10`) statt aus `drAlBlkSiz` gelesen; Signaturvergleich
   little-endian-abhängig (`readmfs.c:169`); ungültige Signatur ist nur
   eine Warnung, kein Abbruch.

**Folge:** als Oracle nur für die **Verzeichnisebene** (Namen,
Fork-Längen — vom Bug unberührt) tauglich, für Extraktion erst nach
Fix. Der Bug ist zugleich der beste Beweis, warum die Einfrier-Regel-
Bedingungen (benannte Referenz = Inside Macintosh II, Rotbeweis zuerst,
Referenz im Header) auch auf der Dateisystem-Ebene gelten müssen.

### Das eigentliche Fundstück: ein PD-lizenziertes echtes MFS-Abbild

`sample.img` (419 200 Byte, gemessen): **800×512 Byte Daten + 800×12 Byte
Tag-Bytes** — die kopflose DC42-Ablage; Signatur `0xD2D7` an Block 2
(Offset 0x400) selbst nachgelesen, Volume „Sample", 10 Dateien
(Apple-TechNote TN.002 + SillyBalls-Beispielcode), `drAlBlkSiz=1024`.
README bestätigt: „Some disk images have a 84 byte header (DiskCopy?)
prepended" — dieses hat keinen. Der Korpus (`inv["korpus"]`, 22 Einträge,
gegengelesen) enthält **kein einziges Mac-Abbild**; die
Korpus-Images-Spalte von `dc42` steht auf „—".

**Inhalts-Copyright beachten:** die Disketten-*Dateien* sind Apple-
Material (TechNote, DTS-Beispielcode) — die Repo-Lizenz (PD) deckt das
Abbild als Werk des Repo-Autors, nicht Apples Inhalte. Der Korpus führt
bereits historische, urheberrechtlich geschützte Abbilder gitignored mit
SHA-256-Manifest (`tests/corpus/c64pp_bountybob.g64`, Herkunft „real/
historical") — derselbe Weg stünde offen. Entscheidung: Eigentümer
(→ UNGEKLÄRT 3).

### Einhängepunkt (im Baum auffindbar)

`docs/PLAN_v4.1.7.md` Phase 1 „VFS-P1, lesend" (Zeile 188 ff.): die
Leiter führt vier Kern-Dateisysteme (CBM DOS, AmigaDOS, Atari DOS 2,
FAT12, Zeile 205-230) und sperrt neue Fähigkeiten bis dahin (Zeile 526).
**MFS ist der natürliche Kandidat Nr. 5** — kleinstes reales Dateisystem
der Liste (flach, ein Verzeichnis, 12-Bit-Karte), Oracle und Fixture
lägen dann schon bereit. Kein Vorziehen: das Plan-Gate gilt.

---

## Frage B — DFXML: sauber recherchiert, und trotzdem Fundus

### Was der Standard ist (Netzrecherche, 2026-08-29)

* **Schema:** `github.com/dfxml-working-group/dfxml_schema`. Tags per
  `git ls-remote --tags` selbst gemessen: `v1.0`, `v1.1.0`, `v1.1.1`,
  `v2.0.0-beta.0` — stabile Fassung ist **1.1.1**, 2.0 ist beta.
* **Pflege:** DFXML Working Group (Kontakt lt. Repo/NIST-Seite
  `dfxml@nist.gov`); NIST führt eine eigene DFXML-Seite, die Library of
  Congress beschreibt DFXML als Format für die digitale Bestandserhaltung
  (fdd000611). Ursprung: Simson Garfinkel (`simsong/dfxml`,
  Python/C++-Bindings heute unter `dfxml-working-group/`).
* **hfs2dfxml nennt die autoritative Quelle:** `README.md:11` und `:41`
  verweisen auf das Working-Group-Schema (Zeile 11 mit Tippfehler
  „github.org/…/dfxml-schema"); der Code erzeugt ausdrücklich
  `DFXML.DFXMLObject(version='1.1.1', …)` (`hfs2dfxml/hfs2dfxml.py:459`).

### Was hfs2dfxml technisch ist

Kein HFS-Parser: ein Python-3-Wrapper, der `hfsutils`
(`hmount`/`hls`/`hcopy`) per `subprocess` aufruft und die Textausgabe
mit Regexen parst (`hfs2dfxml.py:53-70, 77`), je Datei `md5`/`sha1`/
`libmagic` ergänzt (`:193-222`) und als DFXML-`fileobject`s ausgibt.
Letzter Commit **2018-04-20** (Messung), README selbst: „still in
development … check your results against another tool". Für ein
C/Qt-Projekt ist hier nichts zu portieren; der Wert ist die
**Feldliste** (crtime/mtime als ISO, Hashes, Forks) als Beispiel dessen,
was Archive austauschen.

### Kennzahl-Prüfung (streng, Regel 9)

Ein DFXML-Export bewegt **keine** der vier Zahlen: nicht T3 (kein
Format), nicht Wandlungspfade (Report ≠ Rundlauf-Matrix), nicht
leckende Tests, nicht Bench-Alter. Auch die begründete fünfte
(ungeklärte Herkunft) nicht. Dass die Zielgruppe Archive sind, ist ein
„wäre nützlich" — und das ist ausdrücklich kein Kriterium.
**→ Fundus, kein OPEN_ITEMS-Vorschlag.** Fürs Protokoll des Fundus:
UFT hat heute keinerlei DFXML-Erwähnung (`grep -rni dfxml src/ include/
docs/ scripts/` → 0 Treffer), und der reale Export-Bestand sind
GUI-Panels (`src/forensictab.cpp`, `src/gui/uft_otdr_panel.cpp`) plus
Provenance (`src/forensic/uft_provenance.c`) — die Sechs-Formate-Liste
in CLAUDE.md §6 ist laut MF-366 Zielbild. Sollte je ein
Berichts-Baustein in einen Plan kommen, wäre DFXML 1.1.1 der
Archiv-Anschluss; das Schema wird zur Laufzeit validiert, nicht
vendored (diskdefs-Muster).

---

## Frage C — disk-peek: Fundus, ein Absatz

Browserbasierter (100 % client-side) Viewer für MSX-`.dsk`/Atari-`.st`/
FAT12-`.img`, MIT, aktiv (letzter Commit 2026-07-17), 1 679 Zeilen JS
(gezählt). Interessant sind drei Kleinigkeiten, die UFTs
`src/fs/uft_fat12.c` (854 Zeilen) nachweislich nicht hat (grep auf
`cross|contiguous|fallback`: 0 Treffer): **Cross-Link-Erkennung**
(Cluster von >1 Datei beansprucht), **Contiguous-Fallback** bei
genullter FAT (flat dumps) und eine Größen→Geometrie-Tabelle als
BPB-Rückfall (`js/fat12.js:1-27`). Das sind Schadensfall-Ideen — und
Phase 1 beweist ausdrücklich *nicht* den Schadensfall
(`PLAN_v4.1.7.md:251`). Bewegt keine Zahl → **Fundus** (Merkposten für
die Zeit nach VFS-P1). Rest (MSX-BASIC-Detokenizer, Screen-Renderer,
ZIP-Handling) für UFT irrelevant.

---

## OPEN_ITEMS-Vorschläge (2 von max. 5 — mehr trägt dieser Zyklus nicht)

| # | Fund | Kennzahl |
|---|---|---|
| SCOUT-88 | **Erstes reales Mac-Korpus-Abbild liegt frei:** mfsreader `sample.img` (Unlicense/PD, 800×512+800×12 kopflose DC42-Ablage, MFS-Signatur 0xD2D7 an Block 2 gemessen, Volume „Sample", 10 Dateien, 391 Alloc-Blöcke). `dc42` (T2) hat heute **kein** Korpus-Abbild; 84-Byte-DC42-Kopf per Cross-Tool voranstellen (Kandidat: das bereits gebaute `hxcfe`, ungemessen) → dc42-Korpustest wird möglich. Inhalts-Copyright (Apple-TechNote/Beispielcode) wie beim Bounty-Bob-Präzedenz gitignored+Manifest lösen — Eigentümer-Entscheid | Tier-Leiter T2→T1b (`dc42`) — Präzedenz SCOUT-60 |
| SCOUT-89 | **MFS als Kandidat Nr. 5 der VFS-P1-Leiter** (nach dem Plan-Gate, `PLAN_v4.1.7.md:526`): Referenz = Inside Macintosh Vol. II (von mfsreader `README.md:3` benannt); mfsreader (PD) als Verzeichnis-Oracle registrierbar, **Extraktion ausgeschlossen** — belegter Precedence-Bug `readmfs.c:207` verliert das High-Nibble jedes ungeraden 12-Bit-Karteneintrags (quelltextbelegt, nicht ausgeführt). Fixture aus SCOUT-88 liegt dann schon | Tier-Leiter (`dc42`) mittelbar; Vorarbeit VFS-P1 |

**Differenzlauf-Plan zu SCOUT-89** (falls die Leseseite je gebaut wird):
mfsreader-Binary vs. UFT-Testtreiber, Korpus = `sample.img` (+
DC42-Wrap), Metrik = je Verzeichniseintrag (Name, DATA-Länge,
RSRC-Länge), Toleranzliste = leer; Extraktion ist aus dem Lauf
ausgenommen, bis der Karten-Bug im Oracle behoben ist.

**Beschaffungsliste** (gegen `inv["korpus"]` geprüft — nichts davon
liegt): (1) `sample.img` aus dem Klon, (2) DC42-Wrap davon per
Cross-Tool, (3) *optional* Inside Macintosh Vol. II als Spec-Quelle
(vintageapple.org/bitsavers, frei abrufbar). Keine Hardware (MF-310).

## UNGEKLÄRT

1. **DC42-Wrap-Werkzeug:** ob `hxcfe` (liegt gebaut vor, SCOUT-49) aus
   Rohdaten+Tags ein DC42 mit korrekten Prüfsummen schreibt — nicht
   gemessen. Rückfall: anderes Cross-Tool suchen; UFTs eigener
   dc42-Writer wäre **keine** unabhängige Hand.
2. **mfsreader-Build unter MinGW:** nicht versucht (POSIX-I/O, dürfte
   bauen — „dürfte" ist keine Messung).
3. **Korpus-Aufnahme trotz Apple-Inhalts-Copyright:** Eigentümer-Vorlage
   (Präzedenz: gitignored + SHA-256-Manifest wie `c64pp_bountybob.g64`).
4. **Lizenz des `simsong/dfxml`-Submodules:** nicht geprüft; ohne Folge,
   solange nichts portiert wird.
5. **Nebenbefund im Baum:** die Inventar-Abfrage `moof` liefert
   `vorhanden: false` bei `plugin_liste_vollstaendig: true`, obwohl
   `src/formats/apple/uft_moof_parser.c` und `test_moof_roundtrip`
   existieren — MOOF scheint kein SSOT-registriertes Plugin zu sein
   (womöglich absichtlich unter `woz` mitverwaltet). Von Hand prüfen,
   bevor jemand daraus eine Lücke oder eine Dublette macht.

## Negativliste

Alle drei Repos nach diesem Zyklus als `bewertet` eingetragen
(nicht `verworfen` — mfsreader trägt einen aktiven Vorschlag).
