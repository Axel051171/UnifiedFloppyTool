# UFT Software Tester — Strategie

**Doppelziel:**
1. Beweisbare Kompatibilität zu Greaseweazle (`keirf/greaseweazle` v1.23)
2. Beweisbar *besser* als Greaseweazle in den Punkten, die UFT differenzieren

**Kernidee:** Die beste Art zu beweisen, dass UFT GW-kompatibel ist, ist
**Differential Testing**: bei jedem Test laufen `gw <command>` und
`uft <command>` mit identischem Input, Outputs werden byte-genau verglichen.
Abweichungen sind nur erlaubt, wenn sie in einer **Divergence Registry**
explizit dokumentiert und begründet sind.

"Besser" wird durch eine separate **Improvement Test Suite** bewiesen —
Tests, die UFT besteht und GW *erwartungsgemäß nicht* (LossReport,
Audit-Log, Multi-Device, GUI, etc.).

> **Status (2026-05-05):** Strategie-Dokument aufgenommen. Implementation
> startet erst NACH P1 (V2-Provider-Migration). Konkrete Tasks landen
> als P3 in `docs/REFACTOR_TASKS.md`.

---

## Teil 1 — Was "Software Tester" für UFT konkret bedeutet

Nicht "ich pip install pytest und schreibe ein paar Asserts". Für ein
Forensik-Tool mit Hardware-Bezug heißt Tester ein **multi-layer System**:

```
┌─────────────────────────────────────────────────────────────────┐
│  Schicht 7 — User Acceptance (Hand-Tests durch Axel)             │
│  Echte Disketten, echte Hardware, dokumentierte Szenarien        │
├─────────────────────────────────────────────────────────────────┤
│  Schicht 6 — Hardware-in-the-Loop                                │
│  Reale GW/SCP/KryoFlux gegen Test-Disketten · skip wenn no-HW    │
├─────────────────────────────────────────────────────────────────┤
│  Schicht 5 — Performance Regression                              │
│  Benchmarks · CI-Lane vergleicht gegen main-Baseline             │
├─────────────────────────────────────────────────────────────────┤
│  Schicht 4 — Differential Conformance (gegen GW)                 │
│  Für jedes GW-Command: gw vs. uft auf identischem Input          │
├─────────────────────────────────────────────────────────────────┤
│  Schicht 3 — Improvement Tests (UFT-only Features)               │
│  LossReport, Multi-Device, GUI, forensische Features             │
├─────────────────────────────────────────────────────────────────┤
│  Schicht 2 — Integration / Property / Fuzz                       │
│  HAL+Format-Pipeline · Hypothesis · malformed inputs             │
├─────────────────────────────────────────────────────────────────┤
│  Schicht 1 — Unit (klassisch)                                    │
│  Funktion → erwartetes Verhalten                                 │
└─────────────────────────────────────────────────────────────────┘
```

Die meisten Tools haben Schicht 1 + 2. UFT braucht alle sieben — sonst
ist "kompatibel und besser" eine Behauptung, kein Beweis.

---

## Teil 2 — Greaseweazle Compatibility: Differential Testing als Contract

### 2.1 Das Prinzip

Für jedes GW-Feature gilt:

```
    Identischer Input
          │
    ┌─────┴─────┐
    ▼           ▼
   gw         uft
   │           │
   ▼           ▼
 OutputGW  OutputUFT
    │         │
    └────┬────┘
         ▼
    Byte-Vergleich
         │
    ┌────┴────┬─────────┬─────────┐
    │         │         │         │
   IDENT    DIVERGE   DIVERGE   FAIL
            (intended)(unexpected)
   PASS      PASS     FAIL       FAIL
```

**IDENT:** Outputs sind byte-identisch. UFT verhält sich exakt wie GW.
**DIVERGE-OK:** Outputs unterscheiden sich, aber die Divergenz steht in
der Registry mit Grund. Z.B.: UFT schreibt zusätzlich einen
`.loss.json`-Sidecar — das ist nicht GW-Verhalten, aber dokumentiert.
**DIVERGE-BAD:** Outputs unterscheiden sich, ohne Eintrag in Registry →
Test failt. Du wirst gezwungen, entweder den Code zu fixen oder den
Eintrag zu schreiben.
**FAIL:** Eines der beiden Tools ist abgestürzt oder hat keinen Output
produziert.

