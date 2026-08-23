# Mammut-Plan — geprüfte Fassung

**Stand:** 2026-08-23 · **Basis:** HEAD `9f23c1fc` (MF-492)
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

Die Sync-Suche selbst bleibt davon unberührt (12 Tests, gegen den belegten
Dekoder abgeglichen). Ihr Wert für die **Datenrettung** entsteht erst als
Startwert für 2.2 Dewarp — siehe dort.

---

## 2. Sofort machbar (ohne Fremdquelle, ohne Fundus, ohne VFS)

Nach Aufwand sortiert, alle mit vorhandenem Anschluss im Baum.

| Baustein | Anschluss im Baum | Aufwand |
|---|---|---|
| **2.2 Dewarp-Stufe** | `uft_media_profile.c` (MF-471/483) misst die Umdrehung bereits; `uft_flux_sync_search` liefert **lokale** Takte je Marke — die Warp-Kurve ist die Kurve dieser Werte über die Spur | M |
| **§2.1.3 Fit-Anzeige** | `uft_sync_median_clock()` vorhanden, Feineinsteller-Feld vorhanden (MF-480) | S |
| **1.4 OEM-Namens-Tabelle** | reine Datentabelle, Bootsektor-Detail existiert | S |
| **1.4 `.tc` (Transcopy)** | Spec aus SAMdisk (im Baum, `src/samdisk/`) statt aus DiskImageTool | S–M |
| **1.1 diskdefs-Parser** | `src/formats/cpm/uft_cpm_diskdef.c` — Zielverzeichnis korrigiert; Grammatik ist im Vorschlag korrekt beschrieben und öffentlich | S |

**Dewarp ist der nächste Schritt.** Begründung: die Sync-Suche hat die
Datenquelle dafür gerade erst geschaffen, und der gemessene Fall liegt
schon vor — eine Spur mit zwei Geschwindigkeiten (Anfang um 70–80 %
gedehnt) verliert Sektoren, die kein einzelner Taktwert rettet, weil es
*keinen richtigen einzelnen Wert gibt*. Gemessen: 3 Sektoren mit der
besten festen Vorgabe. Das ist exakt Dewarps Aufgabe.

Prototyp-Messung (Wegwerf-Code, Vorwärts-/Rückwärts-EWMA über dem
Sofort-Takt `d/k`, k ∈ {2,3,4}), Spalten *gefunden / heil / inhaltlich
falsch trotz heiler Prüfsumme*:

| Fall | ohne | α = 0,01 | α = 0,05 | α = 0,20 |
|---|---|---|---|---|
| 2000 sauber | 11/11/0 | 11/11/0 | 11/11/0 | 11/11/0 |
| 1200, 4 % Zittern | 10/10/0 | 10/10/0 | 10/10/0 | 10/10/0 |
| 1200, 20 % Zittern | 7/0/0 | 7/0/0 | 7/0/0 | **10**/0/0 |
| zwei Tempi (35 % ×1,7) | 8/7/0 | 8/7/0 | **11/10**/0 | **11/10**/0 |
| Rampe +25 % + 6 % Zittern | 11/11/0 | 11/11/0 | 10/10/**0** | 10/10/0 |

Drei Schlüsse, alle aus dieser Tabelle:

1. **Dewarp rettet Daten**, wo die Sync-Suche allein nur findet: auf der
   Zwei-Tempo-Spur 7 → 10 **heile** Sektoren.
2. **Es gibt kein sicheres festes α.** Bei 0,05/0,20 kostet die Rampe einen
   Sektor. Das ist die Überanpassung, die flux-analyze im Kommentar
   erwähnt — hier gemessen statt zitiert. Konsequenz für die Umsetzung:
   dieselbe *Bester-Durchlauf*-Auswahl wie in MF-492, damit Dewarp
   gewinnen, aber nichts kosten kann.
3. **Die dritte Spalte ist durchgehend 0.** Dewarp hat in keinem
   gemessenen Fall einen Sektor mit heiler Prüfsumme und falschem Inhalt
   erzeugt. Das ist die Bedingung, ohne die die Stufe nicht in den
   Lesepfad dürfte.

---

## 3. Blockiert — und woran

| Baustein | Blockiert durch |
|---|---|
| 1.2 AMSDOS | `src/formats/cpc/` fehlt, kein AMSDOS im Baum; `sector-cpc` als Referenz nicht vorhanden |
| 1.3 Fundus-Manifest | **Fundus existiert nicht** |
| 2.3 Timeline-Slices | braucht 2.2; Multi-Capture-Teil braucht Fundus |
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
  (MF-363) gilt weiter: neue Format-/Decoder-Bausteine dieses Plans
  (1.1, 1.2, 1.4 `.tc`) sind Format-Layer und damit
  moratoriumspflichtig; Rettungsalgorithmen (2.2, 2.3) sind es nicht
