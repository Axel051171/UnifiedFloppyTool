# Gutachten: dbalsom/fluxfox

> Gemessen 2026-08-29 gegen HEAD `1d72ff1` (2026-08-11).
> Messdatei: `tools/uft-scout/work/fluxfox.messung.json`.
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `bd2d5616`).
> Auftrag: Block 4, Andockstellen **FMT-15** und **Fluss-Dekodierung/GUI-7**.

## Kategorie

**Verbesserung (benannte Referenzen für zwei bestehende offene Punkte)
+ Oracle-Kandidat.** Drei getrennte Funde in einem Repo.

## 1. Was es ist

Rust-Bibliothek für Disk-Images mit PC-Fokus (aktiv, 421 Dateien,
zuletzt 2026-08-11), plus CLI `fftool`
(`crates/fluxfox_cli/src/{info,dump,convert,find,create}`). Parser:
86F, HFE, IMD, **IPF (eigene MIT-Implementierung)**, KryoFlux, MFI,
HxC-MFM, PCE PFI/PRI/PSI, raw, SCP, Transcopy TC, TD0, WOZ/MOOF/A2R
(`src/file_parsers/`).

## 2. Fund A — FMT-15: die Bootsektor-Plausibilität, die dem IMG-Weg fehlt

FMT-15 (MF-691) nennt als Schließweg 1 „Bootsektor-Plausibilität für
IMG". fluxfox hat genau diese Prüfung, klein und belegbar:
`src/boot_sector/bpb.rs:61-86` `BiosParameterBlock2::is_valid()` —
sechs Bereichsprüfungen (bytes_per_sector 128–4096,
sectors_per_cluster ≤ 2, FATs 1–2, root_entries 0x70–0xF0,
total_sectors 320–5760, sectors_per_fat 1–9) plus
Media-Descriptor-Zuordnung (`bpb.rs:112-118`) und
Größen-Zuordnung (`bpb.rs:96-103`). Boot-Signatur ausdrücklich als
**nicht verlässlich** dokumentiert (`bootsector.rs:37-39`: DOS 1.0
setzte 0x55AA nicht) — dieselbe Vorsicht, die FMT-15 verlangt.
Zweite, unabhängige Referenz: DiskImageTool (eigenes Gutachten,
11-Kriterien-`IsValid()`).

## 3. Fund B — GUI-7: die benannte Referenz für ein `tolerance`-Comeback

`include/uft/flux/uft_flux_decoder.h:284-296` verlangt: wer `tolerance`
wieder einführt, baut „zuerst die Lesestelle gegen eine benannte
Referenz". fluxfox' PLL ist eine solche: `src/flux/pll.rs:40`
`MAX_CLOCK_ADJUST = 0.20`, `:98-116` `max_adjust`/`clock_gain = 0.05`/
`phase_gain = 0.65`, Fenstergrenzen `:445-446`. MIT — als Konzept UND
als Port zulässig (mit Attribution-Header, samdisk-Muster).

## 4. Fund C — Oracle-Kandidat `fftool` für T3-Formate

UFT-`86f` steht auf **T3** (`docs/VERIFICATION_TIERS.md:58`), UFT-`ipf`
auf T3. Inventar-Abfrage zitiert: `"86f": vorhanden: true, tier: "T3"`;
`"ipf": vorhanden: true, tier: "T3"`; `"td0": T2`, `"mfi": T2`,
`"imd": T2`. Ein gebautes `fftool` (MIT) kann als **unabhängige
Zweithand** für 86F-Geometrie/Inhalt dienen und Hebungs-Arbeit an 86f
tragen; für IPF ist es neben ipf-flux die **zweite** blob-freie
Implementierung (Relevanz für LIZ-2, siehe ipf-flux-Gutachten).

Grenzen, ehrlich: auf dieser Maschine liegt **kein cargo** (`which
cargo` leer) — der Bau ist nicht erbracht. Nach ORACLES.md gilt „kein
Oracle auf Zusicherung": der Vorschlag unten enthält Bau + Kalibrierung
als Bedingung. Kalibrierdatei nach Hausmaß: die 127-Byte-Markerdatei
(Korpus-D64/ADF); fftool ist primär Container- nicht Datei-Ebene —
falls es keine Dateiinhalte herausgibt, entfällt die
Längensemantik-Frage und es bleibt Spur-/Sektor-Oracle.

## 5. Lizenz

`LICENSE` = **MIT** (aus der Datei; `vermessen.py`: alle 8
Unterverzeichnis-LICENSE ebenfalls MIT — Vendoring geprüft). Zone
**GRÜN**: Konzept, Oracle und Port zulässig. Achtung bei
`fluxfox_fat`: `Cargo.toml:114-117` bezieht einen **Fork von
rust-fatfs** als Abhängigkeit — bei einem etwaigen Port von
FAT-Verhalten dessen Lizenz separat prüfen (nicht vermessen, liegt
nicht im Repo).

**Attribution:** fluxfox, Copyright 2024-2025 Daniel Balsom, MIT.
Kein Code übernommen; bei einem späteren Port: Quelle+Commit im Header.

## 6. Bewegte Kennzahl

* Fund C: **ungeprüfte Formate (T3) ↓** — 86f ist der belegbare erste
  Kandidat (Methode: Abgleich Parserliste `src/file_parsers/` gegen
  T3-Zeilen in `docs/VERIFICATION_TIERS.md`; Treffer = gleicher
  Formatname; exakt belegt: 86f, ipf).
* Fund A/B: Zulieferung an bestehende Punkte FMT-15 / GUI-7 (FMT-15
  führt selbst „Kennzahl: keine").

## 7. Einhängepunkte (im Baum auffindbar)

* `docs/OPEN_ITEMS.md` § FMT-15 (MF-691), Schließweg 1.
* `include/uft/flux/uft_flux_decoder.h:284-296` (GUI-7-Verweis).
* `docs/ORACLES.md` § „Registrierte Oracles" + `tests/differential/oracles.py`.

## 8. Beschaffungsliste

* Rust-Toolchain (rustup) + `cargo build -p fluxfox_cli` — Werkzeug,
  kein Korpus.
* Für die 86f-Hebung: mindestens ein fremd erzeugtes 86F-Abbild
  (86Box-Export); liegt laut `inv["korpus"]` (24 Einträge) **nicht**.

## 9. Aufwandsklasse

**M** (Bau + Kalibrierung + Registry-Eintrag + ein erster
86f-Differenzlauf). Fund A/B je **S** als Spec-Zulieferung.

## Differenzlauf-Plan (für die T3-Hebung 86f)

Beide Binaries: `fftool dump/info` vs. UFT-86f-Leser (über die
Testtreiber, GUI-only beachten — kein UFT-CLI). Gemeinsamer Korpus: das
zu beschaffende 86Box-Abbild + ggf. `fftool create`-Erzeugnisse
(Eignung als Fixture-Erzeuger UNGEKLÄRT). Metrik: Geometrie
(Spuren/Köpfe/Sektoren), Sektor-Hashes. Toleranzliste: leer starten,
jede Abweichung einzeln urteilen.

## UNGEKLÄRT

* Ob `fftool create` 86F schreiben kann (nur `src`-Struktur gesehen,
  nicht ausgeführt).
* Lizenz des `fluxfox_fat`-Forks (Abhängigkeit, nicht im Repo).
* Ob fftool Dateiinhalte (FAT-Ebene) ausgibt → Längensemantik-Pflichtfeld.