Das ist der **Contract**. UFT hält sich daran oder erklärt sich.

### 2.2 Was alles getestet werden muss

GW v1.23 Hauptkommandos:

| Command | Was es tut | UFT-Equivalent | Test-Strategie |
|---------|-----------|----------------|----------------|
| `gw read` | Liest Diskette → Image | `uft read` | Differential auf SCP/HFE/RAW-Output; Hardware-Mock möglich |
| `gw write` | Schreibt Image → Diskette | `uft write` | Differential auf Flux-Stream-Output (vor Hardware) |
| `gw convert` | Konvertiert Image-Format | `uft convert` | **Tausende Differentials** — eines pro Format-Paar |
| `gw erase` | Löscht Diskette | `uft erase` | Hardware-only |
| `gw info` | Zeigt Disk-Info | `uft info` | Differential auf JSON/Text-Output (mit Divergence-Tolerance) |
| `gw rpm` | Misst Drive-RPM | `uft rpm` | Differential ±0.5 RPM |
| `gw seek` | Positioniert Kopf | `uft seek` | Hardware/Mock |
| `gw delays` | Setzt Drive-Timings | `uft delays` | State-Differential |
| `gw reset` | Reset Hardware | `uft reset` | State-Differential |
| `gw pin` | Pin-Control | `uft pin` | Hardware-only |
| `gw update` | Firmware-Update | `uft update` | Identisches Binär-File expected |
| `gw bandwidth` | USB-Throughput | `uft bandwidth` | ±10 % Toleranz |

GW-Formate (Auszug v1.23, ~50 Stück):

```
ibm.*           — IBM PC (360k, 720k, 1.2M, 1.44M, 2.88M)
amiga.*         — Amiga ADF, EXT2-ADF
acorn.*         — Acorn ADFS, DFS (single-density, double-density)
mac.*           — Mac 400k, 800k, 1.44M, GCR variants
apple2.*        — Apple ][ DOS 3.3, ProDOS, Nibble
c64.*           — Commodore 64 GCR
trs80.*         — TRS-80 DMK
thomson.*       — Thomson MO/TO
kaypro.*, eagle.*  — CP/M variants
micropolis.*    — Hard-sector MFM
edsk            — Extended DSK
```

Jedes Format hat Read- und Write-Pfade. Konservativ ~80 Format-Pfade ×
5 Test-Disketten = **400 Format-Conformance-Tests**, die UFT jeden
Day-1 bestehen muss.

### 2.3 Die `gw_corpus`-Strategie — Inputs einfrieren, nicht Outputs

Naiv wäre: *"Wir lassen `gw read` einmal laufen und vergleichen UFT
gegen den Output."* Problem: `gw` ändert Output-Format zwischen
Versionen (siehe Release-Notes v1.21–1.23: DMK-Fixes,
hard-sector-Updates). Tests werden dann brittle.

Bessere Strategie: **`gw_corpus`** — eine versionierte Sammlung aus:

```
tests/gw_corpus/
├── inputs/                              # Was reingeht
│   ├── flux_streams/                    # SCP/HFE/KryoFlux RAW von echten Disketten
│   │   ├── ibm_360k_dos_boot.scp
│   │   ├── amiga_adf_workbench13.scp
│   │   ├── mac_800k_system6.scp
│   │   └── c64_gcr_loadstar.scp
│   ├── disk_images/                     # IMG/ADF/IMD-Files von bekannten Disketten
│   │   ├── pc_dos_622.img
│   │   ├── amiga_kickstart.adf
│   │   └── ...
│   └── mock_hardware_traces/            # Aufgezeichnete USB-Traces für Hardware-Sims
├── outputs_gw/                          # Was gw v1.23 daraus macht (eingefroren)
│   ├── v1.23/
│   │   ├── ibm_360k_dos_boot.scp.gw_read.img.sha256
│   │   ├── ibm_360k_dos_boot.scp.gw_read.img.bin
│   │   ├── ibm_360k_dos_boot.scp.gw_info.json
│   │   └── ...
│   └── v1.22/                          # Vorgängerversion für Cross-Check
└── README.md                            # Wie der Corpus gepflegt wird
```

