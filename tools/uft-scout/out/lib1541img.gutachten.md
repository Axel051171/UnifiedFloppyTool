<!-- uebernommen: MF-648 -->
# Gutachten: excess-c64/lib1541img

**Zyklus:** 2026-08-28 · Inventar `work/inv.json` (erzeugt 2026-08-28 17:41,
UFT-HEAD `cf5fa96f`, SSOT ok) · Messung `work/lib1541img.messung.json`
(67 Dateien, Zone GRUEN) · Klon-HEAD `face2dd` (50 Commits,
2020-01-06 bis 2020-05-31 — seit sechs Jahren ruhend).
**Hinweis zur Erstellung:** `gutachten.py` wegen Ratenbremse nicht
verwendet; dieses Gutachten ist von Hand nach `playbook/03_gutachten.md`
geschrieben. Es setzt keine neue SCOUT-Marke.

## TL;DR

Kleine (5 441 LOC), sechs Jahre ruhende, **eigenständige** C11-Bibliothek
für D64 mit CBM-DOS-Dateisystem. **BSD-2-Clause — Zone GRÜN**, damit die
erste CBM-Quelle dieses Projekts, aus der Stufe 4 sogar Code mit
Attribution portieren dürfte. Der wertvollste Ertrag ist keine neue
Fähigkeit, sondern eine **Verhaltens-Referenz**, die einen konkreten
Fehler in UFTs eigenem D64-Parser sichtbar macht: `d64_parse_bam` liest
bei 40-Track-Abbildern die Disknamen-Bytes als BAM-Einträge der Spuren
36–40. Zwei Vorschläge, beide am bestehenden PH1-1-Baustein verankert.
Die Briefing-Annahme „D64/D71/D81, GCR" trifft nicht zu: das Repo kann
**nur D64**, kein D71/D81, kein GCR.

## Lizenzurteil

* `LICENSE.txt`: BSD-2-Clause („Copyright (c) 2020, Excess"; zwei
  Klauseln — Quell- und Binär-Redistribution — keine Advertising-/
  Endorsement-Klausel). Gelesen, nicht aus dem README geschlossen.
* Quelldateien tragen **keine** eigenen Lizenzköpfe (geprüft:
  `src/lib/1541img/d64reader.c`, `zc45reader.c` — beginnen direkt mit
  Includes); kein Vendoring im Quellbaum, die Repo-Lizenz gilt
  durchgängig.
* Submodul `zimk` (`.gitmodules`) ist reines Build-System (GNU-make-
  Includes), nicht geklont, Lizenz **UNGEKLÄRT** — für eine
  Verhaltens-Referenz irrelevant, vor einem etwaigen Binary-Build zu
  prüfen.
* **Zone GRÜN** nach `playbook/lizenzmatrix.md`: Code portierbar mit
  Attribution-Header (samdisk-Muster), Konzept nachbaubar, Oracle
  zulässig.

## Die Unabhängigkeitsfrage (fünfte Registrierungsfrage, ORACLES.md)

**Dritte, eigene Hand — belegt.**

* `grep -rni "vice\|nibtools\|based on\|port of\|adapted" src include`
  im Klon: **0 Treffer**.
