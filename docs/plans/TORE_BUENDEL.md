# Tore-Bündel — sieben Entscheidungen für eine Sitzung
Stand: nach MF-642. Ziel: nach dieser Sitzung ist **kein Baustein mehr
eigentümer-blockiert**, die Agenten haben für Wochen freie Bahn.
Format je Posten: Frage · was gemessen ist · Empfehlung · Folge der
Entscheidung.

---

## 1 · SCOUT-4 — fdc_bitstream: Oracle verdrahten oder aus dem Bau nehmen

> ### ERLEDIGT (MF-644) — dieser Posten ist keine Entscheidung mehr
>
> Der Eigentümer hat die Empfehlung (a) **zurückgezogen**, nachdem der
> veraltete Stand sichtbar wurde, und den besseren Weg benannt:
>
> **fdc_bitstream wird als *externes* Oracle registriert, nicht
> zurückgeholt.** Der Wunsch — ein zweiter unabhängiger MFM-Decoder als
> Schiedsrichter — ist damit erfüllt, ohne 2795 Zeilen in den Baum zu
> holen, die die Verwaisten-Regel bei jedem Durchgang erneut anfassen
> müsste. Kein Code im Baum, keine Lizenzfrage, keine Baulast.
>
> Der Anker gehört nach [`ORACLES.md`](../ORACLES.md), nicht in einen
> Plan: ein Oracle ist kein Baustein, den man verdrahtet, sondern ein
> Werkzeug, das urteilt.
>
> **Aus einer Eigentümer-Entscheidung ist damit eine Agenten-Aufgabe
> geworden.** Sieben Posten sind noch sechs.

> ### Statuskorrektur (MF-643): der Code ist bereits draußen
>
> Dieser Posten beschreibt den Stand **vor** MF-626. Dort wurde
> `src/flux/fdc_bitstream/` samt seiner vierzehn Header **gelöscht** —
> 6483 Zeilen — nach der Verwaisten-Regel, weil kein Plan-Anker
> vorlag. Die Entscheidung „Oracle verdrahten" bedeutet deshalb
> **`git revert e...` auf MF-626**, nicht „den vorhandenen Bestand
> anschließen".
>
> Das ist billig (MIT-lizenziert, Git hat alles) und war beim Löschen
> ausdrücklich als Weg benannt. Was sich ändert: es braucht **zuerst**
> einen Anker in `docs/plans/`, sonst löscht die Regel es beim
> nächsten Durchgang wieder. Der natürliche Ort ist `FLUXENGINE.md`
> als FE-7 oder ein eigener Abschnitt — ein zweiter MFM-Decoder als
> Schiedsrichter gehört inhaltlich dorthin.

**Gemessen (Stand MF-626, vor der Löschung):** 2795 Zeilen wurden
gebaut, 0 Einbinder außerhalb des Verzeichnisses, einziger
Test-Treffer war ein Kommentar. Vendoring, keine Fähigkeit.
Differenzlauf-Plan (Korpus, Metrik, Rotbeweis: 1-Bit-Shift muss BEIDE
Decoder röten) liegt im Gutachten.
**Empfehlung: (a) als Differenzlauf-Oracle verdrahten.** Ein zweiter,
unabhängiger MFM-Decoder als Schiedsrichter ist genau, was die
Rettungskette und die Tier-Hebung später brauchen — und er liegt schon
im Baum. (b) Rauswerfen wäre sauber, aber verschenkt einen Oracle.
**Folge (a):** neuer Baustein in `ORACLES.md`, Kennzahl-Bezug „stützt
Tier-Hebung". **Folge (b):** `.pro:400-414` entfernen, ein Commit.

## 2 · SCOUT-5 — D77-Fixture: Lizenz des Disketten-Inhalts
**Gemessen:** Repo ist MIT, aber der Diskinhalt (2019-Demo, FM-7) hat
eigene Urheber. `d77` steht T3 ohne alles; Fixture wäre die erste
Referenz.
**Empfehlung:** Fixture **nicht** übernehmen, solange die Demo-Lizenz
ungeklärt ist — ein nicht verteilbares Fixture vergiftet den Korpus
(ROT-Zone für Daten). Stattdessen bei nächster Gelegenheit eine
FM-7-Diskette mit klarer Herkunft selbst dumpen, oder D77 bleibt T3
mit ehrlichem „kein Korpus".
**Folge:** SCOUT-5 schließt als „bewusst nicht übernommen, Grund
Lizenz" — kein offener Rest.