**Vorteile:**
- Tests sind reproduzierbar ohne `gw`-Installation in CI (Output-Cache)
- GW-Versions-Updates sind sichtbar als Diff im Corpus (`v1.23/` vs `v1.22/`)
- UFT wird gegen *eine spezifische GW-Version* verifiziert, nicht
  gegen "irgendein gw das gerade installiert ist"
- Wenn GW v1.24 erscheint, läuft ein Refresh-Job, der den Corpus
  aktualisiert; UFT-Diff zeigt sofort, was sich geändert hat

**Größenmanagement:** Corpus auf Git-LFS oder separates Repo
(`Axel051171/uft-test-corpus`). Wahrscheinlich 500MB – 2GB groß bei
vollständiger Coverage.

### 2.4 Die Divergence Registry

Eine YAML-Datei dokumentiert jede legitime Abweichung. Beispiele
(DIV-001 bis DIV-005) siehe Anhang A in der Original-Strategie. Jeder
Eintrag ist gleichzeitig Test-Spec und Doku.

### 2.5 Wire-Level-Kompatibilität

Nicht nur Datei-Outputs, auch das **USB-Protokoll** zur Hardware muss
kompatibel sein. UFT muss eine echte Greaseweazle-Hardware ansprechen
können, ohne dass die Hardware merkt, dass nicht `gw` selbst spricht.

USB-Traces (USBPcap auf Windows, usbmon auf Linux) einmal aufnehmen,
ins Corpus committen, danach automatisch verifizieren.

---

## Teil 3 — "Better than GW": Improvement Test Suite

```
tests/improvement/
├── forensic/                        # Was GW nicht hat
│   ├── test_lossreport_emitted.py
│   ├── test_audit_chain_integrity.py
│   ├── test_marginal_data_preserved.py
│   ├── test_destructive_op_consent.py
│   └── test_provenance_chain.py
├── multi_device/                    # GW kann nur GW-Hardware
│   ├── test_kryoflux_provider.py
│   ├── test_scp_provider.py
│   ├── test_fluxengine_provider.py
│   └── test_provider_switch_consistency.py
├── format_extension/                # Formate jenseits von GW
│   ├── test_ipf_decode.py
│   ├── test_stx_decode.py
│   ├── test_kfx_decode.py
│   └── test_proprietary_jp_decode.py
├── copy_protection/
│   ├── test_protection_dungeon_master.py
│   ├── test_protection_lenslok.py
│   └── test_protection_long_track.py
├── gui/
│   ├── test_main_window_smoke.py
│   ├── test_hardware_tab_capability_disable.py
│   └── test_format_tab_workflow.py
└── concurrency/
    ├── test_parallel_drives.py
    └── test_long_running_session.py
```

GUI-Tests via `pytest-qt` (siehe Beispiele in Original-Strategie).

---

## Teil 4 — Hardware-in-the-Loop (HIL)

Mocks reichen nicht. Mindestens einmal pro Release muss UFT auf realer
Hardware laufen, gegen reale Disketten. Test-Rig:

- Linux PC + GW V4 + KryoFlux/SCP (falls vorhanden) + 3.5"/5.25"-Drive
- Disketten-Set "Golden Reference" mit Foto, SHA256, bekannten
  marginalen Tracks, bekannten Copy-Protection-Eigenschaften

HIL läuft nur lokal, nicht in GitHub Actions. Jeder Release-Tag
braucht einen passing HIL-Report unter `releases/<v>/hil_report.md`.

---

## Teil 5 — Test-Infrastruktur

