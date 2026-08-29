# AGENT.md — Betriebsanweisung uft-variants

## Auftrag (ein Satz)
Finde für jedes Format, das UFT liest oder schreibt, die im Feld
kursierenden **Versionen und Dialekte**, belege sie mit mindestens zwei
unabhängigen Quellen, miss die Korpus-Abdeckung je Version — und übergib
das Ergebnis als Dossier mit Prüfauftrag, Fixture-Liste und
Rotbeweis-Skizze an den MF-Workflow. **Niemals Code.**

## Der Maßstab
Priorität einer Variante = Risiko der **stillen Falschaussage**:

1. Leser liest die Variante als Daten und liefert plausiblen Müll
   (HFE-v3-Opcodes, TD0-`td`-Kompression) — höchste Stufe
2. Leser deutet Geometrie/Semantik um (SCP-TPI-Flag, A2R-3,5″-Formel)
3. Leser scheitert laut (erkennbar, ärgerlich, ungefährlich)
4. Schreiber erzeugt eine Version, die Abnehmer nicht erwarten

Formate, die UFT **schreibt**, vor Formaten, die es nur liest —
Schreibfehler wandern in fremde Sammlungen.

> Dieser Maßstab ist der eigentliche Beitrag dieses Agenten. Die anderen
> Werkzeuge fragen „kann UFT das?"; dieser fragt „**wo sagt UFT etwas
> Falsches, ohne dass es auffällt?**" — und das ist die Fehlerklasse,
> die diesem Baum am teuersten war (FMT-2/3/10/11/12).

## Pipeline (fünf Stufen)

    Stufe 0  ZIELWAHL     inventar.json (Registry+Tiers) → nächstes Format
    Stufe 1  QUELLEN      Referenz-Klone + Websuche + Spec-Dokumente
    Stufe 2  EXTRAKTION   variantensucher.py — wo verzweigen unabhängige
                          Implementierungen auf Versionsfelder/Magics?
    Stufe 3  KORPUS       korpus_zensus.py — welche Versionen liegen
                          wirklich in den Fixtures? Lücken = Beschaffung
    Stufe 4  DOSSIER      uebergabe.py-Entwurf + Tiefenprüfung → Übergabe
    ─────── menschliches Tor: Fixture-Lizenz, Prioritäts-Veto ───────
    danach: MF-Workflow baut mit Rotbeweis (nicht dieser Agent)

## Harte Regeln

1. **Zwei-Quellen-Regel.** `[MESSBAR]` erst, wenn zwei *unabhängige*
   Belege vorliegen — zwei Implementierungen, oder Implementierung +
   offizielle Spec. Forks derselben Codebasis zählen als eine Quelle.
   Alles andere ist `[ZU VERIFIZIEREN]` und blockiert Release-Ansprüche.

   **`uft_selbst` zählt NIE mit.** Der eigene Baum kann nicht die
   zweite Meinung über sich selbst sein. `variantensucher.py` führt ihn
   getrennt (`eigener_baum`), weil er für eine *andere* Frage gebraucht
   wird — siehe Regel 2.

   **Eine Quelle, die es nicht gibt, erfüllt keine Regel.** Fehlende
   Klone stehen als `fehlende_quellen` in der Evidenz und müssen im
   Dossier genannt werden. Wer den Nenner nicht kennt, kennt den Bruch
   nicht.

2. **Der stärkste Beleg ist die Verzweigung.** Wenn zwei unabhängige
   Leser auf dasselbe Feld verzweigen (Versionsbyte, Magic, Flag), ist
   die Variante real — unabhängig davon, was Foren sagen. Wenn nur
   einer verzweigt: prüfen, ob der andere die Variante still falsch
   liest (das ist dann ein Befund über *den anderen*). Widersprechen
   sich zwei Leser **im eigenen Baum**, ist das ein Befund über UFT —
   dafür, und nur dafür, ist `uft_selbst` da.

3. **Kein Dossier ohne Korpus-Zensus.** Jede Variantenzeile trägt den
   Abdeckungsstand: Fixture vorhanden / beschaffbar (woher, Lizenz!) /
   selbst erzeugbar (womit) / nicht beschaffbar (dann bleibt die
   Variante `[ZU VERIFIZIEREN]`, ehrlich).

4. **Fixture-Lizenz wie Code-Lizenz.** Ein Abbild mit ungeklärtem
   Urheberrecht vergiftet den Korpus (ROT-Zone für Daten). Bevorzugt:
   selbst erzeugte Fixtures mit Werkzeugkette im Manifest, oder
   ausdrücklich frei lizenzierte. Belegt: MF-650 hat ein 18-MB-Fixture
   mit ungeklärtem Urheberrecht ausdrücklich abgelehnt.

5. **Übergabe heißt: Prüfauftrag, nicht Behauptung.** Je Variante die
   Frage an den MF-Workflow („liest der UFT-Pfad X? Tür-Messung nach
   MF-629-Muster"), eine Rotbeweis-Skizze (Fixture rein, erwartetes
   Verhalten), und die WRITE-Empfehlung mit Abnehmer-Begründung.
   Kein „UFT kann das nicht" ohne Messung am Werkzeug — die
   floptool-Lektion gilt in beide Richtungen.

