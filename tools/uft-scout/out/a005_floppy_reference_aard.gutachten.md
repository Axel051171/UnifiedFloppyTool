# Gutachten A-005 — `UFT_Floppy_Reference_AARD_Paket.zip`

**Auftrag:** „arbeite alles sehr genau aus / finde alles und alles raussuchen
was ich übersehen habe / - wo können die formate verbessert werden / - ist es
auf andere formate übertragbar / - welche einstellungen fehlen noch / - was
habe wir noch nicht / - brauch es eine HAL-Erweiterungen / erstelle mir code
Beispiele was besser gemacht werden kann / plan verstanden, was kannst du
besser machen ??"

**Stand:** 2026-09-16 · Baum bei `e5daccf1` (MF-1183) · Posten `A-005`
**Kein Byte des Pakets ist in den Baum geschrieben.** Gemessen wurde an einer
Kopie unter `$CLAUDE_JOB_DIR/tmp/a005`; `git status` zeigt keine Datei des
Pakets unter `src/`, `include/`, `tests/` oder `data/`.

---

## Ergebnis in einem Satz

Die **physische Tafel** (24 Sätze: Spuren/Zoll, Bit/Zoll, Koerzitivität) füllt
eine Lücke, die im Baum gemessen **vollständig** ist — `coercivity`,
`tracks_per_inch` und `bits_per_inch` haben über `git ls-files` **je 0
Treffer**. Der **Abgleicher** dagegen hat einen Befund, der ihn in seiner
heutigen Form unbrauchbar macht: er kann *nicht* unterscheiden und sagt bei
Gleichstand nicht ab — für eine PC-1,44-M-Diskette meldet er **drei Sätze mit
score 100, sieben verglichenen Feldern und null Abweichungen**, und **keiner
davon ist der PC-Satz**.

---

## 1. Methode, und was das Paket über sich sagt — geprüft

Gemessen wurde **über die API des Pakets**, nicht durch Nachlesen der TSV: eine
Messung am ausgelieferten Artefakt statt an seiner Vorlage (Klasse MF-1000 —
nicht dieselbe Quelle befragen wie der Prüfling). Die Messprogramme liegen als
`mess_katalog.c` und `mess_aard.c` im Kratzverzeichnis.

**Drei Selbstzusagen des Pakets tragen, und das ist gemessen:**

| Zusage | Messung |
|---|---|
| „compile as strict C11 with `-Wall -Wextra -Werror`" | **trägt.** Beide Tests bauen mit `-Wall -Wextra -Werror -pedantic` (gcc 13.1.0) ohne eine Warnung und bestehen: `floppy reference tests: OK`, `AARD artifact tests: OK` |
| „`UFT_FLOPPY_REF_WRITE_SAFE` is unset for all 113 records" | **trägt.** Über `uft_floppy_reference_is_write_safe()` gezählt: **0 von 113** |
| „`conclusive` is always `false`" | **trägt** in allen gemessenen Fällen, auch bei gesetzter Flagge |

**Eine Zusage trägt nicht, und sie stand schon in der Aufnahme:**
`INTEGRATION.md` sagt zweimal, der Baum sei bereits verdrahtet — „The current
UFT workspace is already wired" und „The repository copy already contains this
wiring in `tests/CMakeLists.txt`". Gemessen:
`git ls-files | grep -iE 'aard|floppy_reference'` ist **leer**. Kein Byte des
Pakets liegt im Baum, weder Quelle noch Test noch Daten, und in
`tests/CMakeLists.txt` steht keine Zeile dazu. Das ist dieselbe Klasse wie die
„aktuelle Verdrahtung" der C64PP-Zulieferung (`A-008`) und der
Atari-ST-Zulieferung (`A-009`) — siehe §10.

**Bestand, gemessen:**

```
logische Sätze                                113
physische Sätze                                24
feste Geometrie (alle vier Felder eindeutig)   95 von 113
mit Bereich oder Lücke                         18
Flagge INCOMPLETE                               3
bytes_per_sector == 0                           3
write_safe gesetzt                              0   (Zusage: 0)
mit gerechneter Nutzlast                       95
```

---

## 2. Der Befund, der alles andere überlagert

`uft_floppy_reference_match()` rangiert und sagt bei Gleichstand **nicht ab**.
Gemessen an vier Geometrien, die UFT sicher kennt:

| Beobachtung | was der Abgleicher meldet |
|---|---|
| PC 1,44 M — 80×2×18×512, 300 U/min, MFM, 3,5" | `wiki-apple-ii-05` **100**, `wiki-macintosh-02` **100**, `wiki-atari-st-tt-falcon-03` **100** — je 7 verglichen, **0 abweichend** |
| PC 160 K — 40×1×8×512, 300 U/min, MFM, 5,25" | `wiki-coleco-adam-01` **100**, `wiki-ibm-pc-compatibles-05` **100**, `…-06` 81 |
| C64 1541 Zone 1 — 35×1×21×256, GCR, 5,25" | `wiki-commodore-64-8-bit-01` **100**, `…-02` 80, `wiki-apple-ii-01` 75 |
| Amiga DD — 80×2×11×512, 300 U/min, MFM, 3,5" | `wiki-amiga-03` **100**, `wiki-amiga-02` 95, `wiki-apple-ii-04` 90 |

**Der Abgleicher rechnet richtig und antwortet falsch.** Alle drei
100er-Treffer der ersten Zeile *sind* 80×2×18×512 bei 300 U/min in MFM auf
3,5 Zoll — die Geometrie ist über vier Plattformen hinweg **dieselbe**. Was
fehlt, ist nicht Genauigkeit, sondern die **Absage bei Gleichstand**. Wer
`matches[0]` liest, bekommt für eine PC-Diskette „Apple II", und für eine
PC-160-K-Diskette „Coleco Adam" — entschieden hat das die
**Tabellenreihenfolge**, denn `match_compare()` bricht den Gleichstand
zuletzt über `reference_index`
(`src/formats/reference/uft_floppy_reference.c:124-134`).

Das ist wörtlich die Lage, die dieser Baum zweimal behoben hat:

* **MF-729:** „`uft_probe_ranking.tied` meldete dabei **1** — ‚eindeutig', wo
  niemand etwas erkannt hatte." Der Abgleicher hat **kein** `tied`-Feld.
* **MF-1039:** „Seit MF-1039 sagen Sonde und `open` bei Mehrdeutigkeit **ab**
  statt zu raten" — bei `cpm` trafen drei Definitionen dieselbe Größe, und die
  alte Auswahl nahm die erste.
