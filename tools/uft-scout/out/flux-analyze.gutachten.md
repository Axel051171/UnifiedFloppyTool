# Gutachten: kristomu/flux-analyze

> Gemessen 2026-08-30 gegen HEAD `e50d897c05` (2026-04-24).
> Messdatei: `tools/uft-scout/work/flux-analyze.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692), Andockstelle **Fluss-Dekodierung** —
> „was kann unser PLL/Decoder NICHT?"
>
> **Neubesuch-Anlass (Regel 6):** das Repo steht als `bewertet` in
> `data/known_negatives.json` („Gutachten liegt vor: Ordinal/Dewarp/
> Timeline, GPL-3.0 Konzept"). Die neue Frage ist neu seit den Landungen
> von Baustein 2.1 (MF-492), Dewarp (MF-495) und Timeline-Karte
> (MF-501/564): **was bleibt übrig, nachdem die Hauptideen geerntet
> sind?** Zusätzlich führt `docs/MAMMUT_PLAN.md:26` das Repo als
> Spec-Quelle, die „nicht im Baum" liege — der Klon liegt jetzt.

## Kategorie

**Weitgehend geerntet** — die Kernideen sind im Baum gelandet oder
gemessen verworfen. Restwert: **Daten (Fixture-Kandidaten)** und zwei
ungeerntete Konzepte für den Fundus.

## 1. Was es ist

Experimentelles C++-Werkzeug (5 245 Zeilen `csrc/**/*.cc`, `wc -l`) zur
robusten Dekodierung von IBM-MFM-Disketten aus Flusswechseln (altes
FluxEngine-`.flux`-Format als Eingabe, `README.md:33-35`).
Multi-Image-Kombination mehrerer Aufnahmen derselben Diskette
(`csrc/README.md:7-11`), Ausgabe `output.img` + `output.mask`
(Byte-Maske der geretteten Bytes, `csrc/README.md:14-18`). Zuletzt
aktiv 2026-04-24.

## 2. Was UFT davon schon hat — gemessen, nicht erinnert

| flux-analyze | UFT-Baum | Beleg |
|---|---|---|
| `ordinal_search.cc` / `rabin_karp/` | **gebaut, gemessen, entfernt** | `include/uft/flux/uft_flux_sync_search.h:40-53`: Ordinal-Vorfilter brachte 1,4× ohne unterscheidbaren Rotbeweis; ein falscher Vorfilter verwirft still die richtige Stelle |
| Dewarping (`dewarp_test.cc`) | vorhanden (MF-495) | `src/flux/uft_dewarp.c`; `include/uft/flux/uft_dewarp.h:47` zitiert flux-analyze **namentlich** (Überanpassungs-Warnung) |
| `timeline.cc` | Baustein 2.3, Karte fertig | `docs/MAMMUT_PLAN.md:170` (MF-501/FLUX-17, §2.3.1 MF-564) |
| Multi-Image-Kombination | Multiread Voting / Weighted Voting | CLAUDE.md-Recovery-Pipeline; Inventar `"mfm": vorhanden: true` |
| Alt-Doku im Baum | `src/flux/analyze/FLUX_ANALYZE.md` | existiert seit früherem Zyklus, nennt Repo + Vergleichstabelle |

Nicht im Baum gefunden (grep über `src/` + `include/`, 0 Treffer):
**k-Median-Bitzellen-Klassifikation** (`csrc/stats/kmedian/`) und
**EWMA-Suche** (`csrc/ewma_search/`, letzter Commit betrifft genau
sie). Beides GPL-3.0 → nur Konzept/Verhaltens-Spec. Kein offener
Plan-Baustein verlangt sie → **Fundus**.

## 3. Der Restwert: die Schadens-Fixtures in `tracks/`

16 `.flux`- und 6 `.au.bz2`-Dateien (Messung `sprachen`), darunter:

* `low_level_format_with_noise.flux` — jeder Sektor 0x00 oder 0xF6,
  **Inhalt bekannt** (`README.md:47`). Kein fremdes Urheberrecht am
  Inhalt denkbar; als Recovery-Fixture mit Soll-Wert brauchbar.
* `MS_Plus_disk3_warped_track.flux` — verzogene Spur, „FluxEngine
  can't cleanly decode every sector, but both flux-analyze versions
  can" (`README.md:49`). Das ist exakt Prüfmaterial für
  `uft_dewarp` + Sync-Suche — **aber** der Inhalt ist eine
  Microsoft-Plus!-Diskette: Redistribution im eigenen Korpus → Zone
  PRÜFEN, lokale Messnutzung davon getrennt bewerten (Vorlage).
* `RETRY-77-4-*.flux` — Herkunft im Repo nicht erklärt → UNGEKLÄRT.

Hürde, ehrlich: UFT liest das alte FluxEngine-`.flux`-Format nicht
(Inventar-Abfrage zitiert: `"fluxengine": vorhanden: true, treffer:
["flux", "fluxengine"]` — das sind das Decoder-Verzeichnis und der
Qt-Provider, **kein** Dateiformat-Leser; Formatliste
`plugin_liste_vollstaendig: true`, kein `.flux`-Plugin). Nutzung
bräuchte eine Konvertierung (FluxEngine-Binary → SCP) — ein
zusätzlicher Werkzeugschritt mit eigener Fehlerquelle.

## 4. Lizenz

`LICENSE` = **GPL-3.0** (aus der Datei; Messung `lizenz_zone: GELB`,
73 Dateien geprüft, keine abweichenden Unterverzeichnis-Lizenzen).
Konsequenz: **kein Port**, Verhaltens-Spec und Oracle zulässig.
**Attribution:** flux-analyze (GPL-3.0, kristomu) ist im Baum bereits
korrekt als Konzeptquelle zitiert (`uft_dewarp.h:47`); dabei bleibt es.

## 5. Bewegte Kennzahl

**Keine der vier.** Decoder-Robustheit ist keine Release-Kennzahl;
die Fixture-Kandidaten hängen an einer PRÜFEN-Vorlage und einem
Konvertierungsschritt. **Fundus, nicht Auftrag.**

## 6. Einhängepunkt (im Baum auffindbar)

`docs/MAMMUT_PLAN.md:26` (Zeile „9 Fremd-Repos als Spec-Quelle") —
der Klon unter `tools/uft-scout/work/flux-analyze/` löst für dieses
eine Repo die dort benannte Blockade „braucht erst die Quelle".

## 7. Oracle-Kandidat

Schwach. `flux-analyze` (cmake-Bau) liefert `output.img` +
`output.mask` — die Maske ist eine saubere „gerettet ja/nein"-Semantik
auf Byte-Ebene, keine Datei-Ebene (Längensemantik-Pflichtfeld
entfällt, Sektor-/Byte-Abgleich stattdessen). Gegen UFT nur nach
Format-Brücke (.flux↔SCP) sinnvoll; nicht kalibriert, Bau hier nicht
erbracht. Kein Registrierungs-Vorschlag.

## 8. Beschaffungsliste

Für den Fundus-Fall nichts. Falls die Fixtures je gehoben werden:
FluxEngine-Binary (Werkzeug, kein Korpus) für die Konvertierung;
Eigentümer-Vorlage für die MS-Plus!-Spur.

## 9. Aufwandsklasse

**S** (nur Doku/Fundus). Fixture-Weg wäre M (Vorlage + Konvertierung +
Manifest).

## UNGEKLÄRT

* Herkunft und Inhalt der `RETRY-77-4-*`-Spuren.
* Ob k-Median/EWMA gegenüber unserem Dewarp+Sync-Suche-Paar messbar
  etwas hinzufügen — nur per Differenzlauf zu klären, den kein offener
  Baustein derzeit verlangt.
* Rechtslage der lokalen Messnutzung der MS-Plus!-Spuren (Vorlage,
  falls je gewollt).
