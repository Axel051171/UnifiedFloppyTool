# Plan — die C64-Schreibseite und der blinde Fleck der Verwaisten-Grundlinie

> **Stand: 2026-09-05.** Entstanden aus der Prüfung der Berichte UFT-08
> (CSDb-D64-Sammlung), UFT-09 (Copydisc), UFT-10 (64Copy/Schepers) und
> UFT-15 (Orphan-Baseline), gehalten gegen den Baum.
>
> Verbindlich bleiben: [`DESIGN_PRINCIPLES.md`](DESIGN_PRINCIPLES.md), die
> Einfrier-Regel in der Fassung MF-498
> ([`VERIFICATION_PLAN.md`](VERIFICATION_PLAN.md) §Einfrier-Regel) und
> Regel 9 (jeder Baustein benennt seine Kennzahl, `CLAUDE.md`).
>
> Offene Einzelpunkte gehören nach [`OPEN_ITEMS.md`](OPEN_ITEMS.md), nicht
> hierher. Dieser Plan ordnet nur die Reihenfolge.

---

## Warum dieser Plan

Die vier Berichte waren als Formatrecherche gedacht. Gemessen kam etwas
anderes heraus: **auf der C64-Schreibseite stehen zwei Fehler, die kein
Test fangen konnte, weil die Prüfstände die falschen Zahlen festnagelten.**
Dazu ein struktureller Befund, der erklärt, warum solche Stellen im Baum
überdauern.

Kein Punkt dieses Plans stammt aus einem Bericht allein. Jeder ist am Baum
oder an der Aufnahme `tests/corpus_free/vice_c1541_35trk.g64` nachgemessen;
wo eine Berichtsaussage nicht trug, steht das dabei.

---

## Phase 0 — Die belegte Grundlage

**Diese Phase ist abgeschlossen.** Sie ist hier festgehalten, damit die
folgenden Phasen ihre Quellen nicht neu suchen müssen.

### 0.1 Die maßgeblichen Referenzen

| Aussage | Quelle | Ort |
|---|---|---|
| Speed-Zonen der 1541 | Peter Schepers, G64-Spezifikation Rev. 1.9 | `docs/format_specs/commodore/G64.TXT:285-291` |
| dieselben, unabhängig | VICE 3.10 `c1541`, echte Aufnahme | `tests/corpus_free/vice_c1541_35trk.g64` |
| Zonentabelle im Baum (**richtig**) | SSOT seit MF-434 | `src/formats/cbm/uft_cbm_geometry.c:30-35`, `:64` |
| D64-Fehlerbytes | Schepers Rev. 1.11, nach Immers/Neufeld | `docs/format_specs/commodore/D64.TXT:419-566` |
| Rücknahme der Odd/Even-Regel | Schepers selbst | `docs/format_specs/commodore/INTRO.TXT:123` |

### 0.2 Was die Aufnahme sagt (selbst ausgelesen, nicht zitiert)

```
Spur  Laenge  SPEED
   1    7692      3
  17    7692      3
  18    7142      2
  24    7142      2
  25    6666      1
  30    6666      1
  31    6250      0
  35    6250      0
```

### 0.3 Erlaubte Schnittstellen (gemessen, nicht angenommen)

```c
/* src/formats/cbm/uft_cbm_geometry.c, Header:
   include/uft/formats/cbm/uft_cbm_geometry.h */
int    uft_cbm_speed_zone   (uft_cbm_family_t family, int track);
size_t uft_cbm_track_capacity(uft_cbm_family_t family, int track);
int    uft_cbm_gap_length   (uft_cbm_family_t family, int track);
int    uft_cbm_sectors_per_track(uft_cbm_family_t family, int track);
int    uft_cbm_total_blocks (uft_cbm_family_t family, int tracks);
int    uft_cbm_max_track    (uft_cbm_family_t family);
```

`UFT_CBM_1541` ist die Familienkennung. **Keine dieser Funktionen erfinden
oder um Parameter ergänzen** — die Liste ist am Header gemessen.

### 0.4 Was NICHT stimmt an den Berichten

Damit niemand darauf aufbaut:

* **Die Odd/Even-Tail-Gap-Regel ist im Baum nicht vorhanden.** Gesucht in
  `src/`, `include/`, `tests/`: `tail_gap` = 0 Treffer, `(sector & 1)` im
  Gap-Kontext = 0 Treffer. Der Gap wird **spur**-indiziert gesetzt
  (`src/formats/c64/uft_d64_g64.c:803-806`), nie sektor-indiziert.
