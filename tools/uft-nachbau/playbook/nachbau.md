# Playbook Nachbau-Werkstatt — der Ablauf in fünf Stufen

1 TRIAGE: Lizenzdatei der Vorlage lesen (nie README-Behauptung),
  Datum der Lizenzeinführung messen (lizenzloser Zeitraum = ROT),
  Herkunft der Vorlage klären: woher hat SIE ihr Wissen? Route nach
  AGENT.md-Tabelle. GRÜN => Port-Auftrag, hier endet die Werkstatt.
2 MESSLAUF (messlauf.py): Vorlage-Binary pinnen (Version/SHA), über
  Fixtures fahren. Ablehnungs-Läufe zuerst (kaputte/fremde Eingaben) —
  wie sich die Vorlage bei Müll verhält, ist die halbe Spec.
3 SPEC (spec_gen.py + Tiefenprüfung): je Zeile Provenienz (M-ID oder
  Doku-Zitat). Die Hatari-Frage an jede Beobachtung: Formatgesetz
  oder Leser-Eigenheit der Vorlage? Nur Ersteres wird Soll-Verhalten.
  Weißliste der Fakten anlegen (Magics, offizielle Feldnamen, beide
  Schreibungen) — sie füttert kontamination.py.
4 VEKTOREN: je Verhaltenszeile Fixture-Hash -> Erwartung. Ablehnung,
  die vorher schreibt, ist keine (dim_atari-Lektion): Schreibversuch-
  Vektoren prüfen den Dateizustand NACH dem Lauf.
5 ÜBERGABE: Spec + Vektoren + Fixture-Manifest (Lizenz je Datei) +
  Oracle-Entwurf (fünfte Frage beantwortet) + Kontaminations-
  Grundlinie + Eigentümer-Fragen. EIN Paket je Zyklus, vollständig.

NACH Hand B: kontamination.py Neubau vs. Vorlage mit der Spec-
Weißliste. Befunde => Herkunftsaudit (`docs/QUARANTINE_PROCESS.md` §4).
Null Befunde sind notwendig, nicht hinreichend — das Audit bleibt.
