<!-- uebernommen: MF-670 -->
<!-- Aufgenommen als erste Fassung von docs/SETTINGS_ROADMAP.md; die
     offene Arbeit daraus steht als SET-1 in docs/OPEN_ITEMS.md. Vier
     der Traeger-Behauptungen (pll_lock_threshold, weak_bit_cv,
     noflux_threshold, detect_weak_bits) wurden vor der Uebernahme an
     den genannten Zeilen nachgemessen, nicht uebernommen. -->
# Gutachten: Triage der 34 Reglernamen aus `src/advanceddialogs.h`

**Auftrag:** Vorarbeit für `docs/SETTINGS_ROADMAP.md` (Datei existiert noch
nicht — gemessen: `ls docs/SETTINGS_ROADMAP.md` → fehlt). Dreiweg-Urteil je
Name: (a) Synonym eines vorhandenen Mechanismus, (b) baubar mit zwei
unabhängigen Quellen, (c) Streichung.
**Datum:** 2026-08-29 · **Baum-Stand:** HEAD `8d701d84` (aus Inventar-Build)
**Nicht in der Triage:** `revsToRead`/`revsToUse`/`mergeRevs`/`mergeMode`
(vom Auftraggeber bereits auf `use_multiple_revs`/`synthetic_revolutions`
umgehängt) sowie das tote PLL-Dialog-Feld `tolerance`.

**Kategorie:** Verbesserung (Settings-Konsolidierung) · **Lizenzzone:** siehe
§Lizenz · **Bewegte Kennzahl:** keine der vier (Begründung in
§OPEN_ITEMS) · **Einhängepunkt:** `docs/OPEN_ITEMS.md:3058` (dort wird die
Löschung der drei Advanced-Dialoge geführt); künftig
`docs/SETTINGS_ROADMAP.md`.

## Methode (gilt für jede Zahl in diesem Gutachten)

* „Lesestelle" = Treffer von `grep -n` auf den Feldnamen in der genannten
  Datei, von Hand als Lese- (nicht Schreib-)Zugriff eingeordnet.
* „0 Aufrufer" = `grep -rln <Funktionspräfix> src tests` (alle `*.c*`),
  definierende Datei ausgenommen; bei `uft_advanced_*`/`uft_kalman_*`/
  `uft_god_mode*` zusätzlich Kettenverfolgung (siehe §Erreichbarkeit).
* Aussagen des Auftraggebers (lebende/tote Felder in
  `flux_decoder_options_t`, `clock_tolerance_pct`) wurden **nicht**
  nachgemessen — sie sind als Vorgabe übernommen und so gekennzeichnet.
* Der gelöschte fdc_bitstream-Baum wurde über
  `git show 70d2e0e7^:<pfad>` gelesen (Löschung MF-626).

## Inventar-Abfrage (zitiert, `tools/uft-scout/work/inv.json`)

```
"weak bits":  { "vorhanden": false, "abgedeckt": false, ... }
"pll":        { "vorhanden": false, "abgedeckt": false, ... }
"half track": { "vorhanden": false, "abgedeckt": false, ... }
"gcr":        { "vorhanden": true,  "treffer": ["gcr", "uft_c64_gcr"] }
"sync":       { "vorhanden": true,  "treffer": ["uft_amiga_syncs"] }
```

`abgedeckt: false` heißt „Index kennt den Begriff nicht", nicht „fehlt"
(AGENT.md Regel 4, SCOUT-1). Alle Fähigkeitsfragen wurden deshalb von Hand
im Baum geklärt; die Belege stehen bei jedem Urteil.

## Erreichbarkeits-Vorklärung (bestimmt mehrere Urteile)

Drei Kandidaten-Träger sind **Bestand, nicht Fähigkeit** — sie zählen für
kein (a)-Urteil:

* **Kalman-PLL** (`src/algorithms/uft_kalman_pll.c`, hat
  `weak_bit_threshold`, gelesen :219): einzige Nenner sind
  `uft_god_mode_api.c` und `uft_kalman_pll_v2.c`. God-Mode-API wird nur aus
  `uft_advanced_mode.c`/`uft_smart_open.c` genannt; `uft_smart_open()` hat
  keinen Aufrufer (`docs/KNOWN_ISSUES.md:1746`, PRINC-2/MF-444), und
  `uft_advanced_*` wird außerhalb dieses Zirkels nur von
  `uft_god_mode_api.c` und `uft_v3_bridge.c` genannt — dessen eigener
  Kopf sagt „Not currently reachable" (`src/formats/uft_v3_bridge.c:28`).
  Die Kette ist zirkulär und endet nirgends im Produktionspfad.
