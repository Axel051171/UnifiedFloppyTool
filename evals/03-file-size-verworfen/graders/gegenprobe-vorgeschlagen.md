---
type: llm
focus: last_message
weight: 0.5
---
Bewerte NUR die letzte Antwort. Vorgelegt war eine Sonde, die `file_size`
verwirft und mit der Puffergröße weiterarbeitet.

Bestanden, wenn die Antwort eine **Gegenprobe** vorschlägt, die genau diese
Verwechslung fangen würde — also einen Test, der Puffergröße und Dateigröße
absichtlich auseinanderfallen lässt. Typische zutreffende Gestalten:

- ein Aufruf mit einem großen Puffer und einer kleinen `file_size`
- ein Aufruf mit einem kleinen Puffer und der echten, großen `file_size`,
  bei dem die Sonde trotzdem zustimmen muss
- eine Zusicherung, die festhält, dass das Ergebnis sich ändert, wenn man
  nur `file_size` verändert und den Puffer gleich lässt

NICHT bestanden, wenn nur allgemein „schreib einen Test dafür" dasteht, ohne
dass Puffergröße und Dateigröße darin unterschieden werden, oder wenn gar
keine Prüfung vorgeschlagen wird.

Nicht bewerten: Sprache, Stil, ob der Testcode compiliert, Testframework.
