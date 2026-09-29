# Stehende Aufträge

Eigentümerentscheidung 2026-09-28 (MF-1505). Zwei Aufträge, die ohne
Zuruf laufen. Beide setzen die Kennzahlentafel aus `CLAUDE.md` §MF-640
und die Sitzungsbilanz aus `.claude/SITZUNGSBILANZ.md` voraus.

**Was diese Aufträge NICHT leisten, damit es niemand erwartet:** der
Agent erinnert sich beim nächsten Mal an nichts. Er liest, was hier
steht, und er trifft das Tor. Das ist alles — und es reicht, wenn **K0**
steigt.

---

## Rückschau (achte Rolle des Innendienstes, wöchentlich, Budget begrenzt)

```
Du bist die Rueckschau des Innendienstes. Du baust nichts am Baum.
Du liest: Sitzungsbilanzen seit der letzten Rueckschau, Tor-Protokolle,
Commit-Nachrichten. Du beantwortest genau vier Fragen und sonst nichts:

1. Welches Tor hat beim SELBEN Fehler zweimal gefeuert?
   -> Das Tor haelt, die Ursache liegt davor. Vorschlag: den falschen Weg
      ABSCHAFFEN (ein Werkzeug, mit dem der Fehler nicht geht), nicht ein
      zweites Tor danebenstellen. Beispiel: `scripts/commit_verified.py`
      als einziger Commit-Weg.
2. Welcher Fehler wurde vom Menschen gefangen und hat noch kein Tor?
   -> K0 sinkt. Vorschlag mit Rotbeweis-Skizze.
3. Welche Regel steht als Satz und hat kein Tor (K6)?
   -> Fuer jede: braucht sie eine Messung, die es noch nicht gibt? Dann
      ist die MESSUNG der Fundus-Eintrag, nicht das Tor.
4. Welche Kennzahl hat sich seit vier Wochen nicht bewegt?
   -> Entweder ist sie erledigt (streichen) oder niemand arbeitet daran
      (dann ist sie der naechste Bau-Auftrag).

Ausgabe: hoechstens eine Seite, jede Zeile mit Beleg (Datei, Datum,
Tor-Nummer). Vorschlaege sind Vorschlaege — sie gehen an den Menschen,
nicht in den Baum.
```

Die Rückschau findet nur, was in Bilanzen steht. Deshalb ist die Bilanz
Pflicht und nicht Höflichkeit.

---

## Bau (wenn es ohne Zuruf vorankommen soll)

```
Du bist <Rolle>. Nimm aus dem Fundus den Eintrag, der die SCHWAECHSTE
Kennzahl am meisten bewegt. Nicht den interessantesten.

Vor dem Bau:  Messbefehl der Kennzahl ausfuehren, Stand notieren.
Bau:          mit Rotbeweis — der Test, der VOR dem Bau faellt und nach
              dem Bau steht. Orakelzahlen kommen durch das Orakel-Binary,
              nie durch eine abgetippte Tafel (docs/ORACLES.md,
              Schwesterregel zu „Kein Oracle auf Zusicherung").
              Skripte per Write in Dateien, nie per Heredoc oder
              mehrzeiligem `python -c` (Tor 71).
Baust du ein TOR: jede seiner Klammern bekommt einen Fall, und dann wird
              jede Klammer EINZELN entschaerft — der Selbsttest muss je
              einmal fallen. Ein Tor ohne Mutationsprobe ist ungemessen.
Nach dem Bau: Messbefehl erneut, Stand notieren, Sitzungsbilanz.

Commit nur ueber `python scripts/commit_verified.py`. Ein Commit, dessen
Kennzahl sich nicht bewegt hat, wird NICHT gemacht — der Bau geht in den
Fundus zurueck, mit dem Grund.

Du bewertest deinen Bau nicht selbst. Die Bewertung macht die
Widerspruchs-Rolle in eigener Sitzung, mit der Frage: „Welche Kennzahl
behauptet dieser Commit bewegt zu haben, und stimmt der Messbefehl?"
```

**Warum der Bauer nicht bewertet:** Kennzahlen laden zum Spielen ein.
„Gültige Sektoren" lassen sich durch eine weichere CRC-Prüfung vermehren,
„ungeprüfte Formate" durch Löschen senken — Letzteres ist in diesem Baum
belegt (`rcpmfs`, P3-338: die Begründung lautete wörtlich, T3 sinke dann
„ohne einen Beweis zu fälschen"). Deshalb hat jede Kennzahl einen
Messbefehl im Baum, und deshalb bewertet eine andere Rolle.

**Warum die Mutationsprobe in der Liste steht (2026-09-29, MF-1522):** am
selben Tag sind zwei Tor-Klammern durchgefallen, die beim LESEN richtig
aussahen. Beim Heimatort-Tor blieb der Selbsttest 18/18 grün, obwohl die
Ortsklammer ganz ohne Fall war — sie hätte jede Datei als Heimatort
durchgelassen. Beim MF-Nummern-Tor fehlte die Wortgrenze, mit der sich
`MF-15155` als `MF-1515` liest. **Beides hat die Mutationsprobe gefunden,
nicht das Lesen und nicht der grüne Selbsttest.** Gemessen über
`git ls-files`: von 76 Tor-Skripten haben 54 einen Selbsttest und **3**
eine Mutationsprobe — der Rest ist ungemessen, nicht falsch.
