# UnifiedFloppyTool — Showcase

**Version:** v4.1.5 (Demo-Ready Release, 2026-06-15)
**Status:** Public release with documented limits.

---

## Was ist UFT?

UnifiedFloppyTool ist eine **forensische Sicherungsplattform für historische
Floppy-Disketten**. Sie liest Flux-Transitions auf 25-ns-Auflösung, dekodiert
über alle gängigen 8-Bit/16-Bit-Format-Konventionen hinweg (Commodore, Apple,
Atari, Amiga, IBM PC, Amstrad, BBC, Japan), und dokumentiert jedes Bit das
verloren ginge **bevor** es verloren geht — über einen Single-Chokepoint
LOSS.preflight an allen 44 Konvertierungspfaden.

Die Architektur ist **Type-Driven**: 9 Hardware-Controller liegen hinter einer
einheitlichen V2-Provider-Abstraktion mit Compile-Time-Konzepten und Honest-
Stub-Semantik — kein silent no-op wenn ein Controller nicht angeschlossen ist.

UFT ist Open Source, ohne Telemetrie, ohne Cloud-Bindung. Wird von Archiven,
Museen, Retrocomputing-Enthusiasten, digitalen Forensikern und Kopierschutz-
Forschern genutzt.

---

## Was geht heute

Detailliertes Capability-Bild: [`CAPABILITIES.md`](CAPABILITIES.md).

**Kurzform:**
- **1 Production-Controller** (Greaseweazle, Tier-3 PASS).
- **3 read-real** über Subprocess-Wrapper (KryoFlux, FluxEngine, FC5025).
- **3 libusb-wired**, byte-Protokoll-validiert via Mock (SCP-Direct, XUM1541, Applesauce).
- **1 USB-CDC-wired** mit Sim (ADF-Copy).
- **1 Linux-only** (USB-Floppy via SG_IO).
- **80 Format-Plugins**, 138 Format-IDs, 100% Prinzip-7-Compliance (`spec_status` + `features` populiert).
- **8 DeepRead-Module** (Adaptive Decode, Weighted Voting, Encoding Boost, Write-Splice, Magnetic Aging, Cross-Track, Revolution Fingerprint, Soft-Decision LLR).
- **55+ Kopierschutz-Schemes** erkannt + dokumentiert.

---

## Was nicht im Demo-Scope ist

- **Real-HW Tier-3 PASS** für SCP/XUM/Applesauce/ADFCopy (braucht Bench-Session, v4.1.6).
- **USB-Floppy auf Windows/macOS** (nur Linux SG_IO ist live).
- **5-and-3 Apple GCR** (DOS 3.2 13-Sektor, v4.1.6).
- **`uft-decode` CLI** (Scaffold da, Build-Wiring v4.1.6).
- **Per-Track exakte Loss-Counts** im Sidecar (aktuell category-level).

Vollständige Liste in [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md) (Einträge mit
`**Demo-Impact: vermeiden**`).

---

## 10-Minuten Demo

Detailliertes Skript + Assets: [`demo/QUICK_DEMO.md`](demo/QUICK_DEMO.md)
(landet in Phase C der Release-Plan).

Vorschau:

```
1. (1 min) git clone + cmake build → laufende GUI
2. (2 min) tests/hil/run_simulated.py → 10/10 SIMULATED zeigen
3. (3 min) GUI: fake-flux laden, decoded sectors zeigen
4. (2 min) Lossy convert → .loss.json Sidecar öffnen
5. (1 min) Plugin-Liste: 80/80 spec_status + features populiert
6. (1 min) audit/MASTER_REPORT.md zeigen
```

Anforderung: frischer Ubuntu 24 Laptop, **keine Hardware nötig**.

---

## Architektur-Highlight: Tier-2.5 Hardware-Simulation

Die größte Forensik-Confidence-Investition in v4.1.5 ist das **Tier-2.5
Simulator-System**: alle V2-Provider können end-to-end exercised werden
**ohne** die entsprechende Hardware zu besitzen.

Drei Mocking-Strategien für drei Transport-Layer:

| Transport | Mock-Strategie | Coverage |
|---|---|---|
| QProcess CLI subprocess (KryoFlux DTC, FluxEngine, FC5025) | Python fake-binary scripts on PATH | 7/7 SIMULATED |
| libusb-direct (SCP-Direct, XUM1541) | Scripted libusb-mock framework | 5+3 SIMULATED |
| USB-CDC virtual serial (GW, Applesauce, ADFCopy) | Python protocol simulators + com0com/socat | 3 byte-compile-verified |

