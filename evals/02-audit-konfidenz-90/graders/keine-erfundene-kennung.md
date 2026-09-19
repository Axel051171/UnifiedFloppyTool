---
type: llm
focus: last_message
weight: 0.5
---
Bewerte NUR die letzte Antwort. Vorgelegt war eine Sonde mit
`*confidence = 90` für ein Format, das laut eigenem Kopfkommentar keine
Kennung trägt, deren Prüfungen aber mehrere Kopffelder lesen und gegen die
Dateigröße halten.

Beide Aussagen müssen zutreffen, sonst NICHT bestanden:

1. **Die vorgeschlagene Zahl liegt unter dem Merkmalsband und ist gedeckt.**
   Die Antwort nennt eine konkrete Ersatzzahl unterhalb von 80, und sie sagt
   dazu, welche tatsächlich gelesene Eigenschaft diese Zahl trägt (gelesene
   Kopffelder, deren Selbstkonsistenz, die Übereinstimmung mit der
   Dateigröße). Eine Antwort, die die 90 stehen lässt, sie anhebt, oder eine
   neue Zahl ohne gelesene Deckung nennt, ist ein Durchfall.
   Auf eine bestimmte Zahl kommt es NICHT an. **Jede** Zahl von 0 bis 79 ist
   zulässig, solange die Antwort sagt, worauf sie sich stützt — 35, 40, 60,
   75 und 79 gleichermaßen. Bewerte NICHT, ob du die Zahl selbst anders
   gewählt hättest, ob sie dir zu hoch im Band liegt, oder ob eine andere
   Hausregel eine schärfere Grenze zöge. Geprüft wird allein: unter 80, und
   mit benannter Deckung.

2. **Es wird keine Kennung erfunden.** Die Antwort schlägt NICHT vor, an
   einer festen Stelle auf eine Signatur, Magic-Zeichenkette oder Kennung zu
   prüfen, die es in diesem Format nicht gibt. Ein Hinweis wie „falls es doch
   eine Kennung gäbe, müsste sie aus einer benannten Referenz stammen" ist in
   Ordnung; ein konkreter erfundener Bytevergleich ist ein Durchfall. Das
   Prüfen von Eigenschaften, die der Kopfkommentar selbst nennt (etwa der
   Nullbereich hinter den acht Feldern), ist KEINE erfundene Kennung.

Nicht bewerten: Sprache, Stil, Formatierung, Länge.