* Keine vendorten Verzeichnisse, keine Fremd-Includes; einzige
  Abhängigkeit ist die C-Standardbibliothek (README: „plain ISO C (C11)
  with no dependencies").
* Autor ist die Demogruppe Excess (Zirias/Felix Palmen) — weder
  VICE-Team (erzeugt unseren Korpus: `tests/corpus_free/vice_c1541_*`)
  noch MAME (floptool) noch nibtools.

Damit wäre lib1541img die dritte unabhängige Hand für CBM-DOS neben
c1541 (Erzeuger!) und floptool. **Aber:**

## Oracle-Tauglichkeit: eingeschränkt

Das Repo baut **nur eine Bibliothek** (`src/lib/1541img/1541img.mk`,
keine `main()`, keine Tests, kein CLI-Werkzeug). Kein Konsolen-Werkzeug
= kein Verzeichnis-Listing, keine Hashes je Datei ohne eigenen Harness.
Der Differenzlauf-Standard aus MF-629 (`flophashes`-Niveau) ist damit
nicht direkt erfüllbar. Ein Registry-Eintrag in `ORACLES.md` ist
**nicht** gerechtfertigt, solange kein Binary existiert. Was bleibt, ist
die stärkere Rolle: **benannte Verhaltens-Referenz** (Zone GRÜN, im
Header zitierbar — genau das, was die Einfrier-Regel für Stufe 4
verlangt). Der GUI-Aufsatz des Autors (v1541commander) wurde nicht
untersucht — siehe UNGEKLÄRT.

## Was es liest, das UFT (erreichbar) nicht liest

### 1. Erweiterte BAM-Varianten mit Sondierung — und der UFT-Befund

lib1541img liest die BAM der Spuren 36–40 an den **richtigen** Offsets
und **sondiert**, welche Variante vorliegt:

* DolphinDOS: `bam + 0x1c + 4*track` → Spur 36 bei **0xAC**
  (`src/lib/1541img/cbmdosvfsreader.c:91-97`, Schreibseite
  `cbmdosfs.c:69-73`)
* SpeedDOS: `bam + 0x30 + 4*track` → Spur 36 bei **0xC0**
  (`cbmdosvfsreader.c:99-105`, `cbmdosfs.c:75-77`)
* PrologicDOS: inline `4*track`, Diskname um **+0x14** verschoben
  (`cbmdosvfsreader.c:50, 107-113`, `cbmdosfs.c:79-86`)
* Sondierung mit Konsistenzprüfung gegen die aus den Dateiketten
  errechnete Belegung (`cbmdosvfsreader.c:395-435, 562-567`)
* 40 **und 42** Spuren (`include/1541img/cbmdosfsoptions.h:53-61`)

**UFT dagegen, gemessen:** `src/formats/d64/uft_d64_parser_v3.c`
akzeptiert 40-Track-Größen (196608/197376, `d64_is_valid_size`
:524-550), aber `d64_parse_bam` (:1029-1066) läuft
`track = 1..disk->tracks` mit `entry_off = 4 + (track-1)*4`. Für
Spur 36 ist das `4 + 140 = 144 = 0x90` — **exakt der Offset, an dem
dieselbe Funktion drei Zeilen später den Disknamen liest** (:1055,
`bam + 0x90`). Bei jedem 40-Track-Abbild sind die „BAM-Einträge" der
Spuren 36–40 also Disknamen-/ID-Bytes, und `free_blocks` ist Zufall.
Die Namen SpeedDOS/DolphinDOS kennt UFT nur als Marketing-Einträge
(`src/formats/uft_format_names_extended.c:139-159`) und
Geometrie-Zeile (`src/formats/uft_format_versions.c:52,195`), nirgends
als BAM-Layout (`git grep -in "speeddos\|dolphindos\|prologic" src
include`: nur ROM-Erkennung und Namenslisten). Der erreichbare Plugin-
Pfad (`uft_d64_plugin.c`) liest gar keine BAM (nur Größen-Geometrie,
:66-71) — der Fehler sitzt genau in dem Parser, den **PH1-1**
(`docs/PLAN_v4.1.7.md`, Nachtrag 1, MF-629) verdrahten will. 42-Track
(205312/206114) nimmt der Plugin-Pfad an (`uft_d64_plugin.c:36-40,68`),
der v3-Parser lehnt es ab (`d64_is_valid_size` kennt nur 35/40) — zwei
Hände im eigenen Baum, die sich widersprechen.

### 2. ZipCode 4-pack/5-pack (lesen UND schreiben)

`zc45reader.c` (199 LOC), `zc45writer.c` (234 LOC), Kompressor/
Extraktor (91 LOC), `zcfileset` — das klassische C64-Archivformat
(`1!name` … `5!name`). UFT: Inventar `zipcode` → `abgedeckt: false`;
Handprüfung `git grep -iln zipcode -- src include` → **0 Treffer**.
Nicht vorhanden. **Fundus** (Moratorium: ein neues Format bewegt keine
Kennzahl nach unten, es hebt T3).

### 3. LyNX-Archive — und eine falsche Behauptung im UFT-Baum

`lynx.c` (496 LOC) erstellt und entpackt C64-LyNX-Archive. UFT:
Inventar `lynx` → `abgedeckt: false`; Handprüfung: `src/formats/misc/
lnx.c` ist **Atari Lynx** (Kopf, Zeile 3), und
`src/formats/cbm/uft_cbm_formats.c` **behauptet** LyNX im Kopf
(Zeilen 3, 13), implementiert aber keine einzige Lynx-Funktion
(Funktionsliste gemessen: 27 Funktionen, keine davon Lynx). C64-LyNX
selbst: Fundus (Moratorium). Die falsche Kopf-Behauptung: Vorschlag 2.

### 4. REL-Dateien mit Side-Sektoren, beidseitig

Lesen `cbmdosvfsreader.c:156-204` (inkl. Fall Datenspur 0 bei REL,
:294), Schreiben inkl. Side-Sektor-Kettenaufbau `cbmdosfs.c:459-548`.
UFT hat die Felder in Headern (`include/uft/formats/c64/
uft_bam_editor.h:122`, `uft_c64_bam.h:132`, `uft_cmd.h:110`), der
v3-Parser ein `rel_track`-Feld (:362); ob eine Side-Sektor-Kette je
verfolgt/validiert wird: UNGEKLÄRT.

### 5. Schreibseitige Authentizität (Fundus)

Spur-Allokationsstrategien (CBM-Original „nächste Spur zu 18",
Trackload ab Spur 1, `CFF_TALLOC_*`) und die **originale
CBM-DOS-Interleave-Eigenart** (bei Wrap über Sektor 0 dekrementieren,
`cbmdosfsoptions.h:38-45`) — relevant, falls UFT je authentische D64
erzeugen will. `CFF_RECOVER` (:66): Rekonstruktion kaputter
Dateisysteme beim Lesen. Alles Fundus.

## Was es NICHT kann (Ehrlichkeit, README §„Not supported" + grep)

Kein G64/GCR, kein GEOS, keine Trackloader, kein C128-Bootsektor,
**keine D64-Error-Info** (wird beim Speichern stillschweigend
verworfen — README-Warnung; UFTs v3-Parser führt Error-Bytes,
`uft_d64_parser_v3.c:45-47, 405`). Kein D71/D81 (`grep -rn "d71\|d81"`
im Klon: 0 Treffer in Code). Für Forensik ist lib1541img damit
**unter** UFTs Anspruch — sein Wert ist das CBM-DOS-Dateisystemwissen,
nicht die Abbild-Treue.

## Nebenbefund im UFT-Baum (Bereitschafts-Messung, Nachtrag-1-Muster)

`src/formats/cbm/uft_cbm_formats.c` (959 LOC, Kopf: „Based on
cbmconvert by Marko Mäkelä (GPLv2+)" — Attribution **mit** Lizenz,
Zone GRÜN, aber ohne SPDX-Zeile) implementiert D64/D71/D81-Offsets,
Verzeichnislesen, T64 lesen/schreiben — und hat **null Aufrufer**
(`git grep -ln uft_cbm_disk_open\|uft_cbm_print_directory -- src
include tests` außerhalb der Datei: leer). Das ist neben dem
v3-Parser (MF-629) der **zweite türlose CBM-Verzeichnisleser** im
Baum — Fall Nr. „Können im Baum, Zugang fehlt" in der Reihe aus
`PLAN_v4.1.7.md` Nachtrag 1. D71 und D81 stehen auf T1b (Inventar).

## Kategorien

| Fund | Kategorie |
|---|---|
| Extended-BAM-Layouts + Sondierung (Dolphin/Speed/Prologic, 40/42) | **Verbesserung** (Referenz für PH1-1-Fix; Differenzlauf-Plan unten) |
| UFT-BAM-Fehler bei 40 Spuren | Befund im eigenen Baum (aus der Referenz ableitbar) |
| ZipCode 4/5-pack | Daten/Fundus (Moratorium) |
| LyNX-Archiv | Daten/Fundus (Moratorium); Kopf-Korrektur separat |
| REL-Side-Sektoren beidseitig | Daten/Fundus, bis UNGEKLÄRT geklärt |
| Schreibstrategien/Interleave/`CFF_RECOVER` | Daten/Fundus |
| lib1541img als Oracle-Binary | **nicht tauglich** ohne Konsolen-Werkzeug |

## Vorschläge (2 von max. 5)

### V1 — PH1-1-Nachtrag: Rotbeweis „40-Track-BAM liest den Disknamen"

Bevor PH1-1 (`docs/PLAN_v4.1.7.md`, Nachtrag 1) den D64-Verzeichnispfad
verdrahtet: Rotbeweis, dass `d64_parse_bam`
(`src/formats/d64/uft_d64_parser_v3.c:1029-1066`) bei einem
40-Track-Abbild für Spuren 36–40 Disknamen-Bytes als BAM liest
(Arithmetik: `4+(36-1)*4 = 0x90` = Namensoffset derselben Funktion,
:1055). Benannte Referenz für den Fix: lib1541img
`cbmdosvfsreader.c:88-115` + `cbmdosfs.c:55-94` (BSD-2-Clause, Zone
GRÜN — Attribution-Header nach samdisk-Muster zulässig; Layouts:
DolphinDOS 0xAC, SpeedDOS 0xC0, PrologicDOS inline mit Namensversatz
0x14). **Kennzahl:** ungeprüfte Formate (T3) — **runter**, mittelbar:
D64-Hebung T1b→T1 ist Moratoriums-Bedingung (VERIFICATION_PLAN
§Einfrier-Regel); ein Differenzlauf, der auf 40-Track-Abbildern falsche
Blocks-free ausgibt, würde die Hebung entweder scheitern lassen oder —
schlimmer — mit falschem BAM-Ausgang bestehen. Aufwand: **S**.

### V2 — Eigentümer-Vorlage: zwei türlose CBM-Verzeichnisleser, eine Tür

`uft_d64_parser_v3.c` (MF-629) und `uft_cbm_formats.c` (959 LOC,
cbmconvert-Ableitung GPLv2+, null Aufrufer) lesen beide
CBM-Verzeichnisse; keiner ist erreichbar. Entscheidung nach
Verwaisten-Regel (`docs/plans/README.md` §3): welcher wird die
PH1-Tür (v3 für D64; deckt `uft_cbm_formats.c` D71/D81 ab, die sonst
neu gebaut werden müssten?), der andere bekommt `# ANKER:` oder wird
gelöscht. Dazu zwei kleine Wahrheitspflichten an `uft_cbm_formats.c`:
Lynx-Behauptung aus dem Kopf entfernen (Zeilen 3, 13 — nicht
implementiert) und SPDX-Zeile zur bereits erklärten
GPLv2+-Attribution ergänzen (Grundsatz MF-636, Liste LIZ-1).
**Kennzahl:** ungeprüfte Formate/Tier-Leiter — D71/D81 stehen auf T1b;
ein erreichbarer Verzeichnisleser plus floptool-`flophashes` (liest
d71 laut MF-623) ist ihr Hebungsweg. Einhängepunkt:
`docs/plans/TORE_BUENDEL.md` (Tagesordnung) + PH1-1. Aufwand:
Vorlage **S**, Verdrahtung **M**.

## Differenzlauf-Plan (zu V1, Pflicht nach Regel 7)

* **Fixture:** ein 40-Track-D64 mit SpeedDOS- und eines mit
  DolphinDOS-BAM (Beschaffung unten). Kein solches liegt im Korpus
  (Inventar `korpus`: nur `vice_c1541_35trk.d64`).
* **Seiten:** UFT-v3-Parser (nach PH1-1-Verdrahtung) vs. floptool
  `flopdir/flophashes d64 cbmdos` — **sofern** floptool erweiterte BAM
  versteht (UNGEKLÄRT, vorab messen); sonst Schiedsrichter = von Hand
  nachgerechnete Belegung aus den Dateiketten (dieselbe Konsistenz-
  Idee wie `cbmdosvfsreader.c:562-567`).
* **Metrik:** Blocks-free gesamt + frei je Spur (1–35 und 36–40
  getrennt ausgewiesen), Verzeichnisliste, Hash je Datei.
* **Toleranz:** keine — Zählwerte sind exakt.
* **Erwartung heute:** UFT liefert für Spuren 36–40 Werte aus
  Disknamen-Bytes; der Lauf ist **rot**, bevor der Fix existiert
  (Rotbeweis zuerst).

## Beschaffungsliste (gegen `inv["korpus"]` geprüft)

| Was | Wozu | Liegt schon? |
|---|---|---|
| 40-Track-D64 mit SpeedDOS-BAM | V1-Rotbeweis/Differenzlauf | nein (Korpus hat nur 35-Track-D64 von c1541) |
| 40-Track-D64 mit DolphinDOS-BAM | dito, zweites Layout | nein |
| optional 42-Track-D64 (205312) | Plugin-vs-v3-Widerspruch | nein |
| ZipCode-4-pack eines bekannten D64 | nur falls ZipCode je aus dem Fundus geholt wird | nein |

Erzeugungsweg, falls kein historisches Abbild auffindbar: Stufe 4 darf
einen Mini-Harness gegen lib1541img bauen (BSD-2; `CbmdosFs_create`
mit `CFF_40TRACK|CFF_SPEEDDOSBAM`) — dann ist lib1541img Erzeuger und
darf im selben Lauf **nicht** Schiedsrichter sein (fünfte Frage,
MF-644).

## UNGEKLÄRT

1. Versteht floptool (MAME cbmdos) SpeedDOS/DolphinDOS-BAM auf
   40-Track-Abbildern? Nicht gemessen — vor dem Differenzlauf messen.
2. Kann VICE c1541 40/42-Track-Abbilder mit erweiterter BAM erzeugen?
   Nicht gemessen.
3. Verfolgt/validiert irgendein UFT-Pfad REL-Side-Sektor-Ketten
   (Felder existieren in drei Headern)? Nicht gemessen.
4. Lizenz des `zimk`-Submoduls (Build-System; für Verhaltens-Referenz
   irrelevant).
5. v1541commander (gleicher Autor, GUI-Aufsatz auf lib1541img): hat es
   ein CLI, das die Oracle-Frage neu öffnet? Nicht untersucht.
6. Behandelt `uft_cbm_formats.c` (cbmconvert-Ableitung) die erweiterte
   BAM korrekt? Nicht in der Tiefe gelesen — ohne Aufrufer derzeit
   ohne Folge, für V2 relevant.

## Negativlisten-Eintrag (vom Übernehmenden einzutragen)

`excess-c64/lib1541img`: status **bewertet** — „Gutachten liegt vor:
BSD-2-Referenz für erweiterte BAM (Dolphin/Speed/Prologic, 40/42
Spuren), ZipCode/LyNX im Fundus; als Oracle untauglich (kein CLI);
ruht seit 2020." (Nicht selbst eingetragen: drei parallele Zyklen
schreiben potenziell dieselbe JSON.)
