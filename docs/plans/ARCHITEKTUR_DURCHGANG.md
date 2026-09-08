# Architektur-Durchgang — 520 572 Zeilen, in Phasen geschnitten

**Angelegt:** 2026-09-06 (MF-939)
**Anlass:** `/claude-mem:learn-codebase` — „jede Quelldatei vollständig lesen".
**Warum ein Plan und nicht einfach lesen:** siehe §1.

---

## 1. Der Grund für die Phasenteilung

Der Baum hat **1817 Dateien / 520 572 Zeilen** C/C++/H (gemessen 2026-09-06 über
`git ls-files`, nicht geschätzt). Das sind bei ~15 Token je Zeile grob **7–8 Mio.
Token** reines Lesen.

Zwei gemessene Eigenschaften machen ein Durchlesen am Stück wertlos:

1. **Der Kontext wird unterwegs zusammengefasst.** Was in Phase 1 gelesen wurde,
   ist verdichtet, bevor Phase 9 beginnt. Ein Cache, der sich selbst
   überschreibt, ist keiner.
2. **Der Ertrag liegt im Lesen MIT FRAGE.** Beleg aus dem ersten Anlauf: MF-938
   entstand nicht durch Lektüre, sondern weil in der Dateiliste
   `src/core/uft_mfm_encoder.c` gegen die Titelseiten-Behauptung „der Baum hat
   keinen MFM-Encoder" stand. Vier Monate alt, in Produktion benutzt,
   rundlaufgeprüft.

**Konsequenz:** jede Phase ist eigenständig, hat einen benannten Eingang, und
liefert ein **dauerhaftes Artefakt**, das die Zusammenfassung überlebt.

---

## 2. Was jede Phase liefert (verbindlich)

| | |
|---|---|
| **A — Gedächtnisdatei** | `memory/arch_<subsystem>.md` nach dem Muster von `arch_kernvertraege.md`: Verträge, Invarianten, benannte Fallen, Erreichbarkeit |
| **B — Registerzeilen** | jeder Befund als `P3-nnn` in `docs/OPEN_ITEMS.md`, mit Messung in der mittleren Spalte |
| **C — Abnahme** | `check_consistency.py` alle Kategorien 0, `ctest` unverändert grün |

**Ohne A ist die Phase nicht fertig.** Gelesen und nicht aufgeschrieben zählt
nicht — genau das ist die Klasse, die MF-938 gekostet hat.

---

## 3. Phase 0 — Bestandsaufnahme (ERLEDIGT)

Die Skill-Vorgabe verlangt eine Discovery-Phase vor der Umsetzung. Sie ist
**durch direkte Messung** erledigt, nicht durch Delegation — die Quellen stehen
hier, damit niemand sie annehmen muss:

| Frage | Antwort | Quelle |
|---|---|---|
| Umfang | 1817 Dateien / 520 572 Zeilen | `git ls-files '*.c' '*.cpp' '*.h' '*.hpp'` + Zeilenzählung je Datei |
| Was ist **nicht** UFTs Code | `src/samdisk` 23 068 Z. + `src/a8rawconv` 9 195 Z. | `UnifiedFloppyTool.pro:229` — beide stehen nur im **INCLUDEPATH**, in keinem `SOURCES` |
| Zentraler Vertrag | `include/uft/uft_format_plugin.h` | `struct uft_disk`, `struct uft_track`, `uft_format_plugin_t` |
| Fehlercodes | **generiert** aus `data/errors.tsv` | Kopf von `uft_error.h` |
| Registry-Grenze | `MAX_FORMAT_PLUGINS 192` | `src/core/uft_format_plugin.c:35` |
| ABI-Wächter | `sizeof(uft_format_plugin_t) == 240` | `uft_format_plugin.h`, gepinnt MF-665 |

