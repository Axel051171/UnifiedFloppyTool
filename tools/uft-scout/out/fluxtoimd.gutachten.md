# Gutachten: brouhaha/fluxtoimd

> Gemessen 2026-08-30 gegen HEAD `ea074ada76` (2020-04-03).
> Messdatei: `tools/uft-scout/work/fluxtoimd.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692), Andockstelle **Fluss-Dekodierung**.

## Kategorie

**Fundus (M2FM-Verhaltensreferenz)** — plus ein **Werkzeug-Befund am
eigenen Vermesser**, der eine falsche ROT-Zone produziert hat.

## 0. Vorab: der Vermesser-Befund

`vermessen.py` meldet für dieses Repo `lizenz_zone: ROT` mit dem
Hinweis „keine LICENSE/COPYING-Datei gefunden". Das ist ein
**Dateinamens-Artefakt**: die Lizenz liegt als `gpl-3.0.txt` im
Wurzelverzeichnis (Zeile 1-2: „GNU GENERAL PUBLIC LICENSE / Version 3,
29 June 2007", Volltext), und **jede** Quelldatei trägt den
GPLv3-only-Kopf (`fluxtoimd.py:2-16`, `modulation.py:2-17`:
„…under the terms of **version 3** of the GNU General Public
License…"). Ein falsches ROT unterdrückt Funde in dieselbe stille
Richtung wie ein falsches „vorhanden" — der teuerste Fehler dieses
Agenten (AGENT.md Regel 4). Der Vermesser sollte auch
`gpl-*.txt`/`LICENSE.*`-Namen erkennen; Notiz an den Eigentümer, kein
Release-Thema.

## 1. Was es ist

Python-3-Werkzeug von Eric Smith (11 Dateien): liest
**DiscFerret-DFI** (`dfi.py:25/77`) und **KryoFlux-Stream-ZIPs**
(`kfsf.py:216`), demoduliert und schreibt **IMD** (`imagedisk.py:30`).
Software-ADPLL (`adpll.py:22`). Vier Modulationen (`modulation.py`):
FM (`:45`), MFM (`:82`), **IntelM2FM** (`:138`, Intel MDS 800/SBC 202,
README:27-29) und **HPM2FM** (`:188`, HP 7902/9885/9895, README:31-32;
HP-Sonderfall: ID-Feld nur 2 Bytes, `fluxtoimd.py:99`; keine
Indexmarken, `fluxtoimd.py:194-195`).

## 2. Abgleich gegen den Baum

* **M2FM kennt UFT nur als Namen, nicht als Fähigkeit.** Gemessen:
  `M2FM` erscheint als Enum/String (`src/core/uft_ir_format.c:1251,
  1605`, `include/uft/core/uft_encoding.h` u. a.), aber
  `m2fm_decode`/`decode_m2fm` hat **0 Treffer** in `src/` und
  `include/`. Es gibt keinen M2FM-Decoder.
* **DFI ist als Plugin-Verzeichnis vorhanden.** Inventar-Abfrage
  zitiert: `"dfi": vorhanden: true, treffer: ["dfi"], tier: null,
  plugin_liste_vollstaendig: true` — kein Fund, sondern Bestand.
* `"imd": vorhanden: true, tier: "T2"`.

Der einzige echte Abstand ist also der **M2FM-Decode-Pfad**
(Intel- und HP-Variante). Das wäre neuer Decoder-Code: **von der
EINFRIER-REGEL erfasst, auch als Vorschlag.** Der regelkonforme Weg,
falls Stufe 4 ihn je geht: benannte Referenzen fluxtoimd
(Verhaltens-Spec, GPL-3.0 — kein Port) + Intel-SBC-202-/HP-Doku,
Rotbeweis zuerst, Referenz im Header, und nach der Rückstands-Regel
1:2 bezahlt. Bis dahin: Fundus.

## 3. Lizenz

**GPL-3.0-only** (aus `gpl-3.0.txt` + Datei-Köpfen, siehe §0). Zone
**GELB**: kein Port; Verhaltens-Spec und Oracle-Nutzung zulässig.
**Attribution:** fluxtoimd, Copyright 2016 Eric Smith, GPL-3.0. Kein
Code übernommen, keiner übernehmbar.

## 4. Bewegte Kennzahl

**Keine der vier.** M2FM ist eine fehlende Fähigkeit, kein
T3-Format; kein Wandlungspfad, kein Test, kein Bench. **Fundus, nicht
Auftrag.**

## 5. Einhängepunkt

Keiner erforderlich. Falls je ein M2FM-Bestand (Intel-MDS-/HP-Flux)
auftaucht, wäre der Anker `docs/VERIFICATION_PLAN.md`
§ Einfrier-Regel.

## 6. Oracle-Kandidat

Bedingt: Python, läuft ohne Bau — als **Flux→IMD-Zweitmeinung** für
FM/MFM-Bestände aus KryoFlux-Streams denkbar. Längensemantik: IMD ist
Sektor-, nicht Dateiebene → statt Längensemantik gilt
Sektorgrößen-/CRC-Abgleich. Nicht kalibriert; gegen `inv["korpus"]`
(24 Einträge) liegt **kein** KryoFlux-Stream — ohne Beschaffung ist
die Registrierung sinnlos. Kein Vorschlag.

## 7. Beschaffungsliste

Für den Fundus-Fall nichts. (Ein Intel-MDS- oder HP-M2FM-Flux wäre die
Vorbedingung für alles Weitere; bitsavers/Community — Engpass.)

## 8. Aufwandsklasse

**S** (Fundus-Notiz + Vermesser-Korrektur ist ein Ein-Zeilen-Fix im
Scout-Werkzeugkasten, nicht im Produktbaum).

## UNGEKLÄRT

* Ob die vier Modulationsklassen HP-M2FM vollständig abdecken (nur
  Struktur gelesen, nicht ausgeführt).
* Ob unser `dfi`-Plugin dasselbe DiscFerret-DFI liest wie `dfi.py`
  (tier: null, ungeprüfte Klasse — nicht verglichen).
