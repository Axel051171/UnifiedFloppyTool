# Ground-Truth-Quellen ohne eigene Hardware

Ein Parser ist erst dann über `T3` hinaus, wenn eine reale Datei dieses Formats
sauber durch ihn läuft. Ohne Diskettenlaufwerk kommen solche Dateien aus drei
Quellen — nach Beweiskraft geordnet.

> **Achse nicht verwechseln.** Eine reale Datei hebt den **Verifikations-Tier**
> (T3 → T1/T1b). Sie hebt *nicht* `spec_status`: das beschreibt, wie gut das
> *Format* spezifiziert ist, und `UFT_SPEC_DERIVED` heißt „De-facto-Standard
> ohne formale Spec", nicht „an einer echten Datei geprüft".

## Wo das im Baum liegt

| Pfad | Inhalt |
|---|---|
| `tests/corpus_manifest/manifest.json` | **die In-Repo-Wahrheit**: SHA-256 + Provenienz jedes Referenzabbilds |
| `tests/corpus_free/` | getrackte Abbilder — rechtefrei, selbst erzeugt |
| `tests/corpus/` | gitignored — urheberrechtlich beschränkt, aus der Provenienz neu zu beschaffen |
| `docs/VERIFICATION_PLAN.md` | SSOT der Tier-Definitionen |
| `docs/VERIFICATION_TIERS.md` | generierte Tabelle (`scripts/gen_verification_tiers.py --write`) |
| `docs/spec_verification.json` | Spec-Belege pro Plugin (T2-Evidenz) |

Ein Eintrag mit `origin: "real"` hebt das Format auf **T1**, `origin:
"cross-tool"` auf **T1b** — beides nur, wenn der genannte ctest-Test existiert
**und das Abbild tatsächlich durch das UFT-Plugin öffnet**. Ein Manifest-Eintrag
ohne konsumierenden Test bewegt gar nichts.

## 1. Cross-Tool erzeugte Images (→ `T1b`)

Eine kanonische Fremdimplementierung erzeugt das Image, der Inhalt ist deiner.
Rechtefrei, und — das ist der Punkt — **außerhalb von UFTs eigener Annahme
erzeugt**. Genau das durchbricht den Zirkel, in dem UFTs synthetische
Generatoren die Fabrikationen nicht fangen konnten: sie waren zur erfundenen
Spec konsistent.

Im Baum bereits vorhanden und als Vorlage brauchbar:

| Format | Werkzeug | Kommando (aus dem Manifest) |
|---|---|---|
| D64 / D71 / D81 / G64 | VICE 3.10 `c1541` | `c1541 -format "uftcorpus,42" d64 <img> -write marker.txt "uft marker"` |
| ADF | amitools `xdftool` | `xdftool <img> create + format "UFTCORPUS" ofs + write marker.txt` |
| ATR | `atrcopy` 10.1 Template | `templates/dos2sd.atr` unverändert (leerer VTOC, keine DOS-Dateien) |
| HFE / SCP | greaseweazle 1.23 | `gw convert --format=amiga.amigados <adf> out.hfe` |

Weitere gangbare Wege: WinUAE (Amiga), HxC Floppy Emulator (HFE aus bekanntem
Image), `mkfs.fat -F12` auf Loop-Device (PC FAT12), Hatari (Atari ST), SAMdisk.

**Der Inhalt muss rechtefrei sein**, sonst gehört die Datei nach
`tests/corpus/` statt `tests/corpus_free/`. Selbst geschriebene Marker-Dateien
sind der einfachste Weg dahin.

## 2. Öffentliche Archive (→ `T1`, wenn die Herkunft dokumentiert ist)

- **Internet Archive** — Images mit dokumentierter Provenienz
- **TOSEC** — kuratierte Sammlung, Katalognummern als Herkunftsnachweis
- **zxart.ee**, **C64 Preservation Project** — im Baum bereits als Quelle genutzt

Urheberrechtlich beschränkte Abbilder liegen lokal unter `tests/corpus/`
(gitignored); im Repo steht nur der Manifest-Eintrag. Er muss so präzise sein,
dass ein Dritter die Datei neu beschaffen und den Hash prüfen kann — die
vorhandenen Einträge nennen Sammlung, Datei-ID, inneren Pfad und URL.

## 3. Herstellerspezifikationen (→ `T2`, nie `T1`)

Ein Dokument belegt die Struktur, aber nicht, dass der Parser sie richtig liest.
Deshalb reicht es nur für `T2` (synthetischer Roundtrip **plus** Spec-Beleg in
`docs/spec_verification.json`). Dokument **und** reale Datei zusammen ist die
stärkste Lage.

- **KryoFlux** — Stream-Format-Dokumentation vom Hersteller
- **SuperCard Pro (SCP)** — offizielle Formatbeschreibung
- **HFE** — HxC-Projektdokumentation
- **ADF / AmigaDOS** — dokumentierte AmigaDOS-Strukturen
- **FAT12** — Microsofts FAT-Spezifikation
- **G64** — Pete-Rittwage-Spec (im Baum bereits als Quelle zitiert)

Jede Spec-URL wird im Parser-Kommentar **und** im Bericht zitiert. Eine Struktur
ohne zitierte Quelle bleibt `T3`, egal wie sicher sie wirkt.

## Was NICHT als Ground Truth zählt

- **UFTs eigene synthetische Fixtures** (`uft-flux-fixtures`). Sie sind zur
  angenommenen Spec konsistent — ist die Spec falsch, ist die Fixture es auch.
  Nützlich für Regressionstests, wertlos als Wahrheitsquelle. Das ist die
  Definition von `T3`.
- **Ein Image, das nur UFT selbst je erzeugt hat.** Zirkelschluss.
- **Eine Formatbeschreibung aus Blog oder Forum** ohne reale Datei dahinter.
  Hypothese, kein Beleg — gehört als solche in den Bericht.
- **Ein Manifest-Eintrag ohne Test, der ihn liest.** Die Datei liegt dann da,
  beweist aber nichts über den Parser; `gen_verification_tiers.py` zählt sie
  zu Recht nicht.

## Einen Eintrag hinzufügen

1. Datei erzeugen oder beschaffen, Provenienz **beim Erzeugen** notieren
   (Werkzeug + Version + exaktes Kommando bzw. URL + Katalog-ID).
2. Rechtefrei? → `tests/corpus_free/`. Sonst → `tests/corpus/` (gitignored).
3. Eintrag in `tests/corpus_manifest/manifest.json` nach `_entry_schema`:
   `file`, `sha256`, `format` (Plugin-Symbol aus `gen_format_list.py`),
   `origin` (`real` | `cross-tool`), `tool`, `source`, `test`.
4. Einen ctest-Test schreiben, der das Abbild **durch das UFT-Plugin** öffnet
   und den Inhalt prüft — nicht nur, dass es nicht abstürzt.
5. `python scripts/gen_verification_tiers.py --write`, dann
   `python scripts/check_consistency.py` (die Tier-Tabelle wird auf Frische
   geprüft und blockiert sonst den Commit).
