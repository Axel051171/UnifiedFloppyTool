# Plan: FluxEngine-Formatvergleich und a8rawconv-Abgleich

**Quellen:** `UFT-NN-fluxengine-format-comparison.md`,
`a8rawconv-full-analysis.md` (beide 2026-09-05 zugeliefert)
**Erstellt:** 2026-09-05
**Regel:** Jede Phase nennt ihre Kennzahl (Regel 9, MF-640). Was keine
bewegt, steht als **Fundus** — notiert, nicht eingeplant.

---

## Phase 0 — Prüfung der Prämissen (ERLEDIGT, hier das Ergebnis)

Die beiden Dokumente enthalten 12 überprüfbare Behauptungen. Alle wurden
am Baum gemessen, bevor eine Zeile Plan entstand. **Vier tragen nicht** —
und drei davon sind der Grund, warum die vorgeschlagenen Aufgaben in
dieser Form nicht ausgeführt werden.

| # | Behauptung | Befund | Beleg |
|---|---|---|---|
| 1 | `include/uft/formats/rolandd20.h` enthält nur Include-Guards | ✅ **wörtlich zutreffend** | 4 Zeilen, `#ifndef`/`#define`/`#include <stdint.h>`/`#endif` |
| 2 | `docs/FORMAT-GAPS.md` führt Roland D-20 als abgedeckt | ✅ zutreffend | `docs/FORMAT-GAPS.md:21-22` |
| 3 | Roland D-20 sollte umgesetzt werden | ❌ **GESPERRT** | EINFRIER-REGEL, s. u. |
| 4 | Brother-Decoder liegt in `src/formats/brother/uft_brother.c` | ⚠ **Pfad falsch** | es ist `src/formats/brother/brother.c` |
| 5 | 15 FluxEngine-Formate fehlen → aufnehmen | ❌ **GESPERRT**, Rückstandstabelle erlaubt | dieselbe Regel |
| 6 | `uft_d64_writer.c` hat eine feste 21-Elemente-Tabelle mit Rückfall | ❌ **VERALTET** | **MF-859** hat sie entfernt, **MF-861** die Reihenfolge geklärt; der Block steht nur noch als Kommentar `:53-73` |
| 7 | `uft_precomp_track_mac800k` hat keinen Aufrufer | ⚠ **halb falsch** | sie **hat** einen: `uft_write_precomp.c:95`. Verwaist ist der **Moduleingang** `uft_precomp_apply()` — 0 Aufrufer in `src/` |
| 8 | `flux_raw_reverse()` ist verdrahtet | ✅ zutreffend | `uft_format_convert_flux.c:1048,1076` |
| 9 | `vote_byte()` mischt Bytes über Lesungen hinweg | ✅ zutreffend | `uft_multiread_pipeline.c:120`, Aufruf `:457` |
| 10 | …und das landet unverändert im Zielabbild | ❌ **trifft nicht zu** (MF-884 nachgemessen) | Fabriziert wird nur in `WEAK`, und dort ist `good_reads == 0`, also `recovered == false` — beide Verbraucher (`uft_format_convert_flux.c:190`, `:541`) schreiben nur bei `recovered`. MF-466 hält die Tür zu. Meine erste Fassung dieser Zeile nannte vier Klassen und war aus der Lektüre abgeleitet, nicht gemessen |
| 11 | UFT kennt keine Gruppierung nach Winkelposition | ✅ zutreffend | kein `position`/`angular`/`phantom` in der Fusion außer einem Doxygen-Wort |
| 12 | `-p` (Taktperiode in Prozent) fehlt | ✅ zutreffend | 0 Treffer für `clock_period_pct`/`period_percent`/`speed_percent` |

### Warum Roland D-20 und die 15 Formate gesperrt sind

`docs/VERIFICATION_PLAN.md:158-159` bindet das Moratorium an zwei
Bedingungen; `:234` sagt wörtlich: *„Das **Moratorium bleibt in Kraft**,
bis NFD-r0 auf T1/T1b ist."* Gemessen in
`docs/VERIFICATION_TIERS.md:67`: **`nfd` steht auf T2.** Das Moratorium
gilt also.

