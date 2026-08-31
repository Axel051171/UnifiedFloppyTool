# AGENT.md — Betriebsanweisung uft-nachbau (die Werkstatt)

## Auftrag (ein Satz)
Nimm eine fremde Referenz-Implementierung als Vorlage und bereite den
Nachbau vollständig vor — Lizenz-Route, Verhaltens-Spec aus Doku und
Blackbox-Messung, Prüfvektoren, Oracle-Registrierung, Kontaminations-
Grundlinie — so, dass der MF-Workflow implementieren kann, **ohne die
fremde Quelle je gesehen zu haben.**

## Wo diese Werkstatt im Baum steht (Anpassung MF-696)

Sie erfindet keinen Prozess — sie **bedient einen, der schon steht**.
[`docs/QUARANTINE_PROCESS.md`](../../docs/QUARANTINE_PROCESS.md)
kennt seit MF-635 drei Rückwege aus der Quarantäne; **Weg 2 —
Clean-Room-Neubau gegen ein Oracle** ist genau der Auftrag dieser
Werkstatt, und §4 *Das Herkunftsaudit* liefert ihr Beweismaß.

Die fünf Stufen unten sind darum an §5 Weg 2 gebunden, nicht daneben:

| Weg-2-Schritt (verbindlich) | Stufe hier |
|---|---|
| 1 Verhaltens-Spec aus Doku + Blackbox, `docs/specs/<quelle>/` | 2 MESSLAUF + 3 SPEC |
| 2 Oracle registrieren, gebaut und gelaufen, Zirkularität aus | 5 ÜBERGABE (Oracle-Entwurf) |
| 3 Rotbeweise zuerst, gegen die Spec | 4 VEKTOREN — Hand B baut sie |
| 4 Neubau, dann Differenzlauf nach `ORACLES.md` | Hand B |
| 5 Liste schließen, Fähigkeit heben | Hand B |

Und §4 sagt, was am Ende zählt: **beweiskräftig sind Idiome, nicht
Fakten.** Genau daran hängt `kontamination.py` — nicht an einer
gepflegten Vokabelliste.

## Die Brandmauer (das eine Prinzip, das alles trägt)
Clean-Room heißt Zwei-Hände-Trennung:

    HAND A — diese Werkstatt          HAND B — MF-Workflow
    darf: Doku lesen, Binary          darf: NUR Spec, Vektoren,
    vermessen, (je Route) Quelle      Fixtures, Oracle sehen
    SICHTEN um Messpunkte zu          darf NICHT: die fremde
    finden                            Quelle öffnen — nie
    schreibt: NIE Produktcode         schreibt: die Implementierung,
                                      Rotbeweise zuerst

Die Spec ist die einzige Brücke. Deshalb gilt für sie das härteste
Gebot: **Fakten ja, Ausdruck nie.** Formatkonstanten, Feldlagen,
Grenzwerte, beobachtete Ein-/Ausgabepaare sind Fakten (frei).
Quelltextzeilen, Funktionszerlegung, Kommentarformulierungen,
erfundene Namen der Vorlage sind Ausdruck (geschützt) — sie dürfen
nicht in die Spec, auch nicht umformuliert-erkennbar.

### Die Brandmauer als Vorrichtung, nicht als Zusage (MF-740)

`kontamination.py` prüft **nach** dem Bau, ob Ausdrücke der Vorlage im
Neubau stehen — ein Nachweis am Ergebnis. `sichtprotokoll.py` führt den
Nachweis am **Vorgang**: welche Datei hat welche Hand geöffnet?

Bisher war das eine Zusage. Zusagen ohne Beleg sind in diesem Baum die
Ausgangslage jedes Befundes gewesen.

    1. --einfrieren <datei>…     Vorlage nach Pfad UND SHA-256 festhalten
    2. --protokoll A --dateien … Hand A meldet, was sie gelesen hat
    3. --arbeitsbaum <pfad>      MECHANISCH: liegt die Vorlage dort?
    4. --protokoll B --dateien … Hand B meldet
    5. --pruefen                 B-Meldung gegen die Vorlagenmenge

**Zwei Hälften, und nur eine ist mechanisch — das steht hier, damit
das Werkzeug nicht mehr verspricht, als es hält:**

* **Mechanisch: der Arbeitsbaum.** Hand B arbeitet in einem Baum, in
  dem die Vorlage nicht liegt. Geprüft nach Pfad *und* nach SHA-256 —
  eine Umbenennung rutscht sonst durch. Wer nicht lesen kann, was nicht
  da ist, ist nicht kontaminiert. Diese Hälfte kommt ohne Ehrlichkeit
  des Gemeldeten aus und ist die tragende.
* **Deklarativ: das Protokoll.** Was eine Hand geöffnet hat, misst das
  Werkzeug nicht — es sitzt nicht im Agenten. Ein Lauf B, der eine
  Vorlagendatei meldet, ist rot; ein Lauf B, der **schweigt**, ist
  *unbelegt*, nicht bestanden.

Abnahme: `--selbsttest` — **9/9**, darunter der Fall „Vorlage
umbenannt", den ein reiner Pfadvergleich durchlässt.

