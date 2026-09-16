# A-008 · Gutachten `UFT_C64PP_Protection_Catalog.zip`

**Stand:** 2026-09-16 · **Posten:** `A-008` · **Kanal:** *Spec* (Inhalt) + MIT (Code)
**Prüfstand:** `origin/main` = `70a940af`

> **Auftrag (Wortlaut):** „arbeite alles sehr genau aus / finde alles und alles
> raussuchen was ich übersehen habe / - wo können die formate verbessert werden
> / - ist es auf andere formate übertragbar / - welche einstellungen fehlen noch
> / - was habe wir noch nicht / - brauch es eine HAL-Erweiterungen / erstelle
> mir code Beispiele was besser gemacht werden kann / plan verstanden, was
> kannst du besser machen ??"

---

## Ergebnis in drei Sätzen

Die Zulieferung nennt als ihren kennzahlwirksamen Teil, die RapidLok-Heuristik
zu einer neutralen Strukturmeldung herabzustufen — **das hat dieser Baum mit
MF-402 bereits getan, und gründlicher.** Ihr Katalog aus 20 belegten Verfahren
ist sauber gearbeitet und quellenbelegt, aber **kein einziger seiner Einträge
lässt sich mit dem belegen, was UFT heute misst**: 15 von 20 verlangen
Merkmale, die `ufm_c64_track_metrics_t` gar nicht trägt, und die übrigen 5
verlangen Bytefolgen, Positionen innerhalb der Spur oder
Mehrfachlesungs-Vergleiche, die es ebenfalls nicht gibt. **Eine Idee der
Zulieferung ist echt neu und im Baum nirgends vorhanden:** dass der Beweisgrad
von der **Aufnahmequelle** abhängt.

**Empfehlung: REFERENCE, nicht MERGE.** Der Auditbericht wird als *Spec*
registriert; vom Code wird nichts übernommen. Zwei Teile sind einzeln
weiterverfolgbar und stehen unten mit Kosten.

---

## 0. Eine eigene Fehlmessung zuerst, weil sie das Gutachten betrifft

Beim ersten Durchgang habe ich notiert, `ufm_c64_metrics_from_gcr()` komme im
Baum **nicht vor**, und daraus geschlossen, der Auditbericht beschreibe einen
Pfad, den es nicht gibt. **Das war falsch.** Richtig gemessen (`git grep`,
rc 0, ohne Pfadliste): **43 Treffer in 15 Dateien**, darunter
`src/protection/ufm_c64_metrics.c`, `include/uft/protection/ufm_c64_metrics.h`
und **vier** Stellen in `src/gui/ProtectionAnalysisWidget.cpp`.

Ursache: die Messung lief als `git grep … -- $(git ls-files) 2>/dev/null`.
Die Pfadliste sprengt die Argumentlänge, git bricht mit rc≠0 und **leerer
Ausgabe** ab, `2>/dev/null` verschluckt das `fatal:` — und eine leere Ausgabe
sieht aus wie „0 Treffer". Klasse `grep_exitkode_bricht_die_kette`, dritter
Fall an einem Tag.

**Der Auditbericht hat an dieser Stelle recht:** die Kette
G64 → rohe GCR-Spur → `ufm_c64_metrics_from_gcr()` → `ufm_c64_prot_analyze()`
ist genau der Produktivpfad, so wie er ihn beschreibt.

---

## 1. Was im Baum steht — gemessen, nicht angenommen

### 1.1 Der erreichbare Pfad

| Stufe | Stelle |
|---|---|
| Hauptfenster | `src/mainwindow.cpp:107` — `m_statusTab = new StatusTab()` |
| Reiter | `src/statustab.cpp:458` — `new ProtectionAnalysisWidget(dlg)` |
| Metriken | `src/gui/ProtectionAnalysisWidget.cpp` — `ufm_c64_metrics_from_gcr()` |
| Erkenner | `…:215` — `ufm_c64_prot_analyze()` → `src/protection/ufm_c64_scheme_detect.c:124` |

**Das ist ein Dialog, den ein Bediener anklickt** — kein toter Pfad. Er ist
damit der Maßstab für alles Weitere.

### 1.2 Der zweite Rater ist tot, und beide seiner Türen sind es

