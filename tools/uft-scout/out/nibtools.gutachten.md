<!-- uebernommen: MF-634 -->
# Gutachten: rittwage/nibtools (Zyklus 7)
<!-- uebernommen: MF-634 (Commit 87ebfe30, docs/OPEN_ITEMS.md "Scout-Zyklus 7: nibtools" Z.1698ff; Berichtigung der eigenen Zahl dort Z.1902) -->
Stand: 2026-08-28 · Messung: `work/nibtools.messung.json`
(HEAD `0abdc11505`, letzter Commit 2025-06-26, „added error check for D64 output")
· Inventar: UFT `f98e0576` (`work/inv.json`, 88 Plugins, SSOT ok, 22 Korpus-Abbilder)
· vom Eigentümer benannt (kein Negativlisten-Eintrag; Erstbewertung, Regel 6 greift nicht).

## TL;DR

nibtools ist das kanonische Werkzeug des C64 Preservation Project für
1541-GCR auf Datei-Ebene — und der Baum **benutzt es bereits als benannte
Referenz** (`docs/KNOWN_ISSUES.md:1276`, MF-382, gepinnt auf denselben
HEAD `0abdc11`). Fünf Funde, alle gemessen:

1. **Oracle, sofort einsatzfähig:** `nibconv`/`nibscan`/`nibrepair` bauen
   **ohne Hardware und ohne OpenCBM-Bibliothek** mit MinGW 13.1.0 aus dem
   Klon (heute gebaut, 0 Patches) und laufen auf unserem Korpus.
2. **Die dritte Quelle, die MF-536 ausdrücklich vermisst, existiert:**
   nibconv G64→D64 weicht von der VICE-Referenz in **denselben 3 Sektoren
   und 143 Byte-Positionen** ab wie UFTs Wandler. Heute gemessen.
3. **C64-NIB liegt im Baum und ist unerreichbar** — dieselbe Gestalt wie
   DeepRead/D64-Verzeichnis/Fluss-Widget: 1100 Zeilen nibtools-basierter
   Leser ohne Plugin, das registrierte `nib`-Plugin ist Apple-II-only.
   Dazu **drei gemessene Spec-Abweichungen** gegen die autoritative Quelle,
   eine davon verliert still den letzten Track.
4. **Lizenz-Altlast:** vier UFT-Dateien sagen „Based on nibtools" —
   Upstream ist seit 2025-01-30 GPL-3.0, davor ohne Lizenzdatei. Beides
   ist für ein GPL-2.0-Projekt ein Problem, WENN es Ports sind.
   Eigentümer-Vorlage, keine eigene Entscheidung (Regel 8).
5. **Zwei Verhaltens-Definitionen**, die der Baum ausdrücklich vermisst
   (`density_deviation`) bzw. nicht hat (GCR-Fehllese-Modelle), stehen in
   nibtools mit Datei+Zeile.

## Kategorie

**Oracle** (primär) + **Daten/Verhaltens-Spec** + ein Fall
„vorhanden-aber-unzugänglich" im eigenen Baum. Keine „besser"-Behauptung
ohne Differenzlauf-Plan (steht unten).

## Lizenz (aus Dateien, nie aus dem README)

| Ebene | Beleg | Urteil |
|---|---|---|
| Repo-Wurzel | `LICENSE` = GPL-3.0-Volltext; Commit `a549c18` „Create LICENSE", 2025-01-30 | **GELB** |
| `md5.c` | Kopf: „GNU General Public License … either version 2 … or (at your option) any later version" (Christophe Devine) | GPL-2.0-or-later — für UFT irrelevant, eigene Hashes vorhanden |
| `crc.c` | Kopf: „placed into the public domain" (Michael Barr, 2000) | PD |
| `gcr.c`, `read.c`, `write.c`, `prot.c`, `fileio.c`, `bitshifter.c`, `lz.c` | nur Copyright-Zeilen (Brenner/Rittwage/Menge/Geelnard), **keine eigene Grant-Klausel** | Repo-Lizenz GPL-3.0 gilt |

**Zone: GELB → Konsequenz: KEIN Code-Port. Erlaubt: Verhaltens-Spec und
Oracle-Binary.** Die Vermutung im Auftrag („vermutlich GPL-2, grüne
Zone") trifft **nicht** zu — die Lizenzdatei sagt GPL-3.0, und vor
2025-01-30 hatte das GitHub-Repo gar keine (= alle Rechte vorbehalten).
Für die Oracle-Nutzung ist das unschädlich: verglichen wird die Ausgabe,
es wandert kein Code ein (Muster aus `tests/differential/oracles.py`).

## Was das Inventar sagt (Abfragen zitiert)

```
nib  → vorhanden: true, treffer: [nib, nib_nbz], tier: T3
nbz  → vorhanden: true (nur über nib_nbz)
nb2  → abgedeckt: false  → von Hand geprüft (s.u.)
g64  → vorhanden: true, tier: T1
d64  → vorhanden: true, tier: T1b
gcr  → vorhanden: true, treffer: [gcr, uft_c64_gcr]
halftrack / track cycle → abgedeckt: false → von Hand geprüft (s.u.)
```

Handprüfung hinter den Treffern (Regel 4 — Name ≠ Fähigkeit):

* Das **registrierte** `nib`-Plugin ist **Apple II**: `src/formats/nib/uft_nib.c:5`
  („Apple II NIB nibble format core"), Probe verlangt exakt
  `file_size == 232960` (`uft_nib.c:26-27`). Eine Commodore-NIB
  (0x100 + n·0x2000, Kennung `MNIB-1541-RAW`) fällt durch.
* `nib_nbz` ist ein **6-Zeilen-Platzhalter**: `src/formats/apple/nib_nbz.c:6`
  („placeholder").
* Kein registriertes Plugin im Baum kennt die Kennung `MNIB-1541-RAW`
  (Suche über `src/` + `include/`: einzige Treffer in
  `src/formats/c64/uft_nib_format.*`).
* Halftracks/Track-Cycle/GCR-Alignment: **vorhanden** in
  `src/formats/c64/uft_gcr_ops.c` (818 Z.), `src/protection/c64/uft_track_align.c`
  (1175 Z., kompletter Alignment-Katalog inkl. vmax/vmax_cw/pirateslayer/
  rapidlok/fat/bitshifted) — beide „Based on nibtools". Track-Align hat
  **nur Test-Aufrufer**; die GCR-Ops laufen über `ProtectionAnalysisWidget`
  (instanziiert in `src/statustab.cpp:411`).

## Fund 1 — Oracle: nibconv/nibscan/nibrepair bauen hardwarefrei und laufen auf unserem Korpus

**Gemessen heute (alle Befehle ausgeführt, nicht vermutet):**

* Build: `gcc -I include/WINDOWS/ -D WIN32 -std=c99 …` (MinGW 13.1.0)
  übersetzt `nibconv`, `nibscan`, `nibrepair` aus je `main + gcr.c prot.c
  fileio.c crc.c md5.c lz.c` — **ohne** `-lopencbm`, ohne Patch. Die
  OpenCBM-Header liegen im Repo (`include/WINDOWS/opencbm.h`); die
  Hardware-Objekte (`drive.o read.o write.o ihs.o`) braucht nur
  nibread/nibwrite (`GNU/Makefile:78-118`).
* `nibscan vice_c1541_35trk.g64`: Dichte-/Längenprofil je Spur
  (7692/7142/6666/6250 an den korrekten Zonen), 0 Fehler, 0 Bad-GCR,
  **BAM/DIR-CRC 0xB5D4A0F0, Full-CRC 0x24D55663**.
* `nibscan c64pp_bountybob.g64` (reale geschützte Diskette): Fehlerkarte
  je Sektor, 244022 Bad-GCR-Bytes, 40 Spuren mit Nicht-Standard-Dichte
  (`density:1!=0?`), Halbspuren gelistet.
* SHA-256-Anker: `nibconv.exe f98c05c3…`, `nibscan.exe ad95eea3…`
  (Build aus `0abdc11`, MinGW 13.1.0; `VERSION` ist nur ein Build-Datum
  → Registry-Eintrag braucht `version_is_unaskable` + SHA-256, der
  Mechanismus existiert seit MF-629).

**Welche Behauptung entscheidet es?** (Regel: ein Oracle, das nichts
entscheidet, gehört nicht in die Liste)

1. „UFT dekodiert die Sektoren einer G64 richtig" — nibscan/nibconv sind
   eine von VICE **unabhängige Hand**. `docs/PLAN_v4.1.7.md:224-230`
   verlangt für D64 ausdrücklich ein zweites Werkzeug, weil `c1541`
   Korpus-Erzeuger UND Oracle ist. nibscan liefert BAM/DIR-CRC und
   Full-CRC über die dekodierten Spuren — das Spur-/Disk-Level-Analogon
   zu `flophashes` (kein Datei-Level; das bleibt floptool).
2. „diese Spur trägt Killer/Fat/RapidLok/Nicht-Standard-Dichte" —
   nibscan zählt diese Signale auf Dateibasis (`nibscan.c:532-536, 630-636`).
3. Die NIB-Familien-Struktur selbst — nibtools **definiert** NIB/NB2/NBZ
   (`fileio.c:424-452` read_nib, `:455-586` read_nb2, LZ in `lz.c`).

**Einschränkung (dieselbe-Hand-Regel):** die beiden realen Korpus-G64
(`c64pp_*`) stammen aus Rittwages eigener Sammlung — gegen DIESE Dateien
ist nibtools keine Zweitmeinung, gegen die VICE-erzeugten schon.
Interaktiv-Fallen für die Automatisierung: nibconv fragt bei
existierender Ausgabedatei `Overwrite? (y/N)` (`nibconv.c:119-124`),
nibrepair fragt je Reparatur (`nibrepair.c:312ff`) → Oracle-Nutzung nur
mit frischem Ausgabepfad; nibrepair nur lesend/diagnostisch verwenden.

**Aufwand: S** (Registry-Eintrag + Build-Rezept dokumentieren).

## Fund 2 — Die dritte Quelle für MF-536 existiert, und sie stimmt positionsgenau überein

`src/core/uft_roundtrip.c:146-181` (MF-536) lässt bei G64→D64
ausdrücklich offen, ob UFT oder die VICE-Referenz recht hat: „das
entschiede eine dritte Quelle, und die gibt es hier nicht."

**Heute gemessen:** `nibconv vice_c1541_35trk.g64 → out.d64`
(174848 B, sha256 `6cb37b5d…`) gegen `tests/corpus_free/vice_c1541_35trk.d64`:

```
143 Byte verschieden, 3 Sektoren betroffen:
  Spur 17/Sektor 0: 127 Byte
  Spur 18/Sektor 0:   6 Byte   (BAM)
  Spur 18/Sektor 1:  10 Byte   (Verzeichnis)
```

Das sind **exakt die Sektoren und Byte-Zahlen aus MF-536**. Die ersten
Abweichungen (Offset 0x15001ff.) sind PETSCII-Fälle: VICE-D64 `55 46 54`
(„UFT"), G64-dekodiert `75 66 74` — die Differenz steckt also im
G64-Inhalt selbst, nicht im Wandler.

**Vorsicht vor dem Fehlschluss:** UFTs `uft_d64_g64.c` ist selbst „Based
on nibtools" — die Übereinstimmung zweier verwandter Implementierungen
ist schwächer als zwei unabhängige Hände. Sie beweist aber, dass der
**heutige Upstream** (13 Jahre weitergepflegt, letzter Commit 2025-06)
an denselben Stellen dasselbe liest, und sie verschiebt die offene Frage
von „wer wandelt richtig?" zu „warum schreibt c1541 zwei Fassungen?".

**Differenzlauf-Plan (Pflichtfeld, da Urteils-Behauptung):**
* Binaries: UFTs Wandler über `tests/test_convert_roundtrip_measured.c`
  (belegter Pfad) und `nibconv.exe` (SHA-256 `f98c05c3…`, Build-Rezept oben).
* Korpus: `tests/corpus_free/vice_c1541_35trk.g64` (liegt), zusätzlich
  `c64pp_bountybob.g64` + `c64pp_aliensyndrome.g64` für den Fehlerfall.
* Metrik: Byte-Identität der beiden D64-Ausgaben; bei Abweichung Liste
  (Offset, UFT-Wert, nibconv-Wert), Sektor-zugeordnet.
* Toleranzliste: leer auf der VICE-Disk (erwartet: identisch); auf den
  geschützten Disks sind Differenzen in Sektoren mit Fehlercode zulässig
  und einzeln zu begründen (Fehlerbyte-Konventionen D64 error map).

**Aufwand: S** (eine Messung, dann Matrix-Kommentar in Stufe 4 ergänzen).

## Fund 3 — C64-NIB: das Können liegt im Baum, der Zugang fehlt — und drei Spec-Abweichungen

Dieselbe Gestalt wie DeepRead (MF-627), Plattform-Profile (MF-628),
D64-Verzeichnis (MF-629), Fluss-Widget (MF-630) — der fünfte Fall:

* `src/formats/c64/uft_nib_format.c` (1100 Z., „Based on nibtools",
  2026-01-16) liest NIB **und** NB2 **und** NBZ (eigene LZ77-Fassung),
  hat einen grünen Test (`test_nib_format`, CMakeLists:126) — aber
  **keinen Produktions-Aufrufer**: außerhalb von Datei+Test bindet
  niemand `uft_nib_format.h` ein.
* `uft_disk_open()` kann deshalb **keine einzige Commodore-NIB öffnen**;
  Wandlungstabelle (`uft_format_convert_tables.c:161-222`) kennt NIB nur
  als Apple-Format (NIB↔DSK/WOZ). `UFT_FORMAT_NBZ` existiert als ID
  (`uft_types.h:152`) ohne Plugin.

Spec-Abgleich des vorhandenen Moduls gegen die autoritative Quelle
(`fileio.c` @0abdc11) — **drei gemessene Abweichungen:**

| # | UFT (`uft_nib_format.c`) | nibtools | Folge |
|---|---|---|---|
| a | `track < NIB_MAX_TRACKS` (=84) verwirft Eintrag 84; Arrays `track_data[84]` (Index 0–83) (`uft_nib_format.h:96-99`, `.c:398`) | Halbspur-Index läuft 2…84, Puffer `(MAX_HALFTRACKS_1541+2)`=86 Slots (`gcr.h:21-23`, `nibscan.c:31-34`) | eine 42-Track- oder Halbspur-NIB verliert **still die letzte Spur**. Rotbeweis-Fixture liegt: nibconv-erzeugte NIB hat 83 Einträge bis Halbspur 84 (Messung unten) |
| b | `density &= ~NIB_FLAG_MATCH` — behält 0x20/0x40/0x80 (`.c:396`) | `track_density[track] %= BM_MATCH` — behält **nur Bits 0–1**, alle Flaggen fliegen (`fileio.c:442-443`) | Leser-Semantik weicht vom Format-Eigner ab; Flag-Erhalt ist als Metadatum vertretbar, muss aber als **Abweichung dokumentiert** sein, nicht als Spec |
| c | `capacity_min/max = {6100,6500,7000,7500}/{6400,6850,7300,7900}` (`.c:33-34`) | `{6183,6598,7073,7616}/{6311,6726,7201,7824}` bzw. `DENSITY/305…/295`-Variante (`gcr.c:78-86`) | UFTs Schranken entsprechen **keiner** der beiden nibtools-Fassungen — Zahl ohne Quelle, genau die Klasse aus MF-498(b) |

**Einfrier-Regel, präzise:** (a)–(c) sind **Spec-Korrekturen gegen eine
autoritative Quelle + Bugfix an Bestehendem** — ausdrücklich erlaubt.
Das **Registrieren** eines C64-NIB-Plugins ist dagegen „neue
Registrierung" im Sinn von MF-363/498 und bleibt hinter dem Moratorium
(nach dessen Erfüllung: 1:2, kostet zwei Hebungen). Der regelkonforme
Weg für Stufe 4 steht damit vollständig: benannte Referenz =
`rittwage/nibtools @0abdc11 fileio.c:424-452` (Referenz in den Header),
Rotbeweis zuerst = das nibconv-erzeugte Fixture unten gegen
`nib_load_buffer()` (muss an (a) **rot** sein), jede Zahl gemessen.

**Fixture-Rezept (heute ausgeführt):**
`nibconv.exe vice_c1541_35trk.g64 out.nib` → 680192 B = 0x100 + 83·0x2000,
Kennung `MNIB-1541-RAW`, Version 3, Halftrack-Flag 1, Einträge (2,3),(3,0),
(4,3)… bis Halbspur 84, sha256 `1f98dce4…`. Inhalt ist UFT-eigenes
Korpus-Material (von c1541 erzeugt), die Struktur stammt vom
Format-Eigner selbst — als Format-Konformanz-Fixture ist genau das die
richtige Hand; für Dekodier-Korrektheit gilt sie als dieselbe Hand wie
nibtools und braucht dann eine Zweitmeinung.

**Aufwand: M** (Spec-Fixes S; Plugin-Frage Eigentümer + Moratorium).

## Fund 4 — Lizenz-Altlast: vier Dateien „Based on nibtools" in einem GPL-2.0-Baum

Nachgemessen: `src/formats/c64/uft_nib_format.c:5`, `uft_gcr_ops.c:5`
(„Based on nibtools gcr.c"), `uft_d64_g64.c:5`,
`src/protection/c64/uft_track_align.c:5` („Based on nibtools by Pete
Rittwage and Markus Brenner"; `:479` „based on nibtools prot.c
align_rl_special()") — alle datiert 2026-01-16, im v4.1.0-Release
eingeflossen. Upstream war zu diesem Zeitpunkt bereits GPL-3.0
(LICENSE-Commit `a549c18`, 2025-01-30); davor führte das Repo **keine**
Lizenzdatei. Beide Lesarten — Ableitung von GPL-3-Code oder Ableitung
von unlizenziertem Code — sind mit unserer GPL-2.0-`LICENSE` (ohne
„or later") unverträglich; **falls** es dagegen unabhängige
Verhaltens-Nachbauten sind, ist nichts zu tun. Ob „based on" hier
Port oder Nachbau heißt, ist eine Urheberrechts-, keine Codefrage →
**Eigentümer-Vorlage nach Regel 8**, dieselbe Familie wie SCOUT-18
(MF-620), nur mit echtem Fremdursprung. Möglicher Lösungsweg neben
Löschen/Neuschreiben: Rittwage um Freigabe unter GPL-2 bitten (er hat
die Korpus-Sammlung bereits ausdrücklich freigegeben, s.
`KNOWN_ISSUES.md:1297ff`) — auch das entscheidet der Eigentümer.
MF-620s Zensus konnte das nicht sehen: die Dateien tragen keinen
GPL-3-SPDX, nur die „Based on"-Zeile.

**Aufwand: S** (Entscheidungsvorlage; die Folgen je nach Entscheid nicht).

## Fund 5 — Zwei Verhaltens-Definitionen mit Datei+Zeile

* **`density_deviation`:** `docs/KNOWN_ISSUES.md:1267-1273` lässt das
  Feld bewusst null — „braucht … eine Definition, die dieses Projekt
  noch nicht gegen eine autoritative Quelle belegen kann". nibtools
  definiert sie: gespeicherte Dichte ≠ `speed_map[track/2]`-Vorgabe
  (`nibscan.c:532-536`), aufsummiert als „tracks with non-standard
  density" (`:636`). Auf `c64pp_bountybob.g64` heute gemessen: 40
  solche Spuren. Das ist derselbe Weg, auf dem MF-382 `has_custom_sync`
  aus nibtools belegt hat — der Baum kennt das Verfahren bereits.
* **GCR-Fehllese-Modelle:** `nibrepair.c:228-230` benennt zwei
  1541-typische Leseverwechslungen — Tri-Bit `01110→01000` und
  Low-Frequency `10010→11000` — und repariert Header-/Daten-Prüfsummen
  gezielt darum herum (`repair_GCR_sector`, `:224-410`). Suche über
  `src/` + `include/`: kein Treffer für dieses Fehlermodell in UFTs
  Recovery-Schicht (CRC-Korrektur ja, GCR-bitmuster-spezifisch nein).
  Kandidat für eine Verhaltens-Spec des Recovery-Layers — **kein**
  Format-Code, aber Decoder-nah, also gilt auch hier: Referenz + Rotbeweis.

**Aufwand: S** (Spec-Texte), Umsetzung Stufe 4.

## Einhängepunkte (bestehende Pläne, kein „wäre schön")

| Fund | Ort |
|---|---|
| 1 | `docs/PLAN_v4.1.7.md` Phase 1 Nr. 1 (D64) — dort steht die Forderung nach dem zweiten Werkzeug wörtlich (`:224-230`); Registry `tests/differential/oracles.py` |
| 2 | `src/core/uft_roundtrip.c:146-181` (MF-536, „AUSDRÜCKLICH OFFEN") |
| 3 | EINFRIER-Rückstands-Regel (`docs/VERIFICATION_PLAN.md`): `nib` steht auf T3; Spec-Korrektur ist erlaubte Arbeit; Plugin-Frage an `docs/MASTER_PLAN.md`-Governance |
| 4 | `docs/OPEN_ITEMS.md` Lizenz-Familie SCOUT-18/SCOUT-19 (MF-620/621) |
| 5 | `KNOWN_ISSUES.md:1267ff` (PROT-Familie, MF-382-Verfahren) |

## Beschaffungsliste (gegen `inv["korpus"]` geprüft)

* **Nichts vom Eigentümer zu beschaffen.** Oracle = Klon + MinGW-Build
  (Rezept oben, heute verifiziert). G64/D64-Korpus liegt
  (`vice_c1541_35trk.*`, `c64pp_*`). NIB-Fixture ist aus liegendem
  Korpus erzeugbar (Rezept + sha256 oben) — nicht anfordern.
* Optional, nur falls ohnehin vorhanden: reale NIB-Dumps aus der lokal
  liegenden C64PP-10th-Anniversary-Sammlung (der Eigentümer hat sie laut
  `KNOWN_ISSUES.md:1297ff` lokal) — als echte, nicht selbsterzeugte
  NIB-Konformanz-Fälle.

## OPEN_ITEMS-Vorschläge (5 von max. 5, nach Priorität)

> Vorschlagsblock — Übernahme nach `docs/OPEN_ITEMS.md` durch einen
> Menschen; ein Vorschlag ist kein Eintrag.

| # | Vorschlag | Messquelle |
|---|---|---|
| SCOUT-20 | **nibconv/nibscan als hardwarefreies C64-Zweitoracle registrieren.** Beide bauen aus `rittwage/nibtools @0abdc11` mit MinGW 13.1.0 ohne OpenCBM-Bibliothek und liefern auf dem Korpus Dichteprofil, Fehlerkarte, Bad-GCR-Zählung und BAM/DIR-+Full-CRC über dekodierte Spuren — die von Plan v4.1.7 Phase 1 (D64) geforderte zweite Hand neben `c1541`. Registry-Eintrag braucht `version_is_unaskable` + SHA-256 (`nibconv f98c05c3…`, `nibscan ad95eea3…`), da `VERSION` nur ein Build-Datum ist; GPL-3 unschädlich, es wird nur Ausgabe verglichen. | Build+Läufe 2026-08-28, `tools/uft-scout/out/nibtools.gutachten.md` Fund 1 |
| SCOUT-21 | **MF-536 mit der dritten Quelle nachmessen.** `nibconv G64→D64` weicht von der VICE-Referenz in exakt den 3 Sektoren/143 Byte-Positionen aus MF-536 ab (T17/0:127, T18/0:6, T18/1:10; sha256 der Ausgabe `6cb37b5d…`); Stufe 4: UFT-Ausgabe wertgleich gegen nibconv stellen und den Matrix-Kommentar von „keine dritte Quelle" auf das Messergebnis heben. Vorsicht: `uft_d64_g64.c` ist selbst „Based on nibtools" — als verwandte Hand kennzeichnen. | Messung 2026-08-28, Gutachten Fund 2 + Differenzlauf-Plan |
| SCOUT-22 | **C64-NIB-Leser: drei Spec-Abweichungen gegen die autoritative Quelle beheben (einfrier-frei), Plugin-Frage dem Eigentümer.** `uft_nib_format.c` verliert still Halbspur 84 (Arrays 84 Slots, Index läuft bis 84), behandelt Dichte-Flags anders als `fileio.c:442` und führt Kapazitätsschranken, die keiner nibtools-Fassung entsprechen; Rotbeweis-Fixture aus nibconv liegt (83 Einträge bis Halbspur 84, sha256 `1f98dce4…`). Registrierung als Plugin ist „neue Registrierung" unter MF-363 und wartet aufs Moratorium (danach 1:2). | Gutachten Fund 3, Tabelle a–c mit Datei+Zeile |
| SCOUT-23 | **Lizenz-Entscheidungsvorlage: vier Dateien „Based on nibtools" (GPL-3 bzw. vorher lizenzlos) in einem GPL-2.0-Baum.** `uft_nib_format.c`, `uft_gcr_ops.c`, `uft_d64_g64.c`, `uft_track_align.c` (alle 2026-01-16); ob Port oder Verhaltens-Nachbau, entscheidet der Eigentümer — Optionen: Herkunft klären/Rittwage-Freigabe erbitten/neuschreiben. MF-620s SPDX-Zensus konnte es nicht sehen, die Dateien tragen keinen fremden SPDX. | Gutachten Fund 4, Header-Zitate + LICENSE-Commit `a549c18` |
| SCOUT-24 | **`density_deviation` nach nibtools definieren, GCR-Fehllese-Modelle als Recovery-Spec aufnehmen.** Definition: gespeicherte Dichte ≠ `speed_map`-Vorgabe (`nibscan.c:532-536`; auf `c64pp_bountybob.g64` gemessen 40 Spuren) — derselbe Beleg-Weg wie MF-382; dazu Tri-Bit-/Low-Frequency-Verwechslung (`nibrepair.c:228-230`) als benanntes Fehlermodell, das UFTs Recovery-Schicht nachweislich nicht führt. | Gutachten Fund 5 |

## UNGEKLÄRT

* Ob UFTs G64→D64-Ausgabe **wertgleich** zu nibconvs ist — heute nur
  Positions-/Anzahl-Gleichheit gegen die VICE-Referenz belegt; der
  UFT-Lauf selbst ist Stufe-4-Arbeit (SCOUT-21).
* Ob „Based on nibtools" Port oder Nachbau bedeutet — nicht aus dem Code
  entscheidbar ohne Zeile-für-Zeile-Vergleich; bewusst dem Eigentümer
  vorgelegt (SCOUT-23).
* nibtools' Lizenzlage **vor** 2025-01-30 außerhalb dieses GitHub-Repos
  (SourceForge-/Distributions-Historie, mnib-Erbe) — nicht geprüft, hier
  zählt nur die Datei im Repo.
* Semantik-Abgleich der nibscan-Fehlercodes (E2/E4 …) gegen UFTs
  D64-Error-Map — nicht verglichen.
* Funktion-für-Funktion-Parität von `uft_track_align.c` gegen `prot.c`
  @0abdc11 (13 Jahre Upstream-Pflege seit dem Port-Datum unklar) — nur
  Namensbestand geprüft, keine Verhaltensdifferenz gemessen.
* `bitshifter.c` (Arnd Menge, „ALPHA") — KryoFlux-orientierte
  Bitshift-Ausrichtung; ob UFTs `align_bitshifted_track`-Fassung dem
  entspricht: ungemessen.

## Regeln, die für diesen Fund gelten

* Zone GELB: NUR Verhaltens-Spec + Oracle — kein Code-Port (GPL-2.0-inkompatibel).
* Kein Code aus diesem Agenten (AGENT.md Regel 1).
* nibread/nibwrite (Hardware, XUM1541) sind NICHT Teil des Oracle-Vorschlags.
