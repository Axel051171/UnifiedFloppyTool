# UFT v4.1.5 — 10-Minuten Quick-Demo

**Zweck:** Auf einem **frischen** Linux-Laptop (Ubuntu 24.04 LTS empfohlen)
in unter 12 Minuten zeigen, was UFT v4.1.5 ohne jegliche Hardware kann.

**Voraussetzungen:**
- Ubuntu 24.04 LTS frische Installation (oder Debian Trixie / Fedora 41)
- Internet (für git clone, sonst tarball-fähig)
- `sudo apt install build-essential cmake qt6-base-dev libqt6charts6-dev python3 git` (~3 min, einmalig)
- KEINE Floppy-Hardware nötig
- KEIN root-Zugang nach Setup

**Demo-Dauer:** 10 Min effektiv + 2 Min Setup-Buffer = 12 Min total

---

## 0. Setup (1 min, einmalig)

```bash
sudo apt update
sudo apt install -y build-essential cmake qt6-base-dev libqt6charts6-dev python3 git
```

Setup zählt nicht zur Demo-Zeit. Auf einem Demo-Laptop schon vorinstalliert.

---

## 1. Clone + Build (1 min)

```bash
git clone https://github.com/Axel051171/UnifiedFloppyTool.git uft
cd uft
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Was zu sagen:**
- "Reines C/C++ mit Qt6. Keine Python-Runtime, kein Cloud, keine Telemetrie."
- "Build ist in CI auf Linux GCC + macOS Clang + Windows MinGW grün."
- "Erwartete Build-Zeit auf 4-Core ThinkPad: 3-4 Minuten — wir nutzen die als
  Überleitung zur nächsten Demo-Stufe."

**Was zu zeigen:** Build-Output. `build/UnifiedFloppyTool` Binary existiert.

---

## 2. Tier-2.5 Hardware-Simulation (2 min)

Während der Build noch läuft (oder direkt danach):

```bash
python3 tests/hil/run_simulated.py
```

**Erwarteter Output (Auszug):**

```
[SIM] kryoflux info banner round-trip — DTC -i0, 5 lines parsed
[SIM] kryoflux read 3-track raw streams — 3 files × ~261 B
[SIM] fluxengine version-banner round-trip — 'FluxEngine 0.99.99-sim ...'
[SIM] fluxengine rpm parser regression
[SIM] fluxengine read 1-track flux file — 65536 bytes
[SIM] fluxengine write 1-track flux file — 24290948 bytes consumed
[SIM] fc5025 read msdos360 6 tracks — 27648 B sector dump
[SIM] usb_mock_self_test — header-substitution OK
[SIM] scp_direct_usb_mock — 5 wire-protocol cases PASS
[SIM] xum1541_usb_mock — 3 wire-protocol cases PASS
[SIM] greaseweazle_sim_compile — protocol simulator byte-compile OK

Summary: 10 SIMULATED · 0 FAIL · 0 NOT_RUN
```

**Was zu sagen:**
- "10 Hardware-Pfade. Kein einziges echtes Device. Aber: nicht Mock-PASS,
  sondern SIMULATED — strikt zwischen NOT_RUN und PASS."
- "Drei Mocking-Strategien: Python-fake-binary für CLI-subprocess-Controller,
  libusb-Mock-Framework für direkte USB-Kommunikation, Python-Protocol-Sim
  für USB-CDC-Serial-Geräte wie Greaseweazle."
- "CI läuft das auf jeden Push. Wenn ein Mock-Mismatch passiert, blockt CI
  den Merge."

**Was zu zeigen:** den Output. Optional `cat releases/v4.1.5/hil_simulated.md`
für die Markdown-Variante.

---

## 3. GUI starten + Flux laden (3 min)

```bash
./build/UnifiedFloppyTool &
```

**Was zu zeigen:**
1. Hauptfenster öffnet sich. **Hardware-Tab** ist sichtbar.
2. Provider-Liste zeigt 9 Controller. Greaseweazle hat grünes "Production"
   Badge, die anderen 8 haben oranges "Preview" Badge.
3. **Niemals einen Controller anschließen** — die orange-markierten sagen
   beim Connect-Klick ehrlich `not_implemented: backend not wired`,
   kein silent no-op.
4. **Flux-Capture-Tab** → "Open Flux File" → `tests/differential/corpus/flux/ibm_dd.scp`.
5. UFT lädt das, zeigt **Track-Heatmap**, **Flux-Histogram-Widget** (OTDR-
   inspiriert), **Sektor-Decode-Status** pro Track.

**Was zu sagen:**
- "Das ist ein synthetischer IBM-DD 360KB Flux-Stream — Teil unseres
  Differential-Corpus. UFT dekodiert ihn byte-identisch zu `gw 1.23` —
  das ist eines der 6/6 verifizierten Goldene-Referenz-Paare."
- "Die Heatmap zeigt Sektor-Status pro Track. Grün = OK, Gelb = recovered
  via DeepRead, Rot = unrecoverable. Bei einem echten beschädigten Disk
  würde hier die DeepRead-Pipeline mit Adaptive Decode + Weighted Voting
  + OTDR-Analyse arbeiten."

**Was zu zeigen:** Heatmap, Histogram, ein paar Sektoren-Details per Klick.

---

## 4. Lossy-Convert + Sidecar-Demo (2 min)

In der GUI:

1. **File → Convert** öffnen.
2. **Source:** `tests/differential/corpus/flux/ibm_dd.scp`
3. **Target:** `/tmp/demo_out.img`
4. **Format:** IBM IMG (Sektor-Dump)
5. Wizard zeigt **orangefarbene Warnung:** `LOSSY-DOCUMENTED — Flux-Timing
   und Weak-Bits gehen verloren. Sidecar `.loss.json` wird emittiert.`
6. Checkbox **"I accept this conversion is lossy"** ankreuzen.
7. **Convert** klicken.
8. Im Terminal:
   ```bash
   ls -la /tmp/demo_out.img /tmp/demo_out.img.loss.json
   cat /tmp/demo_out.img.loss.json | python3 -m json.tool | head -30
   ```

**Was zu sagen:**
- "Prinzip 1: kein stiller Datenverlust. UFT lässt Sie die lossy Konvertierung
  nicht ohne explizite Zustimmung machen — und dokumentiert exakt welche
  Datenkategorien verloren gehen."
- "Das Sidecar ist Schema-versioniert (`uft-loss-report-v1`). 11 Verlust-
  Kategorien — FLUX_TIMING, WEAK_BITS, INDEX_PULSES, MULTI_REVOLUTION,
  COPY_PROTECTION usw."
- "Alle 44 Konvertierungspfade laufen durch denselben Single-Chokepoint
  (`uft_convert_file()`). Niemand kann das umgehen."

**Was zu zeigen:** Wizard-Warnung, Sidecar-JSON.

---

## 5. Plugin-Compliance (1 min)

```bash
python3 scripts/audit_plugin_compliance.py
```

**Erwarteter Output:**

```
Plugin compliance audit:
  Total plugins:       80
  spec_status set:     80/80  (100%)
  features populated:  80/80  (100%)
  is_stub explicit:    80/80  (100%)
  compat_entries:       1/80   (ADF — Exemplar)

