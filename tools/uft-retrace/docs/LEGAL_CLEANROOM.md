# Lizenz- und Clean-Room-Regeln

## Was die Paketlizenz abdeckt

GPL-2.0-or-later deckt nur diesen Workflow und seine synthetischen Tests ab.

## Was sie nicht abdeckt

- historische Kopierprogramme;
- Kickstart-ROMs;
- Diskettenimages;
- automatisch erzeugte Disassemblies;
- nahezu wörtliche Rekonstruktionen fremder Routinen.

Eine MIT-Datei neben einer Sammlung beweist nicht automatisch, dass der
Sammler die enthaltenen historischen Programme unter MIT stellen durfte.

## Saubere Übergabe

Die analysierende Seite dokumentiert Verhalten:

- Eingabezustand;
- Ausgabezustand;
- ausgeführter PC-Bereich;
- Registerzugriffe;
- Entscheidung;
- Gegenprobe.

Die implementierende Seite erhält diese Beschreibung und synthetische
Fixtures, aber keine historischen Maschinenbytes. Sie schreibt eine neue
C-Implementierung und belegt sie mit den Fixtures.

Das ist keine Rechtsberatung. Bei einer Veröffentlichung rekonstruierter
historischer Routinen muss die Rechtefrage separat geklärt werden.
