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
| 10 | …und das landet unverändert im Zielabbild | ⚠ **teilweise behoben** | **MF-845/MF-860** haben den Fall `AMBIGUOUS_GOOD` geschlossen. **Vier andere Klassen nicht** — s. Phase 1 |
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

Ein einziger Befund berührt **erreichbaren, falschen** Code:
**Phase 1**. Alles andere ist Doku-Berichtigung, Bestandsaufnahme oder
Fundus. Das ist kein schlechtes Ergebnis — es heißt, dass die
Vorgängerarbeit (MF-845/859/860/861) bereits gegriffen hat.

---

## Phase 1 — Vier von fünf Fusionsklassen erzeugen weiterhin Bytefolgen, die nie auf einer Diskette standen

**Kennzahl:** keine der vier direkt. Es ist ein **Korrektheits-/
Forensikbefund auf erreichbarem Produktionscode** — nach der
Konfliktordnung („Ehrlichkeit vor Vollständigkeit", und Forensik schlägt
alles) rangiert er vor Kennzahlarbeit. Erlaubt unter EINFRIER als
**Bugfix an Bestehendem**.

### Was gemessen ist

`multiread_execute()` (`src/recovery/uft_multiread_pipeline.c:590`) ist
**in Produktion erreichbar**:

```
src/formats/uft_format_convert_flux.c:181,188   SCP -> D64  (256-B-Sektoren)
src/formats/uft_format_convert_flux.c:530,539   SCP -> ADF  (512-B-Sektoren)
```

Der Ausgabepuffer entsteht in **jedem** Fall aus `vote_buffer()`
(`:641`), das `output[i] = vote_byte(...)` je Byteposition unabhängig
setzt (`:457`). Nur **eine** der fünf Klassen bekommt danach eine ganze
beobachtete Lesung zurückgeschrieben (`:651-681`, MF-845/860):

| Klasse | Ausgabe heute |
|---|---|
| `AMBIGUOUS_GOOD` | ✅ ganze beobachtete Lesung (`memcpy` vom populärsten Pass) |
| `STABLE_GOOD` | byteweise gevotet — bei Einigkeit folgenlos |
| `STABLE_BAD_CRC` | **byteweise gevotet** |
| `WEAK` | **byteweise gevotet** |
| `UNKNOWN` | **byteweise gevotet** |

Bei `WEAK`/`STABLE_BAD_CRC` besteht **keine** Lesung ihre CRC. Dann setzt
`vote_buffer()` `only_crc_ok = false` (`:115-120`), und `vote_byte()`
stimmt über **alle** Lesungen ab — genau die Lage, für die der Kommentar
bei `:610-630` das Problem schon beschreibt:

```
Pass A   01 FF 02 FF 03 FF 04 FF
Pass B   FF 01 FF 02 FF 03 FF 04
Voting   01 01 02 02 03 03 04 04   <- war auf keiner Diskette
```

Der Kommentar nennt das selbst *„für ein Werkzeug mit dem Grundsatz
‚Keine erfundenen Daten' die schwerste Klasse"* — und die Behebung
darunter greift nur für `AMBIGUOUS_GOOD`.

**Bei Gleichstand gewinnt der kleinere Bytewert** (`vote_byte:157-161`,
`counts[v] > max_count` — der erste Höchstwert bleibt stehen). Bei zwei
Lesungen ohne CRC ist jede Byteposition ein Gleichstand: das Ergebnis ist
dann systematisch die byteweise Minimum-Folge beider Lesungen.

### Das Referenzverhalten (benannt, nicht erfunden)

`src/a8rawconv/disk.cpp` — **im Baum vendoriert**, GPL-2.0-or-later,
Attribution bereits geführt (`docs/QUARANTINE.md:126`). Sein Zweig für
„keine Kopie besteht die CRC":

```cpp
for (uint32_t i = 0; i < max_match; ++i)
    if ((*it)->mData[i] != best_sector->mData[i]) { max_match = i; break; }
best_sector->mWeakOffset = max_match;
```

Also: **längster gemeinsamer Präfix einer echten Lesung**, Rest als „ab
hier unsicher" markiert — nie eine synthetisierte Folge. Zweite
unabhängige Quelle, ebenfalls schon im Baum genannt: FluxEngine
`readerwriter.cc::collectSectors()` markiert zwei abweichende `Sector::OK`
als `Sector::CONFLICT`.