* **Der 1581 hat keinen µPD765.** UFT-08 begründet einen Gap-Eintrag damit;
  der Baum führt den 1581 selbst als MFM/WD-Familie
  (`include/uft/detect/mfm_detect.h:201`). Die Begründung trägt nicht.
* **`uft_dms.c` ist nicht verwaist.** UFT-15 F2 nennt es als Kandidaten,
  der in der Grundlinie fehlt. Es fehlt dort, **weil** es seit MF-837 einen
  Aufrufer hat: `src/formats/dms/uft_dms_plugin.c:115,123` ruft
  `dms_unpack()`. Der Bericht bot zwei Lesarten an; richtig ist eine dritte.
* **`g64_export_d64()` schneidet bei 35 ab, nicht bei 40.** UFT-10 sagt
  „Spuren 41/42 werden verworfen"; gemessen ist die Grenze fest bei 35
  (`uft_g64_parser_v3.c:1643,1663`). Der Bericht untertreibt.

---

## Phase 1 — Die invertierte Speed-Zone-Tabelle (MF-877) · **läuft**

### Was

`src/formats/g64/uft_g64.c` führt eine eigene Speed-Zone-Tabelle, die
gegenüber Spezifikation **und** Aufnahme invertiert ist — einschließlich der
Kommentare („Tracks 1-17 (schnellste)", die Spezifikation sagt *slowest*).
Sie wird über `g64_track_speed[]` (`:62-70`) an `:446` in **erzeugte
Dateien geschrieben**, und `g64_create` hängt am registrierten Plugin
(`.create`, Registry `src/formats/format_registry/uft_format_registry.c:238`).

Der zweite G64-Schreiber im Baum ist richtig: `src/formats/c64/uft_d64_g64.c:58`
holt den Wert aus der SSOT.

### Wie — KOPIEREN, nicht neu erfinden

Das Muster steht in `uft_d64_g64.c:58`:

```c
speed_map[t] = uft_cbm_speed_zone(UFT_CBM_1541, t);
```

Die eigene Tabelle **löschen**, nicht korrigieren. Der Baum führt diese
Zahlen bereits elffach (Messung Phase 3); eine zwölfte richtige Kopie ist
schlechter als keine.

### Abnahme

- [ ] `tests/test_g64_speedzonen.c` ist gegen den Vorzustand **rot**
- [ ] danach grün, und liest die Speed-Tabelle aus einer über
      `uft_format_plugin_g64.create` **erzeugten** Datei zurück
- [ ] Gegenprobe im Test: die vier Zonen sind nicht alle gleich, und
      Spur 1 unterscheidet sich von Spur 31
- [ ] Vergleich gegen `vice_c1541_35trk.g64`, wenn vorhanden; sonst
      **benannt** übersprungen
- [ ] `ctest` vollständig grün, `check_consistency.py` 0

### Nicht tun

- Die Zahlen 0/1/2/3 „passend" umsortieren, ohne die Quelle zu nennen.
- `g64_track_sizes[]` stehen lassen, falls es keinen Leser hat — erst
  messen (Kommentare zählen nicht als Nutzung), dann entscheiden.

---

## Phase 2 — Die aufgerundeten Zonenlängen (MF-878) · Werte berichtigt, Rotbeweis offen

### Was

`{ 6250, 6667, 7143, 7692 }` an zwei Stellen, mit der Zusage *„A
VICE-written G64 stores exactly these lengths."* VICE schreibt 6666 und
7142. Es sind die aufgerundeten Nominalwerte (7142,85 → 7143), also
gerechnet statt gemessen.

* `src/protection/ufm_c64_metrics.c:35` — berichtigt
* `include/uft/uft_c64_gcr.h:147` — berichtigt (Funktion ohne Aufrufer)
* `tests/test_c64_metrics_corpus.c:79-83` — nagelte die falschen Werte fest,
  unter dem Kommentar *„Pinned against the lengths VICE actually wrote"*.
  Liest jetzt die Aufnahme wirklich.

### Ehrlich zur Tragweite

`track_length_ratio` war für echte Spuren der Zonen 1 und 2 um **1,4·10⁻⁴**
zu klein. Die Schwelle für „lange Spur" liegt bei 1,02
(`src/protection/ufm_c64_scheme_detect.c:102`), die Anzeige rundet auf drei
Nachkommastellen. **Es hat sich nichts falsch verhalten — falsch war die
Aussage.** Das gehört in den Commit, sonst liest sich der Fix größer als er ist.

### Abnahme

- [ ] Rotbeweis nachholen: mit `{6250, 6667, 7143, 7692}` muss
      `zone_tables_match_1541_geometry` **umfallen**
      (der erste Versuch lief gegen einen Zwischenstand des Bauverzeichnisses
      und ist ungültig — er zählt nicht)
- [ ] `ctest` vollständig grün

### Nicht tun

- Parallel zu einem laufenden Subagenten im selben Bauverzeichnis bauen.
  Genau daran ist der erste Rotbeweis gescheitert.

---

## Phase 3 — Elf Kopien, vier Zählweisen, eine Entscheidung

### Was gemessen ist

Die vier Zonengrößen liegen **elffach** im Baum, in **vier** Zählweisen:

| Ort | Werte | Indexrichtung |
|---|---|---|
| `src/formats/g64/uft_g64_parser_v3.c:62-65` | 7692/7142/6666/6250 | Zone 3 = Spur 1-17 |
| `src/formats/g64/uft_g64_parser_v3.c:68-71` | dieselben, **unbenutzt** | Zone 3 = Spur 1-17 |
| `src/formats/g64/uft_g64_parser_v2.c:190-195` | dieselben | Zone 3 = Spur 1-17 |
| `src/formats/cbm/uft_cbm_geometry.c:64` | `{6250,…,7692}` | Index = Speed |
| `src/formats/c64/uft_gcr_ops.c:83` | dieselben | Index = Speed |
| `src/formats/g64/uft_g64.c:55-59` | dieselben | **„Zone 0" = Spur 1-17** ⚠ |
| `src/formats/uft_d64_writer.c:43-48` | dieselben | **„Zone 0" = 21 Sektoren** ⚠ |
| `src/protection/c64/uft_c64_protection.c:365-369` | if-Kette | — |
| `include/uft/xdf/uft_xdf_dxdf.h:49-52` | dieselben | `DXDF_ZONE1..4` |
| `tests/flux_gen/xum1541/flux_gen.c:62-65` | dieselben | Index = Speed |
| `include/uft/uft_c64_gcr.h:147` + `ufm_c64_metrics.c:35` | **6667/7143** | Phase 2 |

### Die Entscheidung, die ansteht

Zusammenführen auf `uft_cbm_track_capacity()` ist richtig, aber es ist
**keine Ein-Commit-Arbeit**: die vier Zählweisen sind nicht mechanisch
ineinander überführbar, und zwei Kopien liegen in Headern, die andere
Header einbinden.

**Vorschlag zur Reihenfolge, nicht zur Ausführung in dieser Strecke:**

1. Ein **Tor** vor der Zusammenführung: alle Fundstellen der vier Zahlen
   sammeln und auf einem Wert festhalten (Muster: `scripts/audit_fat_boundary.py`,
   MF-875 — mit Kommentar-Strip, Selbsttest, Grundlinie).
2. Erst danach Kopien entfernen, je eine pro Commit, jede mit Rotbeweis.

Ohne Schritt 1 ist Schritt 2 die Sorte Aufräumen, die still eine fünfte
Zählweise einführt.

### Kennzahl

Bewegt **keine** der vier Release-Kennzahlen. Nach Regel 9 damit
**Fundus, nicht Auftrag** — bis eine der Kopien nachweislich falsch liest
(zwei tun das bereits, siehe Phase 2 und die zwei ⚠-Zeilen oben).

---

## Phase 4 — Die Gap-Längen: eine Zusage ohne Prüfstand

### Was gemessen ist

`src/formats/cbm/uft_cbm_geometry.c:30-35` führt die Gap-Längen
**9 / 19 / 13 / 10**. An der Aufnahme gemessen (0x55-Läufe zwischen
Datenblockende und nächstem Sync): **8 / 17 / 12 / 9**.

Der Kopfkommentar `:27-29` behauptet dazu, der Erzeuger liefere
*„byte-identical G64 output against the c1541 reference image"*. Der Test,
der das belegen soll, vergleicht **UFTs Blob-Pfad gegen UFTs Plugin-Pfad**
(`tests/test_convert_via_plugin.c:140-157`) — zwei eigene Hände. Kein Test
im Baum vergleicht ein *erzeugtes* G64 byteweise mit der Aufnahme.

nibtools führt wieder andere Werte (`gcr.c:37-45`: 10 / 17 / 11 / 8).

### Was zuerst zu tun ist — und was NICHT

**Zuerst: die Zusage prüfen, nicht die Zahlen ändern.**

Ein Test, der ein erzeugtes G64 byteweise gegen `vice_c1541_35trk.g64`
hält. Er wird vermutlich rot. **Das ist das Ergebnis**, nicht der Anlass,
die Gap-Längen anzupassen: G64.TXT sagt selbst (`:433-440`), die
Tail-Gap-Länge hänge von Laufwerk, Drehzahl und Formatierprogramm ab. Eine
Abweichung gegen *eine* Aufnahme belegt nicht, dass 9/19/13/10 falsch sind
— sie belegt, dass die Zusage „byteidentisch" nicht trägt.

**Nicht tun:** die Gap-Längen auf 8/17/12/9 setzen, weil eine Aufnahme das
sagt. Das wäre dieselbe Ein-Quellen-Ableitung, die dieser Plan an drei
anderen Stellen berichtigt.

### Kennzahl

Berührt die **Wandlungsmatrix**: D64→G64 steht als verlustfreier Pfad
(MF-532). Ist die Byte-Identität nicht belegt, ist die Einstufung zu prüfen.
Das ist die einzige Phase hier, die eine der vier Zahlen bewegen kann.

---

## Phase 5 — Die Verwaisten-Grundlinie: 9 veraltet, 0 Anker

### Was gemessen ist

* `docs/orphan_baseline.txt`: **223** echte Einträge, **0** Zeilen mit
  `# ANKER`. Nach der eigenen Regel (`:33`) sind damit **alle 223
  Löschkandidaten**. Die Regel ist niedergeschrieben und nie angewendet.
* **0 Karteileichen** — alle 223 Pfade existieren und werden gebaut.
* **9 Einträge veraltet.** `scripts/orphan_module_gate.py` meldet das bei
  jedem Lauf (214 gemessen gegen Grundlinie 223). Davon:
  * 5 sind inzwischen **von Tests** gerufen
  * 1 ist **echt verdrahtet** (`uft_stx_air.c` ← `uft_stx_plugin.c`)
  * 3 sind **Messartefakt**: `strip_anon_namespace()` (MF-663) leert ihre
    Exportmenge, sie fallen vor der Prüfung heraus. **Kein Fortschritt.**
* `docs/QUARANTINE.md:62` führt `uft_stx_air.c` weiterhin als
  „0 Aufrufer, steht in orphan_baseline.txt". Trägt seit der Verdrahtung nicht.

### Reihenfolge

1. Die 6 echten Einträge austragen, die 3 Messartefakte **drin lassen** und
   den Grund danebenschreiben — sie sind nicht erreichbar geworden, nur
   unsichtbar.
2. `docs/QUARANTINE.md:62` berichtigen.
3. Die Anker-Runde ist **Eigentümer-Arbeit**, kein Bugfix: 223 Entscheidungen
   „verdrahten / löschen / dokumentieren". Nicht in dieser Strecke.

### Abnahme

- [ ] `python scripts/orphan_module_gate.py` meldet keinen Rückgang mehr
- [ ] `check_consistency.py` 0

---

## Phase 6 — Der blinde Fleck, und ob er eine Zahl verdient

### Was gemessen ist

Die Grundlinie zählt **Knoten mit Eingangsgrad 0**, nicht Erreichbarkeit ab
einer Wurzel. Belegt am Code (`scripts/audit_orphan_modules.py:229-235`,
`used_elsewhere()` schließt genau eine Datei aus) — und das Skript **sagt
es über sich selbst** (`:241-247`).

Live belegt in `src/analysis/profiles/`: `uft_profiles_all.c` ruft niemand
(steht in der Grundlinie), und die vier Dateien, die **nur sie** ruft,
stehen nicht drin.

Eigene Messung derselben Frage von der anderen Seite — Module, deren
exportierte Funktionen **kein Produktionscode** ruft, wohl aber Tests:

| Klasse | Module | Zeilen | in der Grundlinie |
|---|---|---|---|
| nur von Tests erreicht, keine Registrierung | **84** | **35 857** | 6 |
| über Zeigertabelle registriert (erreichbar) | 8 | 4 550 | 0 |

Die zweite Zeile ist bewusst getrennt: ein Plugin mit
`UFT_REGISTER_FORMAT_PLUGIN` ist erreichbar, auch wenn seine exportierten
Helfer nur Tests rufen. Ohne diese Trennung lautete die Zahl 92/40 407 und
wäre zu groß.

Die tote v3-Kette als benanntes Beispiel: `uft_advanced_open()` hat
**0 Aufrufer** (2 Nennungen, beide Kommentare), `uft_smart_open()` hat
**2 Aufrufer, beide in `tests/`**. Daran hängen 6581 Zeilen, **keine davon
in der Grundlinie**.

### Die Entscheidung, die ansteht

Ein Tor „nur von Tests erreicht" mit Grundlinie 84 wäre baubar (Muster:
`audit_fat_boundary.py`). **Aber:** `CLAUDE.md` verlangt für eine fünfte
Release-Kennzahl eine Begründung, und `audit_orphan_modules.py --dirs`
meldet bereits 37 geschlossene Verzeichnisse / 19 858 Zeilen — nur ist das
**ausdrücklich kein Tor** (`:260`) und speist die Grundlinie nicht
(`orphan_module_gate.py:48` liest nur `--detail`).