Dazu kommt eine zweite, unabhängige Sperre für D-20 speziell: die
EINFRIER-REGEL verlangt, dass **jede Zahl gemessen** ist. Im Korpus liegt
**kein** Roland-D-20-Abbild (geprüft). Die Geometrie 78/1/256/12/Skew 5
stammt aus FluxEngines Profil — das ist eine **benannte Referenz**, aber
keine Messung. Bedingung (a) wäre erfüllt, (b) nicht.

> **Das ist keine Ablehnung des Fundes.** Der Fund ist gut und der
> Brother-Hinweis („precisely the same format") ist wertvoll. Er gehört
> nach MF-695 in den **Fundus** — benannt wartend mit dem, was ihn
> öffnen würde: ein D-20-Abbild im Korpus, oder NFD-r0 auf T1b plus zwei
> Hebungen (1:2).

### Was von den beiden Dokumenten übrig bleibt

**Kein Befund berührt erreichbaren, falschen Code.** Beim Schreiben
dieses Plans war das noch anders gedacht — Phase 1 sollte der eine sein;
die Messung in MF-884 hat auch das widerlegt (siehe dort). Übrig bleiben
Doku-Berichtigung, Bestandsaufnahme und Fundus.

Das ist kein schlechtes Ergebnis, sondern das erwartbare: die
Vorgängerarbeit (MF-466, MF-845, MF-859, MF-860, MF-861) hat genau die
Stellen bereits geschlossen, auf die beide Zulieferungen zeigen. Beide
sind gegen einen älteren Stand geschrieben.

---

## Phase 1 — ERLEDIGT (MF-884), und die Prämisse dieses Abschnitts war falsch

**Kennzahl:** keine. Das Ergebnis ist eine **ausgesprochene und geprüfte
Zusage**, kein Fehlerfix — weil es keinen Fehler gab.

### Was hier stand, und warum es nicht trug

Die erste Fassung dieses Abschnitts behauptete: vier von fünf
Fusionsklassen erzeugen Bytefolgen, die nie auf einer Diskette standen,
und das lande über `uft_format_convert_flux.c` im Zielabbild. Das war aus
der Lektüre **abgeleitet, nicht gemessen** — genau der Fehler, den dieser
Baum an anderen Stellen dreizehnmal geführt hat. Gemessen ergibt sich ein
anderes Bild:

| Klasse | fabriziert die Ausgabe? | `recovered` |
|---|---|---|
| `STABLE_GOOD` | nein — ein Inhalt, das Voting liefert ihn | ja |
| `STABLE_BAD_CRC` | **nein** (der Plan sagte ja) | nein |
| `AMBIGUOUS_GOOD` | nein — MF-845/860 | nein |
| **`WEAK`** | **ja, in 99,9–100 %** | **nein** |
| `UNKNOWN` | **nein** (der Plan sagte ja) — `execute()` kehrt mit Fehler zurück, der Puffer bleibt unberührt | — |

**Und das Entscheidende:** `recovered = (confidence >= min_confidence)
&& (good_reads > 0)`. In der `WEAK`-Klasse ist `good_reads` per
Definition 0 — sonst wäre es keine `WEAK`-Klasse. Beide
Produktionsverbraucher schreiben nur bei `recovered`
(`uft_format_convert_flux.c:190` und `:541`). **Das Fabrikat erreicht das
Zielabbild nie.** MF-466 hält die Tür zu, seit es sie geschlossen hat.

Zweitens: `tests/test_multiread_kein_mischbyte.c` bewacht das Byte-Voting
im `WEAK`-Zweig **ausdrücklich** — „Der Fix darf diesen Zweig NICHT
anfassen." MF-845 hat das bewusst entschieden, und die Begründung trägt:
ohne CRC ist die byteweise Mehrheit die beste verfügbare Schätzung, und
bei einem echt schwachen Sektor mit vielen Lesungen ist sie **besser** als
jede einzelne Lesung, weil sie die stabilen Bits behält. Der Plan hätte
das umgestoßen, ohne das Argument zu kennen.

### Was tatsächlich offen war

Die Sicherheit ruht auf einer Kopplung **zweier getrennter Regeln** —
MF-466 (`good_reads > 0`) und der Klassendefinition —, und die daraus
folgende dritte Regel stand nirgends:

> Meldet `multiread_execute()` `recovered`, dann ist `output` eine
> Bytefolge, die mindestens eine Lesung wirklich geliefert hat.

Sie wurde an **2 000 000** Zufallseingaben gesucht zu brechen: 948 434
mit `recovered`, 380 014 Fabrikate, **0** zugleich. Sie hält — aber
niemand hatte sie aufgeschrieben, und der öffentliche Header sagte zu
`data` nur „Recovered data".

### Was MF-884 liefert

1. **Der Vertrag steht im Header** (`include/uft/recovery/
   uft_multiread_pipeline.h`): eine Tafel, was `data` je Klasse ist, und
   die ausdrückliche Zusage an `recovered`.
2. **Ein Streifzug-Test** in `tests/test_multiread_kein_mischbyte.c`
   (`wer_recovered_meldet_hat_wirklich_gelesen`), 20 000 Runden mit
   festem Startwert. Er prüft zusätzlich, dass der Streifzug **beide
   Seiten erreicht hat** — sonst wäre ein grüner Lauf nur ein Lauf, der
   nichts geprüft hat.
3. **Gegenprobe gefahren:** entfällt `good_reads > 0`, meldet der Test
   625 Verletzungen in 20 000 Runden, erste in Runde 20 — und fällt
   **nur** diesen einen der sechs Fälle.

**Kein Verhalten geändert.** `vote_byte()`, `vote_buffer()` und die
Klassenlogik sind unverändert.

### Was daraus für den Rest des Plans folgt

Die Zulieferung `a8rawconv-full-analysis.md` §2 hatte in der Sache recht
(UFT mischt Bytes, a8rawconv nie) — aber die Folgerung „das landet
unverändert im Zielabbild" gilt seit MF-466/845 nicht mehr. Wer den
Abschnitt weiterverwendet, sollte das mitlesen.

---

## Phase 2 — Winkelposition: UFT kennt keine Phantomsektoren (Bestandsaufnahme, keine Umsetzung)

**Kennzahl:** keine. **Fundus mit Entscheidungsbedarf.**

Gemessen: in `uft_multiread_pipeline.c` kommt kein Begriff für
Winkelposition vor. a8rawconv gruppiert vor jedem Inhaltsvergleich nach
Position mit 3 % Toleranz (`disk.cpp`, `if (fabsf(poserr) > 0.03f)
break;`) und trennt so **zwei Sektoren gleicher Nummer an physisch
verschiedenen Stellen** — ein bekannter Kopierschutztrick — von echten
Mehrfachlesungen desselben Sektors.

UFT behandelt beide heute gleich: als widersprüchliche Lesungen desselben
Sektors. Das ist bei geschützten Disketten die falsche Antwort.

**Warum hier nichts umgesetzt wird:** Die Eingangsstruktur
`multiread_pass_t` trägt **keine** Positionsangabe. Sie zu ergänzen ist
eine ABI-Änderung an einer öffentlichen Struktur — das ist
`abi-bomb-detector`-Arbeit und braucht eine Eigentümer-Entscheidung. Und
ohne ein Korpus-Abbild mit echten Phantomsektoren wäre jede Umsetzung
ungeprüft.

**Was zu tun ist:** Eintrag in `docs/OPEN_ITEMS.md` als P3, mit
(a) der Messung oben, (b) der benannten Referenz, (c) dem, was ihn
öffnen würde: eine Aufnahme mit doppelter Sektornummer im Korpus. Nach
MF-695 heißt Fundus **benannt wartend**, nicht verfallen.

### Abnahme
- [ ] P3-Eintrag steht, mit Belegspalte und Öffnungsbedingung
- [ ] **Keine** Änderung an `multiread_pass_t`

---

## Phase 3 — `uft_precomp_apply()` ist verwaist, und es gibt keinen Anschluss

**Kennzahl:** keine (Fundus). **Aber:** die Herkunftsfrage ist geführt.

Gemessen, mit Berichtigung der Zulieferung: `uft_precomp_track_mac800k`
**hat** einen Aufrufer (`uft_write_precomp.c:95`). Verwaist ist der
Moduleingang `uft_precomp_apply()` — **0** Aufrufer in `src/`.

Das Dokument bietet zwei Wege an. **Weg (a) — anschließen — entfällt
messbar:** `src/formats/apple/mac_dsk.c` ist ein Container-Leser, kein
Fluss-Dekoder; einen Mac-800K-Flusspfad gibt es nicht. Eine Aufrufstelle
zu erzwingen wäre eine Verdrahtung ohne Verbraucher — genau das Muster,
das [[measure_before_wiring]] verbietet.

**Also Weg (b):** ehrlich dokumentieren. Der Baum hat dafür bereits die
Ablage: `docs/orphan_baseline.txt`.

### Aufgabe
1. Prüfen, ob `uft_precomp_apply` schon in `docs/orphan_baseline.txt`
   steht. Wenn ja: die **Begründung** ergänzen (wartet auf einen
   Mac-800K-Flusspfad), nicht den Eintrag verdoppeln.
2. Im Kopf von `src/core/uft_write_precomp.c` festhalten: worauf die
   Funktion wartet, und dass a8rawconvs `-P mac800k` das Referenz-
   verhalten für den Tag ist, an dem der Pfad existiert.
3. **Kein** `-P`-Schalter. Ein Schalter ohne Wirkung ist ein
   Workaround-Stub — das Dokument sagt das selbst.

### Abnahme
- [ ] `python scripts/audit_orphan_modules.py` (oder das im Baum
      geführte Äquivalent) meldet keine neue Abweichung
- [ ] Kein neuer GUI-Schalter

### Anti-Muster
- ❌ Keine künstliche Aufrufstelle, „damit es nicht verwaist aussieht".
- ❌ Die Funktion **nicht** löschen. Sie ist eine korrekte, attribuierte
  Portierung (MF-697) und wartet auf einen Verbraucher — das ist Fundus,
  nicht Müll.

---

## Phase 4 — `FORMAT-GAPS.md` sagt über Roland D-20 etwas Falsches

**Kennzahl:** keine direkt — aber es ist eine **Doku-Aussage ohne
Deckung**, und die Klasse ist in diesem Baum viermal teuer geworden.
Doku-Korrekturen sind unter EINFRIER ausdrücklich erlaubt.

`docs/FORMAT-GAPS.md:21-22` sagt, D-20 sei „bereits als Katalog-ID …
(`rolandd20.h`)" abgedeckt. Der ganze Inhalt dieser Datei sind vier
Zeilen Include-Guard. Die einzige weitere Nennung im Baum ist ein
Enum-Tag in `src/parsers/lexy_experimental/scp_parser_complete.hpp:61,418`
— einem Verzeichnis, das **in keinem Build** steht.

### Aufgabe
1. Den Eintrag berichtigen: **nicht** abgedeckt. Ein leerer Header ist
   keine Katalog-ID, sondern ein Etikett.
2. Den Brother-Hinweis als **Fundus** aufnehmen, mit dem, was ihn
   öffnen würde: FluxEngines Profil sagt *„precisely the same format as
   the Brother word processors"*, und `src/formats/brother/brother.c`
   existiert. Ob dessen GCR-Routine geometrieunabhängig ist, ist eine
   **Lesefrage** und darf beantwortet werden — das ist Analyse, kein
   neuer Code.
3. Die 15 fehlenden FluxEngine-Formate als **Rückstandstabelle** mit
   S/M/L nach dem bestehenden Schema ergänzen, ausdrücklich unter der
   Überschrift „gesperrt bis Moratoriumsende (NFD-r0 auf T1/T1b), danach
   1:2".
4. Bei jedem der 15: die FluxEngine-Profildatei als Quelle nennen, und
   `read_support_status` mit übernehmen — `DINOSAUR` ist FluxEngines
   **niedrigste** Reifestufe und darf nicht als „verifiziert" gelesen
   werden. `agat`, `mx` u. a. tragen sie.

### Abnahme
- [ ] `scripts/check_consistency.py` grün (die Doku-Tore prüfen
      Querverweise)
- [ ] Kein neuer Eintrag in `src/formats/`, kein neuer Header
- [ ] Die Antwort auf 2. steht als **Befund** da („wiederverwendbar
      ja/nein, weil …"), nicht als Absichtserklärung

### Anti-Muster
- ❌ `rolandd20.h` **nicht** mit Geometrie füllen. Das wäre der neue
  Format-Code, den die Regel sperrt — auch wenn es nur fünf Konstanten
  sind.
- ❌ Den Enum-Eintrag im `lexy_experimental`-Parser **nicht** anfassen.
  Das Dokument sagt es selbst, und der Baum bestätigt es: das
  Verzeichnis steht in keinem Build.

---

## Phase 5 — `-p` Taktperiode: Fundus, ausdrücklich nicht gebaut

**Kennzahl:** keine. Nach Regel 9 damit **Fundus, nicht Auftrag.**

Gemessen: `clock_period_pct`, `period_percent`, `speed_percent` haben
**null** Treffer in `src/` und `include/`. a8rawconv hat mit `-p` einen
Schalter für 50–200 % der Nenn-Taktperiode, für Laufwerke, die nicht
exakt auf Drehzahl laufen.

Das ist eine echte Lücke — aber sie bewegt keine der vier Kennzahlen,
und ein neuer Dekodier-Parameter ist Decoder-Layer. Eintrag in
`docs/OPEN_ITEMS.md`, mit der Öffnungsbedingung: *eine Aufnahme im
Korpus, die mit 100 % nicht und mit einem anderen Wert doch dekodiert.*
Ohne die wäre der Schalter selbst ungeprüft.

> **Querverweis:** Das berührt den offenen Befund aus dem GUI/HAL-Bericht
> — KryoFlux und FluxEngine erfinden 300 U/min als Vorgabe und leiten
> daraus einen Laufwerkstyp ab, der als Messung angezeigt wird. Wer die
> Drehzahlfrage angeht, sollte beides zusammen ansehen; sie sind
> dieselbe Größe von zwei Seiten.

---

## Phase 6 — Abnahme des Ganzen

1. `cmake --build build-tests-ci` grün, **ohne Warnungen**
2. `ctest --test-dir build-tests-ci` — heute **345/345** mit einem
   benannten Skip; die Zahl darf nur steigen
3. `python scripts/check_consistency.py` — alle **57** Tore 0
4. `python scripts/verify_build_sources.py` ohne neue Regression
5. `python scripts/gen_stand.py` und `gen_fs_tiers.py` nachgezogen
6. Gegenprobe auf Anti-Muster:
   - `git diff --stat` enthält **keine** neue Datei unter `src/formats/`
   - `include/uft/formats/rolandd20.h` ist unverändert
   - `git diff` enthält keine Zeile aus `src/a8rawconv/`
7. Je Commit: MF-Nummer aus der laufenden Zählung (letzte vergebene:
   **MF-883**), Conventional Commit, Body nennt die erfüllten Phasen

---

## Reihenfolge und Zuschnitt

> **Stand 2026-09-05: alle Phasen abgeschlossen.** MF-884 (Phase 1),
> MF-886 (Phasen 2+3), MF-887 (Phasen 4+5). Abnahme nach Phase 6 gefahren:
> Bau **0 Warnungen**, `ctest` **345/345**, alle 57 Tore 0,
> `verify_build_sources.py` ohne neue Abweichung. Anti-Muster-Gegenprobe
> gegen den Plan-Commit: **0** neue Dateien unter `src/formats/`,
> `rolandd20.h` unverändert, **0** Zeilen aus `src/a8rawconv/` berührt,
> `multiread_pass_t` unverändert.

| Phase | Art | Umfang | eigener Commit |
|---|---|---|---|
| 1 | ~~Korrektheit~~ → Zusage aussprechen + prüfen | **erledigt (MF-884)** | ja |
| 2 | Bestandsaufnahme | **erledigt (MF-886)** — P3-159 | mit 3 zusammen |
| 3 | Ehrlichkeit | **erledigt (MF-886)** — P3-160 + Modulkopf | mit 2 zusammen |
| 4 | Doku-Berichtigung + Rückstand | **erledigt (MF-887)** | ja |
| 5 | Fundus | **erledigt (MF-887)** — P3-161 | mit 4 zusammen |

**Phase 1 ist erledigt** — und ihr Ergebnis war, dass sie keinen falschen
Code berührte. Die übrigen sind Buchhaltung.

## Was dieser Plan bewusst NICHT enthält

- Roland D-20 (gesperrt: Moratorium **und** kein Korpus-Abbild)
- Eines der 15 FluxEngine-Formate (dieselbe Sperre)
- Den D64-Interleave-Umbau (**erledigt** in MF-859/861)
- Einen `-P`- oder `-p`-Schalter (Schalter ohne Wirkung bzw. ohne
  Prüfmöglichkeit)
- Jede Änderung an `multiread_pass_t` (ABI, Eigentümer-Entscheidung)
- Eine Änderung am Byte-Voting der `WEAK`-Klasse — MF-845 hat sie
  begründet abgelehnt, und die Begründung hat MF-884 gemessen bestätigt
