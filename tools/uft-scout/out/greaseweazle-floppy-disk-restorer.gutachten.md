# Gutachten: JYewman/Greaseweazle-Floppy-Disk-Restorer

Stand: 2026-08-26 · Inventar: UFT HEAD `756806c3`
(`scout_inv.json`: 88 Plugins, SSOT ok, 22 Korpus-Abbilder) ·
Fremd-Repo HEAD `31b2f144` (2026-05-21), Vermessung per Shallow-Clone.
Zyklus vom Eigentümer benannt (kein Scout-Lauf); Vermessung von Hand,
jede Aussage mit Datei+Zeile.

## Ergebnis in einem Satz

**Kein Fund.** Die einzigen Fähigkeiten, die UFT fehlen würden, sind im
fremden Repo Scheinmessungen (belegt unten); alles Substanzielle hat UFT
bereits — Verwerfung, Eintrag in `data/known_negatives.json`.

## Kategorie

**irrelevant** (Fabrikations-Befund in genau den Modulen, die neu wären).

## Repo-Steckbrief (gemessen)

- Python-3.10+/PyQt6-Workbench um `keirf/greaseweazle` als Git-Dependency
  (`pyproject.toml`, dependencies: `greaseweazle @ git+…@latest`)
- Ein Autor (Joshua Yewman), Commits 2026-01-15 … 2026-05-21, 2 Sterne
- `hardware/gw_mfm_codec.py:1-9`: Adapter, „Based on Greaseweazle code by
  Keir Fraser, released into public domain" — nichts GPL-fremdes vendored
- Tests vorhanden (`tests/unit/`, `tests/integration/`), decken Geometry/
  Recovery/Sector-IO, nicht die unten belegten Hohlstellen

## Lizenz (aus der Datei, nicht README)

- Einzige Lizenzdatei im Baum: `LICENSE` (Repo-Wurzel), Volltext
  **MIT License, Copyright (c) 2026 Joshua Yewman**. Rekursive Suche nach
  `LICENSE*`/`COPYING*` ergab keine weiteren (kein Vendoring mit eigener
  Lizenz).
- **Zone: GRÜN** (lizenzmatrix.md Zeile MIT) → Code wäre mit
  Attribution-Header portierbar. **Konsequenz hier: gegenstandslos** —
  der Substanz-Befund (unten) macht das Portier-Recht wertlos.

## Was das Inventar dazu sagt (Abfrage zitiert)

`inventar.py query scout_inv.json "head alignment" "azimuth"
"drive calibration" "rpm measurement" "pll tuning" "pll parameter sweep"
"convergence" "bit slip recovery" "surface treatment" "degauss" "snr"
"jitter" "weak bits" "multi capture voting"` lieferte für **alle 14**
Begriffe wörtlich:

```json
{"vorhanden": false, "treffer": [], "schwache_treffer": [], "tier": null, "hinweis": ""}
```

**Diese Antwort ist für Fähigkeitsfragen nicht belastbar** (Werkzeug-
Befund W-2 unten): der Suchindex enthält nur Formatnamen, Verzeichnisse,
Controller und Vendored-Pfade (`inventar.py:149-156`) — UFT hat z. B.
Jitter-Analyse und Multi-Rev-Voting nachweislich, das Inventar meldet
beides als fehlend, ohne auch nur einen schwachen Treffer. Deshalb wurde
**jede** Fähigkeit von Hand im UFT-Baum gegengeprüft:

| Fähigkeit | UFT-Gegenprobe (manuell) | Urteil |
|---|---|---|
| Head-Alignment/Azimut-Diagnose | `grep -ri azimuth src include` → **0 Treffer** | fehlt wirklich — aber Fremdcode ist Scheinmessung (s. u.) |
| Drive-RPM + Jitter | `src/hardwaretab.cpp:1276-1277` „RPM=… (jitter=…%, … revs sampled)", Capability `MeasuresRPM` (MF-157, Z. 267) | **vorhanden** |
| Bit-Slip-Erkennung/-Korrektur | `src/recovery/uft_bitstream_recovery.c:258` `detect_bit_slip()`, `:301` `correct_bit_slip()` | **vorhanden** |
| Multi-Rev-Voting/Fusion | `src/algorithms/advanced/uft_multi_rev_fusion.c`; „enough good passes"-Logik `src/recovery/uft_multiread_pipeline.c:594-600` | **vorhanden** (DeepRead Weighted Voting) |
| SNR/Signalqualität | `src/analysis/deepread/uft_deepread_aging.c`, `otdr_event_core_v10/v11.c` u. a. | **vorhanden** (OTDR/Confidence) |
| Adaptiver PLL-Re-Decode | Adaptive Decode ±33 % (CLAUDE.md DeepRead; `uft_kalman_pll_v2.c`) | **vorhanden** |
| Track-Erase | `src/hal/uft_greaseweazle_full.c:955` `uft_gw_erase_track()` | **vorhanden** (HAL) |
| Degauss-/„Refresh"-Workflow | `grep -ri degauss src include` → 0 Treffer | fehlt — bewusst nicht vorgeschlagen (s. u.) |