* **Sonden-Doktrin, Regel 3** (`docs/SONDEN_DOKTRIN.md`, Eigentümer-
  Entscheidung 2026-09-15): „bleibt es gleich, gewinnt **keiner**
  (‚mehrdeutig' mit beiden Namen)."

**Zweiter Befund, gemessen: „unbekannt" und „widerspricht" sind dasselbe.**
`score_field()` erhöht `possible` und `compared` **vor** der Prüfung, und
`in_range()` verlangt `minimum != 0 && maximum != 0`
(`uft_floppy_reference.c:61-80`). Ein Satz, dessen Feld der Katalog nicht
kennt, bekommt also einen **Abweichungszähler** wie ein Satz, der der
Beobachtung widerspricht. Gemessen mit der Beobachtung „nur rpm = 300": **3
Sätze ohne Drehzahlangabe, alle 3 als abweichend gezählt.** Dasselbe trifft die
3 Sätze mit `bytes_per_sector == 0`, also genau die nicht byteorientierten
Medien, für die das Feld gar nicht existieren *kann*.

Die Flagge `UFT_FLOPPY_REF_INCOMPLETE` gibt es (3 Sätze tragen sie) — der
Bewerter liest sie **nicht**. Das ist die Regel aus **MF-980** („das Format
sagt 0xE5" und „hier wurde 0xE5 gelesen" sind zwei Aussagen) und die
Spaltenregel **D6**.

**Dritter Befund, kleiner:** der Rückgabewert von
`uft_floppy_reference_match()` bedeutet zwei verschiedene Dinge — bei
`matches_capacity == 0` die **Gesamtzahl der Sätze** (113), sonst die Zahl der
**geschriebenen** Treffer (`uft_floppy_reference.c:168-186`). Zwei Aussagen in
einem Feld, Klasse MF-1015.

---

## 3. Frage 1 — wo können die Formate besser werden

**Nicht durch neue Geometrien, sondern durch eine Achse, die der Baum schon
angelegt hat und nicht füllt.** Von den 113 Sätzen tragen **18** einen
*Bereich* statt eines festen Werts, und genau die sind interessant: ein
Spurbereich oder eine Drehzahlspanne beschreibt **zonenweise Aufzeichnung** und
**variable Drehzahl**. Der Baum hat dafür Felder —
`uft_drive_profile_t.speed_zones` und `.half_tracks`
(`include/uft/hal/uft_drive.h`) — und die Zulieferung hat dafür Flaggen
(`UFT_FLOPPY_REF_VARIABLE_SPT`, `_VARIABLE_RPM`, `_ZONED_RECORDING`).

**Verbessert werden können die Formate also an der Stelle, wo UFT heute eine
Zahl führt, wo eine Spanne hingehört.** Vorgeführt an MF-1026: dort lagen zwei
Zonengrenzen von `victor9k` um eins daneben, und weil sich +1 und −1 aufhoben,
blieb die Summe 1224 — eine Größenprüfung konnte es nie sehen. Ein
Referenzsatz mit `sectors_min/sectors_max` hätte genau diesen Fall als
Widerspruch gemeldet.

**Was der Katalog dagegen NICHT kann, und das gehört gesagt:** eine Tier-Stufe
heben. Die Quelle ist die Wikipedia-Seite „List of floppy disk formats" und
damit eine **Sekundärquelle**; `docs/ORACLES.md` verlangt für einen Eintrag ein
Werkzeug, das gebaut und **ausgeführt** wurde, oder eine Primärbeschreibung.
Eine Tafel, die sagt „so war es", ist kein Erzeuger und kein Leser.
**Kennzahl: keine der vier.**

---

## 4. Frage 2 — ist es auf andere Formate übertragbar

**Ja, und zwar genau ein Teil davon: die Bereichsform.** Nicht der Katalog.

Der Baum hält dieselbe Größe wiederholt an mehreren Stellen, und MF-1177 hat
daraus einen Grundsatz gemacht („eine Größe, eine Rechnung"). Die Bereichsform
`min/max` ist die billigste Bauform gegen die Klasse, die dieser Baum am
teuersten bezahlt hat:

| Fall | was passierte | was ein `min/max` gemeldet hätte |
|---|---|---|
| MF-1026 `victor9k` | zwei Zonengrenzen um eins daneben, Summe blieb 1224 | Sektorzahl außerhalb des Bereichs für Spur 48 und 70 |
| MF-1037 `dim` | zwei Medienbytes falsch zerlegt, Konfidenz 88 | Geometrie außerhalb des Bereichs des Medienbytes |
| MF-1016 `jv1` | zweite Seite erfunden | `sides_max = 1` |

**Übertragbar ist also die Form, nicht der Inhalt** — und sie ist übertragbar
auf die Tafeln, die der Baum ohnehin führt: `fat_geometry_*` (8 Zeilen, seit
MF-1183 mit Produktionsaufrufer), `uft_cpm_diskdef.c` (17 Definitionen),
`uft_fdc_profiles` (17 Profile), `dsk_generic` (49 Tafeln).

---

## 5. Frage 3 — welche Einstellungen fehlen noch

**Das Paket nennt acht, und fünf davon hat der Baum schon. Das ist die
wichtigste Messung dieses Gutachtens**, denn die `README.md` sagt, diese Felder
„must continue to come from a verified controller/profile source such as the
separately harvested OmniFlop definitions, a primary technical manual, or
measured media" — und übersieht dabei den eigenen Baum.

Gemessen über `git ls-files` je Bezeichner:

| was die README als fehlend nennt | im Baum | Messung |
|---|---|---|
| controller data rate and clock | **ja** | `uft_drive_profile_t.data_rate_dd/hd/ed_kbps`, `bit_cell_dd/hd_us` (`include/uft/hal/uft_drive.h`) |
| GAP1/GAP2/GAP3/GAP4 | **ja** | `uft_fdc_gap_space` 3 `.c`, `uft_fdc_calc_gap3` 3 `.c` (MF-1177) |
| IAM and sync layout | **ja** | die `iam`-Angabe je FDC-Profil (MF-1177) |
| sector IDs and physical sector order | **ja** | `uft_sector_order` 4 `.c`, 6 Anordnungen als je EINE Datenzeile (MF-1175) |
| interleave, head skew, track skew | **ja** | `uft_interleave` 5 `.c` / 3 `.h` |
| CRC layout and deleted-data marks | teilweise | je Format, keine gemeinsame Achse |
| write precompensation | **ja, aber tot** | `write_precomp_ns` in acht Profilen gefüllt, **0 Leser** — P3-409 |
| write current | **nein** | 0 Treffer |

**Fünf von acht sind da. Was wirklich fehlt, ist nicht die Angabe, sondern der
Leser** — und das ist kein neuer Befund, sondern P3-429 und P3-409.

---

## 6. Frage 4 — was haben wir noch nicht

**Die physische Medienachse. Und das ist gemessen vollständig leer:**

```
coercivity          .c/.cpp  0    .h  0
tracks_per_inch     .c/.cpp  0    .h  0
bits_per_inch       .c/.cpp  0    .h  0
```

Kein Treffer im ganzen Baum. Die 24 physischen Sätze der Zulieferung tragen
genau das — `tracks_per_inch`, `bits_per_inch_min/max`, `coercivity_label`,
`unformatted_capacity_label` — und das ist **der einzige Teil des Pakets, der
eine gemessene Lücke füllt** statt eine vorhandene Tafel zu verdoppeln.

**Warum das mehr ist als Buchhaltung:** eine Spurdichte entscheidet, ob ein
80-Spur-Laufwerk eine 40-Spur-Diskette lesen kann (doppelte Spurbreite,
halbe Schrittweite), und eine Koerzitivität entscheidet, ob ein HD-Laufwerk
eine DD-Diskette **schreiben** darf, ohne sie zu beschädigen. Das sind
Aussagen über das Medium, die UFT heute an keiner Stelle führt — und es sind
genau die `limitations[]`, von denen MF-1176 sagt, die HAL-Merkmalstafel trage
sie und niemand lese sie.

---

## 7. Frage 5 — braucht es eine HAL-Erweiterung

**Nicht eine neue, sondern einen Leser für die vorhandene — und dabei ist mir
ein Fehler in meiner eigenen Arbeit von heute aufgefallen.**

`uft_drive_profile_t` (`include/uft/hal/uft_drive.h`) führt bereits
`data_rate_dd_kbps`, `data_rate_hd_kbps`, **`data_rate_ed_kbps`**,
`bit_cell_dd_us`, `bit_cell_hd_us`, `track_length_bits`, `write_precomp_ns`,
`half_tracks`, `speed_zones`. Gemessen:

```
src/hal/uft_hal_profiles.c:103:    .data_rate_ed_kbps = 1000.0,
Leser im ganzen Baum (ohne die eigene Datei) : 0
Quellenangabe in der Datei
  (Quelle|Datenblatt|Source|ECMA|ISO|uPD765|WD177|belegt) : 0
```

**BERICHTIGUNG zu MF-1183 (`e5daccf1`), heute committet.** Dort steht:
„2,88 M (36 × 512 = 18 432, 1000 kbit/s): im Baum gibt es **keine
Ratenquelle** dafür." Der Satz ist als Aussage über eine *Quelle* richtig — es
gibt keine —, aber er liest sich wie „die Zahl steht nirgends", und **die Zahl
steht da**: 1000,0 in `uft_hal_profiles.c:103`. Was fehlt, ist ihre Quelle und
ihr Leser. **Am Code von MF-1183 ändert das nichts:** eine ungenannte Zahl aus
einer Tafel mit null Lesern zu übernehmen wäre genau MF-1077 („die Zahl darf
nicht das Motiv sein"). Die Absage bleibt richtig; ihr Grund wird genauer, und
er heißt jetzt P3-409/P3-429 statt „gibt es nicht".

**Die HAL-Erweiterung, die sich daraus ergibt, ist also klein und hat einen
gemessenen Anlass:** `uft_drive_profile_t` um die drei Medienfelder erweitern
(TPI, BPI, Koerzitivität), **und zwar mit benannter Quelle je Zahl** — nicht
aus der Wikipedia-Ernte, die als Sekundärquelle dafür nicht reicht, sondern aus
den Datenblättern, die P3-409 schon nennt (WD177x, uPD765, WD9216). Die Ernte
liefert die **Kandidatenliste**, nicht die Werte.

---

## 8. Code-Beispiele — was besser gemacht werden kann

Jedes Beispiel nennt die Stelle, die es ersetzt, und die Kennzahl nach MF-640.

### 8.1 Der Gleichstand muss eine Absage sein (schwerster Befund)

**Ersetzt:** `uft_floppy_reference.c:117-121` (`score_record`) und
`:168-186` (`uft_floppy_reference_match`). **Kennzahl:** keine der vier —
es verhindert eine Falschaussage, und das ist nach MF-1077 die richtige
Richtung.

```c
/* Neu im Kopf, neben uft_floppy_match_t: */
typedef struct uft_floppy_ranking {
    size_t   count;          /* geschriebene Treffer                   */
    size_t   catalog_size;   /* Gesamtzahl der Saetze — EIGENES Feld,  */
                             /* nicht mehr der Rueckgabewert (MF-1015) */
    unsigned tied;           /* wie viele teilen den Spitzenrang       */
    unsigned best_compared;  /* Felder, die den Spitzenrang tragen     */
    bool     ambiguous;      /* tied > 1 -> der Aufrufer darf NICHT    */
                             /* matches[0] als Antwort nehmen          */
} uft_floppy_ranking_t;

size_t uft_floppy_reference_match_ranked(
        const uft_floppy_observation_t *observation,
        uft_floppy_match_t *matches, size_t capacity,
        uft_floppy_ranking_t *ranking);
```

```c
/* Im Rumpf, nach dem qsort: */
if (ranking) {
    unsigned t = 0u;
    for (i = 0u; i < count; ++i)
        if (all[i].score == all[0].score &&
            all[i].mismatches == all[0].mismatches) ++t;
    ranking->count         = written;
    ranking->catalog_size  = count;
    ranking->tied          = t;
    ranking->best_compared = all[0].compared;
    /* Gemessen an PC 1,44 M: t == 3 (Apple II, Macintosh, Atari ST) —
     * dieselbe Geometrie auf vier Plattformen. Regel 3 der Sonden-
     * Doktrin: bleibt es gleich, gewinnt KEINER. */
    ranking->ambiguous     = (t > 1u);
}
```

**Rotbeweis-Skizze:** die Beobachtung 80×2×18×512/300/MFM/3,5" muss
`ambiguous == true` und `tied == 3` liefern; die Gegenprobe ist Amiga DD
(80×2×11), wo `tied == 1` sein muss — ohne die zweite Hälfte wäre
„immer mehrdeutig" genauso grün.

### 8.2 „Unbekannt" ist kein Widerspruch

**Ersetzt:** `uft_floppy_reference.c:61-80` (`in_range`, `score_field`).
**Kennzahl:** keine der vier. **Regel:** MF-980, D6.

```c
typedef enum { FELD_UNBEKANNT, FELD_TRIFFT, FELD_WIDERSPRICHT } feldurteil_t;

static feldurteil_t urteile(unsigned wert, unsigned min, unsigned max)
{
    if (wert == 0u)             return FELD_UNBEKANNT; /* Beobachtung leer */
    if (min == 0u || max == 0u) return FELD_UNBEKANNT; /* KATALOG leer —   */
                                 /* vorher: WIDERSPRICHT, gemessen 3 von 3 */
    return (wert >= min && wert <= max) ? FELD_TRIFFT : FELD_WIDERSPRICHT;
}

static void score_field(unsigned wert, unsigned min, unsigned max,
                        unsigned gewicht, unsigned *punkte,
                        unsigned *moeglich, unsigned *verglichen,
                        unsigned *abweichend, unsigned *unvergleichbar)
{
    switch (urteile(wert, min, max)) {
    case FELD_UNBEKANNT:
        if (wert != 0u) ++*unvergleichbar;  /* eigene Zahl, eigene Aussage */
        return;                             /* NICHT in `moeglich` */
    case FELD_TRIFFT:
        *moeglich += gewicht; ++*verglichen; *punkte += gewicht; return;
    case FELD_WIDERSPRICHT:
        *moeglich += gewicht; ++*verglichen; ++*abweichend; return;
    }
}
```

**Rotbeweis-Skizze:** die Beobachtung „nur rpm = 300" muss für die 3 Sätze ohne
Drehzahlangabe `mismatches == 0` und `uncomparable == 1` melden. Heute
gemessen: `mismatches == 1` für alle 3.

### 8.3 Die Medienerkennung ist eine Aufzählung

**Ersetzt:** `uft_floppy_reference.c:37-52` (`medium_code`).
**Kennzahl:** keine der vier. **Regel:** MF-930 (Aufzählung statt Messung) —
dieselbe Bauform, die im Kopf eines *Tores* dieses Baums stand.

`medium_code()` zählt elf Schreibweisen auf (`3½`, `31⁄2`, `3 1/2`, `3.5`, …)
und gibt für alles andere 0 zurück; `medium_equal()` fällt dann auf exakten
Textvergleich zurück. Zwei Folgen, beide klein aber messbar:
`strstr(text, "2 inch")` trifft auch in „12 inch", und eine zwölfte
Schreibweise fällt still durch.

```c
/* Statt Schreibweisen aufzuzaehlen: die ZAHL vor dem Zollzeichen lesen
 * und in Achtel-Zoll rechnen. Eine Rechnung veraltet nicht.
 *   3.5 / 3,5 / 3 1/2 / 3½  -> 28
 *   5.25 / 5¼               -> 42
 *   8                       -> 64
 * Unerkanntes gibt 0 und faellt wie bisher auf Textvergleich zurueck. */
static unsigned medium_achtel_zoll(const char *text);
```

### 8.4 Der AARD-Schwellenwert gehört an die Leiter, nicht an eine Handzahl

**Ersetzt:** `src/analysis/uft_aard_artifact.c:80-93` (`finish_report`).
**Kennzahl:** keine der vier. **Regel:** MF-1153, Eigentümer-Entscheidung vom
2026-09-15 — ausgeführt in §9.

---

## 9. Die AARD-Hälfte, getrennt bewertet

**Zwei Dinge sind gemessen, und sie fallen verschieden aus.**

**(a) Die Zufallsrate ist in Ordnung, und das ist jetzt eine Zahl statt einer
Beteuerung.** Das Paket sagt „XOR candidates can occur by chance" und liefert
keine Messung. Gemessen an 64 × 1 474 560 = 94 371 840 Byte Pseudozufall
(eigener festgesäter LCG, damit die Zahl reproduzierbar ist):

```
XOR-Kandidaten gesamt           : 6
ausgerechnete Erwartung         : 5.60      (255 / 2^32 je Stelle)
Läufe mit >= 1 XOR-Kandidat     : 6 von 64
Literaltreffer "AARD"           : 0
possible_aard_artifact gesetzt  : 0 von 64
Nullpuffer                      : 0 / 0 / confidence 0 / Flagge nein
3-Byte-Puffer                   : 0 / 0 / Flagge nein
```

Die gemessene Rate trifft die Arithmetik, und weil der XOR-Zweig nur **15**
Punkte gibt und die Flagge bei **45** steht, kann Zufall sie **nicht** setzen.
Das ist die Eichung, die MF-729 verlangt (auf einem Nullpuffer darf nichts
≥ 50 melden), und sie besteht. **Das ist ein Punkt für das Paket**, und er
stand bisher nur als Vorsatz da.

**(b) EIN gewöhnliches englisches Wort setzt die Flagge.** Gemessen:

```
Eingabe : 1,44 M Leerzeichen + das Wort AARDVARK an Versatz 12345
literal_aard_hits       : 1
confidence              : 45
possible_aard_artifact  : JA
conclusive              : nein
```

`memcmp(last4, "AARD", 4)` trifft an **jeder** Stelle, ohne Kontextbedingung,
und `finish_report()` gibt dafür allein 45 Punkte — genau den Schwellenwert.
Jede Diskette mit einer Wortliste, einem Wörterbuch, einem niederländischen
oder deutschen Text („Aardappel", „Aardvark") oder dem Namen „Aardman" wird
als möglicher AARD-Fund gemeldet.

**Und hier liegt der Befund, der über die Falschtrefferrate hinausgeht:** die
Skala 45/15/20/10/+10 ist eine **von Hand vergebene Konfidenz** in einem Baum,
der am **2026-09-15** entschieden hat, dass Konfidenzen abgeleitet werden
(`docs/SONDEN_DOKTRIN.md`, Grundsatz MF-1153). Die Leiter dort vergibt +50 nur
für eine „Kennung an **fester Position**, formatspezifisch" — eine
Zeichenfolge, die überall stehen darf, ist das nicht, und ohne Kennung ist die
Obergrenze **45**. Die Skala landet also zufällig genau auf der Obergrenze der
Doktrin, während sie das Kennungsband beansprucht.

**Dazu eine Messung, die diese Zulieferung nicht wissen konnte:** das Tor
`scripts/audit_sondendoktrin.py` würde diese Zahl **nicht sehen**. Es liest das
Feld `.probe =` und danach genau dessen Rumpf; eine Konfidenz in einem
Analysemodul ohne Plugin-Struktur ist für das Tor unsichtbar — das ist die
Lücke, die ich heute in MF-1182 als **P3-440** gemessen habe (19 delegierende
Sonden, ≥ 8 mit echter Zahl eine Ebene tiefer). Die fallende Grundlinie steht
bei **82**; diese Zulieferung würde eine **83.** Handzahl einführen, ohne dass
das Tor anschlägt.

**Vorschlag, nicht Änderung:** die drei Merkmale trennen und benennen statt
zu addieren — `literal_at_any_offset`, `xor_candidate`, `context_marker` —, den
Schwellenwert über `uft_probe_konfidenz()` bilden und `possible_aard_artifact`
erst setzen, wenn ein Literaltreffer **und** ein Kontextmerkmal
zusammenkommen. Gemessen wäre das an genau der AARDVARK-Probe, die heute
durchgeht.

---

## 10. Ein Muster über drei Zulieferungen — und das ist keine Stilfrage

Gemessen in `A-008` (C64PP), `A-009` (Atari ST) und hier:

| Paket | mitgelieferte lebende Baudatei | Baum | Paket | Differenz |
|---|---|---|---|---|
| `A-008` | `tests/CMakeLists.txt` | 361 386 | 341 230 | **−20 156** |
| `A-009` | `tests/CMakeLists.txt` | 361 386 | 341 513 | **−19 873** |
| `A-009` | `src/mainwindow.cpp` | 25 457 | 25 318 | **−139** |
| `A-005` | — (liefert keine Baudatei mit) | — | — | — |

`A-005` ist der **saubere** Fall: es liefert `INTEGRATION.md` mit qmake- und
CMake-**Zeilen**, keine ganzen Dateien. Genau so gehört es. Die anderen zwei
würden beim Kopieren fremde Verdrahtung still zurücknehmen — unter anderem den
CMake-Zweig, den MF-1182 heute angelegt hat.

**Was allen drei gemeinsam ist:** eine Verdrahtungszusage, die sich nicht
messen lässt. `A-005` sagt „already wired" (leer gemessen), `A-008` sagt
„aktuelle Verdrahtung" (20 KB veraltet), `A-009` dasselbe. Das gehört als
**eine** Zeile nach `docs/OPEN_ITEMS.md`, nicht dreimal.

---

## 11. Lizenz — Eigentümer-Entscheidung, keine Empfehlung

**Der Code ist unproblematisch:** `LICENSE` ist GPL-2.0-or-later, © 2024–2026
Axel Kramer, und beide Quelldateien tragen `SPDX-License-Identifier:
GPL-2.0-or-later` in Zeile 1. Eigener Code, keine Fremdübernahme.

**Die Daten sind die Frage, und ich entscheide sie nicht.** Die 113 + 24 Sätze
sind aus „List of floppy disk formats" geerntet, verfügbar unter **CC BY-SA
4.0**. Zwei Punkte, beide sachlich:

1. **CC BY-SA 4.0 ist copyleft.** Eine Weitergabe der Sätze verlangt
   Namensnennung und Weitergabe unter gleichen Bedingungen. Die TSV-Dateien
   führen die Quell-URL, und `UFT_FLOPPY_REFERENCE_SOURCE_URL` gibt sie über
   die API aus — das ist die richtige Richtung, aber ob es genügt, ist eine
   Rechtsfrage.
2. **Das EU-Datenbankherstellerrecht (§§ 87a ff. UrhG) ist eine zweite,
   eigene Schicht** — unabhängig davon, ob die einzelnen Zahlen als Tatsachen
   frei sind. Das ist **genau die Frage, die bei der OmniFlop-239-Ernte offen
   steht** und wegen der sie nicht im Baum liegt.

**Eine dritte Frage, sachlich und ohne Empfehlung:** ob eine Sekundärquelle in
einem Werkzeug stehen soll, dessen Leitsatz „Keine erfundenen Daten" ist. Der
Katalog behauptet nichts Falsches — aber er ist die erste Tafel im Baum, deren
Herkunft nicht auf ein Werkzeug, eine Primärbeschreibung oder eine Messung
zurückgeht. Das ist kein Einwand, sondern eine Einordnung.

---

## 12. Was ich besser machen kann als das Paket

Vier Dinge, alle gemessen und nicht gemeint:

1. **Die Zahl liefern, die fehlt.** Das Paket sagt „can occur by chance"; ich
   habe die Rate gemessen (6 gegen 5,60 erwartet, 0 von 64 Flaggen) und damit
   die Vorsicht *bestätigt*. Eine Beteuerung, die man nachrechnet, wird zu
   einem Beleg — und wenn sie fällt, zu einem Befund. Hier hielt sie.
2. **Den Gleichstand messen, statt ihn zu rangieren.** Die drei 100er für eine
   PC-Diskette fallen nur auf, wenn man den Abgleicher mit einer Geometrie
   füttert, deren Antwort man kennt. Das Paket testet gegen sich selbst
   (`test_floppy_reference.c`, synthetisch); ich habe es gegen vier Geometrien
   gehalten, die dieser Baum belegt hat.
3. **Den eigenen Baum befragen, bevor man eine Lücke behauptet.** Fünf der acht
   „fehlenden Einstellungen" sind da. Das ist keine Nachlässigkeit der
   Zulieferung, sondern die Klasse MF-930: eine Aufzählung veraltet still, und
   sie stand hier in einer README.
4. **Den eigenen Fehler mitmessen.** Die ED-Rate `1000.0` in
   `uft_hal_profiles.c:103` widerlegt die Formulierung meines eigenen
   MF-1183-Commits von heute. Ein Gutachten, das nur die Zulieferung prüft und
   die eigene Arbeit auslässt, prüft die Hälfte.

**Was ich NICHT besser kann:** die physische Tafel. 24 Sätze mit TPI, BPI und
Koerzitivität sind Arbeit, die im Baum niemand geleistet hat, und die Lücke ist
mit 0/0/0 Treffern vollständig. Der Weg dorthin führt über die Datenblätter aus
P3-409, nicht über die Ernte — aber **die Kandidatenliste ist die Ernte**, und
die ist bares Geld.

---

## 13. Was nicht geprüft ist

* **Die Richtigkeit der 113 Sätze.** Ich habe nicht einen einzigen Wert gegen
  eine Primärquelle gehalten. Stichproben gegen vier Geometrien, die UFT belegt
  hat, waren **widerspruchsfrei** — das ist keine Prüfung der Tafel.
* **Der GUI-Teil.** `ToolsTab::onAnalyze()` wurde nicht gelesen. Die README
  beschreibt zwei Schalter dort; im Baum liegt keine Zeile davon, und der
  Gedächtnisstand zu `ToolsTab` nennt drei Knöpfe, die in die andere Richtung
  lügen. Das gehört gemessen, bevor dort etwas angeschlossen wird.
* **`uft_aard_scan_file()`** — nur `uft_aard_scan_buffer()` ist gemessen. Die
  Datei-Variante liest in 64-KiB-Blöcken mit einem 11-Byte-Übergabefenster; ob
  eine Marke über eine Blockgrenze wirklich gefunden wird, ist **behauptet und
  nicht nachgemessen**.
* **Die TSV gegen die `.inc`.** Ob die generierten Datendateien die TSV
  vollständig abbilden, ist nicht geprüft; gemessen ist nur, was die API
  ausgibt.

---

## 14. Vorschläge für `docs/OPEN_ITEMS.md`

Fünf, keiner davon eingetragen — das ist eine Eigentümer-Entscheidung.

| Vorschlag | Kern | Kennzahl |
|---|---|---|
| **V1** | Die physische Medienachse fehlt vollständig (`coercivity`/`tracks_per_inch`/`bits_per_inch` je 0 Treffer). Kandidatenliste liegt vor, Werte brauchen Datenblätter (P3-409-Quellen) | keine der vier |
| **V2** | Drei Zulieferungen tragen eine Verdrahtungszusage, die sich nicht messen lässt; zwei liefern lebende Baudateien mit, die 20 KB hinter dem Baum liegen | keine der vier |
| **V3** | Berichtigung zu MF-1183: `data_rate_ed_kbps = 1000.0` steht in `uft_hal_profiles.c:103`, mit 0 Lesern und 0 genannter Quelle. Die Absage bleibt richtig, ihr Grund heißt P3-409/P3-429 | keine der vier |
| **V4** | Ein Rangierer ohne `tied`-Feld ist die Lage aus MF-729 in neuer Gestalt — gemessen an einem fremden Abgleicher, der für eine PC-Diskette drei 100er meldet | keine der vier |
| **V5** | Die Doktrin gilt auch für Konfidenzen **außerhalb** der Plugin-Struktur, und das Tor sieht sie nicht (P3-440). Eine Analysefunktion mit Handskala ist derselbe Fall | keine der vier |

---

## Abnahme dieses Gutachtens

* Kein Byte des Pakets unter `src/`, `include/`, `tests/`, `data/` — geprüft
  über `git status --short`.
* Jede Zahl in diesem Dokument ist gemessen; die Messprogramme
  (`mess_katalog.c`, `mess_aard.c`) liegen im Kratzverzeichnis und sind
  wiederholbar.
* Die Lizenzfragen stehen als Fragen, nicht als Empfehlungen.
* Vier Dinge sind ausdrücklich als **nicht geprüft** benannt (§13).