`g64_detect_protection()` (`src/formats/g64/uft_g64_parser_v3.c:1662`) vergibt
weiterhin fünf Markennamen an Schwellwerten („Vorpal/RapidLok" 0,90 aus
`weak_tracks > 0 && half_tracks > 0`, „V-Max!" 0,85, „Epyx FastLoad" 0,75).
Erreichbarkeit gemessen:

* `uft_advanced_open()` — einziger Aufrufer ist
  `tests/test_advanced_guete_ohne_messung.c:274`
* `uft_advanced_detect_protection()` — **nur Prototyp und Definition, null
  Aufrufer**

Deckt sich mit `P3-147`. Nach Regel 9 bewegt ein Umbau dort **keine** der vier
Kennzahlen → Fundus. Die Zulieferung ändert daran nichts.

### 1.3 MF-402 hat den erreichbaren Pfad bereits bereinigt

Der Beitrag, den die Aufnahme des Postens als „Rücknahme einer Falschaussage"
gewertet hat, liegt vor:

* `ufm_cbm_check_vmax()` **entfernt** — die Bedingung war tautologisch
  (`has_custom_sync` impliziert `sector_count == 0`), sie unterschied nichts
  und hängte trotzdem „V-MAX!" mit 85 % an jede kopflose Spur
* `ufm_cbm_check_rapidlok()` → **`ufm_cbm_has_half_track_beyond_35()`**
  umbenannt, weil die Bedingung keine RapidLok-Identifikation tragen kann
* die Klassifikation nennt seither **Strukturen** statt Produkte
  (`UFM_PROT_HALF_TRACK`, `UFM_PROT_CUSTOM_SYNC`, …)

Belegstellen: `src/protection/ufm_c64_scheme_detect.c:62-78` und `:186-200`,
`include/uft/protection/ufm_cbm_protection_methods.h:15-27`. Die Begründungen
stehen **zitiert an Ort und Stelle** — Hausregel „nicht entfernen, weiter
erweitern".