**Der billigere Schritt zuerst:** `--dirs` in das bestehende Tor
einspeisen, statt ein neues zu bauen. Damit ist die Frage beantwortet, ohne
eine zweite Messgröße neben eine bestehende zu stellen.

### Nicht tun

- Eine fünfte Kennzahl einführen, ohne sie zu begründen.
- Die 84 Module als „toter Code" melden. Sie sind **getestet** — der
  Zustand heißt „gebaut, geprüft, vom Benutzer nicht erreichbar", und weder
  „verwaist" noch „lebendig" trifft ihn.

---

## Phase 7 — Doku-Driften (billig, unabhängig)

Vier Aussagen, die ihre eigene Messung nicht halten. Alle bewegen keine
Kennzahl, alle sind in Minuten erledigt:

| Ort | steht da | gemessen |
|---|---|---|
| `include/uft/formats/uft_fdc_gaps.h:165` + `docs/OPEN_ITEMS.md:257` | „alle **sechs** oeffentlichen Funktionen" | **sieben** |
| `include/uft/formats/uft_fdc_gaps.h:166` | „`gap3_fmt` … an **null** Stellen gelesen" | 5 Lesestellen in `tests/`, **vom selben Commit** hinzugefügt |
| `include/uft/formats/uft_fdc_gaps.h:391` | `UFT_FDC_FORMAT_COUNT 17` | von Hand gepflegt, **nirgends gelesen** |
| `docs/QUARANTINE.md:62` | `uft_stx_air.c`: „0 Aufrufer" | verdrahtet (Phase 5) |

