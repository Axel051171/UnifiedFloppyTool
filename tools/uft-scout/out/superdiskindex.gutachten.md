# Gutachten-Entwurf: superdiskindex
Stand: 2026-08-26 · Messung: `superdiskindex.messung.json`
(HEAD `52463ced6d`, letzter Commit 2020-05-07)
· Inventar: UFT `3f80164`

## Messwerte
- Dateien: 59, Sprachen: [['.cc', 23], ['.h', 22], ['.md', 4], ['.deb', 2], ['.html', 1]]
- Letzte Commit-Botschaft: version bump
- Domänen-Score: 17 (Schwelle 3)

## Lizenz (aus Dateien, nicht README)
  - `LICENSE` → MIT (GRUEN)
- **Zone: GRUEN** → Konsequenz: Code portierbar mit Attribution (samdisk-Muster)

## Inventar-Abgleich (Abfrage, kein Urteil)
- `adf` → vorhanden: adf, adf,adl, adf-copy, adf_adl, adf_arc (Tier T1b)
- `crc` → vorhanden: uft_air_crc32, uft_crc
- `d64` → vorhanden: d64 (Tier T1b)
- `decode` → vorhanden: dec
- `filesystem` → vorhanden: st
- `floppy` → vorhanden: flashfloppy
- `flux` → vorhanden: flux, flux_profiles, fluxdec, fluxengine, kryoflux
- `gcr` → vorhanden: uft_c64_gcr
- `greaseweazle` → vorhanden: greaseweazle
- `mfm` → vorhanden: dsk_mfm, mfm, mfm_native, uft_mfm
- `revolution` → vorhanden: ti
- `scp` → vorhanden: scp, uft_scp, uft_scp_writer (Tier T1b)
- `sector` → vorhanden: hardsector, uft_hardsector

## Domänen-Fundstellen
  - ADF: FormatDiskAmiga.cc, NOTES.md, README.md
  - CRC: CRC.cc, CRC.h, DiskMap.cc
  - D64: FormatDiskC64_1541.cc, VirtualDisk.cc, VirtualDisk.h
  - GCR: Buffer.cc, Buffer.h, FluxData.cc
  - MFM: Buffer.cc, Buffer.h, FluxData.cc
  - SCP: FluxData.cc, NOTES.md, README.md
  - bitcell: FluxData.cc, legacy/main2.cc
  - checksum: NOTES.md
  - decode: Buffer.cc, Buffer.h, FormatDiskAmiga.cc
  - directory: DiskMap.cc, DiskMap.h, FormatDiskAmiga.cc
  - filesystem: DiskMap.cc, NOTES.md
  - floppy: FormatDiskC64_1541.cc
  - flux: CHANGELOG, Config.cc, Config.h
  - greaseweazle: README.md, legacy/main2.cc
  - revolution: Config.h, FluxData.cc, FluxData.h
  - sector: CHANGELOG, DiskLayout.cc, DiskLayout.h
  - track: BitStream.cc, BitStream.h, Config.cc

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
- Zone GRUEN: Code portierbar mit Attribution (samdisk-Muster)
- Kein Code aus diesem Agenten (AGENT.md Regel 1)
