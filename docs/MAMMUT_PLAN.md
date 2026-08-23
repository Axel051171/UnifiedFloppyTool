# Mammut-Plan — geprüfte Fassung

**Stand:** 2026-08-23 · **Basis:** HEAD nach MF-495
**Herkunft:** Vorschlag „Mammut-Umsetzung: Konsolidierter Plan aus der
20-Repo-Evaluierung", gegen den Baum nachgemessen und korrigiert.

Dieses Dokument ersetzt den Vorschlag. Jede Zahl darin ist entweder
nachgemessen (mit Befehl) oder als **offen** gekennzeichnet. Der
Vorschlag behauptete „alle technischen Anker gemessen" — sieben davon
hielten der Nachmessung nicht stand; sie stehen unten in §0.

---

## 0. Was die Nachmessung ergab

| # | Behauptung des Vorschlags | Gemessen | Folge |
|---|---|---|---|
| 1 | Basis `6fecf4d` (MF-489) | `6fecf4d0` existiert, HEAD ist inzwischen `9f23c1fc` (MF-492) | Plan war 3 Commits alt |
| 2 | §1.1 „CP/M **von 4** auf 131" | UFT hat **55** Definitionen (`src/formats/cpm/uft_cpm_diskdefs.c`, `all_diskdefs[]`) | Gewinn ist 55→131, nicht 4→131 |
| 3 | §1.1 Ziel `src/formats/rcpmfs/uft_diskdefs_parser.c` | `src/formats/rcpmfs/` enthält nur `uft_rcpmfs.c`; die CP/M-Definitionen liegen in `src/formats/cpm/` | falsches Zielverzeichnis |
| 4 | §1.2 Ziel `src/formats/cpc/uft_amsdos.c` | `src/formats/cpc/` existiert nicht; **AMSDOS kommt nirgends im Baum vor** | Verzeichnis neu, kein Anschluss vorhanden |
| 5 | §2.1 Strategie „zwischen `AGGRESSIVE` und `MULTIREV`" | beide existieren (`uft_recovery_wizard.h:46-47`) | ✅ trägt |
| 6 | §3.2 „Multirev-Klassifikation (WEAK/**MARGINAL** je Position)" | `MARGINAL` **existiert nicht** (5 Klassen: UNKNOWN, STABLE_GOOD, STABLE_BAD_CRC, WEAK, AMBIGUOUS_GOOD) | Klasse erfunden |
| 7 | §3.2 „je **Position** … Rohdaten liegen vor" | `weak_offset` ist ein **Byte-Offset im Sektor** (Header `:158-163`), keine Winkelposition | **Polarkarte hat keine Datenquelle** |
| 8 | §1.3/§3.x „Fundus", §3.1/§3.4 „VFS-P1" als vorhanden vorausgesetzt | `src/vfs/` fehlt; „Fundus" kommt im Baum nicht vor | 4 Bausteine hängen an Nicht-Existentem |
| 9 | 9 Fremd-Repos als Spec-Quelle | **keins** im Baum (nur SAMdisk); geprüft: flux-analyze, superdiskindex, cpmtools, RIDE, floppyai, DiskImageTool, plus3, diskimgr, sector-cpc | jeder „Spec-Nachbau" braucht erst die Quelle |

Nachmessbar mit:

```sh
grep -c '\.name\s*=\s*"' src/formats/cpm/uft_cpm_diskdefs.c   # -> 55
grep -rn 'MARGINAL' include/uft/recovery/                     # -> leer
sed -n '158,163p' include/uft/recovery/uft_multiread_pipeline.h
ls -d src/vfs src/formats/cpc                                 # -> beide fehlen
```

---

## 1. Erledigt

### 2.1 Sync-Suche ohne PLL — ✅ **FERTIG** (MF-492, `9f23c1fc`)

Geliefert als `src/flux/uft_flux_sync_search.c`. Abweichungen vom
Vorschlag, jede gemessen begründet:

- **Ordinal-Vorfilter entfernt.** Gebaut, gemessen: 1,4× (0,110 statt
  0,155 ms/Spur) bei identischem Ergebnis; kein Rotbeweis unterschied ihn
  von seiner Abwesenheit. Sein Fehlermodus ist ein *stilles*
  Falsch-Negativ — im forensischen Lesepfad kein Handel für 45 µs/Spur.
- **Rabin-Karp entfällt.** Bei 8 Symbolen Musterlänge teurer als der
  direkte Vergleich; derselbe Einwand.
- **Der Name `uft_ordinal_am.c` entfiel mit dem Verfahren.** Was trägt,
  ist der Clock-Fit `d_i ≈ k_i·T` mit unbekanntem T — der Vorschlag
  führte ihn nur als Bestätigungsschritt, tatsächlich ist er der Erkenner.
- **Einhängung anders als geplant.** Nicht als eigene
  `UFT_REC_STRATEGY_ORDINAL`, sondern als zweiter Durchlauf in
  `flux_decode_amiga()` — dort greift sie ohne Zutun des Bedieners, und
  genau das war die gemessene Lücke. Eine Wizard-Strategie bleibt
  möglich, sobald es einen Bediener-Ablauf gibt, der sie wählt.
- **Nur automatischer Pfad.** Gesetzte `bitcell_ns` (MF-471),
  `use_pll=false` und gesetzter Feineinsteller (MF-480) haben Vorrang.
  Ohne diese Sperre kippte der Rettungspfad drei Vertragstests.

Punkt 3 des Vorschlags (§2.1.3, „Ordinal-Fit schlägt 96,3 % vor" in der
Feineinsteller-Anzeige) ist **noch offen** — die Messung liegt vor
(`uft_sync_median_clock()`), die GUI-Anzeige fehlt.

**Nachgemessen und richtiggestellt (MF-494).** Die erste Fassung von
FLUX-14 schrieb MF-492 drei Erfolge zu. Nachgeprüft, wann der zweite
Durchlauf *überhaupt läuft* und wie viele Sektoren eine *heile Prüfsumme*
haben, bleibt davon einer:

- 1200 ns mit 20 % Zittern: 0 → **7 gefundene** Sektoren, davon **0 heile**.
  Gefunden ist nicht gerettet (MF-466).
- Zwei weitere Zeilen gehörten dem **Histogramm** (MF-488) — dort läuft der
  zweite Durchlauf gar nicht.
- Eine Zeile läuft, ändert aber nichts.

**Und die Korrektur war selbst zu streng (MF-497).** Sie maß nur rohe
Intervallfelder ohne Index-Marken — dort greift die Profil-Stufe (MF-471)
gar nicht. Auf dem echten Wandlungspfad, wo sie greift und um den Faktor
zwei danebenliegt, rettet der zweite Durchlauf **22 von 22 Sektoren, alle
heil**; ohne ihn sind es 0. Das ist Datenrettung, und die erste Korrektur
hat sie übersehen, weil ihr Messaufbau den Pfad nicht enthielt.

Arbeitsteilung, gemessen: Kandidat 2 greift bei **gleichmäßigem** Strom mit
falsch abgeleitetem Takt (dort steigt die Entzerrung wegen Spanne < 1,02
aus), Kandidat 3 bei **ungleichmäßiger** Geschwindigkeit (dort trifft kein
einzelner Taktwert beide Abschnitte). Keiner ersetzt den anderen.

---

## 2. Sofort machbar (ohne Fremdquelle, ohne Fundus, ohne VFS)

Nach Aufwand sortiert, alle mit vorhandenem Anschluss im Baum.

| Baustein | Anschluss im Baum | Aufwand |
|---|---|---|
| ~~**2.2 Dewarp-Stufe**~~ | ✅ **fertig** (MF-495, FLUX-15): zwei Tempi 7 → 9 heile Sektoren; kein Mittel gegen Zittern; `RECOVERED_DEWARP` offen (Provenance ist Operations-Kette, keine Sektor-Klasse) | — |
| ~~**§2.1.3 Fit-Anzeige**~~ | ✅ **fertig** (MF-496, FLUX-16) als Meldung im Wandlungsergebnis: „Die Sync-Marken messen 50 % der abgeleiteten Zellendauer". Das GUI-Feld selbst ist offen — nicht wegen Aufwand, sondern weil ein Widget in dieser Sitzung nicht prüfbar ist | — |
| **1.4 OEM-Namens-Tabelle** | reine Datentabelle, Bootsektor-Detail existiert | S |
| **1.4 `.tc` (Transcopy)** | Spec aus SAMdisk (im Baum, `src/samdisk/`) statt aus DiskImageTool | S–M |
| **1.1 diskdefs-Parser** | `src/formats/cpm/uft_cpm_diskdef.c` — Zielverzeichnis korrigiert; Grammatik ist im Vorschlag korrekt beschrieben und öffentlich | S |

### 2.2 Dewarp — ✅ **FERTIG** (MF-495, FLUX-15)

Geliefert als `src/flux/uft_dewarp.c`. Vorwärts-/Rückwärts-EWMA über dem
Sofort-Takt `d / k` (k ∈ {2,3,4}), danach Umrechnung auf einen gemeinsamen
Bezugstakt. Als dritter Kandidat in `flux_decode_amiga()` verdrahtet, mit
derselben Bester-Durchlauf-Auswahl wie MF-492.

| Fall | Spanne | ohne | mit |
|---|---|---|---|
| zwei Tempi (35 % ×1,7) | 1,459 | 8 gef / 7 heil | **10 / 9** |
| Rampe +25 % + 6 % Zittern | 1,253 | 11 / 11 | 11 / 11 |
| Rampe +40 % + 12 % Zittern | 1,406 | 11 / 11 | 11 / 11 |
| sauber | 1,000 | 11 / 11 | Stufe läuft nicht |

In keinem gemessenen Fall entstand ein Sektor mit heiler Prüfsumme und
falschem Inhalt.

**Abweichungen vom Vorschlag, jede gemessen begründet:**

- **Keine eigene Pipeline-Stufe „nach Capture, vor PLL".** Der Vorschlag
  wollte sie global; sie läuft stattdessen als Kandidat im Decoder. Grund:
  eine globale Stufe müsste entscheiden, *ob* sie zuschlägt, bevor jemand
  das Ergebnis kennt. Als Kandidat tritt sie gegen den unentzerrten
  Durchlauf an und kann nur gewinnen.
- **Kein Automatik-Auslöser über die Drehzahlmessung (MF-471/483).** Der
  Auslöser ist die *selbst gemessene* Spanne aus dem Strom (≥ 1,02), nicht
  die Umdrehungsdauer. Die Drehzahlmessung sieht Unterschiede **zwischen**
  Umdrehungen, der Gleichlauffehler sitzt aber **innerhalb** einer.
- **`RECOVERED_DEWARP` gibt es nicht.** `uft_provenance.h` führt Herkunft
  als Operations-Kette (CAPTURE/DECODE/…), nicht je Sektor. Eine
  Sektor-Herkunft wäre eine Änderung an `flux_decoded_sector_t` — offen.
- **Die Warp-Kurve wird nicht angezeigt.** `uft_dewarp_result_t::warp_span`
  steht als Diagnosewert bereit („das Medium eiert um 21 %"), das
  OTDR-Panel greift ihn noch nicht ab — offen, klein.

**Zwei Befunde, die über den Baustein hinausgehen:**

1. **Dewarp ist kein Mittel gegen Zittern.** Über je 20 Spuren mit 0…12 %
   Zittern (1320 Dekodierungen): ein heiler Sektor mehr, insgesamt. Das ist
   Rauschen. Wer die Schwelle senkt, kauft Rechenzeit, keine Sektoren.
2. **Der Startwert entscheidet mit.** Zwei-Tempo-Spur mit dem gemessenen
   Startwert (2040 ns aus der Sync-Suche): 9 heile Sektoren; mit 1200 ns —
   auch plausibel — nur 3. Damit ist nachträglich belegt, wozu 2.1 im
   Lesepfad steht: als Lieferant des Startwerts für 2.2, nicht als eigener
   Retter.

**Nächster Schritt.** Von den sofort machbaren Bausteinen ist die
Fit-Anzeige (§2.1.3) der kleinste mit echtem Nutzen: Spanne und gemessene
Zellendauer liegen vor, der Mensch sieht sie noch nicht. **2.3
Timeline-Slices** ist durch 2.2 nicht mehr blockiert, braucht für den
Multi-Capture-Teil aber weiterhin den Fundus.

---

## 3. Blockiert — und woran

| Baustein | Blockiert durch |
|---|---|
| 1.2 AMSDOS | `src/formats/cpc/` fehlt, kein AMSDOS im Baum; `sector-cpc` als Referenz nicht vorhanden |
| 1.3 Fundus-Manifest | **Fundus existiert nicht** |
| 2.3 Timeline-Slices | 2.2 ist fertig; nur der Multi-Capture-Teil braucht noch den Fundus |
| 2.4 Mining-Targets | braucht Hardware mit Motor+Seek (**kein Gerät vorhanden**, MF-310) und Fundus für die append-only-Iterationen |
| 3.1 `uft-catalog` | superdiskindex nicht im Baum; braucht Fundus |
| 3.2 Polarkarte | **Datenquelle existiert nicht** (siehe §0.7): das Multiread-Ergebnis trägt keine Winkelpositionen |
| 3.3 HOST_OS_CONTAMINATION | Datenerhebung braucht Referenz-Images + Windows-Mount-Messung |
| 3.4 fsck / VFS-Treiber | `src/vfs/` fehlt |

**Zwei Vorbedingungen entscheiden über acht Bausteine:** ein Fundus
(Archiv-Ablage) und `src/vfs/`. Solange beide fehlen, ist Welle 3
praktisch nicht adressierbar; der Vorschlag setzt beide als vorhanden
voraus (Leitprinzip 2: „Abhängigkeiten nur … auf Vorhandenes").

### Was §3.2 zuerst bräuchte

Eine Polarkarte verlangt, dass jede Sektorlesung ihre **Winkelposition**
kennt. Die Rohdaten dafür sind vorhanden (Flux-Übergangszeiten relativ
zum Index), sie werden nur nicht durchgereicht: `multiread_sector_t` trägt
`weak_offset` (Byte im Sektor) und `distinct_contents`, keinen Winkel.
Der ehrliche erste Schritt ist deshalb nicht das Widget, sondern ein Feld
`index_offset_ns` im Multiread-Ergebnis — ohne das ist jede Polarkarte
gemalt und nicht gemessen.

---

## 4. Lizenz

`LICENSE:35` sagt **`GPL-2.0-or-later`** (nicht GPL-2.0-only, wie
mehrere Vorschläge angenommen haben). Für die Matrix des Vorschlags
ändert das die Verträglichkeit mit Apache-2.0-Quellen (diskimgr):
GPL-2.0-**or-later** darf nach GPLv3 wechseln und damit Apache-2.0-Code
aufnehmen; GPL-2.0-only dürfte das nicht.

Unverändert gilt: **solange die Lizenz einer Quelle ungeklärt ist, wird
sie nicht ausgelagert und nicht behalten, sondern entfernt, bis die
Herkunft belegt ist.** Offen bleibt LIC-1 (`uft_multiread_pipeline.c`
trägt `SPDX: MIT`, dokumentiert sich aber als „nach a8rawconv
`sift_sectors`", GPLv2+).

---

## 5. Verwandte Dokumente

- [`MASTER_PLAN.md`](MASTER_PLAN.md) — führendes Statusdokument
- [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) — FLUX-14 (MF-492) beschreibt
  Baustein 2.1 vollständig, samt der drei Korrekturen und der offenen
  Punkte
- [`VERIFICATION_PLAN.md`](VERIFICATION_PLAN.md) — die EINFRIER-REGEL
  (MF-363), **präzisiert durch MF-498**

  Die frühere Fassung dieser Zeile behauptete, 1.1/1.2/1.4 seien
  moratoriumspflichtig, „Rettungsalgorithmen (2.2, 2.3)" dagegen nicht.
  Das war mit sich selbst nicht konsistent: 2.3 Timeline-Slices ist
  genauso neuer Decoder-Layer-Code wie ein Parser. Unter der präzisierten
  Regel entfällt die Unterscheidung nach *Schicht* — es zählt, ob der
  Baustein **oracle- oder rotbeweis-zuerst** baubar ist:

  | Baustein | Referenz, gegen die gebaut wird |
  |---|---|
  | 1.1 diskdefs | `cpmls`-Roundtrip als Oracle, Rotbeweise vor dem Parser |
  | 1.4 `.tc` | SAMdisk-Quelle im eigenen Baum (`src/samdisk/`) |
  | 2.3 Timeline | Scheiben-Invarianten als Rotbeweise, vor dem Code |
  | 1.2 AMSDOS | `sector-cpc` — **nicht im Baum**, also weiter blockiert |

  Damit sind 1.1, 1.4 und 2.3 zulässig, 1.2 bleibt es nicht — nicht wegen
  der Schicht, sondern weil die Referenz fehlt.
