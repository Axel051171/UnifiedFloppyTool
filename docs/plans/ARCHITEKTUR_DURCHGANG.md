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
| 1 | ◐ begonnen — `arch_kernvertraege.md` liegt, `src/core` erst zu ~40 % gelesen |
| 2–10 | offen |

**Bereits aus Phase 1 gefallen:** MF-938 (`3e0025e4`) — die Titelseiten-Aussage
„der Baum hat keinen MFM-Encoder" war seit vier Monaten falsch.

Verwandt: [`VARIANTEN_UND_FAEHIGKEITEN.md`](VARIANTEN_UND_FAEHIGKEITEN.md),
[`TORE_BUENDEL.md`](TORE_BUENDEL.md), `docs/OPEN_ITEMS.md`.
