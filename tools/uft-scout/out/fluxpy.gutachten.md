# Gutachten: bgottula/fluxpy

> Gemessen 2026-08-30 gegen HEAD `34602b5a7e` (2021-01-29).
> Messdatei: `tools/uft-scout/work/fluxpy.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692), Andockstelle **Fluss-Dekodierung**.

## Kategorie

**Fundus (Rettungs-Fallstudie, MIT)** — ein Ein-Datei-Skript, dessen
Methode UFT im Prinzip schon hat und dessen Zielformat-Verwandtschaft
zu unserem `brother`-Plugin ungeklärt ist.

## 1. Was es ist

Ein einzelnes Python-Skript (671 Zeilen, `wc -l fluxpy.py`) zur
Rettung von **Brother-WP-1**-Textverarbeitungs-Disketten. Liest das
FluxEngine-`.flux`-Format (sqlite3, `fluxpy.py:7`; Bytecode-Dekodierung
nach FluxEngine-Doku, `fluxpy.py:68-77` mit Quellenangabe) und findet
Sektoren per **Kreuzkorrelation** gegen bekannte Muster
(`SECTOR_HEADER`/`SECTOR_DATA`, `fluxpy.py:23-24`;
`find_pattern(..., threshold=0.8)`, `fluxpy.py:122`) statt über eine
PLL — bewusst mit **verkürztem** Suchmuster, weil das lange Muster
durch Drift die Zeitschätzung verschlechtert (Kommentar
`fluxpy.py:18-22`). Geometrie 39 Spuren × 12 Sektoren, feste
Sektorreihenfolge (`fluxpy.py:11-12,25`). README: rettete Disketten
vollständig, die FluxEngines Decoder nur teilweise las (auf falsches
Medium geschriebene Disketten).

## 2. Abgleich gegen den Baum

* **Brother:** Inventar-Abfrage zitiert: `"brother": vorhanden: true,
  treffer: ["brother"], tier: null, plugin_liste_vollstaendig: true`
  — `src/formats/brother/brother.c` existiert (eigene
  5-zu-8-GCR-Tabelle, `brother.c:12-17`), ist aber nicht tier-geführt.
  **Ob WP-1-Disketten dasselbe Format sind wie das, was unser Plugin
  liest, ist UNGEKLÄRT** — fluxpy nennt sein Format „proprietary"
  (README), und ein Muster-Abgleich fluxpy-Sync (0x…FD57/FDDB) gegen
  unsere Tabelle wurde nicht durchgeführt.
* **Methode:** massstabs-/taktunabhängige Mustersuche im Flussstrom
  hat UFT seit Baustein 2.1 (`include/uft/flux/uft_flux_sync_search.h`,
  MF-492) — fluxpys Korrelations-Variante ist dieselbe Familie
  (Matched Filter auf Signalebene statt Integer-Verhältnis-Abgleich).
  Kein belegter Abstand, der einen Differenzlauf rechtfertigt.
* **FluxEngine-`.flux`:** kein UFT-Leser (siehe
  flux-analyze-Gutachten, §3 — dieselbe Feststellung, dieselbe
  Konsequenz).

Ein Brother-WP-1-Flux-Decoder wäre neuer Decoder-Code → EINFRIER-REGEL
erfasst auch den Vorschlag. Regelkonformer Weg, falls je beauftragt:
fluxpy als benannte Verhaltens-Referenz (MIT — sogar Port zulässig,
mit Attribution-Header), Rotbeweis zuerst, Referenz im Header, 1:2
bezahlt.

## 3. Lizenz

`LICENSE` = **MIT** (aus der Datei; Messung `zone: GRUEN`). Konzept,
Oracle, Port zulässig. **Attribution:** fluxpy (MIT, bgottula). Kein
Code übernommen.

## 4. Bewegte Kennzahl

**Keine.** `brother` steht in keiner T3-Zeile (tier: null); kein
Wandlungspfad, kein Test, kein Bench. **Fundus, nicht Auftrag.**

## 5. Einhängepunkt

Keiner nötig. Bei einer etwaigen Brother-Hebung:
`docs/VERIFICATION_PLAN.md` § Einfrier-Regel.

## 6. Oracle-Kandidat

Nein — ohne Brother-WP-1-Flux im Korpus (gegen `inv["korpus"]`
geprüft: 24 Einträge, keiner Brother) gibt es nichts, worüber es
urteilen könnte.

## 7. Beschaffungsliste

Nichts für den Fundus-Fall.

## 8. Aufwandsklasse

**S** (Fundus-Notiz).

## UNGEKLÄRT

* Verhältnis Brother-WP-1-Format (fluxpy) zu Brother-WP/LW-Format
  (unser `brother.c`) — Sync-Muster und GCR-Tabellen nicht
  gegeneinander geprüft.
* Ob unsere Sync-Suche (Baustein 2.1) auf fluxpys Problemklasse
  („falsches Medium, starke Drift") genauso trüge — nur per
  Differenzlauf klärbar, den keine Kennzahl verlangt.
