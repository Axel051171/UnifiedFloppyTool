# Gutachten-Entwurf: dfsimage
Stand: 2026-08-28 · Messung: `dfsimage.messung.json`
(HEAD `de24cf0ff5`, letzter Commit 2021-02-20)
· Inventar: UFT `cf5fa96f`

## Messwerte
- Dateien: 38, Sprachen: [['.py', 23], ['.rst', 5], ['.toml', 1], ['.cfg', 1], ['.yml', 1]]
- Letzte Commit-Botschaft: API documentation in progress
- Domänen-Score: 9 (Schwelle 3)

## Lizenz (aus Dateien, nicht README)
  - `work/dfsimage\LICENSE` → MIT (GRUEN)
  - `work/dfsimage\dfsimage\wildparse\LICENSE` → UNGEKLAERT (PRUEFEN)
- **Zone: PRUEFEN** → Konsequenz: Eigentümer-Vorlage vor jedem weiteren Schritt

## Inventar-Abgleich (Abfrage, kein Urteil)
- `atr` → **vorhanden**: atr (Tier T1b)
- `crc` → **vorhanden**: uft_air_crc32, uft_crc
- `floppy` → **vorhanden**: flashfloppy
- `sector` → **vorhanden**: hardsector, uft_hardsector

## Domänen-Fundstellen
  - ATR: .github/workflows/python-package.yml
  - CRC: dfsimage/inf.py
  - decode: dfsimage/cli.py, dfsimage/conv.py, dfsimage/entry.py
  - directory: .github/workflows/python-package.yml, dfsimage/cli.py, dfsimage/entry.py
  - filesystem: dfsimage/conv.py, dfsimage/image.py
  - floppy: description.rst, dfsimage/__init__.py, dfsimage/cli.py
  - interleave: dfsimage/cli.py, dfsimage/image.py, readme.rst
  - sector: dfsimage/__init__.py, dfsimage/cli.py, dfsimage/consts.py
  - track: dfsimage/cli.py, dfsimage/consts.py, dfsimage/image.py

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
- Zone PRUEFEN: Eigentümer-Vorlage vor jedem weiteren Schritt
- Kein Code aus diesem Agenten (AGENT.md Regel 1)