Und die Oberfläche ist seit **MF-508** ebenfalls ehrlich: das Feld
`confidence` ist entfernt (nicht auf 0 gesetzt, „ein Feld, das da ist, wird
irgendwann wieder gefüllt"), jeder Eintrag trägt `Heuristik` als Grundlage und
**die Regel, die gefeuert hat**, im Klartext neben sich
(`ProtectionAnalysisWidget.cpp:286-350`).

### 1.4 Die Klasse hat bereits ein Tor

`scripts/audit_protection_claims.py` (MF-557), verdrahtet als Kategorie in
`scripts/check_consistency.py:752`. Seine gemessenen Zahlen: **33** Dateien in
`src/protection/`, **369** `uft_`-Funktionen, **3** von außerhalb gerufen,
**353** von keinem Test berührt. Sein Kopf sagt wörtlich, den Katalog
anzuschließen hieße, „353 ungeprüfte Funktionen an ein forensisches Urteil zu
hängen — genau die Lage, aus der die fünf fabrizierten Parser kamen".

**Die 20 Einträge der Zulieferung würden genau dorthin gelegt.**

---

## 2. Urteil je Katalogeintrag (b)

Maßstab ist nicht die Zulieferung, sondern was UFT **beobachten** kann. Die
einzige Datenstruktur, die den erreichbaren Erkenner erreicht, ist
`ufm_c64_track_metrics_t` (`include/uft/protection/ufm_c64_protection_taxonomy.h:46-70`).
Sie trägt: `track`, `side`, `bitcell_count`, `sync_count`, `sector_count`,
`bad_gcr_count`, `duplicate_ids`, `track_length_ratio`, `density_deviation`,
`jitter_rms`, `has_half_track`, `has_custom_sync`, `revolutions`,
`bitlen_min/max`, `weak_region_bits`, `weak_region_max_run`,
`illegal_gcr_events`, `max_sync_run_bits`, `is_half_track`,
`has_meaningful_data`, `track_x2`.

Der Katalog führt **26** Merkmalsflaggen. Zwölf davon haben in dieser Struktur
**kein Gegenstück**: `FAT_TRACK`, `TRACK_ALIGNMENT`, `CUSTOM_HEADER`,
`SLIDING_BITS`, `BYTE_COUNT`, `RPM_SENSITIVE`, `SECTOR_PARITY`,
`LOADER_TIMING`, `SYNC_POSITION`, `MIXED_FORMAT`, `GAP_SIGNATURE`,
`EXACT_SIGNATURE`.

| Eintrag | fehlendes Merkmal | Urteil |
|---|---|---|
| `ea-fat` | FAT_TRACK, TRACK_ALIGNMENT | **Kandidat** |
| `mindscape` | — (aber Beweis verlangt Mehrfachlesungs-Varianz) | **Kandidat** |
| `pirateslayer-1` | CUSTOM_HEADER, SLIDING_BITS | **Kandidat** |
| `pirateslayer-2` | CUSTOM_HEADER, SLIDING_BITS | **Kandidat** |
| `radwar-1` | — (Beweis verlangt exakte Sync-Länge, nicht das Maximum) | **Kandidat** |
| `radwar-2` | — (Beweis verlangt Bad-GCR-Lauf **nach Schlüsselbytes**) | **Kandidat** |
| `rapidlok-1` | TRACK_ALIGNMENT, BYTE_COUNT | **Kandidat** |
| `rapidlok-2` | + RPM_SENSITIVE | **Kandidat** |
| `rapidlok-5` | + SECTOR_PARITY | **Kandidat** |
| `rapidlok-6` | + SECTOR_PARITY | **Kandidat** |
| `rapidlok-7` | + LOADER_TIMING | **Kandidat** |
| `cyan-old` | — (Beweis ist eine **Signatur** in Spur 40 Sektor 16) | **Kandidat** |
| `cyan-new` | — (Beweis verlangt **achtfache** Wiederholungslesung) | **Kandidat** |
| `vmax-0-1` | BYTE_COUNT | **Kandidat** |
| `vmax-2` | RPM_SENSITIVE | **Kandidat** |
| `vmax-3` | SYNC_POSITION, MIXED_FORMAT, TRACK_ALIGNMENT | **Kandidat** |
| `vorpal-early` | BYTE_COUNT, CUSTOM_HEADER | **Kandidat** |
| `vorpal-later` | SLIDING_BITS | **Kandidat** |
| `xemag-2` | FAT_TRACK, TRACK_ALIGNMENT | **Kandidat** |
| `spiradisc` | FAT_TRACK, TRACK_ALIGNMENT | **Kandidat** |

**Gezählt: 15 von 20 verlangen ein Merkmal, das die Struktur nicht trägt;
die restlichen 5 verlangen Bytefolgen, Positionen innerhalb der Spur oder
Mehrfachlesungs-Vergleiche, die es ebenfalls nicht gibt. Damit ist heute
0 von 20 belegbar und 20 von 20 höchstens Kandidat.**

**Widerlegt ist keiner** — und das ist eine Aussage über UFT, nicht über den
Katalog: wer nichts messen kann, kann auch nichts widerlegen. Genau das ist
`P3-79` („Artefaktsignaturen identifizieren Codefamilien, nicht Produkte") und
`P3-187` („C64-Signaturkonstanten ohne benannte Quelle") in anderer Gestalt.

**Der Katalog ist damit nicht wertlos, sondern falsch eingeordnet:** er ist
eine **Beschaffungsliste**, keine Erkennung. Er sagt präzise, *was gemessen
werden müsste* — und das ist mehr, als der Baum heute irgendwo aufgeschrieben
hat.

---

## 3. Die GUI-Hälfte: Zeilendiff statt Dateiübernahme (c)

| | Zeilen |
|---|---|
| Baum `src/gui/ProtectionAnalysisWidget.cpp` | **712** |
| Zulieferung, gleiche Datei | **885** |
| Diff | **17 Blöcke**, **+185**, **−22** |

### 3.1 Die 22 entfernten Zeilen sind der Grund gegen eine Übernahme

Unter ihnen steht ein **gemessener Kommentar über einen behobenen Defekt**
(Baum, im Umfeld von `:249-258`):

> *„Index conventions differ between the two APIs and must be converted, not
> passed through: `g64_get_track()` index 2 = track 1.0;
> `ufm_c64_metrics_from_gcr()` index 0 = track 1.0. Passing `halftrack`
> straight through shifts every track by one and lands exactly on the three
> speed-zone boundaries (17→18, 24→25, 30→31), where the nominal capacity
> changes. Measured: that produced three spurious ‚long track' hits on BOTH
> clean reference disks."*

Eine Dateiübernahme würde diese Begründung **löschen** — und mit ihr das
Wissen, warum die Umrechnung dort steht. Das ist wörtlich die Klasse, gegen
die `kommentar_traegt_oder_erwaehnt` geschrieben ist: **dieser Kommentar trägt
die Aussage, er erwähnt sie nicht.**

Ebenfalls unter den 22: die `"RapidLok"`-Heuristik samt ihrer Regelzeile. Ihre
Rücknahme ist der eine gute Teil — aber sie ist **zwei Zeilen**, nicht eine
Datei, und der Baum hat sie bereits mit `Heuristik` + Regeltext entschärft.

### 3.2 Was die 185 ergänzten Zeilen bringen

Drei Schalter (`m_catalogEnabled`, `m_showCandidates`, `m_showCapturePlan`),
ein Katalogfeld (`createCatalogPanel()`), eine Sammelfunktion
(`collectCatalogViews()`) und der Lauf `runCatalogAnalysis()` gegen
`uft_c64pp_analyze()` / `uft_c64pp_build_capture_plan()`.

**Sie hängen vollständig am Katalog aus §2** und sind ohne ihn leer.

### 3.3 Empfehlung zur GUI

**Nichts übernehmen.** Falls der Eigentümer den Katalog später will, ist der
Weg ein **eigener Diff von Hand** gegen die heutige Datei, Block für Block —
nicht die Datei der Zulieferung.

---

## 4. Verworfen: die zwei Baudateien (d)

| Datei | Zulieferung | Baum | Differenz |
|---|---|---|---|
| `tests/CMakeLists.txt` | **341 230** B | **361 386** B | **−20 156** |
| `UnifiedFloppyTool.pro` | **81 628** B | **83 293** B | **−1 665** |

`README_EINBAU.md` der Zulieferung nennt beide „**aktuelle Verdrahtung**".
Gemessen sind sie **älter** als die des Baums. Eine Übernahme wäre eine
**stille Rücknahme fremder Verdrahtung** — der teuerste denkbare Fehler in
einem Baum, in dem zwei Sitzungen arbeiten.

Ebenfalls verworfen: `tools/uft-c64pp-catalog.c` (454 B) — ein
Kommandozeilenwerkzeug. UFT ist GUI-only; ein CLI-Einstieg widerspricht der
Projektregel.

---

## 5. Die fünf Fragen des Auftrags (a)

### 5.1 „wo können die formate verbessert werden"

**Eine Stelle, gemessen, und sie ist klein:** weder
`src/protection/ufm_c64_scheme_detect.c` noch `src/protection/ufm_c64_metrics.c`
kennen **Nachbarschaft**. Baumweit gemessen (`git grep`, rc 0) gibt es in
`src/protection/` **genau einen** Treffer für Nachbarschaftsbegriffe, und der
ist ein **Kommentar** über Apple-Rotationsversatz
(`uft_apple2_protection.c:631`).

Die Folge ist eine Falschaussage auf einem **erreichbaren** Pfad:

| Diskette | was UFT heute meldet |
|---|---|
| 17.0 / 17.5 / 18.0 — mit einer 1541 **nicht rückschreibbar** | „Data on half-track position" |
| 1.5 / 17.5 / 35.5 — jede Lage einzeln schreibbar | „Data on half-track position" |

Der physikalische Grund steht als `P3-38`: der **Lesekopf** der 1541 kennt 80
Halbspur-Lagen, der **Schreibkopf** nur 40 ganze — er ist eine ganze Spur breit
und löscht die Nachbarlage beim Schreiben mit. Das unterscheidende Merkmal ist
die Nachbarschaft, nicht die Existenz einer Halbspur.

**Das Feld dafür liegt bereit und wird gefüllt:**
`ufm_c64_track_metrics_t.track_x2` (`src/protection/ufm_c64_metrics.c:142`,
`track*2`, +1 für halb). Nachbarschaft ist damit `|Δ track_x2| == 1` — **ohne
eine einzige neue Konstante**, also ohne `S1`.

**Die Falle dabei ist benannt:** zwei belegte GANZE Spuren (17.0/18.0) haben
`track_x2` 34 und 36, Abstand **zwei**. Wer über die Spurnummer statt über
`track_x2` rechnet, meldet **jede formatierte Diskette** als Befund.

*Der Baum hat einen Nachbarschaftsbegriff — aber er antwortet auf eine andere
Frage und ist unerreichbar:* `src/analysis/deepread/uft_deepread_crosstrack.c`
rechnet NCC zwischen benachbarten Spur-Qualitätsprofilen zur
**Schadensklassierung**. Gemessen steht sein Bezeichner in genau vier Dateien
— `.pro`, **`docs/orphan_baseline.txt`**, eigener Header, eigene `.c` —, also
kein Aufrufer (MF-627/MF-767).

### 5.2 „ist es auf andere formate übertragbar"

Der Auditbericht beantwortet das selbst und **richtig**: übertragbar ist der
**Messoperator**, nicht die Signatur. Eine Ergänzung aus dem Baum: `track_x2`
ist C64-spezifisch, aber die *Frage* „liegen zwei belegte Lagen so dicht
beieinander, dass der Schreibkopf sie nicht trennen kann" ist bei jedem System
mit Halbspuren dieselbe — Apple II (Viertelspuren) voran. Die Antwort hängt an
der Kopfbreite, und **die kennt der Baum nirgends** (0 Treffer für
`kopfbreite`/`head_width`).

### 5.3 „welche einstellungen fehlen noch"

Aus §2 abgeleitet, nicht aus dem Bericht abgeschrieben — es sind genau die
zwölf Flaggen ohne Gegenstück. Die drei billigsten, weil ihre Daten schon
erhoben werden:

1. **Sync-Längen-Verteilung** statt nur `max_sync_run_bits`. `SHORT_SYNC` und
   `SYNC_POSITION` brauchen die Liste, nicht das Maximum.
2. **Mehrfachlesungs-Vergleich.** `revolutions` zählt Umdrehungen; **nichts
   vergleicht sie**. `MULTI_READ_REQUIRED` und `WEAK_OR_UNSTABLE` hängen
   daran, und fünf Katalogeinträge ebenso.
3. **Drehzahl je Spur.** `RPM_SENSITIVE` trifft vier RapidLok-Fassungen und
   `vmax-2`; `jitter_rms` ist etwas anderes.

### 5.4 „was habe wir noch nicht"

**Die stärkste Idee der Zulieferung, und sie ist im Baum nirgends vorhanden:
der Beweisgrad hängt von der AUFNAHMEQUELLE ab.** Ihr
`uft_c64pp_disk_view_t.preserves_flux_timing` ist für SCP, KryoFlux und
Live-Fluss wahr und für ein G64 falsch — ein Merkmal, das Timing braucht, ist
aus einem G64 **prinzipiell** nicht belegbar, egal wie gut der Erkenner ist.

Gemessen: `git grep -liE "preserves_flux|capture_source|source.*preserv"`
über `src/protection/` und `include/uft/protection/` → **rc 1, 0 Treffer**.
`ufm_c64_prot_analyze()` bekommt nur Metriken und weiß nicht, woher sie
stammen.

**Das ist derselbe Gedanke wie die Tier-Stufen** (T1/T1b/T2/T3 sagen, woher
der Beleg kommt) — nur für Schutzbefunde statt für Formate. Er passt zur
Hausphilosophie und wäre auch ohne den Katalog etwas wert.

### 5.5 „brauch es eine HAL-Erweiterungen"

**Nein, und das ist gemessen.** Die drei fehlenden Einstellungen aus §5.3
betreffen die **Auswertung**, nicht die Erfassung: Mehrfachlesungen liefert
Greaseweazle bereits (`revolutions` ist gefüllt), Drehzahl und Sync-Positionen
stecken im Fluss, den die vorhandenen Controller aufnehmen. Was fehlt, ist der
**Weg vom Fluss in die Metrikstruktur** — `ufm_c64_metrics_from_gcr()` nimmt
eine GCR-Spur, keinen Zellstrom.

Der Auditbericht nennt das selbst als „wichtige Restlücke" (kein durchgehender
SCP→C64-GCR-Pfad) und hat damit recht.

---

## 6. „was kannst du besser machen?"

1. **Nicht nachbauen, was schon steht.** Der Hauptbeitrag der Zulieferung ist
   seit MF-402 im Baum. Der Bericht konnte das nicht wissen — aber ein Einbau
   hätte es überschrieben.
2. **Nicht die Datei nehmen, sondern die Zeilen.** §3.1 zeigt, was eine
   Dateiübernahme gelöscht hätte.
3. **Den Katalog als Beschaffungsliste führen, nicht als Erkennung.** §2 sagt,
   was jeweils fehlt — das ist ein Arbeitsplan, kein Befund.
4. **Die kleinste echte Verbesserung zuerst.** Die Nachbarschaftsunterscheidung
   (§5.1) sitzt auf dem **erreichbaren** Pfad, braucht keine neue Konstante,
   keinen Katalog und keine Lizenzentscheidung.

---

## 7. Disposition

| Teil | Urteil | Begründung |
|---|---|---|
| `docs/UFT_C64PP_PROTECTION_AUDIT.md` | **REFERENCE** | quellenbelegte Seitenauswertung; als *Spec* registrierbar |
| `src/protection/uft_c64pp_catalog.{c,h}` | **PARTIAL** → Fundus | 0 von 20 belegbar (§2); Anschluss verstärkt P0-2 |
| `src/gui/ProtectionAnalysisWidget.cpp` | **BLOCKED** | löscht 22 Zeilen, darunter eine gemessene Begründung (§3.1) |
| `tests/test_c64pp_catalog.c` | **BLOCKED** | synthetisch, prüft nur die eigene Tafel — geschlossener Kreis |
| `tools/uft-c64pp-catalog.c` | **nicht übernehmen** | CLI; UFT ist GUI-only |
| `UnifiedFloppyTool.pro`, `tests/CMakeLists.txt` | **nicht übernehmen** | älter als der Baum (§4) |
| Quellenangaben je Eintrag (URLs) | **REGISTER** | gehören zu `docs/ORACLES.md` als *Spec*-Quelle |

**Kein Byte dieser Zulieferung ist nach `src/`, `include/` oder `tests/`
geschrieben worden (e).** Der Einbau ist nach der Abschlussbedingung dieses
Postens ausdrücklich ein eigener Auftrag.

---

## 8. Vorschläge für `docs/OPEN_ITEMS.md`

Höchstens fünf je Zyklus; hier **drei**, jeder mit Kennzahlbezug nach Regel 9.

1. **Nachbarschaft belegter Halbspuren wird nicht unterschieden** (§5.1) —
   verschärft `P3-38` um die Messung, dass der **erreichbare** Erkenner
   betroffen ist, nicht nur der tote `g64_detect_protection()`. *Kennzahl:*
   keine der vier; Ehrlichkeit auf einem Produktivpfad.
2. **Der Beweisgrad kennt die Aufnahmequelle nicht** (§5.4) — `rc 1, 0
   Treffer` über `src/protection/`. *Kennzahl:* keine der vier; es ist die
   Tier-Idee für Schutzbefunde.
3. **`revolutions` wird gezählt und nie verglichen** (§5.3) — fünf
   Katalogeinträge und zwei Merkmalsflaggen hängen daran. *Kennzahl:* keine
   der vier.

---

## 9. Quellen

* Zulieferung: `neue-ideen/UFT_C64PP_Protection_Catalog.zip`, 23 Einträge,
  515 653 B entpackt; alle Quelldateien `SPDX-License-Identifier: MIT`
* Inhaltsquelle der Zulieferung: C64 Preservation Project
  (`rittwage.com/c64pp/…`), je Eintrag als URL hinterlegt
* Baum: `origin/main` = `70a940af`
* Gegengehaltene Befunde: `P3-38`, `P3-39`, `P3-79`, `P3-147`, `P3-187`,
  `P0-2`; Tore MF-402, MF-508, MF-557