* **`uft_multi_rev_fusion.c`** (hat `weak_threshold` :43, gelesen :130):
  einziger Nenner ist `uft_smart_open.c`, und dort wird nur ein Flag
  gesetzt (`:137`), nie die Fusion gerufen.
* Dagegen **lebend und erreichbar**: die OTDR-Pipeline
  (`src/analysis/otdr/floppy_otdr.c`, GUI-Aufrufer
  `src/gui/uft_otdr_panel.cpp`) und die Multiread-Pipeline
  (`src/recovery/uft_multiread_pipeline.c`, Aufrufer u. a.
  `src/core/uft_format_verify.c` + 4 Tests).

## Triage

### (a) Synonym — 19 Namen, null neue Leitungen

| Regler | Vorhandener Mechanismus | Feld (Definition) | Lesestelle |
|---|---|---|---|
| `bitcellPeriod` (µs) | Zellendauer-Vorgabe | `flux_decoder_options_t.bitcell_ns`, `include/uft/flux/uft_flux_decoder.h:259` | vom Auftraggeber als lebend gemessen (2–7 Lesestellen in `src/flux/uft_flux_decoder.c`) |
| `bitCell` (µs) | identisch mit `bitcellPeriod` | dito | dito |
| `pllBandwidth` | PLL-Frequenz-Gain | `flux_decoder_options_t.pll_gain`, `uft_flux_decoder.h:281` | verdrahtet `src/flux/uft_flux_decoder.c:687/788/966/1158/1536` (`pll.freq_gain = opts->pll_gain`), gelesen `:376` |
| `pllLockThreshold` (%) | OTDR-Lock-Klassifikation | `otdr_config.pll_lock_threshold`, `include/uft/analysis/floppy_otdr.h:354` (Default 10 %) | `src/analysis/otdr/floppy_otdr.c:578` (`deviation_pct < cfg->pll_lock_threshold`) |
| `lockThreshold` (%) | identisch mit `pllLockThreshold` | dito | dito |
| `weakBitThreshold` (%) | Weak-Bit-Schwelle der OTDR-Mehrfachlesung | `otdr_config.weak_bit_cv`, `floppy_otdr.h:368` (Einheit: Variationskoeffizient, **nicht** %) | `floppy_otdr.c:705` (`cv > cfg->weak_bit_cv`) |
| `weakBitDetection` | Weak-Bit-Erkennung, zweifach lebend | `otdr_config.detect_weak_bits` (`floppy_otdr.c:94`) **und** Multiread-Pipeline (`src/recovery/uft_multiread_pipeline.c:95`) | `floppy_otdr.c:867`; Mehrheits-Uneinigkeit `uft_multiread_pipeline.c:171-173`. In der GUI bereits schaltbar: `src/gui/uft_otdr_panel.cpp:623/627` |
| `noFluxThreshold` (µs) | No-Flux-Erkennung | `otdr_config.noflux_threshold`, `floppy_otdr.h:367` (Einheit: Vielfaches der Nennperiode, **nicht** µs) | `floppy_otdr.c:514/533/594` |
| `useIndex` | Index-Impulse werden automatisch genutzt, wenn vorhanden und plausibel (MF-475-Tor) | `flux_raw_data_t.index_times` | `src/flux/uft_flux_decoder.c:263-287` |
| `softIndex` | synthetische Index-Marken aus `revolution_ns` (MF-475) + `synthetic_revolutions` | Erzeugung `uft_flux_decoder.c:220-228`; `synthetic_revolutions` (Auftraggeber: 6 Lesestellen) | `uft_flux_decoder.c:263ff` |
| `gcrVariant` | Encoding-Auswahl des Flux-Decoders | `flux_decoder_options_t.encoding` mit `FLUX_ENC_GCR_C64`/`FLUX_ENC_GCR_APPLE` (`uft_flux_decoder.h:71-72`) | Dispatch `uft_flux_decoder.c:1766` + switch darunter. Einschränkungen: „Standard-GCR" existiert nicht; „Victor" fehlt im Flux-Decoder (nur Sektor-Plugin `src/formats/victor9k/uft_victor9k.c:189`) — ein Victor-GCR-Decoder wäre EINFRIER-pflichtig, siehe Fundus |
| `rawNibble` | Rohbitstrom behalten | `flux_decoder_options_t.keep_raw_bits`, `uft_flux_decoder.h:282` | `uft_flux_decoder.c:734/999/1192/1546`. Achtung: das Enum `FLUX_ENC_RAW` (`uft_flux_decoder.h:79`) ist **tot** — der Dispatch beantwortet es mit `FLUX_ERR_INVALID` (default-Zweig nach `:1766`) |
| `preserveGaps` | im Rohbitstrom enthalten | `keep_raw_bits` (s. o.); Bitstream-Formate (G64) erhalten Gaps ohnehin | `uft_flux_decoder.c:734` |
| `preserveSync` | dito — Sync-Läufe stehen im Rohbitstrom | `keep_raw_bits` | `uft_flux_decoder.c:734` |
| `syncPattern` (hex) | Sync-Muster-Liste | `flux_decoder_options_t.sync_patterns`/`sync_count`, `uft_flux_decoder.h:312-313` (MF-453, Amiga-Katalog `uft_amiga_syncs.h`) | vom Auftraggeber als lebend gemessen |
| `ignoreBadGCR` | GCR-Decode ist bereits tolerant und zählt Fehler statt abzubrechen | `gcr_decode(..., size_t *error_count)`, `src/formats/c64/uft_gcr_ops.c:149` | Fehlerzählung `uft_gcr_ops.c:178`; Aufrufer: `uft_d64_g64.c`, `uft_d64_parser_v3.c`, `uft_g64.c`, `uft_g64_parser_v2.c`, `uft_g64_parser_v3.c` (grep) |
| `includeHalfTracks` | G64-Halbspur-Raster | `G64_MAX_TRACKS 84` + `has_half_tracks`, `src/formats/g64/uft_g64.c:41,110` | `track_to_g64_index(track, half_track)`, `uft_g64.c:156-157` |
| `includeQuarterTracks` | WOZ/A2R-Viertelspur-Karte | `quarter_tracks`, `src/formats/apple/uft_woz.c:375` | `uft_woz.c:376-378` (Zählung aus TMAP), `:609` (160 Viertelspur-Slots beim Schreiben); FLUX-Chunk-Erhalt `:327` |
| `trackStep` (1/2/4) | Doppelschritt ist Erfassungs-, nicht Decode-Konfiguration — und existiert dort | `geometry.double_step` (setzen z. B. `src/formats/hfe/uft_hfe.c:510`, `src/formats/img/uft_img.c:100`); HAL-Setter `uft_kf_set_double_step`, `src/hal/uft_kryoflux_dtc.c:693` | `uft_kryoflux_dtc.c:540` |

