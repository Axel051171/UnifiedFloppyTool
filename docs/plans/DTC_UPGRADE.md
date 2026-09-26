# Umsetzungsplan „UFT DTC Upgrade v1" — Baum-Fassung

> **Verdaut, nicht eingespielt.** Das Paket wird **nicht** als Ganzes
> übernommen. Es enthält drei echte, im Baum nachgemessene Befunde, einen
> brauchbaren Baustein und drei Module ohne Aufrufer. Dieser Plan zerlegt
> es in einzeln abnehmbare Schritte, jeder mit eigenem MF, Rotbeweis
> zuerst und genannter Kennzahl (Regel MF-640).

**Quelle:** `UFT_DTC_Upgrade_v1.zip` (Zulieferung vom 2026-09-26, nicht
versioniert). Inhalt: ein Patch gegen `e60d3e7`, dieselben Dateien als
Overlay, zwei Beschreibungen (`README.md`,
`AUDIT_UND_INTEGRATION_DE.md`), `SHA256SUMS`, eine GPL-2-Lizenzdatei.
**Urheber nicht genannt.** Die Dateien tragen
`SPDX-License-Identifier: GPL-2.0-or-later`.

## Was gemessen ist (2026-09-26, gegen `3557ecf`)

| Messung | Ergebnis |
|---|---|
| `sha256sum -c SHA256SUMS` | 25 von 25 Zeilen stimmen |
| `git apply --check` gegen `3557ecf` (22 Commits nach der Basis `e60d3e7`) | sauber, 20 Dateien, +1374 / −61 |
| `tests/test_dtc_upgrade.c` mit `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` | übersetzt, **grün** |
| `test_kryoflux_dtc_befehl`, `test_kryoflux_provider_v2` — **vor** dem Patch | grün |
| dieselben zwei — **nach** dem Patch | grün |
| Vollbau mit Qt6 / `ctest` | **nicht gemessen** — die Umgebung hat kein Qt6 |
| Windows (MinGW), echtes DTC, echtes Gerät | **nicht gemessen** |

**Die vierte und fünfte Zeile zusammen sind der wichtigste Befund:** die
beiden Provider-Tests sind vorher und nachher grün, sie können also
keinen der behobenen Fehler sehen. Der Grund steht im Patch selbst:
die drei Mock-Adapter kopieren `stdout_text` nach `artifact_bytes`
(`test_kryoflux_provider_v2.cpp:119`, `test_kryoflux_dtc_befehl.cpp:103`,
`test_provider_temppfad.cpp:159`). Damit wird die Abkürzung
„stdout ist der Fluss", die der Patch abschaffen will, im Test unter
neuem Namen weitergeführt. Das ist die Klasse MF-1000 / Tor 64.

## Befunde zum Paket

### Bestätigt, und zwar im eigenen Baum

**B1 — `argv[0]` geht doppelt an den Prozess.** Die Provider bauen
`argv` nach der `execve`-Konvention mit dem Programmnamen an Stelle 0
(`kryoflux_provider_v2.cpp:139`: `args.push_back(m_dtc_binary)`), und
`run_subprocess()` setzt dasselbe Programm noch einmal über
`setProgram()` und reicht **alle** Elemente an `setArguments()`
(`qprocess_subprocess_runner.cpp:47-54`). Heraus kommt `dtc dtc -c2 …`.
**Das Paket behebt das nur für KryoFlux.** FluxEngine hat genau
denselben Fehler (`fluxengine_provider_v2.cpp:286` und `:333`, Läufer
`qprocess_subprocess_runner.cpp:119`): `fluxengine fluxengine read …`.
**Der dritte Läufer ist anders gebaut:** FC5025 legt seine `argv`
**ohne** Programmnamen an (`qprocess_subprocess_runner.cpp:238-253`).
Die Behebung darf deshalb nicht pauschal in `run_subprocess()` sitzen.

**B2 — stdout wird als Rohstrom gedeutet.** Das ist der offene Teil
von P3-342, den der Provider-Kopf selbst „test-mode shortcut" nennt.
DTC schreibt den Strom nach `<präfix>NN.S.raw`. Stdout ist nur das
Protokoll.

**B3 — die Umdrehungszahl erreicht DTC nicht.** In
`kryoflux_provider_v2.cpp:302` wird `revolutions` berechnet, aber nie
als `-r` übergeben. Der Produktivaufrufer existiert:
`FluxCaptureJob` setzt `rp.revolutions = m_revolutions` (Vorgabe 2,
`src/fluxcapturejob.cpp:160`).

### Einwände — so nicht übernehmen