CI läuft Audit-Job auf jeden Push, meldet SIMULATED/FAIL/NOT_RUN pro
Controller. **SIMULATED wird nie als PASS gemeldet** — echte Hardware
(Tier 3) bleibt die einzige PASS-Autorität.

Reproduzierbar lokal:

```bash
python3 tests/hil/run_simulated.py
# 10 SIMULATED · 0 FAIL · 0 NOT_RUN
```

---

## Architektur-Highlight: LOSS.preflight

Prinzip 1 lautet: *„Kein Bit verloren. Keine stille Veränderung. Keine
erfundenen Daten."* In v4.1.5 ist das strukturell erzwungen.

`uft_convert_file()` ist der **einzige** Chokepoint für alle 44 Konvertierungs-
pfade. Bevor irgendein Byte geschrieben wird:

- **LOSSLESS** runs silently as before.
- **LOSSY_DOCUMENTED** verlangt explizites `accept_data_loss=true`. Bei
  Success: `<target>.loss.json` Sidecar mit Kategorie-Loss-Einträgen
  (FLUX_TIMING, WEAK_BITS, INDEX_PULSES, MULTI_REVOLUTION).
- **IMPOSSIBLE** und **UNTESTED** brechen hart ab mit Diagnose.

User-Workflow-Auswirkung: GUI-Konverter die früher lossy silent liefen,
verlangen jetzt explizite Bestätigung. Wird im Demo-Schritt 4 gezeigt.

---

## Architektur-Highlight: SCP-Protokoll VENDOR-DOCUMENTED

Vor v4.1.5 trug `include/uft/hal/uft_scp_direct.h` sechs Placeholder-Opcodes
(`0x02-0x40`), traceable zu `a8rawconv` — nicht zur SCP-SDK-Quelle.
MF-261 hat alle 22 USB-Command-Bytes + Response-Code + Checksum-Init gegen
[samdisk `SuperCardPro.h`](https://github.com/simonowen/samdisk/blob/main/include/SuperCardPro.h)
cross-referenziert. **22/22 byte-exakt.**

Heißt: wenn die libusb-Wire-Implementation an echtem SCP-Device validiert ist
(Tier-3 Bench v4.1.5+1), kann das Tool ohne weitere Byte-Recherche read+write
ausführen.

---

## Forensik-Versprechen

UFT ist als forensisch verteidigbares Tool gebaut:

- Hash-Verifikation (MD5/SHA1/SHA256/SHA512 parallel)
- Hash-Chain für Integritäts-Nachweis
- Audit-Trail (40+ Event-Typen, Timestamps, CHS-Kontext)
- Export: JSON / HTML / PDF / Markdown / XML / Plain Text
- Risiko-Scoring 0-100 mit Recovery-Empfehlung
- LOSS.preflight Gate vor jedem Schreibvorgang

Jede Behauptung in dieser SHOWCASE ist überprüfbar:

- Capability-Claims  → [`CAPABILITIES.md`](CAPABILITIES.md)
- Code-Garantien    → [`DESIGN_PRINCIPLES.md`](DESIGN_PRINCIPLES.md)
- Bekannte Limits   → [`KNOWN_ISSUES.md`](KNOWN_ISSUES.md)
- Release-Notes     → [`../RELEASE_NOTES.md`](../RELEASE_NOTES.md)
- Architekturkontext → [`../CLAUDE.md`](../CLAUDE.md)

---

## Wie man es probiert

```bash
git clone https://github.com/Axel051171/UnifiedFloppyTool.git
cd UnifiedFloppyTool
cmake -B build && cmake --build build -j
./build/UnifiedFloppyTool   # GUI startet
```

Oder Binary aus dem
[GitHub Release v4.1.5](https://github.com/Axel051171/UnifiedFloppyTool/releases/tag/v4.1.5)
(`.deb` / `.dmg` / Windows `.zip`).

---

## Bug-Reports / Feature-Wünsche

GitHub Issues. Bitte mit `RELEASE_NOTES.md` v4.1.5 abgleichen — wenn der
Punkt dort als "v4.1.6" gelistet ist, ist er bekannt.

UFT folgt einer harten Konflikt-Hierarchie: **Forensik schlägt Performance.
Forensik schlägt UX. Security schlägt Kompatibilität.** Wenn ein Feature-
Request einer dieser Regeln widerspricht, wird er begründet abgelehnt.