---

## Phase 8 — Abnahme

Für jede Phase, die Code angefasst hat:

```bash
export PATH="/c/Qt/Tools/mingw1310_64/bin:/c/Qt/6.10.2/mingw_64/bin:$PATH"
cmake --build build-tests-ci -j8 2>&1 | grep -E "error:|FAILED"
ctest --test-dir build-tests-ci 2>&1 | grep -E "tests passed|FAILED"
python scripts/check_consistency.py  2>&1 | tail -3
python scripts/verify_build_sources.py 2>&1 | tail -1
python scripts/audit_selbsttest.py   2>&1 | grep "Faelle:"
```

Zusätzlich, gegen die Muster dieses Plans:

```bash
# keine neue Kopie der Zonenzahlen
grep -rn "6667\|7143" --include=*.c --include=*.h src/ include/
# die Odd/Even-Regel darf nicht zurueckkehren
grep -rniE "tail_gap|\(sector *& *1\)" src/ include/
```

**Nichts wird committet, bevor diese fünf Abnahmen sauber sind.** Ein rotes
Tor wird behoben, nicht umgangen.

---

## Was dieser Plan bewusst NICHT enthält

* **Nibbler-Schreibsignaturen als Schutzerkennung.** UFT-09 belegt die
  Turbo-Nibbler-Signatur byte-exakt (`A9 55 A2 07 20 81 06` …, nachgeprüft
  an `turbonibbler 2.prg`, $14DE/$14E5/$14EC/$14F6). Sie identifiziert aber
  das **schreibende Werkzeug**, nicht den Schutz — zwei verschiedene
  Fragen. Ihr natürlicher Ort `g64_detect_protection()` hat keinen Leser
  (P3-147). Fundus, bis die Tür einen bekommt.
* **Ein Commodore-Eintrag in der FDC-Gap-Tabelle.** Deren sieben Funktionen
  haben **null Aufrufer**; nicht einmal ihr eigener Test ruft eine davon.
  Bewegt keine Kennzahl.
* **Die Anker-Runde über 223 Einträge.** Eigentümer-Entscheidung, keine
  Implementierung.