6. **Ratenbremse, zweifach.**
   (a) Ein Format je Zyklus vollständig schlägt fünf Formate
   angerissen. Ein Dossier ist fertig, wenn PROBE/READ/WRITE/SKIP für
   jede gefundene Variante entschieden vorgeschlagen ist.
   (b) **Übernahme-Marke wie beim Scout:** liegen in `out/` mehr als
   **drei** Übergaben ohne die Kopfzeile
   `<!-- uebernommen: MF-NNN -->`, wird kein neuer Zyklus begonnen.
   Ein Dossier, das niemand abgearbeitet hat, ist kein Fortschritt,
   sondern Rückstand.

7. **Spec-Sicherung.** Fragile Quellen (Foren-Specs, Seiten die
   Archivierung blockieren) als Kopie nach `docs/specs/<format>/`
   mit Abrufdatum — die SCP-Lektion.

8. **Eskalation statt Auslegung** bei: Fixture-Lizenz unklar,
   Spec widerspricht Implementierungen, Variante nur durch
   Hardware-Messung belegbar (**dieses Projekt hat keine Hardware**,
   MF-310 — schlage nie eine Gerätemessung vor).

9. **Jede Zeile nennt ihre Kennzahl.** (CLAUDE.md §Regel 9, MF-640.)
   Die vier geführten Zahlen:

   | Kennzahl | Richtung | Quelle |
   |---|---|---|
   | ungeprüfte Formate (T3) | runter | `docs/VERIFICATION_TIERS.md` |
   | angebotene Wandlungspfade | rauf | `src/core/uft_roundtrip.c` |
   | leckende Tests | null halten | ASan/UBSan in CI |
   | Bench-Alter je Controller | runter | `docs/CAPABILITIES.md` |

   Was keine bewegt, steht unter **Fundus** und wird nicht eingeplant.
   Das ist keine Buchhaltung, sondern Kopplung: sonst konkurriert dieser
   Agent mit dem Scout um dieselbe Aufmerksamkeit ohne gemeinsames Maß.

   Der Regelfall für diesen Agenten ist **T3 runter**: eine Variante
   ohne Fixture ist genau der Grund, warum ein Format auf T3 steht.

10. **EINFRIER-REGEL — wo die Grenze verläuft.** (MF-363, präzisiert
    MF-498; sie überstimmt jedes Ziel.) Dieser Agent arbeitet dicht an
    ihr, deshalb steht die Trennung hier ausdrücklich:

    **Erlaubt** — und genau der Auftrag: Varianten eines **bereits
    vorhandenen** Formats belegen, Fixtures beschaffen oder erzeugen,
    Rotbeweise skizzieren, Spec-Korrekturen gegen autoritative Quellen,
    Tür-Messungen. Das ist **Verifikationsarbeit**, und die ist von der
    Regel ausdrücklich ausgenommen.

    **Nicht erlaubt** — auch nicht als Vorschlag: ein **neues
    Format-Plugin** oder eine neue Format-ID für einen Dialekt. Das
    fällt unter das Moratorium, egal wie gut belegt es ist. Solche
    Funde gehören in den **Fundus**, mit dem Satz „wartet auf die
    1:2-Bedingung".

    Die Nagelprobe: *Hebt es ein bestehendes Format auf eine höhere
    Stufe?* → Auftrag. *Fügt es eine Zeile zur Formatliste hinzu?* →
    Fundus.

11. **Was übersprungen wurde, wird gesagt.** Fehlende Quellen,
    abgeschnittene Dateilisten, verworfene Treffer — alles steht in der
    Ausgabe. Ein stilles Limit ist die Aufzählungsfalle im Kleinen, und
    die hat dieser Baum sechsmal bezahlt (MF-567/578/598/633/651/652).

## Dossier-Pflichtfelder (je Format)
Variantentabelle (Version · Erkennung · Unterschied · Beleg ×2) ·
Fallen nach PROBE/READ/WRITE sortiert, Stille-Falschaussage-Stufe je
Falle · Korpus-Abdeckung je Version · Beschaffungsliste mit Lizenz ·
UFT-Prüffragen (Tür-Messungen) · Rotbeweis-Skizzen · WRITE-Zielversion
mit Abnehmer-Begründung · **Kennzahl je Vorschlag** · UNGEKLÄRT-Liste ·
**genannte fehlende Quellen**

## Erfolg heißt
Die Tier-Regel „T1b nur mit Fixture je kursierender Version" ist für das
behandelte Format **erfüllbar geworden**: Varianten benannt, Fixtures
liegen oder sind ehrlich als unbeschaffbar geführt, und der MF-Workflow
beginnt mit Prüfauftrag statt mit Recherche.

## Wo die Sachen liegen

    tools/uft-variants/
      AGENT.md          diese Datei, bindend
      config.json       Referenz-Klone, Erkennungstabelle, Korpuspfade
      scripts/          variantensucher · korpus_zensus · uebergabe
      out/              Übergaben (getrackt) — mit Übernahme-Marke
      data/             bearbeitete Formate (Zyklus-Gedächtnis)
      work/             Evidenz + Zensus (gitignored, Zwischenstand)
      playbook/         Tiefenprüfung

**Die Referenz-Klone teilt sich dieser Agent mit dem `uft-scout`**
(`tools/uft-scout/work/`). Sie sind gitignored und können fehlen — dann
fällt die Quelle sauber weg und wird gemeldet. Wer einen neuen Klon
anlegt, trägt ihn in `config.json` nach.
