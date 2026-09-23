# FS-UAE – Messlauf

FS-UAE eignet sich als zweite Emulatorhand. Es soll nicht denselben Messfehler
wie der erste Aufbau bestätigen.

## Regeln

- dieselben Eingabedateien und Hashes verwenden;
- Emulatorversion und Konfiguration sichern;
- JIT abschalten;
- denselben Speicherbereich vor und nach dem Loaderübergang sichern;
- PC- und MMIO-Ereignisse in das gemeinsame JSONL-/CSV-Format übertragen;
- Ergebnisse nicht bytegenau erzwingen, wenn die Emulatoren unterschiedliche
  Zyklusmodelle verwenden; stattdessen Reihenfolge und Registersemantik prüfen.

## Kreuzprüfung

Belastbar ist ein Befund beispielsweise, wenn beide Emulatoren zeigen:

1. derselbe Loader beschreibt denselben Zielbereich;
2. der erste ausgeführte PC im Zielbereich stimmt;
3. dieselbe Routine programmiert DSKPTH/L und DSKLEN;
4. dieselbe Statusklasse löst einen Retry aus.

Unterschiedliche Zykluszahlen allein widerlegen die Funktionszuordnung nicht.
