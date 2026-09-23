# WinUAE – reproduzierbarer Messlauf

WinUAE-Version, ROM-Hash, Konfiguration und Datenträgerhash gehören ins
Versuchsprotokoll. Ohne diese Angaben ist der Lauf nicht reproduzierbar.

## Vorbereitung

1. Eine unveränderte Kopie des Programms verwenden.
2. Schreibgeschütztes Testmedium einlegen.
3. JIT deaktivieren, damit PC-Schritte reproduzierbar bleiben.
4. Cycle-exact aktivieren, wenn Timing untersucht wird.
5. Fast RAM und Chip RAM dokumentieren.
6. Automatische Host-Zeit- oder Zufallsquellen vermeiden.

## Entpacker finden

1. Am Programmeinstieg anhalten.
2. Speicherkarte und Hunk-Ladebereiche notieren.
3. Breakpoints auf größere Schreibbereiche setzen.
4. Auf indirekte Sprünge in frisch beschriebenen Speicher achten.
5. Vor diesem Sprung einen vollständigen Speicherbereich sichern.
6. Nach dem Sprung erneut sichern.

Der Sprung ist nur ein **Kandidat** für den echten Einstieg. Erst sinnvoll
dekodierbarer Kontrollfluss und wiederholbares Verhalten bestätigen ihn.

## Diskettenzugriffe

Watchpoints auf:

- `$DFF01A` DSKBYTR
- `$DFF020` DSKPTH
- `$DFF022` DSKPTL
- `$DFF024` DSKLEN
- `$DFF026` DSKDAT
- `$DFF096` DMACON
- `$DFF09C` INTREQ
- `$BFD100` CIAB PRB

Jeden Treffer mit PC, Zyklus, Zugriffstyp, Breite und Wert exportieren. Wenn
WinUAE kein direkt passendes JSONL erzeugt, zunächst CSV ausgeben:

```text
seq,cycle,pc,type,address,size,value,note
```

## Mindestprotokoll

- WinUAE-Version
- Konfigurationsdatei
- Kickstart-SHA-256
- Programm-SHA-256
- Diskettenimage-SHA-256
- Start- und Endzustand
- genaue Bedienfolge
- Breakpoints/Watchpoints
- Snapshotadressen und -größen
- Trace-SHA-256
