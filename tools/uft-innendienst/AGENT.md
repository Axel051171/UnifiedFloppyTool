# AGENT.md — Betriebsanweisung `uft-innendienst`

## Auftrag (ein Satz)

Sieben Rollen für die wiederkehrenden Muster der Baumarbeit — Türen
finden, Zensus schmieden, Oracles eichen, Fixtures beschaffen,
Widersprüche aufdecken, Entscheidungen bündeln. Jede findet ihre
Information selbst, belegt sie mit Fundstelle und übergibt an den
MF-Workflow. **Keine Rolle erzeugt Produktcode oder Fixes.**

## Wo dieser Agent steht

Die dritte Aufklärungs-Rolle neben `uft-scout` (fremder Code, in die
Breite) und `uft-variants` (ein Format, in die Tiefe). Der Unterschied:
**beide sehen nach draußen, der Innendienst sieht nach innen.** Er
bringt nichts Neues in den Baum — er misst, was schon drin ist, und
macht daraus einen Prüfauftrag.

Er schreibt **nie** nach `src/`, `include/` oder `tests/`. Seine
Ausgaben landen unter `tools/uft-innendienst/out/` (gitignored, weil
regenerierbar) und als Vorschlagsblock, den ein Mensch nach
`docs/OPEN_ITEMS.md` überträgt. Ein Vorschlag ist kein Eintrag.

## Gemeinsame Regeln (gelten für alle sieben)

1. **Die EINE Liste.** Jeder Befund landet in `docs/OPEN_ITEMS.md` —
   nie in Nebenlisten.
2. **Kein Befund ohne Fundstelle.** Datei:Zeile, Messlauf oder Abfrage.
   „Vermutlich" ist kein Befund.
3. **Selbsttest vor Nenner.** Jede Rolle mit einer Zahl führt ihre
   Fehlklassen als **gepflanzte** Fixtures unter `data/` und muss sie
   finden, **bevor** ihre Zahl gilt. Die Werkzeuge erzwingen das selbst:
   `tuersucher.py` und `widerspruch.py` laufen ihren Selbsttest vor
   jedem echten Lauf und brechen bei rot ab.

   > Warum so scharf: die Erstfassung von `widerspruch.py` meldete
   > `Selbsttest 3/3` im README und lieferte gemessen **0 von 3** — ihr
   > `git ls-files` lief in einem Verzeichnis, das kein Repository ist,
   > und bekam eine leere Dateiliste. Ein Zählwerk, dessen Beweis nicht
   > feuert, kann „nichts gefunden" nicht von „nichts gesucht"
   > unterscheiden. Die Null des echten Laufs war damit wertlos.

4. **Dateimengen kommen aus git, nie aus gepflegten Listen** (MF-636).
   Der gemeinsame Helfer ist `scripts/baum.py`; er benutzt dieselbe
   Regel wie `scripts/repo_scope.py` und **sagt es**, wenn er auf einen
   Verzeichnisgang zurückfallen muss.
5. **Ratenbremse.** Höchstens 5 neue Befunde je Rolle und Zyklus in die
   Liste; der Rest bleibt in `work/`, nach dem Maßstab der Rolle
   sortiert.
6. **Rotbeweis-vor-Reparatur bleibt beim MF-Workflow.** Die Rollen
   liefern Prüfauftrag + Rotbeweis-Skizze, nie den Fix. Ausnahme: ein
   Fehler im eigenen Rollen-Werkzeug — der sofort, mit Beleg.
7. **Regel 9 (MF-640).** Jeder Vorschlag nennt, welche der vier
   Release-Kennzahlen er bewegt. Was keine bewegt, ist **Fundus, nicht
   Auftrag**.
8. **Konfliktordnung:** Messung vor Plan · Lizenz vor Fähigkeit ·
   Ehrlichkeit vor Vollständigkeit.
9. **„Was die Null nicht heißt"** steht in jedem Bericht. Ein leerer
   Zensus belegt die enge Frage, nie die Nachbarfrage.

## Rolle 1 — TÜR-SUCHER · `scripts/tuersucher.py`

Misst je exportierter Funktion die Verwender außerhalb der eigenen
Datei und rechnet **transitiv**: ein Aufruf aus einer selbst verwaisten
Datei ist keine Tür.

    OK · NUR_EIGENES_VERZEICHNIS · NUR_TESTS · WAISE

**Abgrenzung — Pflicht:** `scripts/audit_orphan_modules.py` beantwortet
seit MF-476 die **Modul**-Frage (Datei-Ebene, nur `SOURCES` aus der
`.pro`). Dieses Werkzeug beantwortet die **Symbol**-Frage über den
ganzen Baum. Beide Zahlen nebeneinander ohne diesen Satz wären die
nächste Zahlendrift.

Betriebsregeln aus den Lehrfällen: Header-Deklaration ist keine
Verwendung · Erwähnung in Kommentar oder String ist kein Aufruf ·
vendorte Fremdbäume (`src/samdisk/`, `src/a8rawconv/`) sind ausgenommen,
und zwar über den **Import** von `scripts/audit_spdx_policy.py:
AUSGENOMMEN`, nicht über eine zweite Abschrift.

Abnahme: `--selbsttest` gegen `data/tuer_fixtures/` — ein gepflanzter
Baum mit je einem Vertreter jeder Fehlklasse. **9 Prüfungen, alle
müssen sitzen.**

## Rolle 2 — ZENSUS-SCHMIED · `playbook/zensus.md`

Trägt das Ritual: Einzelfund → Frage schärfen → Zensus → Rotbeweis
(Fix entfernen ⇒ genau 1 Befund) → Tor. Dazu die Checkliste der
gemessenen Zensus-Fehler.

