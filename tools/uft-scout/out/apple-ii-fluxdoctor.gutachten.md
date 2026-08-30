# Gutachten: fredsa/apple-ii-fluxdoctor

> Gemessen 2026-08-30 gegen HEAD `d7aff2243d` (2026-08-28, aktiv).
> Messdatei: `tools/uft-scout/work/apple-ii-fluxdoctor.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692), Andockstelle **Fluss-Dekodierung**.

## Kategorie

**Irrelevant für den Baum** — Werkzeug für Menschen mit
Apple-II-Hardware, nicht für eine Desktop-Anwendung. Eine
Fundus-Notiz für Community-Bench-Anleitungen bleibt.

## 1. Was es ist

Ein **6502-Assembler-Programm** (8 Dateien; die gesamte Logik in
`fluxdoctor.asm`), das **auf echter Apple-II-Hardware läuft** und
Laufwerke in Echtzeit diagnostiziert: Motor-/Seek-Steuerung,
Sektor-Lese-Statistik, Unterscheidung Seek-/Checksum-/
Prologue-Fehler, Sichtbarmachen schwacher Reads (README:1-13).
Verteilt als bootbares DOS-3.3-Abbild (`.DO`). `run.sh` baut das
Abbild im Emulator und **schreibt es per greaseweazle** auf eine
physische Diskette (`run.sh`, Messung `domaenen_treffer.greaseweazle`).

## 2. Abgleich

UFT ist eine Qt6-Desktop-Anwendung; dieses Programm läuft auf der
Zielmaschine der Diskette. Es gibt keinen Codepfad, kein Format und
kein Protokoll, das in den Baum übernehmbar wäre — die
Domänen-Treffer der Messung (GCR, weak, checksum) beschreiben das
Diagnose-Vokabular des ASM-Listings, keine portierbare Bibliothek.

Der einzige Berührungspunkt: **Tier-3-Bench ist in diesem Projekt an
die Community delegiert** (kein Gerät, MF-310). Wer draußen eine
Applesauce-/Apple-Bench-Session fährt, kann mit fluxdoctor das
Laufwerk **vor** dem Capture kalibrieren — das reduziert die Gefahr,
dass eine Bench-Messung Laufwerksfehler als Controllerverhalten
protokolliert. Das ist eine Doku-Zeile für eine
Community-Bench-Anleitung, keine Code-Arbeit.

## 3. Lizenz

`LICENSE.txt` = **Apache-2.0** (aus der Datei; Messung `zone: GELB`).
Konsequenz: kein Port (ohnehin gegenstandslos — 6502-ASM);
Empfehlung/Verweis frei. **Attribution:** apple-ii-fluxdoctor
(Apache-2.0, fredsa); nichts übernommen.

## 4. Bewegte Kennzahl

**Keine.** Auch „Bench-Alter je Controller ↓" wird nicht bewegt —
das Werkzeug macht keine Bench-Session, es verbessert höchstens die
Qualität einer fremden. **Fundus, nicht Auftrag.**

## 5. Einhängepunkt

Falls je eine Community-Bench-Anleitung für Applesauce entsteht
(`docs/CAPABILITIES.md` führt das Bench-Alter): eine Verweiszeile
dort. Kein eigener Eintrag.

## 6. Oracle-Kandidat

Nein — Ausgabe ist ein Bildschirm auf echter Hardware.

## 7. Beschaffungsliste

Nichts.

## 8. Aufwandsklasse

**S** (eine Verweiszeile, falls je gewollt).

## UNGEKLÄRT

* Nichts Entscheidungsrelevantes. (`lizenz_je_datei_geprueft: 0` in
  der Messung heißt nur: keine Quelldateien mit SPDX-Kopf — bei einem
  Ein-Datei-ASM-Projekt ohne Belang; die Wurzel-Lizenz trägt.)
