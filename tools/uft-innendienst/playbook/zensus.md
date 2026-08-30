# Rolle 2 — Zensus-Schmied: das Ritual und seine Checkliste

Das Ritual (unverändert seit Tor 42):
  Einzelfund → Frage schärfen → Zensus → Rotbeweis
  (Fix entfernen ⇒ genau 1 Befund) → Tor registrieren.
Der Schmied liefert Zensus + Tor + Rotbeweis-Skizze — NIE den Fix.

## Die Frage schärfen (der Schritt, an dem Zensen scheitern)
Zu weit gefragt schlägt 70-mal falsch an und wird übergangen; zu eng
gefragt misst nichts. Muster aus Tor 42: nicht "hat das Plugin ein
Magic?" sondern "prüft die Probe ein Magic, das open() nicht prüft?"
— ein Vergleich, der ABLEHNT, ist ein Magic; einer, der hochzählt,
ist ein Hinweis.

## Checkliste der gemessenen Zensus-Fehler (jede Zeile hat gekostet)
- [ ] Ablehnung ≠ Hochzählung (Konfidenz-Vergleiche nicht mitzählen)
- [ ] ALLE Fehlermakro-Familien: UFT_ERROR **und** UFT_ERR_
- [ ] NULL-Funktionszeiger ist kein Pfad (write_track = NULL)
- [ ] src/ zuerst; Stichprobengrenze ins Ergebnis schreiben
- [ ] Dateimengen aus `git ls-files`, nie gepflegte Listen (MF-633)
- [ ] Erwähnung ≠ Aufruf: Kommentare/Strings strippen (Tür-Sucher-Fix)
- [ ] \b-Escaping im Muster durch Selbsttest beweisen (der \\b-Fall
      meldete still ZU WENIG — die gefährliche Richtung)
- [ ] Zuordnung über Identität (git remote, voller owner/repo),
      nie über Namen
- [ ] "Was die Null nicht heißt" als Pflichtabsatz im Bericht

## Rotbeweis-Standard
1. Zensus läuft auf HEAD ⇒ N Befunde erwartet (nach Fix: 0)
2. Den behobenen Einzelfund künstlich zurückdrehen ⇒ GENAU 1 Befund
3. Zurücksetzen ⇒ 0. Ein Beweis, der sich selbst überspringt
   (falsches Korpus-Verzeichnis!), ist keiner.

## Selbsttest-Pflicht
Jeder neue Zensus bekommt Fixtures seiner bekannten Fehlklassen in
data/ und muss sie finden, BEVOR seine Zahl als Nenner gilt.