Hinweis zu den drei OTDR-Schwellen (`pll_lock_threshold`, `weak_bit_cv`,
`noflux_threshold`): die Felder leben und werden gelesen, aber ob das
OTDR-Panel sie heute numerisch setzbar macht, ist **nicht geprüft** —
belegt ist nur der `detect_weak_bits`-Schalter
(`uft_otdr_panel.cpp:623/627`). Sie sichtbar zu machen wäre reine
GUI-Verdrahtung lebender Felder, kein Format-/Decoder-Code.

**Einheiten-Warnung für die Roadmap:** Die Dialoge behaupten `%` und `µs`,
die lebenden Felder rechnen in Variationskoeffizient (`weak_bit_cv`) und
Perioden-Vielfachen (`noflux_threshold`). Wer die Dialog-Einheiten in die
Roadmap übernimmt, baut die nächste stille Falschaussage.

### (b) Baubar — 0 Namen

Kein Name erreicht die Schwelle: zwei unabhängige Quellen **und** ein
Oracle **und** eine bewegte Kennzahl. Die nächstliegenden Kandidaten sind
unten als Streichung-mit-Fundus-Notiz geführt (`filterType`,
`adaptiveGain`), weil je nur **eine** Quelle vorliegt — „unbelegt" ist
nach Auftrag nicht „baubar".