**E1 — Der Läufer liest den Dateinamen aus `argv` zurück.**
`make_kryoflux_qprocess_runner()` zerlegt `-f`, `-s` und `-g` erneut,
um den Pfad der Ausgabedatei zu erraten. Das ist eine **zweite
Rechnung** neben `uft_dtc_artifact_path()` und verstößt gegen MF-1177
(„eine Größe, eine Rechnung"). Außerdem ist es genau die Gestalt, die
P3-342 als Grund benannt hat: „der Läufer kann den Pfad in einer
fertigen `argv` gar nicht erkennen". Richtig ist: der Provider nennt
den erwarteten Pfad ausdrücklich, so wie der FC5025-Läufer ein
Anfrage-Objekt bekommt (`qprocess_subprocess_runner.cpp:206`).

**E2 — Die Befehlszeile ändert sich über die Behebung hinaus.** Aus
dem gegen das Handbuch abgenommenen MF-1046-Befehl
`dtc -c2 -d0 -f<p> -s<c> -e<c> -g<h> -i0` wird
`dtc -p -d0 -c2 -r<n> -t5 -tc2 -tm1 -f<p> -s<c> -e<c> -g<h> -k1 -i0`.
Das heißt: bei **jedem** Spurlesen kommen fünf Schalter hinzu. Für
`-tc`, `-tm`, `-x`, `-ks`, `-v`, `-l`, die Bildtypen 1..25 und die
Variante `a` gibt es im Baum **keinen** Beleg. Die einzige Quelle ist
die Angabe des Pakets, es sei „beobachtbares Verhalten von DTC 3.50".
Und `dtc` steht in `docs/ORACLES.md:170` als **ungemessen**. Die Regel
von MF-1046 gilt deshalb weiter: ein Schalter kommt nur mit
Handbuch-Zitat in den Befehl, und ohne Auftrag gar nicht. Ein Schalter,
der eine gültige andere Option trifft, ist eine stille Veränderung.

**E3 — Drei Module haben keinen Aufrufer.** `uft_capture_policy`,
`uft_rev_path_solver` und beide JSON-Berichte werden im Produktivpfad
von nichts gerufen. `FluxCaptureJob` kennt keinen `uft_copy_plan_t`
(gemessen mit `git grep`). Das wäre Bestand, nicht Fähigkeit, also die
Klasse MF-627/MF-767.

**E4 — Der Segment-Solver fällt unter die EINFRIER-REGEL.** Er fusioniert
Umdrehungen zu einem Bitstrom, und das ist Decoder-Schicht. Es gibt
keine benannte Referenz. Sein einziger Test ist synthetisch und duldet
bis zu 31 falsche Bits (`assert(bad < 32)`). Dazu kommt, dass der Baum
schon zwei Verwandte hat: `src/algorithms/advanced/uft_multi_rev_fusion.c`
(mit Test) und `src/dtc_components/src/match.c` („shifted track
matching"). Ein dritter ohne Abgleich wäre MF-1015/MF-1026 noch einmal.
Und forensisch gilt: ein Mehrheitsentscheid darf die Originalumdrehungen
nie ersetzen, er darf nur neben ihnen stehen.

**E5 — Herkunft.** Die Zulieferung versichert, sie enthalte „keine
DTC-Decompiler-Ausgabe". Der Präzedenzfall `src/dtc_components/`
(MF-1099, `UEBERNAHME.md`) hat für eine sehr ähnlich formulierte
Zulieferung eine **Eigentümer-Entscheidung** und eine Zeile in
`docs/QUARANTINE.md` gebraucht. Hier ist der Urheber nicht einmal
genannt. Eine Attribution ist eine rechtliche Aussage (MF-636).

**E6 — Bau-Verdrahtung.**
* `src/core/CMakeLists.txt` zieht `../hardware/` und
  `../algorithms/advanced/` in `uft_core` hinein. Das kreuzt die
  Schichten.
* Mit `src/hardware/` entsteht ein neues Wurzelverzeichnis neben
  `src/hal/`, dem bestehenden Ort für Controller-Code.
* `test_dtc_upgrade.c` hat keinen eigenen Block in
  `tests/CMakeLists.txt`. Er hängt also an den allgemeinen
  `TEST_LIBS`.
* `packaging/dtc_upgrade/` verdoppelt Dokumentation.
* Die Temp-Artefakte werden nie gelöscht.

## Umsetzung

Jeder Schritt ist ein eigener Commit mit eigenem MF. Kein Schritt
übernimmt Paketdateien ungeprüft; übernommen wird **Verhalten**, das
vorher rot gemessen wurde.

### DTC-0 — Eigentümer-Entscheidungen *(vor allem anderen)*

1. **Herkunft (E5):** Wer hat das Paket verfasst, und unter welcher
   Erteilung? Wird das nicht beantwortet, dann wird nur **nachgebaut**:
   die Befunde B1–B3 sind im Baum gemessen und brauchen keine Zeile aus
   dem Paket. Wird es beantwortet, bekommt es eine Zeile in
   `docs/QUARANTINE.md` wie MF-1099.
2. **Solver (E4):** Wird er Fundus, oder bekommt er den Differenzlauf
   aus DTC-5?

### DTC-1 — ein `argv`-Vertrag für beide Läufer *(B1, ~40 Zeilen)*

* **Kennzahl:** Bench-Alter je Controller / Fähigkeitszusage. Heute
  kann kein echter DTC- oder FluxEngine-Aufruf gelingen.
* **Rotbeweis zuerst:** ein kleines Test-Programm `fake_tool` (C,
  unter `tests/`), das sein `argv` in eine Datei schreibt. Der echte
  QProcess-Läufer ruft es auf. Gegen HEAD steht dann `argv[1] ==
  <programmname>` in der Datei, für **beide** Läufer.
* **Behebung an EINER Stelle, aber nicht in `run_subprocess()`:** der
  FC5025-Läufer übergibt `argv` schon ohne Programmnamen, ein pauschales
  Abschneiden würde dort `-f` verschlucken. Richtig ist EIN Helfer
  „exec-`argv` → Werkzeug-Argumente“, den genau die beiden Läufer mit
  `execve`-Konvention rufen (KryoFlux, FluxEngine). Er sagt ab, wenn
  `argv[0]` nicht der erwartete Programmname ist, statt blind zu
  schneiden.
* **Abnahme:** nach der Behebung grün für beide, und FC5025 kommt
  unverändert an (dritte Zusage, gegen das Überbehandeln). Mutationsmatrix
  mit den Mutationen „Abschneiden weg“, „nur KryoFlux behoben“ und
  „in `run_subprocess()` abgeschnitten“.

### DTC-2 — der Artefakt-Kanal *(B2 + E1, ~120 Zeilen)*

* **Kennzahl:** Bench-Alter / Fähigkeitszusage. Damit ist der
  KryoFlux-Teil von P3-342 erledigt.
* `DtcRunResult` bekommt `artifact_bytes` und `artifact_path`. Dieser
  Teil kann aus dem Paket kommen.
* Der Läufer bekommt den **erwarteten Pfad vom Provider**, statt ihn
  aus `argv` zu raten. Der Pfad kommt aus EINER Funktion
  (`uft_dtc_artifact_path`, Ort `src/hal/`, nicht `src/hardware/`).
* Die Größenschranke kommt mit, vollständiges Lesen ist Pflicht. Die
  SHA-256 wird mit dem vorhandenen `src/core/uft_sha256.c` gebildet.
* **Aufräumen:** das Artefakt wird nach dem Lesen gelöscht. Nur wenn
  die Aufnahme es verlangt, bleibt es liegen (Evidence). Es darf nie
  still liegen bleiben.
* **Rotbeweis:** die Mock-Adapter trennen die beiden Kanäle
  (stdout = Protokolltext, Artefakt = Strom). Gegen HEAD dekodiert der
  Provider dann den Protokolltext und fällt. Dazu kommt ein Fall „nur
  stdout, kein Artefakt": erwartet ist ein Fehler oder `FluxMarginal`,
  kein Fluss. Und `fake_tool` bekommt einen DTC-Modus, der
  `<präfix>NN.S.raw` schreibt. Als Inhalt dient der Korpus-Strom
  `tests/corpus_free/hxcfe_kfx_t00.0.raw`. So ist der Weg
  Prozess → Datei → Läufer → Dekoder ohne Gerät abgenommen.

### DTC-3 — `-r` durchreichen, sonst nichts Neues im Befehl *(B3 + E2, ~60 Zeilen)*

* **Kennzahl:** Fähigkeitszusage (`FluxCaptureJob` fordert 2
  Umdrehungen an, DTC erfährt davon nichts).
* **Rotbeweis:** `test_kryoflux_dtc_befehl` verlangt `-r2`, wenn 2
  Umdrehungen angefordert sind, und zwar vor `-i0`. Gegen HEAD ist
  das rot.
* Die Vorgabe-Befehlszeile bleibt **Byte für Byte** die von MF-1046,
  nur mit `-r<n>` dazu. Eine neue Zusage prüft die **ganze** `argv`
  auf Gleichheit, damit ein zusätzlicher Schalter auffällt.
* Aus dem Paket kommt nur die typisierte Prüfung (`validate`) für die
  belegten Schalter. `-t`, `-p`, `-k`, `-a`/`-b` sind im Handbuch
  belegt (MF-1046). `-tc`, `-tm`, `-x`, `-ks`, `-v`, `-l` und die
  Bildtypen außer 0 kommen erst mit Zitat. Bis dahin stehen sie im
  Fundus.
* **Offen und benannt:** der zweite Befehlsbauer im C-HAL
  (`uft_kf_build_capture_command`, Shell-Zeichenkette mit `2>&1`) bleibt
  vorerst bestehen, wie es das Paket auch vorschlägt. Er wird ein
  eigener Punkt, denn zwei Bauer sind MF-1177.

### DTC-4 — CopyPlan → Aufnahme *(E3, nur mit Aufrufer)*

* **Kennzahl:** keine der vier direkt, daher **Fundus**, bis ein
  Aufrufer existiert.
* Umsetzen nur zusammen mit der Tür: `FluxCaptureJob` bekommt den
  aufgelösten Kopierplan, und die Umdrehungszahl kommt von dort statt
  aus dem festen `m_revolutions = 2`. Ohne diese Tür wird
  `uft_capture_policy` **nicht** eingespielt.
* Wenn es umgesetzt wird: die Abbildung Strategie → Umdrehungen ist
  eine Hausregel und wird als solche benannt. Sie wird herstellerneutral
  und gilt dann auch für Greaseweazle und FluxEngine, nicht nur für
  DTC.

### DTC-5 — Umdrehungs-Ausrichtung *(E4, Fundus bis Beleg)*

* **Was ihn öffnet:** ein Differenzlauf gegen `uft_multi_rev_fusion` an
  **echten** Mehrfachumdrehungen aus dem Korpus (SCP/KryoFlux mit ≥ 2
  Indexmarken). Es muss gemessen werden, wo der lokale Pfad eine Spur
  rettet, die der globale Versatz verliert. Zeigt sich kein solcher
  Fall, bleibt er Fundus.
* Die Ausgabe steht immer **neben** den Umdrehungen und ersetzt sie
  nie. Die Uneinigkeits-Bitmap ist die eigentliche forensische
  Aussage.

### DTC-6 — Doku nachziehen *(mit jedem Schritt, nicht am Ende)*

* `docs/OPEN_ITEMS.md` P3-342: den KryoFlux-Teil erledigen (DTC-2).
* Neuer Punkt: die Eichung von `dtc` in `docs/ORACLES.md` (`dtc -h`,
  Version, SHA des Binärs).
* `docs/CAPABILITIES.md`: KryoFlux Read **bleibt 🟡**, bis DTC-7
  gelaufen ist.
* `src/hardware_providers/kryoflux_provider_v2.h`: der Kopf beschreibt,
  was der Code tut, nicht was er tun sollte.

### DTC-7 — Bench *(Eigentümer, Gerät)*

Die Messliste des Pakets ist gut und wird übernommen:

1. Spur 0/0 mit `-r1`, `-r2` und `-r5` aufnehmen.
2. Zählen, ob die Index-OOB-Blöcke zur Anforderung passen.
3. Dateinamen unter Windows und Linux prüfen.
4. SHA-256 des Artefakts mit dem vergleichen, was der Provider gelesen
   hat.
5. Byteweiser Vergleich gegen einen direkten DTC-Lauf.

Neu dazu kommt die Hilfeausgabe `dtc -h` als Beleg für jeden Schalter
aus E2. Erst danach geht KryoFlux Read auf ✅.

## Reihenfolge

```
DTC-0 ──► DTC-1 ──► DTC-2 ──► DTC-3 ──► DTC-7 (Bench)
                                  └──► DTC-6 (je Schritt)
DTC-4, DTC-5: Fundus, bis ihre Bedingung erfüllt ist
```

DTC-1 kommt zuerst, weil ohne ihn **kein** Prozessaufruf gelingt. Das
gilt auch für FluxEngine, das das Paket nicht erwähnt.

## Nicht geprüft

* Vollbau mit Qt6 und der ganze `ctest`-Lauf: in dieser Umgebung gibt
  es kein Qt6.
* Ob `test_dtc_upgrade` unter den allgemeinen `TEST_LIBS` linkt.
* Laufzeitverhalten unter Windows.
* Jede Aussage über ein echtes DTC oder ein echtes Gerät (MF-310).