## 3 · SCOUT-18 / SPDX-Politik — GPL-3-Datei in GPL-2-Projekt
**Gemessen:** `uft_retro_image_detect.c` trägt `GPL-3.0-or-later`,
eigener Code, keine fremde Herkunft; Projekt-LICENSE ist reines GPL-2.
**Empfehlung:** SPDX-Politik beschließen — *„UFT-eigener Code trägt
`GPL-2.0-or-later`"* (ein Satz in CONTRIBUTING.md), Datei-Kopf
entsprechend korrigieren. Hält GPL-2-Kompatibilität und die Tür zu
GPL-3-Quellen offen.
**Folge:** ein Commit; der Attributions-Zensus wird CI-Tor, damit die
nächste Abweichung Befund statt Zufall ist.

## 4 · capsimg — legaler IPF-Rückweg
**Gemessen:** vier `*_air.c`-Dateien in Quarantäne (GPL-3-Selbst-
erklärung bzw. XCopy-Herkunft); IPF-Lesen war nie legal eingebaut.
IPF-Format ist bewusst undokumentiert → Clean-Room aus Spec unmöglich.
**Empfehlung:** capsimg (offizielle CAPS/SPS-Bibliothek) als
**Helper-Prozess** nach PFS3-Muster prüfen — deine Aufgabe ist die
Lizenzklärung (capsimg: kostenlos nichtkommerziell, eigene Bedingungen).
Bis dahin bleibt „IPF: erkannt, nicht gelesen".
**Folge:** entweder neuer Baustein „IPF über capsimg-Helper" oder
endgültig „IPF bleibt Probe-only" — beides ehrlich.

## 5 · Beschaffungskorb — eine Bestellung, ein Dump-Nachmittag
- **cpmtools** (`apt install cpmtools`) → 131 diskdefs + `cpmls`-Oracle
- **mame-tools** (`apt install mame-tools`) → `floptool` inkl.
  `flophashes` — schon als Oracle geführt, nur Runner-Registrierung
- **SAMdisk `tc.cpp`** aus simonowen/samdisk (MIT) → Transcopy-Lücke
- **sector-cpc** bauen → AMSDOS-Oracle
- **.NET-7-SDK** → dskx bauen (SCOUT-F1, erst laufen lassen, dann
  registrieren)
- **DiscImageManager/DIMConsole** bauen → erstes Acorn-Oracle
  (`flophashes`-Muster, floptool hat kein Acorn) — für die schwächste
  Ecke
- **echte DMS** von Aminet mit freier `.readme`-Lizenz → SCOUT-11
**Folge:** sechs Oracles/Fixtures liegen, Phase-1-Differenzläufe und
mehrere S-Bausteine werden entsperrt. Beschaffungslisten dann leer.

## 6 · Klick-Session — ein Protokoll, vier Posten
`docs/CLICK_SESSION_v4.1.6.md` + neu: MF-496 Feineinsteller, MF-501
Schadenslage, das Fluss-Widget (MF-631/632, hängt unterm OTDR-Panel),
demnächst der D64-Browser sobald Phase 1 Nr. 1 steht.
**Empfehlung:** eine Sitzung, sobald der D64-Browser läuft — dann sind
alle vier auf einmal abnehmbar. **Folge:** GUI-Bausteine verlassen den
„blockiert beim Eigentümer"-Zustand.

## 7 · Hardware-Warenkorb — entsperrt Phase 2
- **USB-Floppy-Laufwerk** (~15–30 €) → UFI-Klasse, billigstes Gerät,
  meiste offene *Code*-Arbeit (Windows-Backend braucht kein Gerät)
- **ZoomFloppy** (~40 $) → XUM1541, stärkste Synergie mit D64-Phase-1
  (liest echte 1541, `c1541` als Oracle)
- **Teensy 3.2** (Schublade?) → ADF-Copy-Bench selbst bauen, liefert
  am selben Nachmittag den ADF-Referenzkorpus aus deinen 300 Disketten
- optional **SCP-Gerät** (~100 $) → SCP-Direct, Code ist der fertigste
**Folge:** je Gerät liegt ein Bench-Protokoll bereit; jede Bench-Session
produziert Referenz-Dumps für die Formathebung. Zwei Kennzahlen
(ungeprüfte Formate, Bench-Alter) bewegen sich mit derselben Arbeit.

---

## Nach der Sitzung offen (kein Eigentümer-Tor mehr)
Nur noch material-/zeitabhängig: Community-Bench der acht geräte-losen
Controller (MF-310, delegiert), nfd-r0-Korpus (Phase 0), die
laufenden Scouts (atrcopy, a8rawconv-Fork). Alles andere ist
Agenten-Arbeit mit Anker und Oracle.
