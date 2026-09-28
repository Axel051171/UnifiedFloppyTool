# Sitzungsbilanz — Pflichtabschnitt am Ende jeder Sitzung

Eigentümerentscheidung 2026-09-28 (MF-1505).

**Ohne Bilanz ist die Sitzung offen.** Nicht als Formalie: die Rückschau
(`.claude/STEHENDE_AUFTRAEGE.md` §Rückschau) findet nur, was in Bilanzen
steht — eine Sitzung ohne Bilanz ist für sie unsichtbar, und damit
wiederholt sich ihr Fehler ungesehen.

Der Anlass ist gemessen. Am 2026-09-28 standen fünf Regeln als Satz im
Baum, und **dieselbe Sitzung hat eine davon im selben Atemzug verletzt**
— die 24. Kopie der CBM-Zonenlängen, eingebaut in einen Test, dessen
Kopfkommentar „eine Größe, eine Rechnung" zitierte. Gefangen hat es das
**Tor**, nicht der Agent. Daraus folgt die Kennzahl **K0**: Anteil der
vom Tor gefangenen Fehler. Steigt sie, halten die Regeln; bleibt sie
niedrig, ist jede weitere Regel als Prosa verschwendet.

## Vorlage

```
## Sitzungsbilanz <Datum> <Rolle>

Kennzahlen bewegt:   K3 23 -> 23 (jetzt Manifest) | ...
Kennzahlen unbewegt: begruendet oder Fundus

Fehler dieser Sitzung — je Zeile: was | gefangen von | Folge
  1. <Fehler>   | Tor <Nr> / Mensch / niemand | Rotbeweis <Test> / Tor <Nr> / OFFEN
  2. ...

K0 dieser Sitzung: <Tor-Faenge> von <Fehler gesamt>

Fundus (nicht gebaut, weil keine Kennzahl bewegt): ...
```

**OFFEN heißt: die Sitzung endet nicht.** Jeder vom Menschen gefangene
Fehler bekommt vor dem Ende ein Tor oder einen Rotbeweis — oder eine
Zeile im Fundus mit der Begründung, warum kein Tor möglich ist (meist:
die Messung fehlt noch).

**Die Bilanz wird beim Auftreten geführt, nicht am Ende rekonstruiert.**
Gemessen am 2026-09-28, dem Tag ihrer Einführung: zwischen dem ersten
Schreiben und dem Sitzungsende kamen drei Fehler dazu, und einer davon
wäre ohne eine Nachfrage von außen unbemerkt geblieben — die erste
Fassung nannte 7, die vollständige 10. Wer sie am Schluss
zusammensucht, misst sein Gedächtnis, nicht die Sitzung.

**Der Bauer bewertet nicht.** Ob eine Kennzahl sich wirklich bewegt hat,
prüft die Widerspruchs-Rolle in eigener Sitzung mit der einen Frage:
*welche Kennzahl behauptet dieser Commit bewegt zu haben, und stimmt der
Messbefehl?* Kennzahlen laden zum Spielen ein — „gültige Sektoren" lassen
sich auch durch eine weichere CRC-Prüfung vermehren. Deshalb hat jede
Kennzahl einen Messbefehl **im Baum**, keinen im Kopf.

## Die Regelliste: höchstens 30 Sätze