**Erlaubte Werkzeuge (statt „erlaubte APIs"):** die Tore in `scripts/audit_*.py`
(60 Stück, jedes mit Selbsttest), `scripts/repo_scope.py` für Dateimengen,
`scripts/check_consistency.py` als Kette. **Nicht** erlaubt: gepflegte
Dateilisten (CLAUDE.md §Dateimengen, viermal belegt).

---

## 4. Die Phasen

Reihenfolge nach **Abhängigkeit**, nicht nach Größe: Verträge zuerst, denn ohne
sie liest man die Plugins falsch.

| # | Subsystem | Dateien | Zeilen | Artefakt |
|---|---|---|---|---|
| **1** | `include/uft` (Wurzel) + `src/core` | 44 + Kern-Header | ~11 000 + | `arch_kernvertraege.md` **(teilweise da)** |
| **2** | `src/flux`, `src/detect`, `src/parsers`, `src/algorithms` | 33 | 16 513 | `arch_flusspfad.md` |
| **3** | `src/fs`, `src/forensic`, `src/policy`, `src/fileops` | 13 | 5 761 | `arch_dateisysteme.md` |
| **4a** | `src/formats` — CBM | 49 | 23 122 | `arch_formate_cbm.md` |
| **4b** | `src/formats` — Atari + Apple | 64 | 24 781 | `arch_formate_atari_apple.md` |
| **4c** | `src/formats` — PC / CP-M / Japan | 42 | 20 189 | `arch_formate_pc.md` |
| **4d** | `src/formats` — Sinclair / Amstrad / BBC / 8-Bit | 54 | 18 037 | `arch_formate_8bit.md` |
| **4e** | `src/formats` — Fluss-Container **+ Wandler** | 61 | 30 469 | `arch_formate_fluss.md` |
| **4f** | `src/formats` — Rest (inkl. `dsk_generic`-Makro) | 92 | 26 016 | `arch_formate_rest.md` |
| **5** | `src/protection`, `src/analysis`, `src/recovery` | 96 | 46 391 | `arch_schutz_analyse.md` |
| **6** | `src/hal`, `src/hardware_providers` | 48 | 20 470 | `arch_hardware.md` |
| **7** | `src/gui`, `src/widgets`, `src/display`, `src/diag` | 27 | 12 568 | `arch_oberflaeche.md` |
| **8** | `src/samdisk`, `src/a8rawconv` — **Fremdcode, nicht übersetzt** | 188 | 32 263 | `arch_fremdcode.md` |
| **9** | `tests/` | 351 | 97 883 | `arch_pruefstand.md` |
| **10** | Abnahme | — | — | siehe §6 |

**Die Zahlen sind gemessen** (2026-09-06, `git ls-files` + Zeilenzählung je
Datei); die Summe der Phasen 4a–4f ergibt exakt die 362 Dateien / 142 614 Zeilen
von `src/formats`. **Die Dateimenge je Phase wird bei Phasenbeginn neu aus
`git ls-files` gezogen, nicht aus dieser Tabelle** — die Tabelle dient dem
Aufwandsschätzen, nicht der Auswahl (CLAUDE.md §Dateimengen).

**Warum die Wandler in Phase 4e liegen:** die 22 Dateien direkt in
`src/formats/` (14 619 Zeilen) sind `uft_format_convert_*.c` — der
Wandlungsverteiler. Er gehört zu den Fluss-Containern, weil dort die
Verlust-Entscheidungen fallen (`src/core/uft_roundtrip.c`, Preflight-Tor).

**Warum Phase 8 spät kommt:** `src/samdisk` und `src/a8rawconv` sind
**Lesestoff, kein Code** — sie stehen in keinem `SOURCES`. Sie zuerst zu lesen
hieße, 32 263 Zeilen fremder Umsetzung im Kopf zu haben, bevor man UFTs eigene
kennt. Genau so entsteht die Verwechslung, die MF-937 bei MGT gefunden hat
(`SAMCoupe.h` fehlt, und es fällt niemandem auf).

**Warum Phase 9 zuletzt:** Tests sind die **Behauptungen** über den Code. Sie
vor dem Code zu lesen heißt, die Behauptung für die Sache zu halten — die
Fehlerklasse aus MF-596 (32 Testdateien konnten gar nicht rot werden).

---

## 5. Wie eine Phase abläuft

Jede Phase ist **in einem frischen Kontext ausführbar**. Der Eingang ist diese
Datei plus die Gedächtnisdateien der Vorphasen.

### 5.1 Schritt für Schritt

1. **Inventar holen — delegiert.** Der Agent `uft-innendienst` misst das
   Subsystem, ohne dass die Dateien in den Hauptkontext wandern:
   Symbole ohne Aufrufer, Doku-Aussagen ohne Quelle, Fixture-Lücken.
   *Er schreibt nie nach `src/`, `include/`, `tests/`* — er liefert Dokumente.
2. **Verträge lesen.** Erst die Header des Subsystems, dann die Umsetzung.
   Der Header sagt, was zugesagt ist; die `.c` sagt, was geschieht.
3. **Vier Fragen je Datei** (das ist der „Frage"-Teil aus §1):
   - Gibt es die zugesagte Fähigkeit, oder nur ihren Namen?
   - Wer ruft das? (`git ls-files | xargs grep`, nicht raten)
   - Steht eine Zahl im Kommentar, die niemand nachgemessen hat?
   - Meldet hier etwas Erfolg für eine Arbeit, die nicht stattfand?
4. **Artefakt schreiben** (§2 A), Register ergänzen (§2 B).
5. **Abnahme** (§2 C), dann committen: `docs(arch): <Subsystem> gelesen (MF-nnn)`.

### 5.2 Prüfliste je Phase

- [ ] Gedächtnisdatei angelegt und in `MEMORY.md` verlinkt
- [ ] Jede Zahl darin ist **gemessen**, mit Befehl oder Fundstelle
- [ ] Jeder Befund als `P3-nnn` eingetragen, mit Rotbeweis-Skizze
- [ ] `python scripts/check_consistency.py` → alle Kategorien 0
- [ ] `ctest` unverändert grün (Zahl im Commit nennen)
- [ ] Was **nicht** gelesen wurde, steht benannt in der Gedächtnisdatei

### 5.3 Was NICHT getan wird (Leitplanken)

| Verboten | Warum |
|---|---|
| Aus dem Lesen heraus Format-Code schreiben | **EINFRIER-REGEL** (MF-363/498): neuer Code im Format-/Decoder-Layer braucht benannte Referenz, gemessene Zahlen, Referenz im Header |
| Eine Zahl aus einem Kommentar übernehmen | MF-930/932/938 — dreimal in einer Woche war eine weitergetragene Zahl falsch |
| Eine Datei-Liste pflegen, um „schon gelesen" zu markieren | CLAUDE.md §Dateimengen; der Fortschritt steht in den Gedächtnisdateien, nicht in einer Liste |
| Ein Feld/Flag anlegen, das niemand setzt | MF-832: ein Flag, das immer `false` ist, sagt nichts |
| Einen Befund „später" notieren ohne Registerzeile | „später" ist in diesem Baum dasselbe wie „nie" (MF-695) |
| Subagenten kaskadieren | CLAUDE.md: **eine** Spawn-Ebene, Rückgabe über CONSULT-Blöcke |

---

## 6. Phase 10 — Abnahme des Durchgangs

1. **Vollständigkeit prüfbar machen:** ein Skript zählt aus `git ls-files`, wie
   viele Dateien je Verzeichnis existieren, und hält es gegen die in den
   Gedächtnisdateien genannten Zahlen. Abweichung = Lücke, benannt.
2. **Alle Tore laufen:** `check_consistency.py` 0, `audit_selbsttest.py` ohne
   rote Fälle.
3. **Die Befundliste ist geschlossen oder eingeplant:** jede in den Phasen
   eröffnete `P3-nnn` hat entweder ✅ oder einen benannten Blocker.
4. **`MEMORY.md` führt alle Phasen-Artefakte** mit einer Zeile.

---

## 7. Aufwand, ehrlich beziffert

| | |
|---|---|
| Reines Lesen | ~7–8 Mio. Token, wenn wirklich **jede** Zeile durch den Hauptkontext geht |
| Mit Delegation an `uft-innendienst` je Subsystem | deutlich weniger im Hauptkontext — die Inventare kommen als Ergebnis zurück, nicht als Dateien |
| Wall-Clock | die Konsistenzkette braucht je Lauf mehrere Minuten; **ein Commit je Phase**, nicht je Datei |

**Nicht versprochen wird**, dass am Ende jede Zeile „im Kopf" ist. Versprochen
wird, dass für jedes Subsystem eine **nachprüfbare** Beschreibung existiert und
jeder Befund eine Registerzeile hat.

---

## 8. Stand

| Phase | Stand |
|---|---|
| 0 | ✅ erledigt (§3) |
| 1 | ◐ **Verträge vollständig, `src/core` teilweise** — zwei Artefakte liegen: `memory/arch_kernvertraege.md` (Header, ABI-Regeln) und `memory/arch_kern_src_core.md` (Registry, Öffnungspfad, Wandlungsmatrix, Preflight-Tor). Ungelesen bleiben 32 der 38 `.c`-Dateien, **namentlich in der Notiz aufgeführt** |
| 2 | ◐ **Verträge vollständig gemessen, Umsetzungen teilweise** — Artefakt `memory/arch_flusspfad.md`. Zwei Phantome gefunden (P3-222, P3-223); was ungelesen blieb, steht namentlich in der Notiz |
| 3 | ◐ **Verträge vollständig gemessen** — Artefakt `memory/arch_dateisysteme.md`. Vier Phantom-Header und 26 Header-Namenskollisionen gefunden (P3-228, P3-229) |
| 4a | ◐ **CBM gemessen** (49 Dateien, 23 122 Z.) — Artefakt `memory/arch_formate_cbm.md`. Registry sauber: 88 Plugin-Tafeln, **kein `.name` doppelt**, also fällt keines still heraus. Zwei Befunde (P3-232 GCR-Tabelle sechsfach + getestete API ohne Produktionspfad, P3-233 zweite BAM-API ohne Umsetzung und ohne Einbinder). **Nicht** gemessen: ob die Leser richtig lesen — das hängt an `VERIFICATION_TIERS.md` |
| 4b | ◐ **Atari + Apple gemessen** (62 Dateien, 24 188 Z.) — Artefakt `memory/arch_formate_atari_apple.md`. Zwei Befunde (P3-234 Apple-GCR-Tabelle siebenfach, oracle-geprüfte Einheit ohne Produktionsaufrufer, `nib` auf T3; P3-235 fünf Zusagen ohne Rumpf). Drei Prüfungen kamen **sauber** zurück: kein neuer Phantom-Header, kein Format fehlt in der Tier-Tafel (88 = 88, zwei Schreibweisen), Tor 57 auf 0. Elf Schreib-Rundlauf-Tests verschärft — **ohne Defekt**, die Vermutung „blind" wurde gemessen und verworfen |
| 4c | ◐ **PC / CP-M / Japan gemessen** (50 Dateien, 23 672 Z.) — Artefakt `memory/arch_formate_pc.md`. Ein Befund, aber ein scharfer: **P3-254** — PH-2/MF-549 entfernte **einen von fünf** gleichartigen Phantom-Headern aus einer zusammenhängenden `#include`-Gruppe und meldete den Gesamtstand; die vier übrigen (900 Z., 39 Prototypen, 0 Umsetzungen, 0 Aufrufe) stehen seit v4.1.0. `src/formats/xdf/DEFERRED.md` auf den gemessenen Stand gesetzt — der Befund war **nicht neu**, er stand seit MF-459 in `KNOWN_ISSUES` |
| 4d | ◐ **Sinclair/Amstrad/BBC gemessen** (40 Dateien, 13 316 Z.) — Artefakt `memory/arch_formate_8bit.md`. **P3-258:** die „dritte Format-Schicht" (`FloppyDevice`) ist **75 Dateien / 8207 Zeilen / 386 Funktionen**, nicht die 19 aus MF-461; 18 Funktionen haben einen externen Aufrufer, **alle 18 sind Tests**. Header-Durchgang sauber (ein Phantom, bereits P3-228) |
| 4e | ◐ **Fluss + Wandler gemessen** (56 Dateien, 30 258 Z.) — Artefakt `memory/arch_formate_fluss.md`. **P3-259:** das Quarantäne-Register driftete in beiden handgepflegten Spalten; Zeilenspalte jetzt vom Tor abgeleitet, Fähigkeitsspalte berichtigt aber weiter gepflegt. Wandler-Frage war bereits durch `test_convert_table_has_dispatch` beantwortet: **46 angeboten, 14 bedient, 32 vom Preflight abgewiesen, 0 ohne Zweig** |
| 4f | ◐ **Rest gemessen** (112 Dateien, 29 648 Z. — als Differenz zu 4a–4e gerechnet, damit nichts durchfällt) — Artefakt `memory/arch_formate_rest.md`. Schwerpunkt `dsk_generic`, das **49 der 137 Plugin-Tafeln** erzeugt: drei tote Tabellenspalten **entfernt** (Drift in 4 von 49, darunter ein Name), **P3-260** Größe widerspricht der Geometrie bei 2 von 49, **P3-261** 38 von 49 kollidieren auf der Dateigröße bei gleicher Konfidenz |
| 5 | ◐ **Schutz/Analyse/Recovery gemessen** (99 Dateien, 46 597 Z.) — Artefakt `memory/arch_schutz_analyse.md`. **P3-262:** die OTDR-Ereignis-Pipeline, 14 Dateien / 4720 Z. im Build, **0 Produktiv-Aufrufer** bei 146 Funktionen — und dem Verwaisten-Tor völlig unsichtbar, weil Tests sie rufen (dritter Beleg P3-223). **P3-263:** P0-2 beziffert — 813 Funktionen, 59 % ohne jeden Aufrufer. Header-Durchgang über 71 Header **sauber** |
| 6 | ◐ **HAL + Provider gemessen** (50 Dateien, 20 815 Z.) — Artefakt `memory/arch_hardware.md`. **Die sauberste Schicht des Durchgangs:** die Honest-Stub-Konvention hält (nur 2 leere `UFT_OK`-Rümpfe, beide legitim benannt), die Provider-Zahl stimmt (9), ein einziges Phantom — und das ist **behoben** (P3-264). Erreichbarkeit: `src/hal` 174 Fkt. (33 extern, 55 nur Test, 86 ohne) |
| 7–10 | offen |

**Aus Phase 1 gefallen — beide durch Lesen mit Frage, nicht durch Lektüre:**

* **MF-938** (`3e0025e4`) — die Titelseiten-Aussage „der Baum hat keinen
  MFM-Encoder" war seit vier Monaten falsch. Das Gegenteil stand an **drei**
  Stellen im Baum, u.a. `uft_roundtrip.c:64`: *„den Encoder, der die ganze Zeit
  im Baum lag und den niemand rief"*.
* **MF-940** (`e6ec1c40`) — `uft_fdc_gaps.h` zählte den Atari-ST-Vorspann
  doppelt. Die Grundlinien-Datei hatte die Ursache seit MF-838 **richtig
  diagnostiziert** und den Fix mangels Quelle abgelehnt; FastCopy III (1990)
  lieferte sie. Tor-Grundlinie 11 → 9.

**Was das über den Plan sagt:** beide Funde kamen aus §5.1 Schritt 3 (die vier
Fragen), nicht aus dem Durchlesen. Das ist der Grund, warum §5.1 so und nicht
anders steht.

Verwandt: [`VARIANTEN_UND_FAEHIGKEITEN.md`](VARIANTEN_UND_FAEHIGKEITEN.md),
[`TORE_BUENDEL.md`](TORE_BUENDEL.md), `docs/OPEN_ITEMS.md`.