Principle-7 compliance: 80/80 (100%) ✓
```

**Was zu sagen:**
- "Prinzip 7: Ehrlichkeit bei proprietären Formaten. Jedes Plugin sagt
  woher seine Spec kommt — OFFICIAL_FULL, OFFICIAL_PARTIAL,
  REVERSE_ENGINEERED, DERIVED — und welche Features es kann."
- "138 Format-IDs, 80 Plugin-Registrierungen. 5/80 hatten eine
  feature-Matrix vor v4.1.5. Jetzt: 80/80."

---

## 6. Audit-Master-Report (1 min)

```bash
$BROWSER audit/MASTER_REPORT.md   # oder vim/less/cat
```

**Was zu zeigen:** Inhaltsverzeichnis des Master-Reports:
- ARCH-0 (positive: honest scaffolds halten) ✓
- ARCH-1..7 Status
- SCP-D1 (USB-Opcodes vs SDK) ✓ MF-261 — 22/22 byte-exakt vs samdisk
- 9 Provider-Auditberichte unter `audit/<provider>/REPORT.md`

**Was zu sagen:**
- "Das ist der Forensik-Audit jedes einzelnen Providers. Jede Behauptung
  hat einen Verdict + Evidence-Pfad. SCP-Direct zum Beispiel: 22 USB-
  Opcodes gegen die samdisk Referenz-Implementation byte-für-byte
  verifiziert."
- "Nichts in UFT ist nur durch 'works on my machine' validiert."

---

## Was die Demo NICHT zeigt

Ehrlich beim Namen genannt:

- **Echte Hardware** — Greaseweazle / SCP / KryoFlux laufen alle simuliert.
  Tier-3 PASS braucht eine Bench-Session, die nicht im Demo-Scope ist.
- **Schwer beschädigte Disketten** — DeepRead-Module sind unit-getestet,
  Real-Disk-Validierung ist work-in-progress.
- **`uft-decode` CLI** — Scaffold da, Build-Wiring v4.1.6.
- **WhinUAE / VICE Emulator-Verifikation** — Plan für 4.3.
- **Windows / macOS USB-Floppy** — nur Linux SG_IO ist live.

Wenn der Zuschauer nachfragt: `docs/CAPABILITIES.md` öffnen, die ehrliche
Matrix zeigen.

---

## Backup-Plan

Wenn ein Schritt fehlschlägt:

| Fehler | Backup |
|---|---|
| GUI startet nicht (Qt6 fehlt) | nur Schritte 1+2+5+6 zeigen (alles Terminal) |
| Build dauert >5 min | parallel Schritt 2 starten, dann zurück |
| Flux-File lädt nicht | `ibm_hd.scp` statt `ibm_dd.scp` versuchen |
| Sidecar wird nicht emittiert | Bug — KNOWN_ISSUE eintragen, Demo-Punkt 4 auslassen, Punkt 5 betonen |
| `run_simulated.py` zeigt FAIL | screenshot, Issue öffnen — Tier-2.5 hat genau dafür CI-Gate |

---

## Nach der Demo

**Links für interessierte Zuschauer:**
- `docs/SHOWCASE.md` — 1-Seiten-Pitch
- `docs/CAPABILITIES.md` — Was geht heute
- `docs/KNOWN_ISSUES.md` — Was nicht geht (ehrlich)
- `docs/DESIGN_PRINCIPLES.md` — 7+4 Prinzipien
- `RELEASE_NOTES.md` — Was sich v4.1.5 geändert hat
- `CLAUDE.md` — Architektur-Überblick

**Wo Bug-Reports hingehen:**
GitHub Issues. Bitte erst KNOWN_ISSUES.md durchschauen.

---

## Demo-Performance-Metriken

Wenn die Demo durchgespielt wurde, in `docs/demo/TESTER_REPORTS.md`
eintragen:

| Tester | System | Datum | Zeit | Probleme |
|---|---|---|---|---|
| (Beispiel) | Ubuntu 24.04, ThinkPad E15 | 2026-06-15 | 11:30 | keine |

Ziel: 3 unabhängige Test-Durchläufe vor Phase D Ende (2026-06-15).
