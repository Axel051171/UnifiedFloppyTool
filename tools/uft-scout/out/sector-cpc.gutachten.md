<!-- stufe: 2 — mechanischer Entwurf, Tiefenpruefung ausstehend (MF-646) -->
# Gutachten-Entwurf: sector-cpc
Stand: 2026-08-26 · Messung: `sector-cpc.messung.json`
(HEAD `4b79ea5124`, letzter Commit 2021-02-06)
· Inventar: UFT `3f80164`

## Messwerte
- Dateien: 16, Sprachen: [['.h', 5], ['.c', 5], ['.txt', 1], ['.png', 1], ['.md', 1], ['.xcf', 1]]
- Letzte Commit-Botschaft: Fix 644 to 664
- Domänen-Score: 6 (Schwelle 3)

## Lizenz (aus Dateien, nicht README)
  - KEINE Lizenzdatei
- **Zone: ROT** → Konsequenz: keine Lizenz = alle Rechte vorbehalten; nur Spec aus Doku/Blackbox + Oracle

## Inventar-Abgleich (Abfrage, kein Urteil)
- `floppy` → vorhanden: flashfloppy
- `sector` → vorhanden: hardsector, uft_hardsector

## Domänen-Fundstellen
  - checksum: amsdos.c, cpm.h, sector-cpc.c
  - directory: cpm.c, cpm.h, docs/README.md
  - filesystem: sector-cpc.c
  - floppy: sector-cpc.c
  - sector: .gitignore, CMakeLists.txt, cpcemu.c
  - track: cpcemu.c, cpcemu.h, cpm.c

## UNGEKLÄRT — von der Tiefenprüfung zu füllen, NIE zu raten
- [ ] Kategorie: Innovation / Verbesserung / Daten / Oracle / irrelevant
- [ ] Was genau fehlt UFT bzw. was wäre besser? (Datei+Zeile zitieren)
- [ ] Bei "besser": Differenzlauf-Plan (Binaries, Korpus, Metrik, Toleranz)
- [ ] Einhängepunkt in bestehende Pläne (welcher Baustein?)
- [ ] Oracle-Kandidat + Baubarkeit
- [ ] Beschaffungsliste (Fixtures, Referenzdateien, Specs)
- [ ] Aufwandsklasse S/M/L
- [ ] OPEN_ITEMS-Vorschlagstext (max. 3 Sätze, mit Messquelle)

## Regeln, die für diesen Fund gelten
- Zone ROT: keine Lizenz = alle Rechte vorbehalten; nur Spec aus Doku/Blackbox + Oracle
- Kein Code aus diesem Agenten (AGENT.md Regel 1)
