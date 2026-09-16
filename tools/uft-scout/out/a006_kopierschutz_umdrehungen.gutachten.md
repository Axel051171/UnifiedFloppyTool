# Gutachten A-006 — `hacking floppy disk.zip` (Kopierschutz · Umdrehungen)

**Auftrag:** „finde alles und alles raussuchen was ich übersehen habe / - wo
können die formate verbessert werden / - ist es auf andere formate übertragbar
/ - welche einstellungen fehlen noch / - was habe wir noch nicht / - brauch es
eine HAL-Erweiterungen / erstelle mir code Beispiele was besser gemacht werden
kann / plan verstanden, was kannst du besser machen ??"

**Stand:** 2026-09-16 · Baum bei `2aa7bda6` (MF-1184) · Posten `A-006`
**Kein Byte des Pakets ist in den Baum geschrieben.** Gemessen an einer Kopie
unter `$CLAUDE_JOB_DIR/tmp/a006`.

---

## Ergebnis in einem Satz

Der Code ist **gut** — der Klassierer trennt Weak von Fuzzy korrekt, sagt bei
fehlenden Messgrößen „unbestimmt" statt zu raten, und seine Tests tragen
eigene Rot-Proben. Die **Lagebeschreibung** ist zur Hälfte veraltet: die
zentrale Behauptung „das Fundament wurde nicht gebaut" ist seit **MF-951/954**
überholt, und die Zulieferung nennt in ihrem eigenen Vertrag eine Funktion,
die sie nicht mitliefert — `uft_rev_align()` —, deren Fehlen **MF-950** an
echten Daten mit **99,71 % falschem Flackern** bezahlt hat.

---

## 1. Was das Paket über sich sagt — geprüft

| Zusage | Messung |
|---|---|
| baut mit `-Wall -Wextra -Wpedantic` | **trägt.** Auch mit `-Werror -pedantic` (gcc 13.1.0): **0 Warnungen** |
| Tests bestehen | **trägt.** 7 Tests, `BESTANDEN (0 Fehler)` |
| Tests tragen Rot-Proben | **trägt, und das ist bemerkenswert.** Test 4: „ein Sucher, der beim ersten Sektorende aufhoert, findet 0 statt 1". Test 5: „ohne Indexzeit 0 Treffer UND Kennzeichen ‚nicht messbar' — der Unterschied ist der Punkt". Test 7: „‚nichts gefunden' heisst nicht ‚ungeschuetzt'" |
| `UNDECIDED` statt Vermutung | **trägt, gemessen** — siehe §5 |

Das ist die sauberste Zulieferung dieser Reihe, was die **Bauform** betrifft.
`SPDX-License-Identifier: GPL-2.0-or-later` steht in jeder Datei.

---

## 2. Die sechs Tatsachenbehauptungen über den Baum, einzeln

Der Header von `uft_revolution.h` führt einen Abschnitt „STAND IM BAUM
(gemessen, nicht vermutet)". Jede Zeile nachgemessen:

| Behauptung | Urteil |
|---|---|
| „Der SCP-BEHAELTER haelt Umdrehungen. `tests/test_scp_weakbit_multirev.c` …" | **trifft zu.** Datei existiert, 7 167 Byte |
| „`src/hal/uft_scp_direct.c:355` sagt es selbst: ‚index_time is currently ignored (samdisk also ignores it)'" | **Wortlaut trifft zu, Stelle nicht.** Der Satz steht bei **Zeile 386**, und zwar genau so. Inhaltlich richtig: `CMD_GET_FLUX_INFO` liefert `[index_time_be32, flux_count_be32]`, und nur `flux_count` wird gebraucht |
| „`src/hal/uft_greaseweazle_full.c` ebenso [verwirft sie]" | **trifft NICHT zu.** `uft_greaseweazle_full.c:1119-1128` legt `fx->index_times` an und füllt es über `uft_gw_decode_flux_index_times()`, dazu `fx->index_count`. Greaseweazle **behält** die Indexzeiten |
| „`uft_flux_revolution_t` und `uft_flux_track_t` … in KEINER `.c`-Datei. Zwei Header, null Verwender" | **trifft zu — und ist schärfer als behauptet.** `include/uft/uft_flux_pll.h:238/249`, **0** `.c`-Verwender. Dazu deklariert `uft_flux_pll.h:782` ein `uft_flux_revolution_alloc()`, das **nirgends definiert** ist: 1 Deklaration, 0 Definitionen. Das ist eine **Phantom-API**, Klasse MF-366 |
| „`src/core/uft_recovery_fusion.c:74`: ‚Flux-level fusion requires PLL pipeline integration — out of scope.'" | **trifft zu**, wortgleich, Zeile 74-75 |
| „17 Dateien in `src/protection/` erwaehnen Weak Bits, 8 erwaehnen Fuzzy Bits" | **Weak 17 trifft zu** (von 42 Dateien). **Fuzzy 9**, nicht 8. Im Bericht zusätzlich: Long Track **18**, nicht 17; No Flux Area **8**, nicht 10 |

