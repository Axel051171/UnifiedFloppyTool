# Plan: Doku-Konsolidierung — was zusammengehört, was archiviert wird, was lügt

**Auftrag:** 37 Dateien durchgehen, Übersehenes finden, Erledigtes finden,
zusammenfassen, Überflüssiges löschen.
**Erstellt:** 2026-09-05
**Regel:** Jede Phase nennt ihre Kennzahl (Regel 9, MF-640). Jede Zahl unten
ist gemessen; wo eine Zulieferung eine andere nannte, steht beides.

---

## Phase 0 — Messung (ERLEDIGT, hier das Ergebnis)

### 0.1 Der Auftrag lässt sich so nicht ausführen — und warum das gut ist

**„Fasse die Dateien zu einer zusammen" kann nicht wörtlich gemeint sein.**
Gemessen: die 37 Dateien umfassen **1,5 MB**, davon allein `OPEN_ITEMS.md`
613 KB und `KNOWN_ISSUES.md` 514 KB. Eine Datei daraus wäre unbenutzbar.

Wichtiger: **es gibt fast nichts zu entdoppeln.** Über alle 53 Dokumente in
`docs/` gemessen (Zeilen ≥ 60 Zeichen, wortgleich in mehreren Dateien):

| Paar | wortgleiche Zeilen |
|---|---|
| `BACKLOG.md` ↔ `OPEN_ITEMS.md` | 14 |
| `FORMAT-CLASSIFICATION.md` ↔ `FORMAT_GROUPS.md` | 3 |
| **alle übrigen Paare** | **0** |

**22 Zeilen** im ganzen Bestand. Die Überschneidung ist **thematisch, nicht
textlich**: dieselbe Sache wird an mehreren Orten *verschieden* beschrieben.
Zusammenfassen hieße hier **neu schreiben**, und beim Neuschreiben von 1,5 MB
belegter Prosa geht genau das verloren, was diesen Baum ausmacht — die
Messung neben der Aussage.

> **Die Leitregel steht schon im Baum**, `docs/STAND.md:86`:
> *„Alte Listen werden nicht gelöscht, sondern umgeleitet … Was gelöscht
> gehört, sind **doppelte Zahlen**, nicht **Gedächtnis**."*
>
> Dieser Plan folgt ihr: er räumt **Zahlen** zusammen und **Familien**, nicht
> Text.

### 0.2 Was in der Liste fehlt — und es fehlt an den falschen Stellen

`docs/` hat **65 Dateien** im Wurzelverzeichnis (86 `.md` insgesamt inkl.
Unterordnern). Die Liste nennt **37**, es fehlen **28**. Kritisch ist nicht
die Zahl, sondern die Auswahl:

| Familie | genannt | vorhanden | fehlt |
|---|---|---|---|
| **M3-Dokumente** | 2 | **4** | `M3_HAL_PLAN.md`, `M3_VIRTUAL_SERIAL_BENCH.md` |
| **Grundlinien** | 5 | **8** | `macro_drift_baseline.txt`, `orphan_baseline.txt`, `shared_guard_baseline.txt` |
| **Verifikation** | 0 | 3 | `VERIFICATION_PLAN.md`, `VERIFICATION_TIERS.md`, `VERIFICATION_TIERS_FS.md` |
| **XCOPY** | 0 | 3 | `XCOPY_*.md` |
| **Quarantäne/Lizenz** | 0 | 3 | `QUARANTINE.md`, `QUARANTINE_PROCESS.md`, `LIZENZ_ANFRAGEN.md` |

Dazu **`DESIGN_PRINCIPLES.md`** — eine **geschützte Datei**
(`.claude/CLAUDE.md` §2), die nicht ohne Rückfrage angefasst wird.

**Eine Familie halb zusammenzulegen ist schlechter als gar nicht.** Wer
`M3_APPLESAUCE_TRANSPORT.md` und `M3_BUILD_QSERIALPORT.md` verschmilzt und
`M3_HAL_PLAN.md` stehen lässt, erzeugt genau die Drift, die dieser Plan
beheben soll.

### 0.3 Acht Dateien sind unantastbar

Gemessen, welche Datei ein **Skript öffnet** (nicht: erwähnt):