## Substanz-Befund: die „neuen" Module sind hohl (Datei+Zeile)

1. **`analysis/head_alignment.py` (885 LOC) — Scheinmessung.**
   Kern `measure_track_margins()` behauptet, bei Offset-Positionen zu
   lesen; Z. 444-447 wörtlich: *„Note: Actual offset seeking depends on
   Greaseweazle firmware support / For now, we simulate by reading at the
   nominal position / In a full implementation, this would use seek with
   offset."* Jeder „Offset"-Messpunkt liest **dieselbe** Spur; das ganze
   Margin-Profil (µm-Angaben, Score 0-100) ist fabriziert. Die
   Azimut-Umrechnung Z. 256-258: *„rough approximation … 1us corresponds
   to approximately 0.5 degrees"* — Konstante ohne Referenz.
2. **`recovery/pll_tuning.py` (1059 LOC) — Sweep gegen fabrizierte
   Erfolgsmetrik.** Der Decoder, gegen den die Parametersuche bewertet,
   rechnet keine CRC: Z. 565-570 *„Read CRC (simplified - just skip)"*,
   dann `crc_valid = len(set(data_bytes)) > 1  # Basic sanity check` mit
   Kommentar *„Real implementation would calculate CRC-CCITT"*. Ein
   PLL-Optimum nach dieser Metrik ist bedeutungslos.
3. **`recovery/bit_slip_recovery.py:921-929`** — Validierung *„For now,
   check that data isn't all zeros or all ones"*.
4. **`recovery/multi_capture.py:805-808`** — Positions-Mapping *„This is
   simplified - ideally we'd map flux positions to sector positions"*;
   Voting läuft positionsweise über Timing-Listen nach
   Kreuzkorrelation-Alignment — konzeptionell schwächer als UFTs
   gewichtete Multi-Rev-Fusion.
5. **`analysis/forensics.py:1192`** — *„simplified version - full
   implementation would …"*.
6. Real ist im Repo v. a. das, was das `greaseweazle`-Paket ohnehin
   liefert (`hardware/greaseweazle_device.py:1230` `get_rpm()` über
   Index-Pulse — UFT hat das produktiv) und die Qt-Oberfläche.

Das ist exakt die Fehlerklasse, die UFT mit MF-363/498 aus dem eigenen
Baum getrieben hat (fabrizierte Parser FMT-2/3/10/11/12). Als benannte
Referenz oder Verhaltens-Spec ist dieses Repo unbrauchbar; als Oracle
erst recht (es prüft selbst keine CRCs).

## Einzeln geprüfte Rest-Kandidaten (und warum kein Vorschlag)

- **Blank-Image-Fixtures** (`src/floppy_formatter/data/disk_images/`:
  6× ADF, 8× IBM-IMG, Mac 400K/800K/1440K, ST, DSD/SSD, CPC-DSK, MSX):
  MIT-lizenziert, aber **ohne Erzeuger-/Provenienz-Angabe** im Repo —
  erfüllt „benannte Referenz" nicht. Gegen `inv["korpus"]` geprüft
  (22 Abbilder, VICE-/xdftool-/atrcopy-/GW-Herkunft): die Lücken
  (Mac-GCR, BBC, CPC) deckt man besser mit werkzeug-benannter Erzeugung
  (z. B. Mini vMac/hfsutils, MMB/BeebEm-Tools) als mit anonymen Blanks.
