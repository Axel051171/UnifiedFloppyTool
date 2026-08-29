# Gesicherte Fremd-Referenzen je Format

Hier liegen **Kopien fremder Dokumente** — Foren-Specs, Herstellerseiten,
Handbuchauszüge —, die ein Varianten-Zyklus als Beleg benutzt hat und die
sonst dem Link-Tod anheimfallen.

Je Format ein Unterverzeichnis: `docs/format_refs/<format>/`.

## Pflicht im Kopf jeder Datei

```
Quelle:      <vollständige URL>
Abgerufen:   <JJJJ-MM-TT>
Lizenz:      <soweit erkennbar; "ungeklärt" ist eine gültige Angabe>
Zyklus:      <MF-NNN>
```

Ohne Quell-URL und Abrufdatum ist eine Sicherung wertlos: sie belegt dann
nur, dass irgendjemand irgendwann etwas gelesen hat.

## Warum das NICHT in `docs/specs/` liegt (MF-689)

Weil dort etwas anderes wohnt, und die Verwechslung wäre teuer.

| | `docs/specs/<quelle>/` | `docs/format_refs/<format>/` |
|---|---|---|
| **Inhalt** | Verhaltens-Specs, die **wir** schreiben | Kopien, die **andere** geschrieben haben |
| **Herkunft** | Dokumentation + Blackbox-Läufe, nie fremder Quelltext | fremdes Dokument, unverändert |
| **Lizenz** | unsere | fremde, oft ungeklärt |
| **Zweck** | Clean-Room-Neubau (Quarantäne-Verfahren Weg 2) | Beleg gegen Link-Tod |

Der entscheidende Punkt ist die **Lizenzlage**: eine selbst geschriebene
Spec und eine gesicherte Fremdkopie dürfen nicht im selben Verzeichnis
liegen, weil sonst niemand mehr sagen kann, welche Datei welchem Recht
unterliegt. Genau dafür führt dieser Baum eine Quarantäne.

Der Konflikt fiel auf, als der Varianten-Agent nach Regel 7 Kopien nach
`docs/specs/` legen sollte, während das dortige README das Verzeichnis
für das Quarantäne-Verfahren reserviert. **Die ältere Zusage gewinnt** —
`docs/specs/` bleibt Quarantäne, die Sicherungen ziehen hierher.

Das ist die Klasse „ein Pfad, zwei Vereinbarungen": keine der beiden Seiten
war falsch, sie wussten nur nichts voneinander. Beschlossen, bevor zwei
Agenten denselben Pfad verschieden füllen.
