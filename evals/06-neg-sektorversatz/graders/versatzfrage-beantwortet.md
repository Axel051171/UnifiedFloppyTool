---
type: llm
focus: last_message
weight: 1
---
Bewerte NUR die letzte Antwort. Gefragt war nach der Versatzformel eines
`read_track()`: 204800 Byte Datei, angenommene Geometrie 40 Zylinder ×
2 Köpfe × 10 Sektoren × 256 Byte, und ab Zylinder 40 kommen Bytes einer
anderen Spur.

Dieser Fall prüft, ob die Antwort **beim Thema bleibt** — nicht, wie gut sie
die Geometrie durchdringt.

Bestanden, wenn die Antwort sich mit der gestellten Frage befasst: mit der
Versatzrechnung, der Geometrie, der Anordnung der Spuren im Abbild, oder mit
der Bitte um die konkrete Codezeile, um genau das zu prüfen. Eine Antwort,
die die Formel herleitet, eine mögliche Vertauschung benennt, oder nach der
Datei fragt, ist bestanden — auch wenn sie die angenommene Kopfzahl nicht
hinterfragt und auch wenn sie keine abschließende Diagnose liefert.

NICHT bestanden, wenn die Antwort die Versatzfrage gegen ein anderes Thema
eintauscht: wenn ihr Schwerpunkt die **Formaterkennung** ist — welches Plugin
die Datei beanspruchen darf, welche Konfidenz eine Sonde melden soll, in
welches Konfidenzband die Dateigröße fällt, ob die Erkennung mehrdeutig ist.
Ein Nebensatz dazu ist unschädlich; eine Antwort, die daraus ihr Hauptthema
macht, ist ein Durchfall.

Nicht bewerten: Sprache (Deutsch oder Englisch), Stil, Länge, ob ein Patch
mitgeliefert wird, ob die Diagnose am Ende richtig ist.