Diese Rolle hat **kein Skript** und braucht keins: sie schmiedet das
Tor für einen konkreten Fund, und jedes solche Tor sieht anders aus.
(Die erste Fassung dieser Datei nannte hier ein `zensus_vorlage.py`,
das es nicht gab — eine Vereinbarung ohne Leser im eigenen Haus, also
genau die Klasse, die Rolle 5 zum Tor macht.)

## Rolle 3 — ORACLE-KALIBRIERER · `scripts/kalibrierer.py`

Hält das Oracle-Register gegen die Kalibriertabelle in
`docs/ORACLES.md` und meldet, wer noch keine Längensemantik hat.
`erzeugen` legt das Hausmaß an: **127 Byte**, krumm, damit eine
gepolsterte Antwort sofort auffällt (254 bei CBM-DOS, 488 bei
Amiga-OFS, 512 bei FAT).

Ein Oracle-Wert, der Erfindung belohnt, ist schlimmer als keiner.

## Rolle 4 — FIXTURE-BESCHAFFER · `scripts/beschaffung.py`

Vergleicht die Korpus-Abdeckung mit dem Vokabular, das der
Varianten-Zensus **erkennen kann** (`tools/uft-variants/config.json` →
`erkennungen`), und ordnet jeder Lücke einen Beschaffungsweg zu: selbst
erzeugen · Originalwerkzeug im Emulator (fremde Hand = das *Werkzeug*
ist fremd, nicht der Bediener) · freies Archiv mit Lizenzzitat.

Soll und Ist stammen aus **derselben** Quelle, damit kein
Namensvergleich nötig ist — die Erstfassung führte ein eigenes Soll und
meldete darum `HFE v1` als fehlend, während der Zensus dieselbe Fassung
längst als `rev0` zählte.

**Fixture-Lizenz wie Code-Lizenz.** Ungeklärte Herkunft ist ROT und
wird abgelehnt. Jeder Posten endet mit genau EINEM menschlichen
Restschritt.

## Rolle 5 — WIDERSPRUCHS-FINDER · `scripts/widerspruch.py`

Hält Doku-Aussagen gegen ihre messbaren Quellen. Drei Prüfungen der
Klasse „Vereinbarung ohne Leser / Behauptung ohne Quelle", die in
diesem Baum viermal aufgetreten ist:

1. „Generiert aus `X`" ⇒ X liegt im Baum **und** jemand ruft es auf.
2. Deklarierte Marken ⇒ es gibt einen Leser (MF-678).
3. Ein Pfad, zwei Vereinbarungen ⇒ Namensraum-Konflikt (MF-689).

Abnahme: `--selbsttest` gegen `data/widerspruch_fixtures/` — **3
gepflanzte Fälle, alle drei müssen fallen.**

## Rolle 6 — TORE-SEKRETÄR · `scripts/sekretaer.py`

Produziert nichts. Sammelt (a) OPEN_ITEMS-Zeilen, die auf den
Eigentümer zeigen, und (b) Gutachten ohne Spur in OPEN_ITEMS — Letzteres
über den **Import** von `scripts/scout_stand.py:ohne_spur()`, nicht über
einen eigenen Nachbau. Formt jede Entscheidung auf **Frage · Messung ·
Empfehlung · Folge**; was er nicht belegen kann, bleibt `AUSZUFUELLEN`.

Ziel: vier Tore fallen in einer Sitzung statt in vierzehn Nachrichten.

## Rolle 7 — KONVERGENZ-SCHIEDSRICHTER · `scripts/konvergenz.py`

Beantwortet **eine** Frage: *ist diese Schleife fertig, und wenn nein,
warum genau nicht?* Fünf Regeln, jede mit Abbruchbedingung. Kein Urteil
über den Inhalt eines Befunds — nur darüber, ob weiterzumachen sich noch
lohnt.

`pruefe_werkzeug()` weist `src/`, `include/` und `tests/` ab; Fixtures
sind SHA-geschützt, damit „konvergiert" nicht heißt „die Messlatte hat
sich bewegt". `diff_fn` ist einspeisbar, damit der Selbsttest nicht vom
Zustand des Arbeitsbaums abhängt.

Abnahme: `--selbsttest` — **8 von 8**.

**Wofür er gebaut wurde, gemessen in einer Sitzung:** `widerspruch.py`
brauchte fünf Schärfungen (18 von 28 Erstbefunden waren Fehler des
Prüfers), `tuersucher.py` ging von 1777 auf 289, und die
LIZ-1-Einordnung (MF-737) brauchte sechs — **drei davon gegen Fehler,
die eine vorherige Schärfung selbst erzeugt hatte**. Genau dort ist die
Frage „sind wir fertig?" schwer, und genau dort wird sie sonst nach
Gefühl beantwortet.

## Bewusst NICHT gebaut

* **Auto-Fixer** — ein Agent, der Zensus-Befunde selbst repariert,
  zerstört Rotbeweis-vor-Reparatur.
* **Prioritäts-Agent** — die Gewichtung ist ein Risiko-Urteil und
  bleibt beim Menschen. Rollen liefern Kennzahl-Bezüge, nie die
  Rangfolge.
* **CI-Tor** — die Rollen sind beratend. Ein beratendes Werkzeug als
  Tor zu verdrahten, erzieht dazu, rote Tore zu übergehen; das ist die
  Lehre aus MF-633.

## Erfolg heißt

Der MF-Workflow beginnt jeden Baustein mit fertigem Prüfauftrag, die
Tore-Sitzung mit fertigem Zettel, der Differenzlauf mit geeichtem
Oracle — und kein „gebaut, gefüllt, nie gelesen" überlebt einen Zyklus
unentdeckt.
