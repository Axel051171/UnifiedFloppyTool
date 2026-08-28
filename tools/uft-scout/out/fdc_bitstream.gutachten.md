<!-- uebernommen: MF-626 -->
# Gutachten: yas-sim/fdc_bitstream (Zweitlauf — Divergenzmessung)
<!-- uebernommen: MF-626 -->

Stand: 2026-08-27 · Inventar: UFT HEAD `bb74f540`
(`scout_inv2.json`: 88 Plugins, SSOT ok, 22 Korpus-Abbilder) ·
Fremd-Repo HEAD `0178992` = `017899212b62870f545ea16bb8e771972ae72216`
(2024-06-04), Vermessung per Voll-Clone.
Zyklus vom Eigentümer benannt; Vermessung von Hand, jede Aussage mit
Datei+Zeile oder Diff-Zählung.

## Negativlisten-Lage (Regel 6, vor der Vermessung geprüft)

`data/known_negatives.json` führt das Repo bereits:
`"yas-sim/fdc_bitstream": {"status": "integriert", "grund": "bereits
vollständig in UFT vendored (src/flux/fdc_bitstream)"}`.

Regel 6 erlaubt eine **Neubewertung** nur bei gemessener wesentlicher
Änderung im relevanten Bereich. Gemessen: **Upstream hat seit unserem
Vendoring null Commits** (HEAD `0178992` vom 2024-06-04; unsere Kopie kam
mit v4.1.0 am 2026-04-13, Commit `4d622192`). Eine Neubewertung des Repos
als Übernahme-Kandidat ist damit **nicht zulässig und findet nicht
statt.** Dieses Gutachten beantwortet stattdessen die vom Eigentümer
gestellte Messfrage: *ist unsere Kopie aktuell, und weicht sie ab?* —
plus vier dabei gefundene Befunde über den eigenen Baum, keiner davon
eine Format-/Decoder-Übernahme.

## Ergebnis in einem Satz

**Die Kopie ist aktuell** (Basis = Upstream-HEAD, Upstream seit über zwei
Jahren ruhend); alle Abweichungen sind absichtliche lokale Härtungen —
aber die Kopie hat **null Aufrufer und null Tests im Baum**, führt den
MIT-Lizenztext **nicht** mit, und der Negativlisten-Status „integriert"
beschreibt Vendoring, keine Fähigkeit.

## Kategorie

**Daten/Oracle + Hygiene am eigenen Baum** (keine Innovation, keine
Übernahme — das Repo ist schon da).

## Divergenzmessung (die Frage des Eigentümers)

### Upstream-Bewegung seit unserem Stand

| Messung | Ergebnis |
|---|---|
| Upstream-HEAD | `0178992`, 2024-06-04 22:34 +0900 |
| Unsere Kopie eingeführt | v4.1.0-Release `4d622192`, 2026-04-13 (`git log --follow src/flux/fdc_bitstream/fdc_bitstream.cpp`) |
| Upstream-Commits seit unserem Stand | **0** — unsere Basis ist der HEAD selbst |
| Spätere Berührungen im UFT-Baum | nur Cleanup/Refactor: `e0add155`, `3b41163c`, `d9358117`, `f188fa4e` |

### Datei-für-Datei-Diff gegen Upstream-HEAD (`diff -w`, Zeilen mit `<`/`>`)