**Und die Aussage, die das ganze Paket trägt, ist überholt.** Der Header
folgert: „Die Schutzerkennung ist nicht unvollstaendig, weil Mechanismen
fehlen, sondern weil das Fundament nicht gebaut wurde." Gemessen an
`docs/OPEN_ITEMS.md`:

* **`P3-238` ✅ MF-951/954** — „`uft_hal_read_flux_ex()` traegt **eine**
  ausgeschriebene Bedeutung: `rev_versaetze[k]` ist der Index in `flux`, an
  dem Umdrehung k beginnt." **SCP ist verdrahtet.** KryoFlux sagt begründet ab
  (sein `index[]` trägt Byte-Positionen → P3-243), GW sagt begründet ab (sein
  Dekoder liest den Strom nicht → P3-244). Tests `test_rev_grenzen.c` 11/11,
  `test_hal_rev_grenzen.c` 4/4. Und: „Fehlt einem Treiber die Erweiterung,
  kommen 0 Grenzen — das heisst ‚kann ich nicht sagen', nicht ‚eine
  Umdrehung'."
* **`P3-236` ✅ MF-949** — `src/algorithms/advanced/uft_multi_rev_fusion.c`,
  272 Zeilen, Stimmenzählung je Bitposition. Tests 7/7, **7 von 7
  Mutationsproben gefangen**. Der Punkt notiert ausdrücklich, ein früherer
  Bericht habe vorgeschlagen, das **neu zu bauen** — „der Vorschlag war falsch
  adressiert".
* **`P3-237` ✅ MF-950** — siehe §5, das ist der wichtigste.

**Das Fundament ist also zur Hälfte gebaut, und die gebaute Hälfte ist genau
die, die das Paket benennt.** Was wirklich fehlt, ist die *Bit*-Ebene:
`uft_flux_revolution_t` mit 0 Verwendern und eine Phantom-API dazu.

---

## 3. Frage „was haben wir noch nicht" — zwei echte Lücken

**(a) Die Bitkonfidenzkarte.** Der Baum zählt Stimmen je Bitposition
(`uft_multi_rev_fusion`), aber er hält **kein Ergebnis je Bit** mit Phasenlage
und Flusszahl. `uft_bit_conf_t` ist neu, und `phase_q8` ist der einzige
Mechanismus im ganzen Material, der Fuzzy von Weak *physikalisch* trennen
kann.