Jede Regel trägt den Vorfall, der sie erzeugt hat, in einer Zeile daneben
(„24. Kopie, 28.9."). **Satz 31 verdrängt den ältesten Satz *ohne* Tor —
oder wird selbst ein Tor.** Eine Regelliste, die nur wächst, wird nicht
gelesen; eine, die einen Preis hat, zwingt zur Entscheidung zwischen
Werkzeug und Verzicht.

---

## Sitzungsbilanz 2026-09-28 — Oracle-Eichung und Tore

```
Kennzahlen bewegt:
  K0  neu eingefuehrt                    -> 1 von 7
  K3  Zonen: Zahl 23 -> Manifest 23      (MF-1504; eine Zahl konnte die
      Regel nicht halten, vorgefuehrt an einem Wegwerf-Baum)
  K4  hxcstream berichtigt (CHKH statt Plugin-ID), C64FRZ als P3-661
      registriert                        (MF-1494/1498)
  K6  neu gezaehlt                       -> 2

Kennzahlen unbewegt:
  K1/K2  Tierstand unveraendert (T1=11, T1b=68, T2=7, T3=1). Der
         Differenzlauf MF-1503 hat `g64` nicht gehoben, weil es bereits
         T1 war — er hat das ORACLE geeicht, nicht das Format.
  K5     199 Waisen, unangetastet.
  K7-K9  nicht beruehrt.

Fehler dieser Sitzung — was | gefangen von | Folge
  1. `git commit | tail` meldete 0, obwohl der Haken abgewiesen hatte
     | Mensch (git nachgeprueft) | scripts/commit_verified.py, Selbsttest 5/5
  2. 24. Kopie der CBM-Zonenlaengen im eigenen neuen Test
     | TOR audit_cbm_zonen | Tafel entfernt, Aussage ueber nibscans
       Ausgabe gefuehrt statt ueber seinen Quelltext
  3. `python -c` mit Backticks in Doppelquotes -> bash fuehrte fremde
     Befehle aus | Mensch (Datei nachgemessen) | Tor 71 erweitert, 39/39
  4. Eigener Parser verwarf 42 von 71 Spurzeilen STILL, weil er
     `(density:0!=3?)` nicht kannte | Mensch (grep -c Gegenprobe)
     | Abbruch-Zusage im Parser — aber nur im Scratchpad: FUNDUS, siehe unten
  5. Gleichungspruefung meldete 44 Abweichungen, die keine waren
     (`[E2Sn]` fragt nach Sollgeometrie, `sector_count` zaehlt Header)
     | Mensch (Quelltext beider Seiten gelesen) | Richtungs-Zusage im
       Test, Mutationsmatrix 5/5
  6. Fast nibscans 244022 „bad GCR bytes" gegen UFTs bad_gcr_count
     gehalten (Byte gegen <=1 je Sync-Block) | Mensch, VOR der Messung
     | P3-665 + t_bad_gcr_zaehler_ist_keine_bytezahl
  7. T2=14/T3=2 gemessen und CLAUDE.md fast als „gedriftet" korrigiert —
     die Datei fuehrt jedes Format ZWEIMAL, die gepflegte Zahl stimmte
     | Mensch (MF-1177 Satz 1 befolgt) | keine Aenderung; der Beinahe-
       Fehler steht hier, weil er sonst unsichtbar bliebe
  8. Warteschleife pruefte „Datei nicht leer" statt das ENDE des Laufs —
     die Datei fuellt sich waehrenddessen, also meldete der Warter
     Vollzug, als das Urteil noch fehlte. Dieselbe Form wie Fehler 1: ein
     Zwischenzustand als Endergebnis gelesen | Mensch | auf `rc=`
     umgestellt; kein Tor moeglich, das ist Werkzeugbedienung
  9. K4 als „Messbefehl fehlt" in CLAUDE.md eingetragen, obwohl
     `gen_erzeuger_zensus.py` genau diese Frage seit MF-1087 misst und
     bei jedem Commit laeuft. Die Kennzahl war nicht unmessbar, sie war
     nur nie mit ihrem Messbefehl VERBUNDEN | Mensch (erst auf Nachfrage)
     | K4 verbunden, Stand 8 von 89 offen
 10. Nach dem Merge drei Generatoren gelaufen und `gen_stand.py`
     ausgelassen — obwohl genau das in der eigenen Gedaechtnisnotiz
     `stand_md_zuletzt_erzeugen.md` steht | TOR (pre-commit,
     `[STAND.md stale]`) | Generator nachgezogen, Merge liegt als 7a1d6b07

K0 dieser Sitzung: 2 von 10

**Befund ueber diese Bilanz selbst, und er gehoert hierher:** zwischen
ihrem ersten Schreiben und dem Sitzungsende kamen die Fehler 8, 9 und 10
dazu — und Fehler 9 waere ohne eine Nachfrage von aussen unbemerkt
geblieben. **Eine am Ende rekonstruierte Bilanz ist unvollstaendig.** Sie
gehoert beim Auftreten gefuehrt, nicht nachtraeglich zusammengesucht;
sonst haengt ihre Vollstaendigkeit daran, dass man sich an den
Sitzungsanfang erinnert. Gemessen am 2026-09-28: die erste Fassung nannte
7 Fehler, die vollstaendige nennt 10.

Und zwei Beobachtungen zu K0, damit die Zahl nicht falsch gelesen wird.
**Sie ist von 1/7 auf 2/10 gestiegen, weil MEHR Fehler gefunden wurden,
nicht weniger** — eine ehrliche Fehlerzahl steigt zuerst. Und die beiden
Tor-Faenge (Fehler 2 und 10) haben beide etwas gefangen, das als Satz
bereits im Baum stand: „eine Groesse, eine Rechnung" und „gen_stand.py
zuletzt". Das ist der ganze Grund fuer diese Kennzahl.

Fundus (nicht gebaut, weil keine Kennzahl bewegt):
  * Fehler 4 bekommt KEIN Tor: der Parser war Wegwerf-Code im
    Scratchpad, ein CI-Tor dafuer waere ein Tor ohne Gegenstand. Statt
    dessen Regelsatz Nr. 2 unter K6: „Wer eine Menge parst, zaehlt sie
    gegen." Ein Tor dafuer braucht erst eine Messung: an welchen Stellen
    im Baum wird eine fremde Ausgabe geparst?
  * K3 fuer die uebrigen Konstantenfamilien (Kapazitaeten, Lueckenmasse,
    CRC-Polynome) — es gibt kein Audit. Der naechste Schritt ist die
    MESSUNG (welche Familien werden mehrfach gefuehrt?), nicht das Tor.
  * Zwei Tragbarkeitsbefunde aus der t4-Zulieferung (`memmem`,
    `system("./...")`) — bewegen keine Kennzahl dieses Baums, dem Autor
    gemeldet, nicht registriert.
```
