# Gutachten: bzotto/picturedsk

> Gemessen 2026-08-30 gegen HEAD `43fad3f389` (2021-02-25).
> Messdatei: `tools/uft-scout/work/picturedsk.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4 (MF-692).

## Kategorie

**Daten (Fixture-Erzeuger-Kandidat für WOZ2) — Fundus.**

## 1. Was es ist

Kleines C-Programm (14 Dateien, MIT) von Ben Zotto: „prägt" ein
BMP-Bild in den magnetischen Fluss einer Apple-II-5,25″-Diskette und
schreibt das Ergebnis als **WOZ2** — Magic-Byte `'2'`
(`woz_image.c:70`), Chunks INFO/TMAP/TRKS/WRIT
(`woz_image.c:25-28`), CRC32 über den Dateirest (`woz_image.c:83`).
46 Spuren (`main.c:21`); Spur 0 ist ein **bootfähiger, regulär
GCR-kodierter** Track mit boot1/boot2-Sektoren und Interleave
(`main.c:95-116`), die übrigen Spuren tragen Bild-Fluss. Eigener
6&2-GCR-Encoder in `apple_gcr.c`.

## 2. Abgleich gegen das Inventar

Abfrage zitiert: `"woz": vorhanden: true, tier: "T2",
plugin_liste_vollstaendig: true`. Gegen `inv["korpus"]` (24 Einträge):
**kein einziges WOZ-Abbild liegt** — der WOZ-Leser hat kein fremd
erzeugtes Prüfstück.

Der Wert des Repos ist deshalb nicht sein Zweck (Kunstprojekt),
sondern seine Rolle als **unabhängiger WOZ2-Schreiber in C ohne
Toolchain-Hürde** (Makefile + gcc, kein cargo/dotnet): er kann fremd
erzeugte WOZ2-Dateien liefern, deren Spur-0-Inhalt regulär und deren
Restinhalt bekannt (aus dem BMP ableitbar) ist. Das ist ein
Grenzfall-Fixture erster Güte: gültiger Container, ungewöhnlicher
Inhalt — genau die Klasse, an der Leser still scheitern.

Ehrlich dazu: für eine **Normalpfad**-Hebung von `woz` wäre ein von
Applesauce oder AppleCommander erzeugtes Standard-Abbild die bessere
erste Wahl; picturedsk deckt den Robustheits-, nicht den Normalfall.
Und `woz` steht auf **T2**, nicht T3 — eine Hebung T2→T1b senkt die
geführte T3-Zahl nicht.

## 3. Lizenz

`LICENSE` = **MIT** (aus der Datei; Messung `zone: GRUEN`, 11 Dateien
geprüft, keine abweichenden Unterlizenzen). Konsequenz: Konzept,
Oracle, sogar Port zulässig; Erzeugnisse (WOZ-Dateien aus eigenem
BMP-Input) frei verwendbar und redistributierbar.
**Attribution:** picturedsk, Copyright 2021 Ben Zotto, MIT. Kein Code
übernommen; falls je der 6&2-Encoder als Referenz dient: Quelle+Commit
im Header (samdisk-Muster).

## 4. Bewegte Kennzahl

**Keine der vier** (`woz` ist T2; kein Wandlungspfad, kein Test, kein
Bench). **Fundus, nicht Auftrag** — als billigste bekannte Quelle für
ein erstes fremd erzeugtes WOZ2 im Korpus notiert.

## 5. Einhängepunkt

Erst relevant, wenn `woz`-Verifikationsarbeit ansteht:
`docs/VERIFICATION_TIERS.md` (Zeile `woz`) und das Korpus-Manifest.

## 6. Oracle-Kandidat

Nein — es liest nichts, es schreibt nur. Als **Fixture-Erzeuger**
geführt, nicht als Oracle (Längensemantik-Frage entfällt).

## 7. Beschaffungsliste

Nichts zu beschaffen: Bau aus dem liegenden Klon + ein beliebiges
eigenes BMP genügt. (Ein Standard-WOZ aus Applesauce/AppleCommander
für den Normalpfad wäre eine separate Beschaffung, wenn die
`woz`-Hebung beauftragt wird.)

## 8. Aufwandsklasse

**S** (Bau + ein Erzeugnis + SHA-256 ins Manifest, sobald gewollt).

## UNGEKLÄRT

* Ob der UFT-`woz`-Leser den WRIT-Chunk kennt (nicht geprüft — Teil
  einer etwaigen Hebung, nicht dieses Gutachtens).
* Ob picturedsk INFO-Feld `disk_type`/`boot_sector_format` normkonform
  füllt (Code nicht Zeile für Zeile geprüft; beim Fixture-Einsatz
  gegen die WOZ2-Spec von applesaucefdc.com abzugleichen).
