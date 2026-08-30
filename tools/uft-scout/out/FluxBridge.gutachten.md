# Gutachten: CopperlineHQ/FluxBridge

> Gemessen 2026-08-30 gegen HEAD `e01a3746c9` (2026-08-05).
> Messdatei: `tools/uft-scout/work/FluxBridge.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692).

## Kategorie

**Fundus** — Lizenzlage PRÜFEN, Fähigkeiten überschneiden sich fast
vollständig mit Vorhandenem; der einzige Abstand (DrawBridge-Protokoll)
ist Hardware-Arbeit ohne Gerät.

## 1. Was es ist

Rust-Bibliothek (28 Dateien, 14 `.rs`): stellt einem Emulator ein
echtes Laufwerk als asynchron bedientes Gerät bereit — Worker-Thread,
nicht-blockierende Track-Reads, dekodiere MFM-Revolutionen (README).
Laut `NOTICE.md` ein **unabhängiger Rust-Port von FloppyDriveBridge**
(RobSmithDev), Port-Baseline `710fa15c…`, mit tabellarischer Zuordnung:
PLL/RotationExtractor, DrawBridge-(Arduino-)Protokoll,
**Greaseweazle**-Protokoll, **SuperCard-Pro**-Protokoll,
Controller-/Cache-Invarianten.

## 2. Lizenz — zuerst, weil sie alles entscheidet

* `Cargo.toml:10`: `license = "LGPL-3.0-or-later AND MPL-2.0"`.
* Datei-SPDX gemischt (Messung `lizenz_je_datei`): 7× MPL-2.0,
  6× LGPL-3.0-or-later.
* `NOTICE.md`: Upstream FloppyDriveBridge ist multi-lizenziert
  „MPL-2.0 OR GPL-2.0-or-later", `ArduinoInterface` LGPL-3.0-or-later;
  FluxBridge wählt MPL-2.0 für das Portierte und kombiniert es mit
  eigenem LGPL-3.0-Material.

**LGPL-3.0 steht nicht in der Lizenzmatrix** → nach Meta-Regel (2)
automatisch **Zone PRÜFEN**; dieses Gutachten ordnet nicht selbst ein
(MF-679). Dazu kommt eine dokumentierte Ableitungskette über ein
multi-lizenziertes Upstream — die Matrix-Zeile „Dual-Lizenz" führt
ebenfalls auf PRÜFEN. Die NOTICE-Führung selbst ist vorbildlich
(Baseline-Commit, Datei-Zuordnung, Vendor-Credits) — Sorgfalt ersetzt
aber keine Einordnung. Konsequenz: **kein Übernahmeweg ohne
Eigentümer-Vorlage**, auch nicht für die MPL-Dateien einzeln.

## 3. Abgleich gegen das Inventar

* Greaseweazle: `"greaseweazle": vorhanden: true` — UFT-HAL
  production (CLAUDE.md).
* SuperCard Pro: HAL M3.1 libusb verdrahtet (MF-254; Tier-3-Bank
  offen, UFT-008).
* PLL/Revolutions-Extraktion: `"mfm": vorhanden: true`; eigener
  PLL-/Multirev-Bestand (`src/decoder/`).
* **DrawBridge/ArduinoFloppy: 0 Treffer** in `src/`, `include/`,
  `docs/CAPABILITIES.md` (grep `drawbridge`, case-insensitiv). Das
  ist der einzige echte Abstand — ein **siebter Hardware-Controller**.
  Dieses Projekt hat kein Gerät (MF-310), jeder neue HAL-Backend-Pfad
  wäre Tier-3-Arbeit an die Community, und die Lizenzlage ist PRÜFEN.
  Kein Vorschlag.

## 4. Attribution

FluxBridge (CopperlineHQ), LGPL-3.0-or-later AND MPL-2.0, seinerseits
Port von FloppyDriveBridge (RobSmithDev, MPL-2.0 OR
GPL-2.0-or-later / LGPL-3.0). Kein Code übernommen.

## 5. Bewegte Kennzahl

**Keine.** Auch „Bench-Alter je Controller ↓" nicht — FluxBridge
liefert keine Bench-Daten für unsere sechs Controller, sondern
implementiert zwei davon selbst. **Fundus, nicht Auftrag.**

## 6. Einhängepunkt

Keiner. (Falls je ein DrawBridge-Provider gewollt ist:
`src/hardwaretab.h:45-62` ist die SSOT der Provider-Liste, und der Weg
führt über eine Eigentümer-Vorlage zur Lizenz und einen
Community-Bench-Plan.)

## 7. Oracle-Kandidat

Nein — es ist eine Laufzeit-Brücke an echte Hardware, kein
Datei-Verarbeiter. Ohne Gerät nicht einmal ausführbar.

## 8. Beschaffungsliste

Nichts.

## 9. Aufwandsklasse

**S** (Fundus-Notiz).

## UNGEKLÄRT

* Die LGPL-3.0-Einordnung insgesamt (Matrix-Lücke → Eigentümer;
  betrifft auch künftige Kandidaten, nicht nur dieses Repo).
* Ob FluxBridges dokumentierte GW-Protokoll-Tests
  (`docs/testing.md`, „Tested on real hardware") als unabhängige
  Verhaltens-Bestätigung für unseren GW-Emulator taugen — nicht
  geprüft, und ohne Kennzahl-Bezug nicht priorisiert.
