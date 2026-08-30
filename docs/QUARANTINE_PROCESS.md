# Verfahren: Quarantäne und Rückweg

> **Quarantäne ist eine Warteschlange, kein Friedhof.** Jede
> quarantänisierte Datei hat entweder einen benannten Rückweg mit
> Oracle — oder ein ausdrückliches „bleibt draußen, Grund X". Ein
> Eintrag ohne beides ist ein Verfahrensfehler.

Verbindlich bleiben: die Einfrier-Regel (`VERIFICATION_PLAN.md`), die
Provenienz-Regel ebendort, die Lizenzmatrix
(`tools/uft-scout/playbook/lizenzmatrix.md`) und die Konfliktordnung
(**Messung vor Plan · Lizenz vor Fähigkeit · Ehrlichkeit vor
Vollständigkeit**).

Die Liste selbst steht in [`QUARANTINE.md`](QUARANTINE.md).

---

## 1. Wann Quarantäne, wann nicht

Quarantäne trifft eine Datei, wenn ihre **Herkunft ungeklärt** ist und
die vermutete Quelle nicht in Zone GRÜN liegt. Auslöser sind typisch:

- Fließtext-Zuschreibung im Kopf („Based on …", „Port of …", „adapted
  from …") ohne geklärte Lizenzzone
- SPDX-Bezeichner, der nicht zur Projektlizenz passt
- Selbsterklärung als Port einer GELB/ORANGE/ROT-Quelle
- Quelle war zum mutmaßlichen Übernahmezeitpunkt **lizenzlos** —
  Datum messen, es entscheidet mehr als die heutige Lizenzdatei

**Nicht** Quarantäne, sondern normale Arbeit:
Waisencode ohne Herkunftsfrage → Verwaisten-Regel. Fähigkeit ohne Tür
→ Tür-Suche (MF-629-Muster). Falscher SPDX auf **eigenem** Code →
Kopf korrigieren, ein Commit.

**Vorrangregel:** Ist die Datei zugleich Waise *und* herkunftsverdächtig,
gewinnt die Verwaisten-Regel — löschen ist billiger als auditieren, und
die Herkunftsfrage erledigt sich mit. Nur was einen Aufrufer hat, wird
auditiert.

> **Anmerkung aus der Anwendung (MF-645).** Die Vorrangregel ist
> wirtschaftlich richtig, hat aber einen Nebeneffekt, den man kennen
> muss: sie **erzeugt keinen Befund**. Eine gelöschte Waise
> hinterlässt keine Aussage darüber, ob sie ein Port war — und damit
> auch keinen Hinweis auf **Geschwister** derselben Quelle, die einen
> Aufrufer haben.
>
> Belegt an `uft_track_align.c`: 0 Aufrufer, also nach der
> Vorrangregel ein reiner Löschfall. Auditiert wurde trotzdem, und
> erst dadurch war der Port belegt (`carryshift`, sieben
> Kommentar-Echos) — was den Blick auf die drei anderen
> nibtools-Zuschreibungen schärfte, von denen zwei sich als
> eigenständig erwiesen.
>
> **Praxis:** löschen wie vorgesehen, aber wenn die Datei eine
> Zuschreibung trägt, **die Quelle notieren**. Ein Zweizeiler in der
> Liste kostet nichts und beantwortet später die Frage „gibt es mehr
> davon?".

## 2. Quarantäne durchführen (vier Schritte, ein Commit)

1. **Aus dem Bau nehmen** — Eintrag in `UnifiedFloppyTool.pro` und
   CMake entfernen. Nicht auskommentieren: auskommentierte Bauzeilen
   sind die nächste stille Altlast.
2. **Aufrufer versorgen** — entweder mit stilllegen oder auf einen
   ehrlichen Ersatzpfad biegen. Nie einen Aufruf ins Leere lassen.
3. **Fähigkeit zurückstufen** — Registry, `VERIFICATION_TIERS.md`,
   `README.md`: die Fähigkeit heißt ab jetzt, was sie ist
   (Muster: „IPF: erkannt, nicht gelesen"). Die Formatliste darf nichts
   versprechen, was der Öffnungspfad nicht hält.
4. **Eintrag in [`QUARANTINE.md`](QUARANTINE.md)** nach dem Schema in §3.

Die Datei selbst bleibt in der Git-Historie. Sie wird **nicht**
mitgeliefert und **nicht** übersetzt.

> **Aus der Anwendung (MF-637):** Schritt 1 heißt **alle** Bau-Systeme.
> Bei `uft_track_align.c` waren es drei — `.pro`, `tests/CMakeLists.txt`
> und ein eigenes `src/protection/c64/CMakeLists.txt`, das eine separate
> statische Bibliothek daraus baute, die niemand verlinkte. Das dritte
> wurde übersehen und hat die CI rot gemacht.
>
> Zwei Abfragen vor dem Commit, nicht eine:
>
> ```
> grep -rn "<basename>" --include=CMakeLists.txt --include=*.pro .
> grep -rn "<basename>" scripts/*.json scripts/*.py
> ```
>
> Die zweite fängt Baseline-Einträge (`define_parity_baseline.json`,
> Ausnahmemuster in `verify_build_sources.py`), die sonst als
> verwaiste Ausnahmen zurückbleiben.

## 3. Die Liste (`QUARANTINE.md`)

Eine Zeile je Datei, alle Felder pflichtig:

| Feld | Inhalt |
|---|---|
| Datei | Pfad zum Zeitpunkt der Quarantäne |
| Verdacht | Quelle + Lizenzzone + **Beleg** (Kopfzeile, Zeilennummer, Messung) |
| Datum der Quelle | Wann trug die Quelle welche Lizenz? Entscheidet den schlimmsten Fall |
| Betroffene Fähigkeit | Was das Werkzeug dadurch nicht mehr kann — oder „keine (Waise)" |
| Audit-Stand | offen / läuft / eigenständig / portiert |
| Vorgesehener Weg | 1 Rehabilitierung · 2 Neubau · 3 Fremdkomponente · X bleibt draußen |
| Oracle | Werkzeug, das den Rückweg belegt — oder „fehlt, Beschaffung: …" |
| MF | Commit der Quarantäne, später der Auflösung |

Die Zahl offener Zeilen ist die Datenquelle für die Kennzahl
**„Dateien mit ungeklärter Herkunft"**.

## 4. Das Herkunftsaudit

Nur für Dateien mit Aufrufer (§1 Vorrangregel). Verglichen wird gegen
die vermutete Quelle:

**Beweiskräftig sind Idiome, nicht Fakten.** Identische Tabellen,
Konstanten, Feldreihenfolgen oder Formatgrößen beweisen nichts — die
hätte jede unabhängige Implementierung auch, weil sie die
Spezifikation sind. Beweiskräftig sind: Funktionszerlegung und
Aufrufreihenfolge, Fehlerbehandlungs-Idiome, charakteristische
Schwellwerte ohne Spec-Grundlage, Kommentar-Echos, übernommene
Variablennamen, mitgeschleppte Eigenheiten der Quelle.

> **Vorgeführt (MF-635).** `shift_buffer_left` stand in beiden Bäumen
> zeichengleich da — inklusive des **erfundenen** Bezeichners
> `carryshift`, des Sentinels `tempbuf[length] = 0x00` und des
> Ausdrucks `(tempbuf[i] << n) | (carry >> carryshift)`. Dazu sieben
> wörtliche Kommentar-Echos („back up a little"). Das ist der Beweis.
>
> Die GCR-**Tabellen** derselben Dateien waren dagegen wertlos als
> Indiz: sie sind Commodore-Spezifikation.
>
> Umgekehrt entlastet dieselbe Methode: `uft_gcr_ops.c` trägt
> nibtools' **Vokabular** (Funktionsnamen mit `gcr_`-Präfix), aber drei
> geprüfte Rümpfe sind je anders gebaut — anderer Algorithmus, andere
> Parameter, andere Speicherstrategie. Verdikt eigenständig.
>
> **Ein einzelner nachgeprüfter Rumpf entscheidet mehr als zehn
> übereinstimmende Konstanten.**

Drei Ausgänge:

- **eigenständig** → Weg 1
- **portiert** → Weg 2 (oder 3, wenn Spec nicht frei)
- **unklar** → wie *portiert* behandeln. Konservativ ist billig; die
  Umkehrung ist es nicht.

Ergebnis mit Beleg in die Liste. Kein Audit endet mit „vermutlich".

## 5. Die drei Rückwege

### Weg 1 — Rehabilitierung *(Audit: eigenständig)*
Kopf korrigieren: aus der Ableitungs-Behauptung wird eine
Referenz-Angabe („Verhalten nach <Quelle>-Dokumentation, eigenständige
Implementierung, Oracle: <Werkzeug>"). Kopfzeilen sind juristische
Aussagen, keine Höflichkeiten. Dann Wiederaufnahme in den Bau **mit
Rotbeweis**, der die Fähigkeit belegt — nicht bloß, dass es übersetzt.
Fähigkeit in Registry/Doku zurücknehmen auf den wahren Stand.

### Weg 2 — Clean-Room-Neubau gegen ein Oracle *(Audit: portiert, Spec frei)*
1. **Verhaltens-Spec** aus Doku und Blackbox-Läufen der Quelle,
   `docs/specs/<quelle>/`, Vorlage `tools/uft-scout/templates/spec.md`.
   Nicht aus dem fremden Quelltext abschreiben — sonst ist der Neubau
   ein Port mit Zwischenschritt.
2. **Oracle registrieren** (`ORACLES.md`), gebaut und gelaufen — nie
   ein Oracle auf Zusicherung. Zirkularität ausschließen: Erzeuger des
   Korpus und Oracle dürfen nicht dieselbe Hand sein.
3. **Rotbeweise zuerst**, gegen die Spec, nicht gegen die
   Implementierung.
4. Neubau, dann **Differenzlauf** nach dem Standard in `ORACLES.md`
   (Struktur → Inhalt → byteweise Hashes).
5. Liste schließen mit MF-Verweis; Fähigkeit in Registry/Doku heben.

### Weg 3 — legitime Fremdkomponente *(Spec nicht frei)*
Wenn das Format bewusst undokumentiert ist, ist Weg 2 versperrt. Dann
die offizielle Bibliothek über eine **Prozessgrenze** rufen, nie
einlinken (Muster: PFS3lib als Helper bei DiskFlashback; capsimg für
IPF). Bedingungen:

- Lizenz der Fremdkomponente prüfen — **Eigentümer-Entscheidung**
- Helper trägt seine Lizenz selbst, der Baum bleibt unberührt
- Datenfluss so schneiden, dass die Quelle (Abbild/Fluss/Hardware) bei
  UFT bleibt und der Helper nur interpretiert
- Fällt der Helper aus, degradiert die Fähigkeit **ehrlich**
  („erkannt, Inhalt nicht lesbar — Helper installieren"), nie still

### Weg X — bleibt draußen
Zulässiger Ausgang, aber nur mit **ausdrücklichem Grund** in der Liste
(Spec nicht frei **und** keine Fremdkomponente; oder Fähigkeit nicht
gewollt; oder Aufwand ohne Kennzahl-Bezug). **Bloßes Liegenlassen ist
kein Weg X** — der Satz „was ein Jahr ohne Weg dasteht, wird Weg X"
stand hier bis MF-699 und war eine Verjährung durch Untätigkeit. Ein
Eintrag ohne Entscheidung bleibt offen und wird älter, nicht
stillschweigend erledigt.

### Die Registerpflicht (MF-699)

**Keine gelöschte oder quarantänisierte Fähigkeit verschwindet ohne
Eintrag.** Jede bekommt eine Zeile mit vier Feldern:

| Feld | heißt |
|---|---|
| **Nachbau-Route** | Weg 1/2/3/X — wie käme die Fähigkeit legal zurück? |
| **Oracle-Kandidat** | woran der Nachbau gemessen würde (oder „offen") |
| **Kennzahl-Bezug** | welche der vier Zahlen der Nachbau bewegt (Regel 9) |
| **Aufwand** | grob, damit die Zeile planbar ist |

**Nachbaupflicht heißt Bewertungspflicht, nicht Bauzwang.** Der
Unterschied ist praktisch: löste jede lizenzlose Datei automatisch
einen Nachbau-Auftrag aus, diktierte die Fundreihenfolge fremder
Repositorien die eigene Arbeitsreihenfolge — der Amiga-Kopierschutz
käme vor der ADF-Tür. Die Reihenfolge entscheidet **Regel 9** wie
überall: bewegt der Nachbau eine Kennzahl, kommt er in die
Warteschlange; bewegt er keine, wartet er im Register — sichtbar, mit
Route, jederzeit abrufbar.

Trägt der Eintrag **keine** Fähigkeit, steht das auch da: „keine
Fähigkeit, kein Nachbau nötig". Eine Pflichtzeile für nichts ist
Rauschen, und Rauschen macht das Register unlesbar.

### Die Reihenfolge: erst der Ersatz, dann die Löschung (MF-699)

**Eine Datei mit Lizenzmangel wird nicht gelöscht, bevor ein Nachbau
gebaut ist und grün läuft.** Eigentümer-Vorgabe, und sie hat einen
messbaren Grund: eine gelöschte Vorlage ist als **Messpunkt** weg. Der
Nachbau braucht sie nicht als Quelltext — die Brandmauer verbietet das
ohnehin —, wohl aber als Verhaltensreferenz für Blackbox-Läufe und als
Beleg, dass die alte Fähigkeit überhaupt eine war.

Der Zwischenzustand dafür ist **Weg 3 in seiner engen Fassung**: aus
dem Verteilpaket nehmen, **nicht** aus dem Baum. Der Eintrag in
`scripts/verify_build_sources.py:NOT_BUILT_BY_DESIGN` verlangt dann ein
benanntes **Ende** — Lizenz gemessen und vereinbar, oder Nachbau grün.
Ein Eintrag ohne Ende wäre Liegenlassen mit Kommentar.

> **Vorgeführt (MF-699).** `src/formats/amiga/uft_amiga_protection.c`
> („C99 port of XCopy Pro", keine gemessene Lizenz) ist seit MF-699 aus
> dem Verteilpaket, aber im Baum. Gemessen: 17 Exporte, **kein**
> Produktionsaufrufer; die zwei Einbinder rufen nichts, nur `#include`;
> ein Test ruft eine Funktion und baut die Datei über die
> Test-CMakeLists weiter. Die ausgelieferte Fassung trägt damit keine
> ungeklärte Herkunft mehr, und der Rückweg bleibt messbar.

## 6. Was Quarantäne **nicht** rechtfertigt

- **Kein Wiedereinbau „weil es sonst nichts kann".** Der Verlust einer
  Fähigkeit ist der Preis eines Fehlers, der bereits passiert ist —
  kein Argument gegen die Quarantäne. Lizenz vor Fähigkeit.
- **Keine Relizenzierung unter Kontaminationsdruck.** Das Projekt
  wechselt seine Lizenz nicht, um einen Port zu heilen. Und im Fall
  einer lizenzlosen Quelle heilt kein Wechsel etwas.
- **Kein `git revert` als Rückweg.** Wiederkehr nur über Weg 1–3, mit
  Anker und Rotbeweis.
- **Kein Neubau ohne Oracle.** Sonst entsteht dieselbe Lage wie bei den
  fünf fabrizierten Parsern, nur mit sauberer Lizenz.

## 7. Vorbeugung (wichtiger als jede Rückführung)

- **Attributions-Zensus als CI-Tor:** Grep auf „Based on / adapted from
  / port of / nach dem Vorbild" in Quellköpfen, Ausgabe neben der
  SPDX-Liste. Neuer Eintrag ohne geklärte Lizenzzone ⇒ rot. Der
  SPDX-Zensus allein sieht Fließtext nicht — genau daran hing
  SCOUT-23.

  > **Stand (MF-636/644):** die Stufe **läuft** in
  > `scripts/audit_spdx_policy.py` und meldet 88 Attributionen als
  > **Liste**. Sie ist noch **kein Tor**, und das ist Absicht: 48
  > Einträge tragen keine genannte Lizenz. Ein Tor würde sie als
  > Grundlinie einfrieren, statt sie zu klären. **Reihenfolge: erst
  > `LIZ-1`/`LIZ-2` abarbeiten, dann scharfstellen.**

- **Zensus-Reihenfolge:** `src/` zuerst. Ein Zensus, der in einem
  Skriptordner anfängt, misst Skriptordner; die Stichprobengrenze
  gehört ins Ergebnis.
- **Dateimengen aus `git ls-files`**, nie aus gepflegten
  Ausschlusslisten — vierfach belegte Fehlerklasse (zuletzt MF-633).
- **Übernahmen klären ihre Herkunft am Tag des Commits**, nicht Monate
  später beim Zufallsfund. Zone, Datum der Quelllizenz und Oracle
  gehören in denselben Commit wie der Code.
