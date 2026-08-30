# Gutachten: GeoKM/hardsector_tool

> Gemessen 2026-08-29 gegen HEAD `99a0898` (2025-12-30).
> Messdatei: `tools/uft-scout/work/hardsector_tool.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4, Andockstelle: unser `hardsector`-Plugin (T3).

## Kategorie

**Verbesserung (Verhaltens-Referenz) — und beim Abgleich fiel ein
Verdachtsfall im eigenen Baum an**, dieselbe Klasse wie FMT-2/3:
eine Geometrie-Tabelle, deren Referenz das Gegenteil dessen belegt,
was sie behauptet.

## 1. Was es ist

Forensisches Python-Werkzeug (8 139 Zeilen `src/hardsector_tool/*.py`,
`wc -l`) zur Rekonstruktion **hartsektorierter** Disketten aus
SCP-Flux: Loch-Gruppierung (`hardsector.py:353`), FM-Decode (`fm.py`,
748 Z.), QC (`qc.py`, 1 611 Z.), Wang-2200-Katalog (`wang.py`,
1 445 Z.). 16 Testdateien; Flux-Fixtures (`tests/fixtures/README.md`)
heißen `ACMS*-HS32.scp` — **HS32 = 32 Hard-Sektoren**. Fokus-Geometrie
laut README: 77 Spuren × **16 Hard-Sektoren** × 256 B.

## 2. Der Befund im eigenen Baum

`src/formats/hardsector/uft_hardsector.c:1-8` nennt als Referenz „IBM
3740/System 34 specs". `include/uft/formats/uft_hardsector.h:30-46`
definiert die 8-Zoll-„Hard-Sektor"-Typen mit **26 Sektoren × 128/256 B**
und `:61-63` etikettiert sie „IBM 3740 compatible" / „IBM System/34
compatible". `hardsector_detect_type()`
(`src/formats/hardsector/uft_hardsector.c:77-90`) erkennt rein an der
Dateigröße (256 256 / 512 512 / 1 025 024 B).

Nur: **IBM 3740 und System/34 sind weichsektorierte Standards**, und
8-Zoll-Hartsektor-Medien haben **32 Sektoren (33 Löcher)**, nicht 26.
Zwei unabhängige Quellen außerhalb des eigenen Baums:

1. retrocmp.de / retrotechnology.com / Wikipedia „Hard sectoring"
   (Web-Recherche 2026-08-29): 8″-Hartsektor = 32 Sektoren, 33 Löcher;
   IBM 3740 = weichsektoriert.
2. hardsector_tool selbst: Fixture-Namen `*-HS32.scp`
   (`tests/fixtures/README.md`) und die 16-Sektor-Fokus-Geometrie —
   keine 26er-Geometrie im ganzen Repo (`grep -rn "26" src/` ohne
   Sektor-Treffer).

Folge, konkret: eine echte Wang-artige Hartsektor-Datei
(77×16×256 = 315 392 B) läuft in „Cannot detect hard-sector geometry"
(`uft_hardsector.c:150`), während jedes gewöhnliche
weichsektorierte 3740-Abbild (256 256 B) als „Hard-Sektor" bejaht
wird. Beides prüfbar ohne Hardware.

## 3. Rotbeweis-Skizze (Stufe 4)

1. 256 256-Byte-Nullbild → `uft_hardsector_probe()` → erwartet:
   **bejaht als HS_TYPE_8IN_SSSD** → der Test dokumentiert die
   Falschaussage (rot gegen die korrigierte Erwartung).
2. 315 392-Byte-Bild (77×16×256) → erwartet: heute „Cannot detect".
3. Danach Korrektur der Tabelle **nur** gegen benannte Quellen
   (Hartsektor-Standards; hardsector_tool als Verhaltens-Referenz für
   die 16er/32er-Geometrien) — Einfrier-Regel-konform, weil Bugfix an
   Bestehendem plus Verifikation, kein neues Plugin.

## 4. Lizenz

`LICENSE` = **Apache-2.0** (aus der Datei). Zone **GELB**: kein Port;
Verhaltens-Spec und Oracle-Nutzung zulässig. **Attribution:**
hardsector_tool (Apache-2.0, GeoKM) als Verhaltens-Referenz; kein Code
übernommen, keiner übernehmbar.

## 5. Bewegte Kennzahl

**Ungeprüfte Formate (T3) ↓.** Inventar-Abfrage zitiert:
`"hardsector": vorhanden: true, tier: "T3", treffer: ["hardsector",
"uft_hardsector"], plugin_liste_vollstaendig: true`. Der Weg: Rotbeweis
+ Korrektur + belegter Test hebt `hardsector` aus T3 — oder stellt es
ehrlich als „Geometrie-Katalog ohne Hartsektor-Semantik" dar.

## 6. Einhängepunkt

`docs/VERIFICATION_PLAN.md` § Einfrier-Regel (Hebungs-Arbeit);
`docs/VERIFICATION_TIERS.md` (T3-Zeile `hardsector`).

## 7. Oracle-Kandidat

`python -m hardsector_tool qc-capture / reconstruct` (Apache-2.0,
Ausführung unbedenklich) — entscheidet, was in einem
Hartsektor-SCP-Flux wirklich steckt. Braucht echte Captures (unten).
Längensemantik: gibt Sektor-Datenbanken aus, keine FS-Dateien —
Kalibrier-Pflichtfeld entfällt, stattdessen Sektorgrößen-Abgleich.

## 8. Beschaffungsliste

Gegen `inv["korpus"]` (24 Einträge): **kein** Hartsektor-Abbild liegt.
Benötigt: mindestens ein hartsektoriertes SCP-Flux (die
ACMS-Captures der hardsector_tool-Tests sind nicht im Repo;
Beschaffung über Community/bitsavers — Engpass, ehrlich benannt).
Für den reinen Rotbeweis aus §3 braucht es **nichts**: die zwei
synthetischen Größen genügen.

## 9. Aufwandsklasse

**S** für den Rotbeweis (zwei Dateien, ein Test), **M** für die
Tabellen-Korrektur mit Quellen, **L** erst bei echter
Flux-Verifikation (Beschaffung).

## UNGEKLÄRT

* Ob irgendein realer Hersteller je 26×128-Hartsektor-8″ nutzte — nicht
  gefunden, aber Abwesenheit ist kein Beweis; die Korrektur braucht
  die Standards-Recherche in Stufe 4.
* Ob die 5,25″-Zeilen (10/16 Sektoren, `uft_hardsector.h:49-57`)
  stimmen — sie sehen plausibel aus (NorthStar/Wang-Klasse), sind aber
  genauso unbelegt wie der Rest der T3-Datei.
