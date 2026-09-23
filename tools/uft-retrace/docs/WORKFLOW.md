# Reproduzierbarer Arbeitsablauf

## Stufen und Abnahmekriterien

| Stufe | Eingabe | Ausgabe | Abnahme |
|---|---|---|---|
| 1 Inventar | Datei, ZIP oder Verzeichnis | `inventory.json`, extrahierte Dateien | SHA-256 und Größen vollständig |
| 2 Hunk-Trennung | Amiga-Hunk-Datei | CODE-/DATA-Segmente | Grenzen aus Container, nicht aus Disassembly geraten |
| 3 Loader-Lauf | unveränderte Binärdatei | Vor-/Nach-Snapshot | Emulatorprofil und Eingabe notiert |
| 4 Differenz | zwei gleich basierte Dumps | `snapshot_diff.json` | Zielbereiche wiederholbar |
| 5 Trace | JSONL oder CSV | Trace-Zusammenfassung | ausgeführte PCs und MMIO-Zugriffe vorhanden |
| 6 Verhaltensprobe | definierter Disk-/Track-Input | erwartete Bytes und Status | Positiv- und Negativprobe |
| 7 Übergabe | belegte Beobachtungen | Clean-Room-JSON | keine historischen Binärbytes übernommen |
| 8 UFT | neue Routine + Adapter | automatisierte Tests | öffentlicher CopyPlan-Pfad wird getestet |

## Reproduktionsnotiz

Zu jedem Lauf gehören mindestens:

- SHA-256 der Originaldatei und der Testdiskette;
- Emulator, Version, Kickstart-Version und Maschinenprofil;
- Kommandozeile dieses Werkzeugs;
- Basisadresse und Zeitpunkt beider Snapshots;
- Tracefilter und Abbruchbedingung;
- Ergebnis-JSON und Bericht;
- getrennte Kennzeichnung von Fakt, Indiz und Hypothese.

## Nicht verwechseln

- Hohe Entropie ist ein Packhinweis, kein Beweis.
- Eine Konstante `0x4489` kann Nutzdaten sein.
- Ein absoluter Registerwert kann in einer Tabelle stehen und nie ausgeführt werden.
- Ein Speicherunterschied kann Selbstmodifikation, Dekompression oder normale Laufzeitdaten sein.
- Der Sprung in einen neuen 64-KiB-Bereich ist nur ein Handoff-Kandidat.

Die Kombination aus Containergrenze, Snapshot-Differenz und ausgeführtem Trace
liefert die belastbare Grundlage.