**Und die Brandmauer gilt für den Agenten selbst.** Wer die Vorlage
liest, um zu sehen, *was* nachzubauen wäre, ist Hand A — und damit als
Hand B ausgeschlossen. Das ist keine Formalie: die naheliegendste
Handlung am Ende eines Nachbaus ist, die Vorlage zu öffnen „um zu
prüfen, ob es stimmt". Genau das hebt die Trennung auf. Geprüft wird
gegen die Spec und gegen ein reales Abbild — nie gegen das Original.

## Lizenz-Routen (Triage VOR jeder Arbeit; Matrix des Scouts bindend)

| Zone der Vorlage | Route | Werkstatt-Leistung |
|---|---|---|
| GRÜN (MIT/BSD/GPL-2…) | **PORT** — Nachbau wäre Verschwendung | Port-Auftrag: Attribution-Header, SPDX, Datei-Liste, Rotbeweis-Skizzen. Quelle darf Hand B hier sehen |
| GELB (GPL-3/Apache/AGPL) | **NACHBAU** | Spec aus Doku + Blackbox; Quellsichtung nur zum Auffinden von Messpunkten, nie zum Übernehmen |
| ORANGE (BSD-4) | **HELPER** | Prozessgrenzen-Bauplan (PFS3-Muster), IPC-Schnitt, Degradations-Meldung |
| ROT (keine Lizenz/proprietär) | **BLACKBOX** | Spec ausschließlich aus Doku + Messläufen am Binary; Quelltext bleibt zu, auch für Hand A |
| Quelle war je lizenzlos | Datum messen! | lizenzloser Zeitraum ⇒ wie ROT, egal was heute draufsteht |

Sonderfall bewusst undokumentierter Formate (IPF-Klasse): Route
BLACKBOX/HELPER; existiert eine offizielle Drittbibliothek, ist sie
der beschlossene Weg und zugleich das Oracle des Nachbaus.

## Harte Regeln

1. **Kein Produktcode aus dieser Werkstatt.** Ausgaben: Spec,
   Vektoren, Fixture-Manifeste, Oracle-Einträge, Kontaminations-
   Grundlinie, Übergabe-Paket. (Prüf-*Vektoren* sind Daten, keine
   Tests — die Rotbeweise schreibt Hand B aus ihnen.)
2. **Jede Spec-Zeile trägt Provenienz:** Messlauf-ID (`M-…` aus
   work/evidenz/) oder Doku-Zitat mit Fundstelle. Eine Zeile ohne
   Quelle ist erfunden und fliegt.
3. **Zwei-Quellen-Regel für Verhaltens-Behauptungen:** Doku + Messung,
   oder zwei unabhängige Messwerkzeuge. Einquelliges heißt
   `[ZU VERIFIZIEREN]` und blockiert die Übergabe nicht, wohl aber
   jedes Tier-Urteil.
4. **Oracle ≠ Korpus-Erzeuger** (fünfte Frage, MF-644) — gilt auch
   hier: die Vorlage selbst darf Oracle sein, dann müssen die
   Fixtures von anderer Hand stammen (oder umgekehrt).
5. **Fixture-Lizenz wie Code-Lizenz.** Messläufe auf lokalen Dateien
   sind frei; in den Korpus wandert nur, was verteilbar ist.
6. **Kontaminations-Grundlinie vor Übergabe, Kontaminations-Prüfung
   nach Implementierung** (scripts/kontamination.py): Neubau vs.
   Vorlage — geteilte Bezeichner/Strings/Kommentare jenseits der
   Spec-Weißliste sind Befunde. Idiome beweisen, Tabellen nicht:
   Formatkonstanten stehen auf der Weißliste, Namens- und
   Kommentar-Echos nie.
7. **Besser-Behauptungen nur mit Differenzlauf-Plan** — „unser Nachbau
   soll X besser machen als die Vorlage" verlangt Korpus, Metrik,
   Toleranzliste im Übergabe-Paket.
8. **Eskalation statt Auslegung:** Doppel-Lizenzen, widersprüchliche
   Doku vs. Messung, Herkunftszweifel der Vorlage selbst (woher hat
   SIE ihr Wissen?) → Eigentümer-Vorlage.

## Ablauf (fünf Stufen)

    1 TRIAGE     Lizenz+Herkunft der Vorlage messen → Route
    2 MESSLAUF   messlauf.py: Binary über Fixtures, Evidenz mit
                 SHA-256 je Ein-/Ausgabe, IDs M-0001…
    3 SPEC       spec_gen.py: Evidenz + Doku-Zitate → Spec-Entwurf;
                 UNGEKLÄRT-Blöcke füllt die Tiefenprüfung
    4 VEKTOREN   je Verhaltenszeile ein Prüfvektor
                 (Fixture-Hash → erwartete Ausgabe/Hash) — daraus
                 macht Hand B die Rotbeweise
    5 ÜBERGABE   Paket: Spec · Vektoren · Fixture-Manifest ·
                 Oracle-Entwurf · Kontaminations-Grundlinie ·
                 offene Eigentümer-Fragen. Ratenbremse: EIN
                 Nachbau-Paket je Zyklus, vollständig.

## Erfolg heißt
Hand B beginnt mit rotem Test statt mit fremdem Code; die
Kontaminations-Prüfung nach der Implementierung ist leer; und das
Herkunftsaudit, das diesen Baum dreimal Wochen gekostet hat, ist für
Nachbauten dieser Werkstatt in Minuten führbar — weil die Grundlinie
schon daliegt.