### (c) Streichung — 15 Namen

| Regler | Grund (ein Satz) |
|---|---|
| `pllFrequency` (Hz) | Kehrwert-Dublette von `bitcell_ns` — zwei Regler für eine Größe sind gebaute Zahlendrift. |
| `pllPhase` (−1…1) | Ein Anfangsphasen-Offset ist wirkungslos: die PLL rastet in wenigen Zellen ein und begrenzt/regelt die Phase selbst (`uft_flux_decoder.c:363-373`); der benachbarte `phase_gain` ist bewusst fest 0.5 (`:68`, gelesen `:363-364`) und wäre ein **neuer** Regler, kein Synonym. |
| `clockTolerance` (%) | Beide bekannten Träger sind tot (Auftraggeber: `opts->tolerance` 0 Lesestellen, `clock_tolerance_pct` 0 Lesestellen); der lebende Weg misst die Zelle statt sie zu tolerieren (`measured_cell_ns`, MF-492) und der Adaptive-Decode re-dekodiert ±33 % automatisch. |
| `indexOffset` (µs) | Index-Lagen sind Daten der Aufnahme (z. B. 86F-Header, gelesen `src/formats/pc/uft_86f.c:256`); ein Verschiebe-Regler erfindet Geometrie — Konflikt mit „Keine erfundenen Daten"; keine zwei externen Quellen gefunden. |
| `clockRate` (MHz) | Die Abtastrate steht in der Flux-Datei (`flux_raw_data_t.sample_rate`), die Datenrate ist 1/`bitcell_ns` — beides schon vorhanden, ein Override widerspräche der Datei. |
| `filterType` (Simple/PID/Adaptive) | Im Baum lebt genau **eine** PLL (`flux_pll`); Kalman-PLL ist unerreichbar (§Erreichbarkeit); die PID-/Adaptiv-Familie stammt aus dem entfernten fdc_bitstream-Vendoring (`vfo_simple/pid/pid2/pid3/experimental`, `git show 70d2e0e7^`) — **eine** Quelle, keine Kennzahl bewegt, neuer Decoder-Mechanismus unter EINFRIER → Fundus-Notiz, kein Vorschlag. |
| `historyDepth` | Nur in `vfo_pid3` des gelöschten Vendorings belegt (11 Treffer `hist/avg` in `70d2e0e7^`); keine zweite Quelle — unbelegt. |
| `adaptiveGain` | fdc_bitstream schaltet Gain hoch im Sync-Feld, niedrig bei Daten (`fdc_bitstream.h:105-127` in `70d2e0e7^`) — reales Konzept, aber **eine** Quelle, im Baum kein Träger (Gains fest, Kalman unerreichbar) → Fundus-Notiz. |
| `unlockThreshold` (%) | Hysterese-Zweitschwelle ohne Konsumenten: die OTDR-Lock-Klassifikation ist zustandslos pro Segment (`floppy_otdr.c:578`); keine zwei Quellen für eine Hysterese-Semantik. |
| `weakBitWindow` (bits) | Die lebenden Weak-Bit-Wege arbeiten pro Bitzelle über ≥2 Umdrehungen (`floppy_otdr.c:867`, `uft_multiread_pipeline.c:171-173`) und brauchen kein Fenster; extern unbelegt. |
| `decodeToSectors` | Komplement von `rawNibble` und der Normalzweck des Decoders — ein Häkchen dafür sagt nichts. |
| `syncLength` (bits) | Sync-Lauflängen behandelt der Decoder selbst und tolerant (`mfm_skip_sync_run`, `uft_flux_decoder.c:460ff`, „tolerates one or two"); ein Längenregler hat keinen Konsumenten. |
| `autoDetectSync` | Auto-Erkennung existiert als `FLUX_ENC_AUTO` → `flux_detect_encoding` (`uft_flux_decoder.c:1764-1766`) plus Amiga-Sync-Katalog; freies Sync-Scanning wäre ein neuer Decoder-Mechanismus (EINFRIER, Oracle-first) und bewegt keine Kennzahl. |
| `fillBadSectors` | Als globaler Decode-Schalter Konflikt mit „Keine erfundenen Daten"; wo Füllen Format-Semantik ist, existiert es bereits lokal (`src/formats/hardsector/uft_hardsector.c:290/329`; IMD-Compressed-Fill `uft_imd_parser_v2.c:373`). |
| `fillByte` | Träger nur innerhalb von Format-Semantik sinnvoll und dort vorhanden (`uft_types.h:534`, Default 0x4E); als freier Regler dieselbe Fabrikationsgefahr. |

## Lizenz

* **fdc_bitstream** (Quelle für `filterType`/`adaptiveGain`/`historyDepth`):
  MIT, aus der Lizenz-DATEI gelesen (`git show
  70d2e0e7^:src/flux/fdc_bitstream/LICENSE.md` — „MIT License, Copyright
  (c) 2022 Yasunori Shimura"). Zone grün, aber ohne Kennzahl-Bewegung kein
  Auftrag.
* **nibtools** (`tools/uft-scout/work/nibtools/LICENSE`): GPL-3.0 —
  für ein GPL-2.0-Projekt **nicht portierbar** (Lizenzmatrix); zulässig
  bleibt Nutzung als externes Oracle (`nibscan`-Binary) und Verhaltens-Spec.
* **HxCFloppyEmulator**: nur als Zweitquellen-Prüfung angefasst
  (`internal_libhxcfe.h` hat `pll_stat`, aber keine Filtertyp-/
  Adaptivgain-Semantik — Zweitquelle **nicht** erbracht); Lizenz nicht
  gelesen, weil nichts daraus vorgeschlagen wird.

## Oracle-Kandidaten (nur für die Fundus-Einträge relevant)

* `filterType`/`adaptiveGain`: fdc_bitstream-Upstream (MIT) baut ein
  Kommandozeilen-Testwerkzeug, dessen Decode-Ausgabe je VFO-Typ prüfbar
  ist — tauglich als Oracle, **falls** je ein Plan-Baustein PLL-Varianten
  verlangt.
* `gcrVariant=Victor`: kein Oracle benannt — erst nötig, wenn Victor-Flux
  überhaupt als Bedarf belegt ist (Korpus enthält kein Victor-Flux-Abbild;
  Inventar `korpus`: 22 Abbilder, keines Victor).

## OPEN_ITEMS-Vorschläge: **keine (0 von max. 5)**

Kein Urteil dieser Triage bewegt eine der vier Release-Kennzahlen
(ungeprüfte Formate ↓ / Wandlungspfade ↑ / leckende Tests 0 / Bench-Alter
↓). Die 19 Synonyme sind Löschungs-/Umbenennungs-Wissen für
`docs/SETTINGS_ROADMAP.md`, die 15 Streichungen sind Begründungen für die
ohnehin beschlossene Dialog-Löschung, und die zwei Fundus-Notizen
(`filterType`, `adaptiveGain`) sind unbelegt als (b). Nach MF-640:
**Fundus, nicht Auftrag.** Dieses Dokument selbst ist die Übergabe.

## UNGEKLÄRT

1. Ob das OTDR-Panel die drei numerischen Schwellen
   (`pll_lock_threshold`, `weak_bit_cv`, `noflux_threshold`) heute setzbar
   macht — belegt ist nur der `detect_weak_bits`-Schalter.
2. Ob der `error_count` von `gcr_decode` irgendwo bis zum Benutzer
   durchgereicht wird (für `ignoreBadGCR`-Sichtbarkeit relevant, nicht für
   das Urteil).
3. Die Semantik von `pllPhase` im Dialog (Offset vs. Gain) — beurteilt
   nach dem dokumentierten Kommentar („−1.0 to 1.0"); als Gain gelesen
   wäre es trotzdem kein Synonym, sondern ein neuer Regler.
4. Ob die Unerreichbarkeit des Kalman-/God-Mode-/Advanced-Mode-Zirkels
   bereits als eigener Befund geführt wird (P0-2-Klasse; `v3_bridge:28`
   sagt es selbst, ein OPEN_ITEMS-Eintrag dazu wurde nicht gesucht).
5. `use_pll` (`uft_flux_decoder.h:280`) als möglicher Sichtbar-Regler —
   vom Auftraggeber als lebend genannt, hier nicht erneut vermessen.