`result->weak_offset` **existiert bereits** (`:632`, gesetzt von
`classify_passes()`). Das Feld ist da; es wird für die Ausgabe nur nicht
benutzt.

### Aufgabe

1. **Rotbeweis zuerst.** Ein Test, der `multiread_execute()` mit zwei
   Lesungen ohne gültige CRC füttert, die sich unterscheiden, und prüft:
   *der Ausgabepuffer ist byteidentisch mit **einer** der Eingaben.*
   Gegen den Vorzustand muss er rot sein. Als Datenmuster das Beispiel
   aus `:614-617` verwenden — es steht bereits im Baum und ist damit
   keine erfundene Konstruktion.
   *Muster:* `tests/test_multiread_selbsttests_leben.c` (bestehend).
2. **Erst danach** die Ausgabe für `WEAK`, `STABLE_BAD_CRC` und
   `UNKNOWN` auf denselben Weg wie `AMBIGUOUS_GOOD` bringen: ganze
   beobachtete Lesung über `multiread_popular_pass()` (existiert,
   `:667`), plus `weak_offset` als Präfixgrenze setzen.
3. Die Fälle, in denen der Bezugspass **kürzer** ist als der
   Ausgabepuffer, so behandeln wie MF-845 es für `AMBIGUOUS_GOOD` schon
   tut (`:679-681`: dann bleibt es beim Voting, und die Klasse meldet den
   Fall) — **keine** neue Sonderregel erfinden.
4. `docs/DESIGN_PRINCIPLES.md` NICHT ändern (geschützte Datei). Wenn ein
   Prinzip berührt scheint: STOPP und fragen.

### Abnahme

- [ ] Rotbeweis war rot gegen den Vorzustand, Ausgabe im Commit zitiert
- [ ] `ctest` vollständig grün (heute 345/345)
- [ ] `check_consistency.py` alle Tore 0
- [ ] Gegenprobe: mindestens eine Mutation je geänderter Klasse fällt
      genau die zugehörige Prüfung
- [ ] Der Kommentar bei `:610-630` ist nachgezogen — er sagt heute, das
      Problem sei „die schwerste Klasse", und beschreibt eine Behebung,
      die nur für eine von fünf Klassen gilt

### Anti-Muster

- ❌ **Nicht** `vote_byte()` löschen. Es liefert weiterhin Konfidenz und
  Weak-Maske, und die kommen aus den Lesungen (`:636-639` sagt das
  ausdrücklich). Nur seine **Byte-Ausgabe** ist in diesen Klassen die
  falsche Antwort.
- ❌ **Keine** neue Schwelle, kein neuer Prozentwert. Jede Zahl, die
  nicht aus `a8rawconv`/FluxEngine oder einer Messung stammt, ist eine
  Erfindung.
- ❌ **Nicht** Code aus `src/a8rawconv/` kopieren. Es ist
  GPL-2.0-or-later und als Referenz geführt, nicht als Vorlage. Das
  Verhalten nachbauen, die Quelle im Header nennen (MF-636: eine
  Attribution ist eine rechtliche Aussage).

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

| Phase | Art | Umfang | eigener Commit |
|---|---|---|---|
| 1 | Korrektheit/Forensik, erreichbar | mittel — Rotbeweis + 3 Klassen | ja |
| 2 | Bestandsaufnahme | klein — ein P3-Eintrag | mit 3 zusammen |
| 3 | Ehrlichkeit | klein | mit 2 zusammen |
| 4 | Doku-Berichtigung + Rückstand | mittel | ja |
| 5 | Fundus | klein — ein P3-Eintrag | mit 4 zusammen |

**Phase 1 zuerst und allein.** Sie ist die einzige, die erreichbaren,
falschen Code berührt; die übrigen sind Buchhaltung und dürfen warten.

## Was dieser Plan bewusst NICHT enthält

- Roland D-20 (gesperrt: Moratorium **und** kein Korpus-Abbild)
- Eines der 15 FluxEngine-Formate (dieselbe Sperre)
- Den D64-Interleave-Umbau (**erledigt** in MF-859/861)
- Einen `-P`- oder `-p`-Schalter (Schalter ohne Wirkung bzw. ohne
  Prüfmöglichkeit)
- Jede Änderung an `multiread_pass_t` (ABI, Eigentümer-Entscheidung)