| Datei | Diffs | Art der Abweichung (gelesen, nicht vermutet) |
|---|---|---|
| `fdc_crc.cpp`, `fdc_vfo_base.cpp`, `vfo_simple.cpp`, `vfo_simple2.cpp`, `vfo_pid.cpp`, `vfo_experimental.cpp` | 0 | identisch |
| `fdc_bitstream.cpp` | 6 | lokal: Value-Init statt `memset`, Overflow-sichere `track_len`-Klammer |
| `bit_array.cpp` | 16 | lokal: `(size_t)`-Casts gegen sign-compare, 8 Stellen |
| `fdc_misc.cpp` | 32 | lokal: Includes vor den Namespace gezogen („FIXED R18"), `size_t`-Schleifen |
| `mfm_codec.cpp` | 17 | lokal: Init-Reihenfolge, `vfo_fixed`-Fall entfernt, `vfo_experimental` hinter `#ifdef UFT_HAS_EXPERIMENTAL_VFO` |
| `vfo_pid2.cpp` | 2 | lokal: `(void)`-Casts gegen unused-Warnung |
| `vfo_pid3.cpp` | 3 | lokal: UB-Fix — `(++m_hist_ptr) & mask` in zwei Anweisungen zerlegt |
| Header (`include/uft/flux/*.h`, 13 Stück) | 0–7 | lokal: Standard-Includes ergänzt, ein Member-Init-Order-Fix (`bit_array.h:27`); kein fehlender Upstream-Inhalt |

**Richtung jeder Abweichung ist belegt lokal→besser** (Kommentare wie
„FIXED R18", `UFT_HAS_EXPERIMENTAL_VFO`, `(void)`-Suppression stehen nur
auf unserer Seite). Es gibt **keinen** Upstream-Inhalt, der uns fehlt.

### Was bewusst nicht vendored wurde

`vfo_fixed.cpp` (entfernt, begründet in `UnifiedFloppyTool.pro:419`:
„7 LOC trivial stub, never selected"), Upstreams `disk_image/`-Klassen
(HFE/MFM/RAW/D77/FDX/RDD-Leser), `fdc_test/`, `tests/`, `test_data/`,
`disk_analyzer/`, `kfx2mfm/`, `pauline2raw/`, `image_converter/`.

## Befunde am eigenen Baum (gemessen)

### B1 — Die Kopie hat null Aufrufer und null Tests

`grep -rn "fdc_bitstream|mfm_codec|fdc_vfo|bit_array"` über `src/`,
`include/`, `tests/` (C/C++/H), Kopie selbst ausgenommen: **2 Treffer,
beide über UFTs eigenes `uft_mfm_codec.c`** (`uft_simd.h:121`,
`uft_types.h:311`) — kein einziger Include der vendorten Header.
In `tests/`: einziger Treffer ist eine Kommentarzeile
(`tests/test_flux_histogram.c:19`). Kompiliert wird die Kopie trotzdem
(`UnifiedFloppyTool.pro:400-414`), **2795 Zeilen** `.cpp`.

Folgen:
- Der Negativlisten-Status „**integriert**" beschreibt Vendoring, keine
  Fähigkeit — nach dem Maßstab von `OPEN_ITEMS.md` §1 („a function that
  exists is not a feature until something calls it") gehört er präzisiert.
- `KNOWN_ISSUES.md` FLUX-12 begründet das Auskommentiert-Lassen von
  `fdc_bitstream.cpp:511-513` mit einem „zweiten Decoder, **der eigene
  Tests hat**" — dieser Halbsatz ist heute unbelegt (kein Test kompiliert
  oder linkt eine der Dateien).
- `MASTER_PLAN.md:435` führt `mfm_codec.cpp` seit dem externen Review
  2026-04-24 als einen von drei parallelen Decoder-Pfaden (1626 LOC
  Redundanz). Der Befund hier verschärft das: der Pfad ist nicht nur
  redundant, er ist **unerreichbar**.

### B2 — MIT-Pflicht nicht erfüllt, Herkunft nicht gepinnt

Upstream-Lizenz: `LICENSE.md`, **MIT, Copyright (c) 2022 Yasunori
Shimura** (aus der Datei gelesen). MIT verlangt: „The above copyright
notice **and this permission notice** shall be included in all copies or
substantial portions." Gemessen im Baum: `grep -rn "MIT|SPDX"` über
`src/flux/fdc_bitstream/` und die 13 vendorten Header → **0 Treffer**
(nur Doxygen-`@copyright Copyright (c) 2022`-Zeilen, ohne Permission
Notice). Außerdem nennt keine Stelle im Baum den Upstream-Commit der
Kopie — die Zuordnung zu `0178992` existiert erst durch dieses Gutachten.
Gleiche Fallfamilie wie P0-5 (MF-580).

### B3 — README der Kopie ist in einem Punkt veraltet

`src/flux/fdc_bitstream/README.md:65+69-71` führt `vfo_fixed.cpp` in der
Tabelle und sagt „These files are compiled into the build" — die Datei
ist entfernt (`UnifiedFloppyTool.pro:419`).

### B4 — Upstream-Testgeschirr und Realdaten wurden nicht mit-vendored

Upstream hat `fdc_test/` (Selbsttest der Bibliothek), `tests/`
(`compare.cpp`, `format.cpp`, `raw2d77.cpp`) und `test_data/` mit einer
**realen FM77AV-Aufnahme in zwei Abtastraten plus passendem
Sektor-Abbild**: `2019FM77AVDemo-4MHz.raw`, `2019FM77AVDemo-8MHz.raw`,
`2019FM77AVDemo.d77`. Der UFT-Korpus (22 Abbilder, `inv["korpus"]`
durchgesehen) enthält **kein** D77-, FM-7- oder RAW-Flux-Material; D77
steht auf **T3** (Inventar-Abfrage unten). Das ist Hebe-Material für
genau die Sorte Arbeit, die die Einfrier-Regel ausdrücklich erlaubt.

## Was das Inventar dazu sagt (Abfrage zitiert)

`inventar.py query` gegen `scout_inv2.json`:

```json
"d77":  {"vorhanden": true,  "abgedeckt": true,  "treffer": ["d77"], "tier": "T3", "plugin_liste_vollstaendig": true}
"fdx":  {"vorhanden": true,  "abgedeckt": true,  "treffer": ["fdx"], "tier": null, "plugin_liste_vollstaendig": true}
"rdd":  {"vorhanden": false, "abgedeckt": false, "treffer": [], "schwache_treffer": [], "plugin_liste_vollstaendig": true,
         "hinweis": "INVENTAR DECKT DAS NICHT AB … `false` heisst hier NICHT `fehlt` …"}
"pauline":              {"vorhanden": false, "abgedeckt": false, "...": "wie oben"}
"wd179x fdc emulation": {"vorhanden": false, "abgedeckt": false, "...": "wie oben"}
"unstable pulse":       {"vorhanden": false, "abgedeckt": false, "...": "wie oben"}
```

Handnachschau zu den `abgedeckt:false`-Fällen (AGENT.md Regel 4):
- **rdd**: `grep -rni "\brdd\b|image_rdd|\.rdd" src include scripts` →
  0 Treffer. UFT hat RDD wirklich nicht.
- **pauline**: nur Enum-Werte ohne Implementierung —
  `include/uft/hal/uft_hal.h:52` (`UFT_CTRL_PAULINE`),
  `include/uft/uft_hardware.h:71`, `include/uft/uft_ir_format.h:151`,
  plus `docs/PLANNED_APIS.md:119` (geplanter Header).
- **wd179x fdc emulation / unstable pulse**: die Fähigkeit „FDC-Emulation"
  liegt exakt in der vendorten Kopie selbst; Weak-Bit-Behandlung hat UFT
  (MF-611-Nachweis aus dem ersten Zyklus).

## Lizenz (aus der Datei, nicht README)

- `LICENSE.md` Repo-Wurzel: **MIT, © 2022 Yasunori Shimura**. Keine
  abweichenden `LICENSE*`/`COPYING*` in den für die Kopie relevanten
  Unterverzeichnissen gesichtet.
- **Zone: GRÜN** (lizenzmatrix.md, MIT) — Vendoring zulässig, **aber**
  Attribution-Pflicht aktuell nicht erfüllt (B2).
- **Grenzfall test_data (Regel 8 → Vorlage, keine eigene Entscheidung):**
  Die drei `test_data`-Dateien sind eine Flux-Aufnahme der
  „2019 FM77AV Demo". Das Repo ist MIT, aber der **Inhalt der Diskette**
  (das Demo-Programm) hat eigene Urheber; ob die Repo-Lizenz ihn deckt,
  ist aus den Dateien nicht entscheidbar. → Vor Aufnahme in
  `tests/corpus/` klären; bis dahin UNGEKLÄRT.

## Einhängepunkt (bestehende Pläne)

- **B1/Differenzlauf**: `docs/MASTER_PLAN.md` §Performance- &
  Algorithmen-Review (drei parallele Decoder-Pfade, Zeile 435) und
  `docs/OPEN_ITEMS.md` §1 (unerreichbarer Code als Kernproblem). Der
  Differenzlauf unten ist Verifikationsarbeit im Sinn von
  `docs/VERIFICATION_PLAN.md`.
- **B2**: P0-5-Familie (`docs/OPEN_ITEMS.md` P0-5, MF-580).
- **B4**: `docs/OPEN_ITEMS.md` §„Korpus-gebundene Tests —
  Beschaffungsliste (MF-588)" + `docs/VERIFICATION_TIERS.md` (D77 = T3).

## Oracle-Kandidat

Die vendorte Kopie **selbst** ist der Oracle-Kandidat: eine unabhängige,
WD179x/MB8876-nachbildende MFM-Decode-Implementierung, MIT, im Baum,
vom Autor mit eigenem Testgeschirr (`fdc_test/`, upstream) belegt. Sie
gegen UFTs Produktionspfad laufen zu lassen ist der seltene Fall, in dem
das Oracle schon kompiliert wird.

## Differenzlauf-Plan (für die Verdrahtungs-Option aus B1)

- **Binaries:** UFT-Testtreiber A ruft den Produktionspfad
  (`src/flux/uft_flux_decoder.c` mit `src/decoder/uft_pll_v2.c`);
  Testtreiber B füttert dieselbe Spur als `bit_array` in
  `fdc_bitstream::read_all_idam()` / `read_sector(cyl, rcd)` mit
  `vfo_pid2` (Upstream-Default laut `fdc_vfo_def.h`).
- **Gemeinsamer Korpus:** vorhandene `tests/corpus/gw_amigados.scp`
  (liegt laut `inv["korpus"]`) für den MFM-Pfad; nach Lizenzklärung
  zusätzlich `2019FM77AVDemo-4MHz.raw` + `.d77` als Ground-Truth-Paar.
- **Metrik je Spur:** Anzahl gefundener IDAMs, Menge der CRC-guten
  Sektoren, Byte-Identität der Sektor-Nutzlast beider Decoder
  gegeneinander und gegen das Sektor-Abbild.
- **Toleranzliste:** Spur-Enden-Behandlung (Upstream kombiniert letzten
  und ersten Puls — siehe Upstream-Commit `15aacb0`); Weak-Bit-Sektoren
  (dürfen differieren, werden ausgewiesen); FM-Spuren (fdc_bitstream ist
  MFM-orientiert).
- **Rotbeweis zuerst:** derselbe Treiber gegen eine absichtlich um 1 Bit
  verschobene Spur — beide Decoder müssen die betroffene Sektor-CRC rot
  melden, sonst misst der Aufbau nichts.

## Beschaffungsliste (gegen `inv["korpus"]` geprüft)

| Posten | Schon da? | Beschaffung |
|---|---|---|
| MFM-Flux mit Ground-Truth | teilweise (`gw_amigados.scp` + `gw_amigados.hfe`) | nichts anzufordern |
| D77 + zugehöriger Real-Flux | **nein** (kein D77/FM-7/RAW im Korpus) | `test_data/` aus dem Upstream-Clone — 0 Kosten, **nach** Lizenzklärung (s.o.) |
| Upstream-Testgeschirr | nein | `fdc_test/`, `tests/raw2d77.cpp` aus dem Clone; MIT, als Referenz/Oracle nutzen, nicht portieren |

## Pflichtfelder kompakt

| Feld | Wert |
|---|---|
| Kategorie | Daten/Oracle + Baum-Hygiene (keine Übernahme) |
| Lizenzzone | GRÜN (MIT aus `LICENSE.md`); Attribution im Baum fehlt (B2); test_data-Inhalt UNGEKLÄRT |
| Inventar | zitiert oben; d77 T3, rdd/pauline `abgedeckt:false` → Handnachschau erfolgt |
| Einhängepunkt | MASTER_PLAN §Decoder-Redundanz; OPEN_ITEMS P0-5-Familie + MF-588-Beschaffungsliste |
| Oracle | die Kopie selbst (WD179x-Nachbildung) + upstream `test_data`-Paar |
| Aufwandsklasse | B2/B3: **S** · B1-Entscheid: **S**, Differenzlauf-Verdrahtung: **M** · B4: **S** (nach Klärung) |
| Differenzlauf | spezifiziert oben |

## Fundus (kein Vorschlag — Einfrier-Regel/Moratorium)

Nur notiert, damit es nicht verloren geht; jedes wäre ein neues
Format-Plugin und ist damit bis Moratorium-Ende gesperrt (danach 1:2):

- **RDD** („Raw Disk Data", `disk_image/image_rdd.cpp` upstream, inkl.
  `MarkUnstablePulses`, Upstream-Commit `02cd614`) — UFT: 0 Treffer.
- **Pauline-Capture-Ingest** (`pauline2raw/`) — UFT hat nur Enum-Werte.
- **MFM/RAW** (yas-sims Eigenformate, Upstream-README:21) — Nische,
  Priorität gering.

## UNGEKLÄRT

1. Lizenz des Disketten-**Inhalts** von `test_data/2019FM77AVDemo.*`
   (Demo-Programm ≠ Repo-Code) — Vorlage an den Eigentümer, Regel 8.
2. Ob die Negativlisten-Kategorie „integriert" projektweit „vendored"
   oder „verdrahtet" meinen soll — betrifft auch `simonowen/samdisk`
   („teil-vendored"). Eigentümer-Entscheid.
3. Verhalten der Kopie bei `VFO_TYPE_FIXED`-Anforderung nach Entfernung
   des Falls aus `mfm_codec.cpp` (fällt vermutlich auf Default; ohne
   Aufrufer folgenlos, bei Verdrahtung zu prüfen).

## Werkzeugkasten-Befunde dieses Laufs (an den Eigentümer)

| # | Sache | Beleg |
|---|---|---|
| W1 | `inventar.py:143` kürzt das Korpus-Herkunftsfeld hart auf 60 Zeichen — `werkzeug` endet mitten im Wort („GTK3VI", „byte-identical t"). Ausgerechnet das Provenance-Feld verliert Information; die Beschaffungsprüfung (Regel 4) liest genau dieses Feld | Ausgabe `inv["korpus"]`, 2026-08-27 |
| W2 | Der `hinweis`-Text von `query` kommt auf der Windows-Konsole mit `�`-Mojibake an (Gedankenstrich; cp1252-Konsole vs. UTF-8-JSON). Inhalt lesbar, aber ein Feld, das die eigene Reichweite ehrlich benennen soll, sollte nicht selbst kaputt rendern | `query`-Ausgaben oben |
| W3 | Positiv: `abgedeckt` und `plugin_liste_vollstaendig` (MF-611-Fixes) haben exakt wie beabsichtigt funktioniert — drei `abgedeckt:false`-Antworten führten zur Handnachschau und verhinderten einen Fehlschluss bei „wd179x fdc emulation" (die Fähigkeit liegt in der Kopie selbst) | Abfragen oben |

## Regeln, die für dieses Urteil galten

Regel 6 (Negativliste): keine Neubewertung ohne gemessene Änderung —
gemessen: 0 Upstream-Commits, also keine Neubewertung, nur die
beauftragte Divergenzmessung. Regel 4 (Inventar vor Vorschlag,
Handnachschau bei `abgedeckt:false`). Regel 3 (Lizenz aus der Datei).
Regel 7 (Differenzlauf statt Meinung). Regel 8 (test_data-Inhaltslizenz
→ Vorlage). Einfrier-Regel: kein Vorschlag ist Format-/Decoder-Code;
die drei Format-Kandidaten stehen ausdrücklich als gesperrter Fundus.

<!-- uebernommen: MF-626 — Verdikt "Baum-Hygiene, keine Uebernahme" ausgefuehrt: vendorte fdc_bitstream-Bibliothek entfernt (Commit 70d2e0e7) -->