**(b) Sektor im Sektor und verschobene Spuren.** Gemessen über
`git ls-files`: `sector_within_sector` 0, `sector_in_sector` 0,
`shifted_track` 0, `overlap_sector` 0 Treffer. In `src/protection/` steht je
**ein** Vorkommen von „overlap" und „overlapping". Die beiden Detektoren
`uft_pscan_sector_within_sector()` und `uft_pscan_shifted_tracks()` sind
damit **wirklich neu** — und sie berühren `P3-47` („Ovl16-Ketten sind
NFA-Kandidaten und werden nicht als Befund gemeldet").

---

## 4. Der Befund, der mich selbst erwischt hat

**Ein Feld trägt drei Namen, und zwei davon sind falsch.**

```
Feldname im Header      flux_count            → „Zahl"
Beschreibung Zeile 110  „gemittelt x16"       → Mittelwert
Umsetzung               (hi - lo) * 16u       → SPANNE
Schwellenname           UFT_WEAK_FLUXVAR_16   → Streuung   ✓ (der einzige richtige)
```

`uft_conf_build()` speichert ausdrücklich die Spanne — der Code sagt es selbst
im Kommentar zwei Zeilen darüber („flux_count traegt die SPANNE x16"), und
`b->flux_count = any ? (uint16_t)((hi - lo) * 16u) : 0xFFFFu;` tut es. **Der
Klassierer ist damit korrekt:** er vergleicht eine Streuung gegen eine
Streuungsschwelle.

**Falsch ist die Feldbeschreibung — und sie hat mich auf Anhieb in die Irre
geführt.** Ich habe aus dem Header vier Erwartungen gebaut und
`uft_rev_classify()` direkt gemessen:

```
alle einig                                       -> NONE       soll NONE       ok
flackert, Phase am Rand (120 > 96), Fluss stabil -> WEAK       soll FUZZY      ABWEICHUNG
flackert, Fluss streut stark (0 statt 16)        -> UNDECIDED  soll WEAK       ABWEICHUNG
flackert, nichts davon auffaellig                -> WEAK       soll UNDECIDED  ABWEICHUNG

1 von 4 wie erwartet, 3 abweichend
```

**Alle drei Abweichungen waren MEINE Fehler, nicht seine.** Unter der echten
Bedeutung ist jede Antwort richtig: `flux_count = 16` heißt „Spanne 1,0
Wechsel" und damit weak, nicht „stabiles Signal"; `flux_count = 0` heißt
„Spanne 0, völlig stabil" und ist deshalb nicht weak. Ich hatte die Werte
nach der Beschreibung gesetzt.

**Das ist der Wert dieses Befundes:** eine Feldbeschreibung, die einen
aufmerksamen Leser beim ersten Versuch dreimal täuscht, täuscht jeden
Integrator. Klasse MF-1015 („zwei Aussagen in einem Feld") — hier drei.
Die Behebung ist **ein Wort**.

---

## 5. Die Lücke, die das Paket selbst nennt und nicht liefert

`uft_revolution.h:132-133` sagt:

> „Die Bitstroeme muessen ausgerichtet sein — gleiche Startposition, gleiche
> Laenge. Dafuer ist `uft_rev_align()` da."

**Gemessen: `uft_rev_align` kommt im ganzen Paket genau einmal vor — in eben
diesem Kommentar.** Die Funktion ist nicht dabei.

Und der Baum hat sie, samt der Messung, warum sie unverzichtbar ist.
**`P3-237`/MF-950** an einem echten Abzug (`gw_amigados.scp`, Spur 0, zwei
Umdrehungen):

> „verglichen 101 343 Bits, uneinig 101 051 = **99,71 %**, mittlere Konfidenz
> 0,5014" — während „Fluss-Rohdaten **50 525 von 50 526 Intervallen GLEICH**,
> bester Bitversatz **+1** → 0,00 % Abweichung über 20 000 Bits".

Dieselbe Diskette, dasselbe Signal, um **ein** Bit verschoben, weil der
Index-Splice das erste Intervall anders teilt (1975 statt 3950 ns).

**Ich habe das an der Zulieferung nachgemessen** — dieselben Bits, dreimal,
nur die Ausrichtung variiert:

```
(1) zwei IDENTISCHE Umdrehungen
    Bits 20000 | stabil 20000 | weak 0 | fuzzy 0 | unbestimmt 0
    FLACKERANTEIL: 0.00 %

(2) EIN Bit Versatz, OHNE Messgroessen
    Bits 20000 | stabil 9966 | weak 0 | fuzzy 0 | unbestimmt 10034
    FLACKERANTEIL: 50.17 %

(3) EIN Bit Versatz, MIT Messgroessen (Phase mittig, Spanne 0)
    Bits 20000 | stabil 9966 | weak 0 | fuzzy 0 | unbestimmt 10034
    FLACKERANTEIL: 50.17 %
```

**Zwei Aussagen, und die erste spricht für das Paket.** Es benennt **nichts**:
weak 0, fuzzy 0, alles „unbestimmt" — auch dann, wenn Messgrößen vorliegen.
Der Klassierer erfindet keinen Befund, und das ist genau, was sein Header
verspricht. Die Gegenprobe in §4 zeigt zudem, dass er weak und fuzzy sehr wohl
sagen **kann** — die Null ist also echt und nicht grün aus dem falschen Grund.

**Die zweite Aussage bleibt trotzdem schwer:** 50,17 % der Bits werden als
flackernd gemeldet, auf einer Spur mit **null** schwachen Bits. Für einen
Bediener heißt „die Hälfte der Spur ist instabil, Ursache unbekannt" nicht
viel weniger als eine Falschaussage. Und **50,17 % ist die untere Grenze**:
mein Strom ist Pseudozufall, wo ein Bitversatz in der Hälfte der Stellen
auffällt. MFM hat Struktur, und dort hat MF-950 **99,71 %** gemessen.

---

## 6. Frage „welche Einstellungen fehlen" und „HAL-Erweiterung"

**Einstellungen:** die Zulieferung bringt zwei Schwellen mit Einheit mit —
`UFT_FUZZY_PHASE_Q8 96` (0,375 Zellbreiten) und `UFT_WEAK_FLUXVAR_16 12`
(0,75 Wechsel Streuung). Beide sind **Vorgaben ohne Quelle**: DrCoolZic
beschreibt das *Phänomen* („at the border of the inspection window"), nicht
die Zahl. Nach dem Maßstab dieses Baums sind das damit zwei Zahlen, die
gemessen gehören, bevor sie fest verdrahtet werden — ein Zellzeit-Histogramm
an einer echten Aufnahme, und das braucht Hardware (MF-310). **S1.**

**HAL-Erweiterung: nein, und das ist gemessen.** `uft_hal_read_flux_ex()` gibt
es seit MF-951/954 und trägt die Umdrehungsgrenzen mit ausgeschriebener
Einheit. Was fehlt, liegt **eine Ebene höher**: die Grenzen kommen an, aber
niemand baut daraus je Bit eine Konfidenz. Die Erweiterung gehört also nicht
in die HAL, sondern zwischen `uft_hal_read_flux_ex()` und
`uft_multi_rev_fusion` — und genau dort ist die Phantom-API
(`uft_flux_revolution_alloc`, 1 Deklaration, 0 Definitionen).

---

## 7. Frage „übertragbar" — ja, ein Teil, und er ist der wertvollste

**Das Muster „ein drittes Urteil statt einer Restkategorie".** `uft_flicker_t`
hat vier Werte, und `UNDECIDED` ist kein Rest, sondern eine Aussage: „es
flackert, und ich kann nicht sagen warum". Der Code sagt es explizit:

> „Flackert, passt in keine Klasse — das ist ein eigenes Ergebnis und keine
> Restkategorie fuer Fuzzy."

Das ist übertragbar auf jede Stelle, an der dieser Baum heute zwei Werte hat,
wo drei hingehören — und der Fall dazu ist dokumentiert: **MF-980**
(„das Format sagt 0xE5" und „hier wurde 0xE5 gelesen" sind zwei Aussagen) und
**D6** (leere Spalten in `VERIFICATION_TIERS.md` unterscheiden). Kandidaten:
`uft_sector_status` (OK/Fehler ohne „nicht gelesen"), und der Abgleicher aus
`A-005`, der „unbekannt" als „widerspricht" zählt.

---

## 8. Code-Beispiele

### 8.1 Ein Wort (schwerster Befund, billigste Behebung)

**Ersetzt:** `include/uft/flux/uft_revolution.h:110`. **Kennzahl:** keine der
vier — es verhindert eine Falschaussage über ein Feld.

```c
/* vorher — täuscht jeden Integrator, mich eingeschlossen: */
uint16_t flux_count; /**< Flusswechsel in dieser Zelle, gemittelt x16  */

/* nachher — sagt, was drinsteht: */
uint16_t flux_spread_16; /**< SPANNE der Flusswechselzahl ueber alle
                          *   Umdrehungen, in Sechzehnteln: (max-min)*16.
                          *   NICHT der Mittelwert. 0xFFFF = nicht
                          *   gemessen. Weak heisst GROSSE Spanne. */
```

**Rotbeweis-Skizze:** `uft_rev_classify()` mit `flux_spread_16 = 0` und
mittiger Phase muss `UNDECIDED` liefern, mit `= 16` muss es `WEAK` liefern —
beide Richtungen, sonst prüft die Zusage nur eine.

### 8.2 Die Ausrichtung ist keine Fußnote

**Ersetzt:** `uft_conf_build()`, `uft_revolution.c`. **Kennzahl:** keine der
vier. **Quelle:** MF-950, 99,71 % an echten Daten.

```c
/* uft_conf_build() nimmt heute ausgerichtete Stroeme AN. Das ist eine
 * Vorbedingung, die niemand prueft — und MF-950 hat gemessen, dass sie
 * an echten Daten regelmaessig verletzt ist.
 *
 * Billigste ehrliche Fassung: die Vorbedingung MESSEN und absagen. */
typedef struct {
    bool     aligned;        /* Vorbedingung geprueft und erfuellt      */
    int      best_shift;     /* gefundener Bitversatz, 0 = ausgerichtet */
    double   disagree_ratio; /* Anteil uneiniger Bits VOR der Korrektur */
} uft_conf_precheck_t;

bool uft_conf_build_checked(const uint8_t *const *streams, size_t nbits,
                            uint8_t nrevs,
                            const int16_t *const *phase_q8,
                            const uint16_t *const *fluxcnt,
                            uft_conf_map_t *out,
                            uft_conf_precheck_t *pre);
/* Regel: disagree_ratio > 0,25 UND ein best_shift != 0, der sie unter
 * 0,01 druecken wuerde  ->  ABSAGE mit beiden Zahlen, keine Karte.
 * Gemessen an meinem Lauf (2): 0,5017 bei best_shift -1. */
```

**Rotbeweis-Skizze:** zwei identische Ströme mit Versatz +1 müssen abgesagt
werden, mit Versatz 0 muss die Karte entstehen. Ohne die zweite Hälfte wäre
„sagt immer ab" genauso grün.

### 8.3 Die zwei Schwellen brauchen eine Quelle oder einen Ort

**Ersetzt:** `uft_revolution.h:180-181`. **Kennzahl:** keine der vier.
**Regel:** MF-1077 — eine Zahl ohne gemessenen Anlass gehört nicht in den
Code.

```c
/* Die beiden Schwellen sind PLAUSIBEL und nicht belegt. DrCoolZic 5.2
 * beschreibt das Phaenomen („at the border of the inspection window"),
 * nicht die Grenze. Also gehoeren sie dorthin, wo der Baum solche Zahlen
 * schon fuehrt — in ein Profil mit Quellenfeld, wie es
 * `uft_fdc_profiles` seit MF-1177 mit `gap_beleg`/`gap_quelle` tut: */
typedef struct {
    int16_t  fuzzy_phase_q8;
    uint16_t weak_spread_16;
    const char *beleg;     /* „gemessen an <Aufnahme>, <Datum>" oder NULL */
    const char *quelle;    /* „DrCoolZic Rev 1.2 §5.2" — Phaenomen, nicht Zahl */
} uft_flicker_schwellen_t;
```

---

## 9. Lizenz und Herkunft — offen, Eigentümer

**Der Code trägt `SPDX-License-Identifier: GPL-2.0-or-later` in jeder Datei**
— das ist geklärt. **Eine `LICENSE`-Datei liegt dem Paket nicht bei**, anders
als bei `A-005`; die Paketlizenz ist damit nur über die Dateiköpfe bestimmt.

**Die Quellen sind benannt und das ist die Stärke dieser Zulieferung:**

| Aussage | Quelle |
|---|---|
| 31 Mechanismen, vier Klassen | Jean Louis-Guérin (DrCoolZic), „Atari Floppy Disk Copy Protection", **Rev 1.2, Juni 2014**, info-coach.fr, Kapitel 2 |
| Weak = keine Flusswechsel, Laufwerk verstärkt Rauschen | Chris Evans 2020; DrCoolZic 5.3 |
| Fuzzy = starkes Signal, grenzwertige Zeitlage | Chris Evans 2020; DrCoolZic 5.1-5.2 |
| Überlappende Sektoren erzeugen unbeabsichtigte Fuzzy Bits | Atari-Forum 21952 |

**Zwei unabhängige Quellen für dieselbe Trennung** — das ist der Maßstab, den
`docs/ORACLES.md` verlangt. **Die offene Frage:** DrCoolZics Kopf trägt
„Copyleft", und die Zulieferung sagt selbst „zitieren und Werte [nehmen]".
*Werte* aus einem Copyleft-Dokument zu nehmen ist die Lage von MF-695 Kanal
**Spec** und unproblematisch; eine *Tabelle* zu übernehmen wäre es nicht. Wo
die Grenze in diesem Fall liegt, ist eine Eigentümer-Entscheidung und steht
hier als Frage. Und C64-/Atari-Schutzarbeit hat in diesem Baum die
nibtools-Vorgeschichte („vier Dateien und Tage gekostet").

---

## 10. Was ich besser machen kann als das Paket

1. **Den Baum befragen, statt ihn zu zitieren.** Drei Behauptungen über den
   Baum sind ungenau (Zeile 386 statt 355; Greaseweazle behält Indexzeiten;
   vier Zählungen um 1-2 daneben) und **eine ist überholt** — die tragende.
   Das Paket sagt selbst, es habe „keinen Blick in den Baum"; genau den habe
   ich.
2. **Die Vorbedingung messen, die das Paket nur nennt.** `uft_rev_align()`
   fehlt; ich habe ausgerechnet und gemessen, was ihr Fehlen kostet
   (50,17 % synthetisch, 99,71 % an echten Daten laut MF-950).
3. **Die eigene Erwartung fallen lassen, wenn sie fällt.** Meine
   Klassierer-Gegenprobe ging **1 von 4** — und alle drei Abweichungen waren
   meine. Ein Gutachten, das seine eigene Fehlmessung zur Feststellung macht,
   ist schlechter als keines. Die Feststellung ist deshalb nicht „der
   Klassierer ist falsch", sondern „die Feldbeschreibung täuscht", und das ist
   belegt — an mir.

**Was ich NICHT besser kann:** die Weak/Fuzzy-Trennung selbst. Zwei
unabhängige Quellen, ein Urteil `UNDECIDED` statt einer Restkategorie, und
Schwellen mit Einheit — das ist die beste Schutzarbeit in diesem Material, und
sie fehlt dem Baum.

---

## 11. Was nicht geprüft ist

* **`uft_protection_scan.c` im Detail.** Gemessen sind seine öffentliche
  Fläche und dass seine sieben Tests durchlaufen; die beiden Detektoren sind
  nicht gegen eine echte geschützte Diskette gehalten — **im Korpus liegt
  keine**, und die Zulieferung sagt das selbst („Alles hier ist synthetisch
  geprueft — deshalb T2-Niveau, nicht T1b").
* **`uft_rev_report()` und `uft_pscan_report()`** — die Textausgaben sind
  gelaufen, aber nicht Zeile für Zeile gegen ihre Zahlen gehalten.
* **Die 31 Mechanismen aus DrCoolZic Kapitel 2** gegen die 42 Dateien in
  `src/protection/` — das ist eine eigene Abgleichsarbeit und nicht Teil
  dieses Gutachtens.
* **Ob `uft_conf_build()` bei `nrevs > 8` richtig rechnet** — `agree` und
  `nrevs` sind `uint8_t`, das ist geprüft; der Überlauf bei sehr vielen
  Umdrehungen ist **nicht** gemessen.

---

## 12. Vorschläge für `docs/OPEN_ITEMS.md`

Fünf, keiner eingetragen — Eigentümer-Entscheidung.

| Vorschlag | Kern | Kennzahl |
|---|---|---|
| **V1** | `uft_flux_revolution_t` hat 0 `.c`-Verwender **und** eine Phantom-API dazu: `uft_flux_revolution_alloc()`, 1 Deklaration, 0 Definitionen (Klasse MF-366) | keine der vier |
| **V2** | Eine Zulieferung nennt `uft_rev_align()` in ihrem Vertrag und liefert sie nicht; ihr Fehlen kostet gemessen 50,17 % (synthetisch) bzw. 99,71 % (MF-950, echt) falsches Flackern | keine der vier |
| **V3** | Ein Feld mit drei Namen: `flux_count` / „gemittelt" / `(hi-lo)*16`. Die Beschreibung täuschte bei der Prüfung 3 von 4 Erwartungen — Klasse MF-1015 | keine der vier |
| **V4** | Der Baum hat keine Erkennung für Sektor-im-Sektor und verschobene Spuren (je 0 Treffer); berührt `P3-47` | keine der vier |
| **V5** | Zwei Schwellen (`96`, `12`) sind plausibel und unbelegt; sie gehören in ein Profil mit Quellenfeld, wie `uft_fdc_profiles` es seit MF-1177 führt | keine der vier |

---

## Abnahme

* Kein Byte des Pakets unter `src/`, `include/`, `tests/` — geprüft über
  `git status --short`.
* Jede Zahl gemessen; die Messprogramme (`mess_ausrichtung.c`,
  `mess_klassierer.c`) liegen im Kratzverzeichnis und sind wiederholbar.
* Drei Behauptungen des Pakets sind berichtigt, **drei eigene Fehlerwartungen
  ebenfalls** (§4).
* Vier Dinge sind ausdrücklich als nicht geprüft benannt (§11).