| Layer | Werkzeug | Begründung |
|-------|----------|------------|
| Test-Runner | `pytest` | Standard, gut integriert mit allem |
| GUI-Tests | `pytest-qt` | Qt-Widget-Tests headless |
| Property-Tests | `hypothesis` | Generiert Edge-Cases automatisch |
| Fuzz | `atheris` | Crash-Tests für Format-Decoder |
| Coverage | `coverage.py` + `gcov` | C+Python parallel |
| Reports | `pytest-html` + JUnit-XML | HTML+CI |
| Diff-Compare | eigene Lib `uft_diff_test` | Differential + Registry |
| Mock-USB | `pyusb` mit Mock-Backend | Hardware-frei für CI |
| Snapshot | Git-LFS für `gw_corpus` | Binär-Files versioniert |

**Eigenes Werkzeug `uft_diff_test`:** Python-Library mit
`differential_command(...)` als Herzstück. In Tests dann nur

```python
def test_gw_read_ibm_360k():
    differential_command(
        command="read",
        args=["--format=ibm.dos.360"],
        input_file=corpus("flux_streams/ibm_360k_dos_boot.scp"),
    ).assert_pass()
```

CI-Pipeline mit jobs: unit / integration / conformance / improvement /
fuzz / perf. HIL läuft NICHT in GitHub Actions — manueller Trigger
über CHANGELOG-Eintrag "HIL: PASS".

Reports pro CI-Lauf: `report.html`, `report.junit.xml`,
`divergence_report.md`, `coverage.html`, `improvement_scoreboard.md`.

---

## Teil 6 — Wie Claude Code das baut

Zwei neue Subagenten landen mit P3 (Test-Strategie):

- `differential-test-author` (Sonnet) — gegeben GW-Command + Format,
  schreibt Differential-Test mit Corpus-Snapshot
- `improvement-test-author` (Sonnet) — gegeben Feature aus
  `DESIGN_PRINCIPLES.md`, schreibt Test der UFT-Überlegenheit beweist

Tasks landen in `docs/REFACTOR_TASKS.md` als P3.1–P3.4 (siehe dort).

---

## Teil 7 — Roadmap

- **Welle 1 (parallel zu P1):** `gw_corpus`-Skeleton, `uft_diff_test`,
  CI-Workflow-Grundgerüst, Divergence Registry mit 5 Bootstrap-Einträgen
- **Welle 2 (nach P1):** Per Command 1–5 Tests (~50), per Format
  Read+Write+Convert-Trio (~150), Wire-Protocol pro Firmware (3)
- **Welle 3 (parallel zu P2):** Forensic (~15), Multi-Device (~10),
  GUI-Smoke (~14), Property/Fuzz (~20)
- **Welle 4 (vor v4.1.4):** Test-Rig + Golden-Reference + erster
  vollständiger HIL-Lauf
- **Welle 5 (continuous):** Quartalsweiser Corpus-Refresh bei jedem
  GW-Release, HIL-Pflichtlauf bei jedem UFT-Major

---

## Teil 8 — Konkrete erste Schritte (wenn Welle 1 startet)

1. `tests/gw_corpus/inputs/` mit **einer** SCP-Datei (PC DOS) +
   SHA256-Manifest
2. `tools/uft_diff_test/__init__.py` mit `differential_command()`-Stub
3. `tests/conformance/divergence_registry.yaml` mit DIV-001
   (Banner-Differenz)
4. `tests/conformance/test_smoke.py` — trivialer Differential
   `uft --version`
5. Pytest grün → Skelett steht, alles weitere wird darauf gestapelt

---

## TL;DR

**Was:** 7-Schichten-Test-System.
**Warum Differential Testing:** "kompatibel zu GW" sonst Behauptung
statt Beweis.
**Was "besser" konkret heißt:** Improvement Test Suite, die zeigt was
UFT kann und GW erwartungsgemäß nicht.
**Wo Claude Code anpackt:** Nach P1, mit `differential-test-author` +
`improvement-test-author` und Tasks P3.1–P3.4.
**Was Axel selbst macht:** HIL-Lauf vor jedem Major-Release,
Golden-Reference-Disketten pflegen.