| Datei | Leser |
|---|---|
| `dead_fields_baseline.txt` | `scripts/audit_dead_fields.py` |
| `decl_conflicts_baseline.txt` | `scripts/audit_decl_conflicts.py` |
| `enum_tables_baseline.txt` | `scripts/audit_enum_tables.py` |
| `evidence_marker_baseline.txt` | `scripts/audit_evidence_marker.py` |
| `fdc_gaps_baseline.txt` | `scripts/audit_fdc_gaps.py` |
| `macro_drift_baseline.txt` | `scripts/audit_macro_drift.py` |
| `orphan_baseline.txt` | `scripts/orphan_module_gate.py` |
| `shared_guard_baseline.txt` | `scripts/shared_guard_gate.py` |

Alle acht laufen über `check_consistency.py`. **Umbenennen, verschmelzen oder
löschen reißt ein Tor mit.** Dazu vier weitere Maschinenleser:
`BACKLOG.md` (`audit_protection_claims.py`), `PLANNED_APIS.md`
(`mark_planned_headers.py`), `OPEN_ITEMS.md` (`sekretaer.py`),
`STAND.md` (**erzeugt** von `gen_stand.py` — Handänderungen werden
überschrieben).

**Und sie sind in bemerkenswert gutem Zustand.** Alle fünf in der Liste
genannten Grundlinien wurden Beleg für Beleg rückwärts geprüft — jeder
Bezeichner, jeder Pfad, jede Zeilenangabe in ihren Kommentarköpfen:

| Grundlinie | Wert | Belege geprüft | noch gültig | verschwunden |
|---|---:|---:|---:|---:|
| `dead_fields` | 1168 | 15 | **15** | **0** |
| `decl_conflicts` | 35 | 8 Pfade | **alle** | **0** |
| `enum_tables` | 0 (hartes Tor) | 5 | **5** | **0** |
| `evidence_marker` | 11 | 6 | **6** | **0** |
| `fdc_gaps` | 11 | 6 | **6** | **0** |

**Keine deckt Verschwundenes.** Das ist der Gegensatz zu Teil 1 dieses
Berichts, wo `API.md` 30 von 40 Bezeichnern nennt, die es nirgends gibt.
Die Grundlinien sind der gepflegteste Teil des Bestands — weil ein Tor
sie liest.