- **Surface Treatment / Degauss-„Media-Refresh"**
  (`recovery/surface_treatment.py:157` `degauss_track()` schreibt
  DC-Erase-Flux): medienverändernd, Nutzenbehauptung („helps restore
  degraded media", Z. 5-8) unbelegt, Hardware-Folge → nach AGENT.md
  Regel 8 wäre das eine Eigentümer-Vorlage, keine Scout-Entscheidung.
  UFT hat `uft_gw_erase_track()` bereits; ein „Refresh-Workflow" steht
  quer zur Forensik-Mission („keine stille Veränderung"). Nicht
  vorgeschlagen.
- **Konvergenz-Abbruch beim Multiread** („stop when no further
  improvement"): UFT bricht nach „enough good passes" ab
  (`uft_multiread_pipeline.c:594-600`); ein informationsbasiertes
  Kriterium wäre eine denkbare Mini-Verbesserung, aber die fremde
  Implementierung ist als Beleg unbrauchbar (Punkt 4 oben) und eine
  „besser"-Behauptung ohne tragfähige Gegenseite lässt sich nicht als
  Differenzlauf formulieren. → UNGEKLÄRT-Liste, kein Vorschlag.

## Pflichtfelder kompakt

- **Kategorie:** irrelevant
- **Lizenzzone:** GRÜN (MIT, aus `LICENSE`) — Konsequenz gegenstandslos
- **Einhängepunkt:** keiner (kein Fund)
- **Oracle-Kandidat:** keiner (Repo prüft selbst keine CRCs)
- **Beschaffungsliste:** leer (Blank-Images verworfen, Begründung oben;
  gegen Korpus-Inventar geprüft)
- **Aufwandsklasse:** —
- **Differenzlauf-Plan:** entfällt (keine Überlegenheits-Behauptung
  übernommen)

## UNGEKLÄRT

- [ ] Gibt es eine **seriöse** Referenz-Methodik für Head-Alignment-
      Diagnose ohne Spezial-Hardware (Alignment-Disks wie Dysan 224/2A,
      ImageDisk TESTFDC), und kann Greaseweazle-Firmware überhaupt
      Offset-Positionen anfahren? Das fremde Repo verneint das implizit
      (head_alignment.py:445). Erst wenn beides bejaht ist, wäre
      „Alignment-Diagnose" ein Kandidat — dann als Eigentümer-Vorlage
      (Hardware-Folge; Projekt hat keine physische Hardware, Tier-3 ist
      community-delegiert).
- [ ] Lohnt ein informationsbasiertes Konvergenz-Abbruchkriterium im
      Multiread zusätzlich zur Pass-Zähl-Logik
      (`uft_multiread_pipeline.c:594-600`)? Messbar erst mit
      Multi-Rev-Realaufnahmen einer degradierten Diskette im Korpus.
- [ ] „Degauss/Refresh vor Neubeschreiben" als Werkbank-Feature: Frage
      an den Eigentümer, ob medienverändernde Restaurations-Workflows
      überhaupt in den Missions-Umfang gehören (Konflikt mit „keine
      stille Veränderung" ist per UI-Einwilligung lösbar, aber das ist
      eine Produktentscheidung).

## Werkzeugkasten-Befunde dieses Test-Laufs (an den Eigentümer)

- **W-1:** `inventar.py query --help` stürzt mit
  `FileNotFoundError`-Traceback ab (`inventar.py:218` öffnet `argv[2]`
  blind als Datei) statt die Usage zu zeigen.
- **W-2 (gewichtig):** Das Inventar ist für **Fähigkeitsfragen
  strukturell falsch-negativ, ohne schwache Treffer**. 14/14 Abfragen
  dieses Laufs kamen als `vorhanden:false, schwache_treffer:[]` zurück,
  darunter „jitter", „weak bits", „multi capture voting" — Fähigkeiten,
  die UFT belegt hat (Tabelle oben). Der Index (`inventar.py:149-156`)
  speist sich nur aus Formatnamen/Verzeichnissen/Controllern/Vendored.
  Der dokumentierte Rotbeweis (falsches „vorhanden" bei
  „flux visualization") deckt nur die eine Richtung; die Gegenrichtung
  — falsches „fehlt" → Dubletten-Vorschlag — hat kein Netz außer der
  manuellen Gegenprobe. Vorschlag: Index um Modul-/Funktionsnamen aus
  `src/**` erweitern oder die Query-Antwort für Nicht-Format-Begriffe
  ehrlich als „Inventar deckt das nicht ab" kennzeichnen.

## Regeln, die für dieses Urteil galten

- AGENT.md Regel 2 (kein Fund ohne Messung): alle Hohlstellen mit
  Datei+Zeile belegt; alle „vorhanden"-Urteile mit UFT-Pfad+Zeile.
- AGENT.md Regel 4: verworfen wurde **nicht** auf Inventar-Treffer hin
  (es gab keine), sondern auf manuell belegte starke Treffer im
  UFT-Baum bzw. auf Substanzlosigkeit der Fremdquelle.
- Kein Code aus diesem Agenten (Regel 1); nichts nach `src/`/`docs/`.