Kleine Abweichungen, alle ohne Wirkung auf den Torwert:
`dead_fields` steht auf 1168, gemessen 1167 (das Skript sagt selbst
„senken"); `evidence_marker:8` nennt „86 Einträge", das Skript misst 161;
`fdc_gaps:16-17` zählt die betroffenen Familien falsch auf (nennt PC-98,
das **nicht** betroffen ist, und lässt PC aus, das es **ist**) — und
widerlegt sich zwei Absätze später bei `:27` selbst.

### 0.4 Die beiden Riesen sind keine Dubletten voneinander

| | `OPEN_ITEMS.md` | `KNOWN_ISSUES.md` |
|---|---|---|
| Umfang | 613 KB, 7496 Z. | 514 KB, 10332 Z. |
| Gliederung | 184 Einträge (161 P3, 15 P0, 8 P1) | nach den 7 Design-Prinzipien, 11 `UFT-xx` |
| **gemeinsame Kennungen** | **0** | **0** |

Sie zu verschmelzen wäre falsch: verschiedene Kennungsräume, verschiedene
Ordnungsprinzipien. `OPEN_ITEMS.md` hat außerdem bereits Abschnitte
„Übernommen aus BACKLOG.md (MF-588)" und „Übernommen aus KNOWN_ISSUES.md
(MF-607)" — eine Zusammenlegung, die **begonnen und nicht beendet** wurde.

### 0.5 Was niemand findet

Von 61 Dokumenten im Wurzelverzeichnis nennt **niemand 15** — kein
Einstiegspunkt, kein anderes Dokument, kein Skript:

`CI_CD.md` · `FAQ.md` · `FORMAT_SOURCES.md` · `HARDENING_AUDIT.md` ·
`HARDWARE.md` (644 Z.!) · `M3_BUILD_QSERIALPORT.md` ·
`M3_VIRTUAL_SERIAL_BENCH.md` · `STUB_TO_PARSER_GUIDE.md` ·
`USER_MANUAL.md` · (+ die Grundlinien, die ihre Leser als Skript haben)

**`USER_MANUAL.md` und `FAQ.md` sind Endanwender-Dokumente.** Die ganze
M3-Familie ist von keinem Einstiegspunkt aus erreichbar:
`M3_APPLESAUCE_TRANSPORT.md` erreicht man nur über zwei Dateien, die selbst
niemand nennt.

### 0.6 Was schon gemacht wurde — die Ernte

Drei parallele Leser haben die 24 inhaltlichen Dateien vollständig gelesen
und gegen den Baum gemessen. Das Wichtigste:

| Doku sagt | Baum sagt | Beleg |
|---|---|---|
| `FORMAT-VARIANTS.md:237` ATR-512-B-Sektoren werden **auf 128 geklemmt**, „jedes 512-B-ATR wird fehlgelesen" | **behoben MF-340** | `tests/test_atr_512.c` — `ASSERT(sector_size == 512); /* not clamped to 128 */` |
| `FORMAT-VARIANTS.md:249` 42-Spuren-D64 wird **abgelehnt** | **behoben** | `tests/test_d64_42track.c`, `tests/test_d64_42_spuren.c` — **in dieser Sitzung als MF-871 mitgeschlossen** |
| `MASTER_PLAN.md:72` Emulatoren „**2 von 9**, XUM1541 in Arbeit" | **9/9** (MF-309) | `ls tests/emulators/` = 9 — **und `MASTER_PLAN.md:613` sagt es selbst** |
| `PLAN_v4.1.7.md:214/227/229` FS-Leser für D64/ADF/FAT12 fehlen | **alle drei gebaut, FS-T2** | MF-683/685/789, je gegen ein **fremd erzeugtes** Abbild |
| `PLAN_NAECHSTE_STRECKE.md` Schritte 1, 2, halb 3 | **erledigt** | MF-804, MF-806, MF-808 |
| `MAMMUT_PLAN.md:204` „offen bleibt LIC-1: SPDX MIT" | **behoben MF-580** | `uft_multiread_pipeline.c:18` = `GPL-2.0-or-later` |
| `CAPABILITIES.md:233` FM-Decoder unvollständig | **behoben MF-864/869** | `flux_decode_fm()` legt Sektoren an; 26/26 gegen eine Aufnahme von 1979 |
| `BENCH_PROTOCOL.md:53-56` fünf Controller „Wiring offen" | **vier von fünf verdrahtet** | MF-301, MF-249/250, MF-252, MF-207 |
| `M3_APPLESAUCE_TRANSPORT.md:3` „Code work pending" | **Transport + 7 Runner gebaut** | MF-249/250, `src/hardwaretab.cpp:863-895` |
| `FORMAT_GROUPS.md` | **4 Plugins fehlen ganz** | `korg`, `akai`, `lisa`, `adf_ext` existieren, stehen in keiner Gruppe |

### 0.7 Die Zahlendrift, gemessen

Dieselbe Größe, verschiedene Antworten:

| Größe | Quellen | wahr |
|---|---|---|
| **Format-Plugins** | `FORMAT_CATALOG:29`=88 · `FORMAT_GROUPS:9`=84 · `FORMAT-GAPS:4`=84 | **88** (`gen_format_list.py`) |
| **T3-Formate** | `VERIFICATION_TIERS:15`=37 · `FORMAT_CATALOG:43`=56 · `FORMAT_SOURCES:3`=68 · `BACKLOG:59`=57 · `PLAN_v4.1.7:392`=56 | **37** |
| **Roundtrip-Matrix** | `MASTER_PLAN:61`=13 · `STAND:15`=14 angeboten | **16 Einträge** |
| **Quelldateien/Header** | `MASTER_PLAN:47`=756/644 · `CLAUDE.md`=715/481 | **722/486** |
| **Attributionen** | `STAND:28`=175 · `CLAUDE.md`=88 bzw. 124 | drei Messzeitpunkte |

### 0.8 Ein Strukturfehler, der 49 Formate betrifft

`FORMAT_CATALOG.md` und `FORMAT-CLASSIFICATION.md` tragen **dieselbe
161-Zeilen-Tabelle** (beide aus der gelöschten `uft_format_registry_v2.c`)
und widersprechen sich bei **49 von 160** Namen in der Schichtangabe.
Selbst nachgezählt — die Verschiebung ist systematisch:

| `FORMAT_CATALOG` | `FORMAT-CLASSIFICATION` | Anzahl |
|---|---|---|
| `unbekannt` | **Klasse 1 — Flux** | 12 (A2R, CFI, DFI, EDD, GWRAW …) |
| `Flux` | **Klasse 2 — Bitstream** | 16 (86F, G64, G71 …) |
| `Bitstrom` | **Klasse 3 — File/Archiv** | 13 (CRT, P00, PRG …) |

Die **12 echten Flux-Formate** stehen im Katalog als „unbekannt", **16
Bitstream-Formate** als „Flux". Eine um eine Stufe verschobene Beschriftung.

*(Ein Leser nannte 33; meine eigene Zählung ergibt 49. Es gilt die
gemessene Zahl.)*

---

## Phase 1 — Dokumente, die etwas Falsches behaupten (ZUERST)

**Kennzahl:** keine der vier. **Aber:** nach der Konfliktordnung
(„Ehrlichkeit vor Vollständigkeit") rangiert eine falsche Aussage vor jeder
Aufräumarbeit. Wer zuerst sortiert und dann berichtigt, sortiert Falsches.

### 1.1 Das Applesauce-Protokoll, das kein Gerät spricht

`docs/M3_APPLESAUCE_TRANSPORT.md:51-93` trägt eine Befehlstabelle unter der
Überschrift *„Verified strings extracted from device firmware"*. Gemessen
gegen `src/hardware_providers/applesauce_serial_runners.cpp`:

| Doku | Code |
|---|---|
| `info` | `?vers` |
| `seek NN` | `head:track %d` + `head:side %d` |
| `track NN hH capture R revolutions` | `disk:readx %d` + `data:?size` |
| `motor:1` | (`sync:on` / `sync:off` um jeden Schritt) |
| `data:<N` | `data:< %u` (mit Leerzeichen) |

**Kein einziger Befehl stimmt.** Das ist keine Alterung, sondern eine
**Falschaussage über ein Drahtprotokoll**, versehen mit einer
Verifikationsbehauptung — dieselbe Klasse wie MF-787 (die Kennung `"SAD!"`,
die in keiner Datei steht).

**Aufgabe:** §1 entweder gegen `applesauce_serial_runners.cpp` neu schreiben
oder **streichen**. Ein falsches Protokoll ist schlimmer als kein
Protokoll — wer danach den nächsten Transport baut, sendet Bytes, die kein
Gerät versteht.

### 1.2 `HARDWARE.md` sagt dem Endanwender vier falsche Dinge

Die Kompatibilitätsmatrix `:9-26` ist die Tabelle, die ein Anwender **zuerst**
liest. Gemessen:

| Zeile | Behauptung | Wirklich |
|---|---|---|
| `:12` | **KryoFlux Write ✅** | `.claude/CLAUDE.md:378` `can_write = (type != KryoFlux)`; `kryoflux_provider_v2.h:12` „read-only by design". **Die einzige Stelle im Projekt, die KryoFlux Schreiben zuschreibt** |
| `:17` | ADF-Copy Write ✅ | kein `make_adfcopy_write_runner` — 5 Runner, keiner schreibt |
| `:19` | Applesauce Write ✅ | implementiert, aber **ungebencht** → 🟡 nach der eigenen Legende, nicht ✅ |
| `:20` | USB-Floppy Write ✅ | **Linux-only** (`ufi_backend.c` registriert nur unter `__linux__`) |
| `:15` | Catweasel Read/Write/Flux ✅ | **kein V2-Provider, kein HAL** — kommt in `CAPABILITIES`, `BENCH_PROTOCOL`, `REFACTOR_TASKS` nicht vor |
| `:22-26` | Fußnote: M3.1/M3.2 „wiring still pending" | **beide gelandet** (MF-254, MF-301) |

Bemerkenswert: bei Applesauce und ADF-Copy sind `HARDWARE.md` und
`CAPABILITIES.md` **gegenläufig** falsch — eine pauschale Korrektur in eine
Richtung ist damit ausgeschlossen.

**Aufgabe:** Matrix `:9-26` je Zelle gegen `src/hardware_providers/` neu
setzen; Legende ergänzen, was ein Häkchen bedeutet (heute nirgends
definiert). Die ~600 Zeilen Betriebswissen darunter (GW-Bootloader-Recovery,
Windows-COM-Kollisionen, ADF-Copy-Pufferkondensator, VID/PIDs, udev) bleiben
**unangetastet** — sie sind korrekt und stehen nirgends sonst.

### 1.3 Zwei behobene Datenverluste, die als bestehend dokumentiert sind

`FORMAT-VARIANTS.md:237` und `:249` — siehe 0.6. Eine Doku, die einen
**behobenen Datenverlust als bestehend** meldet, kostet die nächste Sitzung
Doppelarbeit oder einen Commit auf schon korrekten Code. Beide Stellen
berichtigen, mit MF-Nummer und Testnamen.

Dazu in derselben Datei zwei Selbstwidersprüche: `:73` sagt SCP-Footer-Emit
„verschoben", `:182` sagt „ERLEDIGT (MF-351)"; `:119` sagt Extended-ADF
„fehlt aktuell", `:124` sagt „✅ RESOLVED (MF-352)".

### 1.4 `API.md` beschreibt eine API, die es überwiegend nicht gibt

Der schwerste Einzelbefund. `docs/API.md` (335 Z., letzte Änderung
**2026-04-23**) ist eine öffentliche C-API-Referenz mit Quick-Start und
Codebeispielen. Gemessen:

| | |
|---|---|
| **Modul-Header** | **5 von 8 fehlen** — `uft_config_manager.h`, `uft_recovery.h`, `uft_woz_writer.h`, `uft_protection_pipeline.h`, `uft_format_detection.h` |
| **Funktionen in den Codebeispielen** | **30 von 40 kommen im ganzen Baum nicht ein einziges Mal vor** (`git grep` über `*.c`/`*.h`/`*.cpp` → 0 Treffer, nicht „keine Definition") |
| **Typen** | `uft_config_manager_t`, `uft_error_map_t`, `woz_writer_t`, `UFT_CFG_INT` … → je **0 Treffer** |
| **Build-Anbindung** | `find_package(UFT REQUIRED)` und `pkg-config --libs uft` — weder `UFTConfig.cmake` noch `uft.pc` existieren |
| **eingehende Verweise** | **genau einer**, und der betrifft nur die Versionszeile (`MASTER_PLAN.md:128`) |

Eingelöst ist ein einziger Abschnitt: `uft_d64_writer.h` mit sechs
`d64_*`-Funktionen. Zusätzlich widerspricht die Datei sich selbst — `:3`
sagt „Version 4.1.3", `:328` endet die Versionshistorie bei 4.1.0 — und
macht Thread-Safety-Zusagen über zwei Typen, von denen einer nicht
existiert und der andere keine Funktion hat.

**Und `PLANNED_APIS.md` sagt für vier dieser Header ausdrücklich das
Gegenteil** („declares functions with no implementation"). Zwei Dokumente,
dieselben Header, unvereinbare Aussagen.

**Aufgabe — und hier ist Löschen die richtige Antwort.** Eine
API-Referenz, deren Bezeichner zu drei Vierteln nicht existieren, ist
nicht „veraltet", sondern irreführend: sie lädt dazu ein, gegen etwas zu
bauen, das es nicht gibt. Der eine tragende Abschnitt (`d64_writer`)
gehört als Doxygen-Kommentar an `include/uft/uft_d64_writer.h`, wo er
mit dem Code altert. Danach `API.md` nach `docs/archive/`.

**Vorher zu klären (Eigentümer):** ob UFT überhaupt eine öffentliche
C-API anbieten will. `CLAUDE.md` führt das Projekt als **GUI-only**; eine
API-Referenz ohne Konsumenten und ohne Build-Anbindung ist dann
gegenstandslos. Diese Entscheidung fällt nicht der Aufräumende.

### 1.5 Zwei weitere Dokumente, deren Grundlage weg ist

| Datei | Befund |
|---|---|
| `PLANNED_APIS.md` | **127 von 151** genannten Header-Pfaden existieren nicht mehr. Die Datei erklärt sich selbst als „eingefroren" (`:4-5`) — das ist ehrlich —, aber ihr als **live** markierter Kopf sagt „24 Header tragen noch einen Banner", gemessen sind es **14**. Maschinenleser: `mark_planned_headers.py` |
| `HARDENING_AUDIT.md` | **6 von 8** genannten Safe-Headern fehlen; die empfohlenen Makros (`UFT_REQUIRE_NOT_NULL`, `UFT_BOUND_CHECK`, `uft_safe_mul_size_t`) haben **0 Treffer** — es gibt sie nicht mehr. Die Empfehlung ist ohne Wiederherstellung nicht ausführbar. **Der Kernbefund selbst gilt aber weiter**: `src/formats/atr/uft_atr.c:229` öffnet ohne NULL-Test, `calloc` ohne Überlaufprüfung. Zwei von den sechs fehlenden Headern sind namentlich in `SKELETON_HEADERS_AUDIT.md:72-73` als gelöscht verzeichnet — **die beiden Dokumente widersprechen sich quer, ohne dass es je auffiel** |
| `SKELETON_HEADERS_AUDIT.md` | **ERLEDIGT** — Anhang A führt 9 Skelett-Header „live", **alle 9 gelöscht**; die 13 „worst offender" ebenfalls alle 13. `audit_skeleton_headers.py` meldet **0/0**. Das Wellen-Protokoll (20 Wellen, ~90 000 LOC) ist die einzige Aufzeichnung des größten Aufräumvorgangs im Baum und **bleibt** |
| `CI_CD.md` | Fast jede prüfbare Zeile falsch: 4 statt **6** Workflows, `master`/`develop` als Trigger (nur `main`), Jobs `build-core`/`summary` **existieren nicht**, Qt 6.6.2 (heute Matrix 6.7.3/6.10.1), **MSVC** statt **MinGW**, `VERSION` statt `VERSION.txt`, `-DUFT_BUILD_GUI=OFF` — die Option **gibt es nicht**, die dokumentierte lokale CI-Simulation ist nicht ausführbar. **Niemand verweist auf die Datei** |

### 1.6 `SHOWCASE.md` — die einzige veraltete Datei, die nach außen zeigt

Referenziert aus `README.md`, `CHANGELOG.md`, `RELEASE_NOTES.md` und drei
Doku-Dateien. Kopf steht auf v4.1.5, ausgeliefert ist v4.1.6. Und die
zentrale forensische Zusage — der Wandlungsabsatz `:15-17` — hat
**fünf von fünf Zahlen falsch**:

| SHOWCASE | gemessen (`src/core/uft_roundtrip.c`) |
|---|---|
| „nur 8 gehen durch" | **14** |
| „2 verlustfrei" | **6** |
| „6 mit Zustimmung" | **8** |
| „3 unmöglich" | **2** |
| „33 abgewiesen" | **28** |

Dazu T1b=12/T2=17/T3=57 gegen die erzeugte Tafel (27/22/**37**), und ein
Selbstwiderspruch: `:39` nennt 137 Plugins, `:80` im selben Dokument 80.
`:43-46` korrigiert ausdrücklich die alte Zahl 80 — und `:80` trägt sie
unkorrigiert weiter.

**Das ist der dringlichste Punkt nach 1.1**, weil es das einzige dieser
Dokumente ist, das ein Außenstehender liest.

### 1.7 Abnahme Phase 1

- [ ] `grep -c "info\\\\n" docs/M3_APPLESAUCE_TRANSPORT.md` → 0, oder die
      Tabelle stimmt zeilenweise mit `applesauce_serial_runners.cpp`
- [ ] Kein ✅ in `HARDWARE.md:9-26` ohne Beleg in `src/hardware_providers/`
- [ ] `FORMAT-VARIANTS.md` nennt für ATR-512 und D64-42 die MF-Nummer
- [ ] `check_consistency.py` alle Tore 0

### 1.5 Anti-Muster

- ❌ **Nicht** die Betriebswissen-Abschnitte von `HARDWARE.md` anfassen.
- ❌ **Keine** Zahl korrigieren, ohne sie zu messen — die Drift in 0.7 ist
  dadurch entstanden, dass Zahlen abgeschrieben statt erhoben wurden.
- ❌ **Nicht** die Protokolltabelle „plausibel machen". Entweder aus dem
  Code abgelesen, oder weg.

---

## Phase 2 — Eine Quelle je Zahl

**Kennzahl:** keine direkt — aber die Drift in 0.7 ist die Ursache, aus der
Kennzahlen falsch berichtet werden.

Der Baum hat für jede driftende Zahl bereits einen Erzeuger:

| Zahl | SSOT | driftende Kopien |
|---|---|---|
| Plugin-Anzahl | `scripts/gen_format_list.py` | `FORMAT_GROUPS:9`, `FORMAT-GAPS:4` |
| T-Stufen | `docs/VERIFICATION_TIERS.md` (generiert) | `FORMAT_CATALOG:43`, `FORMAT_SOURCES:3,27`, `BACKLOG:59`, `PLAN_v4.1.7:392` |
| Wandlungspfade | `src/core/uft_roundtrip.c` | `MASTER_PLAN:61` |
| Dateizahlen | `find`-Befehl in `CLAUDE.md` | `MASTER_PLAN:47` |

**Aufgabe:** In jeder Kopie die Zahl durch einen **Verweis** ersetzen
(„siehe `docs/VERIFICATION_TIERS.md`, generiert"), nicht durch die heutige
Zahl. Eine korrigierte Zahl driftet wieder; ein Verweis nicht.

`scripts/update_inventory.py` führt bereits `DERIVED_CLAIMS` — dort gehören
die Stellen hinein, die eine Zahl **nennen müssen**.

### Abnahme
- [ ] `python scripts/update_inventory.py` grün
- [ ] Keine der fünf Zahlen aus 0.7 steht mehr an zwei Orten mit
      verschiedenen Werten

---

## Phase 3 — Familien zusammenlegen, nicht alles

**Kennzahl:** keine. Das ist die eigentliche „Zusammenfassen"-Arbeit — und
sie geht **familienweise**, nicht global.

| Familie | Dateien | Vorschlag |
|---|---|---|
| **Format-Katalog** | `FORMAT_CATALOG.md` + `FORMAT-CLASSIFICATION.md` | **eine** Datei. Beide tragen dieselbe 161er-Tabelle aus derselben gelöschten Quelle und widersprechen sich bei 49 (0.8). Zusammenlegen **erzwingt** die Auflösung |
| **M3** | 4 Dateien | **ein** `M3_STATUS.md` als Dach mit Einstieg; die vier bleiben, werden aber erreichbar |
| **Refactor** | `REFACTOR_BRIEF.md` + `REFACTOR_TASKS.md` | bleiben **historisch**. Aber §STOP und der Pro-Task-Vertrag (`REFACTOR_TASKS:113-149`) gelten laut `.claude/CLAUDE.md` **auf `main` weiter** → dorthin verschieben, nicht mit archivieren |
| **XCOPY** | 3 Dateien (nicht in der Liste!) | erst messen, dann entscheiden |

**Nicht zusammenlegen:** `OPEN_ITEMS.md` und `KNOWN_ISSUES.md` (0.4);
`BENCH_PROTOCOL.md` und `CLICK_SESSION_v4.1.6.md` (beide offen und fällig);
die 8 Grundlinien (0.3).

### Anti-Muster
- ❌ **Keine** Familie halb zusammenlegen (0.2).
- ❌ Die Namensgebung (`FORMAT_` vs. `FORMAT-`) **nicht** vereinheitlichen,
  ohne den Grund zu prüfen: die drei Bindestrich-Dateien tragen alle
  „Web-recherchiert 2026-07-05" und stammen aus derselben Arbeitswelle. Die
  Schreibweise kodiert eine Herkunft, die sonst nirgends steht.

---

## Phase 4 — Erledigtes archivieren, mit einer harten Nebenbedingung

**Kennzahl:** keine — aber es ist der größte Einzelposten.

`OPEN_ITEMS.md` führt **184 Einträge: 91 erledigt, 66 offen, 27 ohne
Zeichen**. Die erledigten Tabellenzeilen sind **133 KB** — gut ein Fünftel
der Datei. `docs/archive/` existiert bereits (4 Dateien).

> **NEBENBEDINGUNG, gemessen:** **14 der 91 erledigten Einträge werden aus
> `src/`, `include/` oder `tests/` heraus zitiert** — P0-14, P0-15, P1-1…4,
> P3-58, P3-59, P3-83, P3-89, P3-111, P3-116 u. a. Wer sie wegarchiviert,
> reißt 14 lebende Code-Verweise. Sie müssen einen auflösbaren Anker
> behalten: entweder bleiben, oder das Archiv wird verlinkt **und** die
> Code-Kommentare ziehen mit.

**Aufgabe:**
1. `docs/archive/OPEN_ITEMS_erledigt.md` anlegen, die 77 **nicht zitierten**
   erledigten Einträge dorthin, mit Rückverweis.
2. Die 14 zitierten bleiben in `OPEN_ITEMS.md`, in einem eigenen Abschnitt
   „erledigt, aber aus dem Code zitiert".
3. Vorher **messen**, nicht schätzen: das Skript aus Phase 0 wird zum Tor.

**Vorher zu klären:** von 82 `##`-Abschnitten tragen nur **24** eine
Statusmarke (MF-694), 58 sind „noch nicht gesichtet". Das Markensystem hat
zwei Leser und ist zu 29 % gepflegt. Archivieren ohne Marken heißt raten.

### Abnahme
- [ ] `grep -rn "P3-111\|P0-14" src/ include/ tests/` löst weiterhin auf
- [ ] `python tools/uft-innendienst/scripts/sekretaer.py` läuft (heute:
      **stürzt auf einer cp1252-Konsole ab**, bevor er etwas ausgibt —
      `python -X utf8` hilft; das ist ein eigener kleiner Fund)
- [ ] `OPEN_ITEMS.md` < 500 KB

---

## Phase 5 — Türen für 15 Dokumente, die niemand findet

**Kennzahl:** keine. Aber `USER_MANUAL.md` und `FAQ.md` sind
Endanwender-Dokumente ohne jeden Einstieg (0.5).

**Aufgabe:** `docs/STAND.md:75-84` führt bereits eine „Doku-Landkarte" mit
Rolle und Pflegehand je Dokument. Sie deckt die 15 nicht ab.
`gen_stand.py` erweitern, sodass die Landkarte **abgeleitet** ist — jede
Datei in `docs/` erscheint, mit ihrer Rolle oder mit „ohne Einstieg".

Das ist die einzige Form, die nicht wieder driftet: eine gepflegte Liste
wäre der fünfzehnte Aufzählungsfall dieses Baums.

---

## Phase 6 — Was wirklich gelöscht wird

**Gemessen: sehr wenig.** Die Löschkandidaten sind nicht die Dateien,
sondern **Aussagen**.

| Kandidat | Urteil |
|---|---|
| `PLAN_FLUXENGINE_A8RAWCONV.md` | **behalten** — alle Phasen erledigt, aber die 12-Zeilen-Prämissentabelle und die `recovered ⇒ wirklich gelesen`-Herleitung sind die **einzige** Begründung dieser Zusage im Baum |
| `BACKLOG.md` | **behalten** — Maschinenleser (`audit_protection_claims.py`); Schadensklassen A–D stehen nur hier |
| `MAMMUT_PLAN.md` | **behalten**, §4 berichtigen — §0 (sieben widerlegte Behauptungen) ist zeitlos |
| `REFACTOR_*.md` | **behalten** als historisch; §STOP nach `.claude/CLAUDE.md` |
| `M3_APPLESAUCE_TRANSPORT.md` §1 | **löschen oder neu schreiben** (1.1) |
| `API.md` | **archivieren** nach 1.4 — der `d64_writer`-Abschnitt wandert vorher als Doxygen an den Header. Eigentümer-Frage: will UFT eine öffentliche C-API? |
| `CI_CD.md` | **archivieren oder neu schreiben** — fast jede prüfbare Zeile falsch, niemand verweist darauf. Einzigartig sind nur die Badge-Vorlagen mit unersetzten `OWNER/REPO`-Platzhaltern |
| `HARDENING_AUDIT.md` | **behalten und berichtigen** — die Grundlage (6 von 8 Headern) ist weg, aber der Kernbefund gilt: `uft_atr.c:229` öffnet ohne NULL-Test. Die Inventarliste der 24 v3.7-Härtungsdateien ist die einzige Aufzeichnung dieses Bestands |
| `SKELETON_HEADERS_AUDIT.md` | **behalten**, Anhang A als erledigt markieren (9/9 gelöscht, Skript meldet 0/0). Das Wellen-Protokoll bleibt — u. a. weil es die Messfalle festhält, dass Löschen von `static inline`-Headern die Zahl *steigen* lässt |
| `PLANNED_APIS.md` | **behalten** (Maschinenleser), Kopf berichtigen: „24 Header" → **14** |

> **Kein Dokument wird gelöscht, dessen Einzigartigkeit nicht vorher benannt
> ist.** Die drei Leser haben je Datei eine „was ginge verloren"-Liste
> geliefert; sie ist die Löschbedingung.

---

## Phase 7 — Abnahme

1. `python scripts/check_consistency.py` — alle **57** Tore 0
2. `python scripts/gen_stand.py` und `gen_fs_tiers.py` nachgezogen
3. `ctest` unverändert **345/345**
4. Anti-Muster:
   - `git diff --name-only` enthält **keine** `*_baseline.txt`
   - `docs/DESIGN_PRINCIPLES.md` unverändert
   - kein Dokument gelöscht ohne Eintrag in der Löschbedingung von Phase 6
5. Je Commit: MF-Nummer aus der laufenden Zählung (**letzte: MF-887**)

---

## Reihenfolge

| Phase | warum in dieser Reihenfolge |
|---|---|
| **1** | Falsche Aussagen zuerst — wer sortiert, bevor er berichtigt, sortiert Falsches |
| **2** | Zahlen an ihre Quelle binden, bevor Dateien bewegt werden |
| **3** | Familien zusammenlegen, wenn die Zahlen stimmen |
| **4** | Archivieren zuletzt — es ist der einzige Schritt, der Text bewegt |
| **5** | Türen, wenn feststeht, was bleibt |
| **6** | Löschen erst, wenn die Einzigartigkeit gesichert ist |

## Was dieser Plan bewusst NICHT vorschlägt

- Alle 37 Dateien in eine (0.1)
- `OPEN_ITEMS.md` und `KNOWN_ISSUES.md` verschmelzen (0.4)
- Eine der 8 Grundlinien anfassen (0.3)
- `DESIGN_PRINCIPLES.md` anfassen (geschützt)
- Die Namensgebung vereinheitlichen (Phase 3, Anti-Muster)
- `BENCH_PROTOCOL.md` oder `CLICK_SESSION_v4.1.6.md` einziehen — beide sind
  **offen und fällig**, nicht veraltet
