# UFT v4.1.5 — Capability Matrix

**Stand:** 2026-05-26
**Quelle:** V415-PLAN Phase A.2 (Demo-Ready Release)
**Authoritative simulator coverage:** [`releases/v4.1.5/hil_simulated.md`](../releases/v4.1.5/hil_simulated.md)

Diese Datei ist die **einzige autoritative Capability-Liste** für v4.1.5.
SHOWCASE.md, RELEASE_NOTES.md und CLAUDE.md verlinken hierhin. Bei Konflikt
zwischen einer Behauptung im Code/in Tests und diesem Dokument: dieses
Dokument anpassen, nicht die Behauptung wegerklären.

---

## Legende

| Marker | Bedeutung |
|---|---|
| ✅ | Funktional verifiziert. Bei Hardware-Spalten: real-HW Tier-3 PASS. Bei Sim-Spalte: Tier-2.5 SIMULATED in `tests/hil/run_simulated.py`. |
| 🟡 | Wired (Code-Pfad existiert, kompiliert, byte-Protokoll dokumentiert) aber **nicht** Tier-3 hardware-verifiziert. Mock-only oder libusb-mock-validiert. |
| ⬜ | Scaffold / Honest-Stub. Capability-Schicht meldet `not_implemented`. Kein silent no-op. |
| `Linux` / `-` | Pfad existiert nur auf einer Plattform, andere ⬜. |
| `-` | Capability ist für diesen Controller protokoll-bedingt nicht vorgesehen (z.B. KryoFlux Write — read-only by design). |

**Tier-Definitionen** (aus `tests/HARDWARE_TRUTH_TESTS.md`):

- **Tier 3 — PASS:** Echter Controller, echte Diskette, byte-exakte Goldene-
  Referenz. Nur Greaseweazle hat das heute.
- **Tier 2.5 — SIMULATED:** Subprocess-Argv und I/O-Round-Trip OK, Provider-
  Kette ist nicht von echter HW gefüttert. Strikt zwischen NOT_RUN und PASS.
  Beweise: argv-Konstruktion, stdout-Parser, Ergebnis-File-Format.
  Beweist **nicht**: HW-Timing, USB-Lifecycle, Signalintegrität.
- **Tier 2.0 — mock-only:** libusb-Mock-Framework hat das Wire-Protokoll
  byte-exakt gegen Spec validiert (z.B. samdisk `SuperCardPro.h`). Kein
  echtes USB-Device gesehen.
- **Tier 1 — scaffold:** Header + Capability-Flags existieren, I/O-Pfad
  liefert `UFT_ERR_NOT_IMPLEMENTED`. Forensisch ehrlich, GUI greift sicher
  auf den Stub.

---

## Hardware-Controller

| Controller    | Read | Write | Tier-3 HW | Tier-2.5 Sim | Transport       | Status |
|---------------|------|-------|-----------|--------------|-----------------|--------|
| Greaseweazle  | ✅   | ✅    | PASS      | SIMULATED    | USB-CDC serial  | **production** |
| KryoFlux      | ✅   | -     | -         | SIMULATED    | DTC subprocess  | read-only (DTC-wrapper) |
| FluxEngine    | ✅   | ✅    | -         | SIMULATED    | CLI subprocess  | read+write (CLI-wrapper) |
| FC5025        | ✅   | -     | -         | SIMULATED    | fcimage CLI     | read-only (fcimage-wrapper) |
| SCP-Direct    | 🟡   | 🟡    | -         | mock-only    | libusb-direct   | libusb wired, 22/22 opcodes byte-exact vs samdisk |
| XUM1541       | 🟡   | 🟡    | -         | mock-only    | libusb-direct   | libusb wired, opencbm bus-timing pending |
| Applesauce    | 🟡   | ⬜    | -         | byte-compile | USB-CDC serial  | `?vers` handshake wired; `?disk` read-state-machine v4.1.6 |
| ADF-Copy      | 🟡   | ⬜    | -         | SIMULATED    | USB-CDC serial  | QSerialPort transport wired, Teensy-probe MF-213 |
| USB-Floppy    | Linux| Linux | -         | -            | SG_IO ioctl     | Linux-only via SG_IO; Win/Mac (DeviceIoControl/IOKit) v4.1.6 |

**TL;DR Hardware-Status v4.1.5:**
- **1/9 production:** Greaseweazle (Tier-3 PASS).
- **3/9 lesen real:** KryoFlux/FluxEngine/FC5025 über Subprocess-Wrapper.
- **3/9 wired aber mock-only:** SCP-Direct/XUM1541/Applesauce (libusb-mock-validiert; Tier-3 braucht echte HW + Bench-Session).
- **1/9 Linux-only:** USB-Floppy (SG_IO).
- **1/9 honest-stub mit Sim:** ADF-Copy.

**Was das für einen Demo-User heißt:**
- "Funktioniert garantiert ohne Tweak":  Greaseweazle.
- "Funktioniert wenn HW + Driver da":   KryoFlux, FluxEngine, FC5025.
- "Funktioniert wenn Mock-Validation Reality matched": SCP, XUM, Applesauce, ADFCopy.
- "Funktioniert nur unter Linux":       USB-Floppy.
- "Wird beim Connect-Click sauber abgelehnt mit Diagnose": alles wo `⬜`.

---

## Format-Plugins

**Status:** 80 Plugin-Registrierungen, 138 Format-IDs (1 Plugin kann mehrere
IDs registrieren, z.B. WOZ v1/v2/2.1).

| Metric | Wert | Quelle |
|---|---|---|
| Plugin-Registrierungen | 80 | `audit_plugin_compliance.py` |
| Format-IDs gesamt | 138 | `uft_format_registry.c` |
| `spec_status` populiert | **80/80 (100%)** | MF-262 |
| `features`-Matrix populiert | **80/80 (100%)** | MF-263 |
| Round-Trip getestet | 6/138 | `tests/conformance/` (IBM-DD, IBM-HD, AtariST, C64-GCR, Apple2-GCR, Amiga) |
| Differential vs `gw 1.23` | 6/6 byte-exakt | `tests/gw_corpus/` |

Plugin-Details siehe `src/formats/*/uft_*.c` (`spec_status` und `features`
fields). Vollständige Liste über `audit_plugin_compliance.py --list`.

**Was funktioniert garantiert in der Demo:**
- D64 / G64 (Commodore 1541)
- ADF (Amiga AmigaDOS)
- IMG / IMA / DMK (IBM PC)
- WOZ v1/v2/2.1 (Apple II)
- SCP / HFE / KryoFlux RAW (Flux-Container)

**Was funktioniert vermutlich** (Plugin registriert, kein Conformance-Test):
- Restliche 132 Format-IDs. Sie haben `spec_status` und `features`-Marker,
  aber kein automatisierter Round-Trip-Beweis. Bei einem Demo-Bug auf einem
  dieser Formate: KNOWN_ISSUE-Eintrag, kein silent fail.

---

## DeepRead / Recovery

| Modul | Status | Tier |
|---|---|---|
| Adaptive Decode (CRC-Fehler → OTDR → Re-Decode → Fusion) | wired | unit-getestet |
| Weighted Voting (Float-gewichtete Multi-Rev) | wired | unit-getestet |
| Encoding Boost (OTDR-Histogramm) | wired | unit-getestet |
| Write-Splice Detection | wired | unit-getestet |
| Magnetic Aging Profile | wired | unit-getestet |
| Cross-Track Correlation | wired | unit-getestet |
| Revolution Fingerprint | wired | unit-getestet |
| Soft-Decision LLR | wired | unit-getestet |

**Caveat:** Alle 8 DeepRead-Module sind als C-Modul implementiert und in der
GUI über `UftOtdrPanel` zugänglich. Real-Disk-Validierung gegen schwer
beschädigte Disketten ist work-in-progress; aktuelle Tests verwenden
synthetische Flux-Vektoren aus `tests/vectors/`.

---

## Kopierschutz-Erkennung

55+ historische Schutz-Schemes erkannt (V-MAX!, RapidLok, CopyLock, Speedlock,
ProLok, Vorpal, Rob Northen, Dungeon Master Fuzzy Bits, …). Liste in
`src/protection/` + Tests in `tests/test_protection_*.c`.

**Erkennung ≠ Bypass.** UFT dokumentiert, was es findet — kein Cracking-Tool.

---

## Forensik-Garantien

| Garantie | Status |
|---|---|
| MD5 / SHA1 / SHA256 / SHA512 parallel | ✅ |
| Hash-Chain für Integritätsnachweis | ✅ |
| Audit-Trail (40+ Event-Typen) | ✅ |
| Export: JSON / HTML / PDF / Markdown / XML / Plain | ✅ |
| Risiko-Scoring (0-100) | ✅ |
| LOSS.preflight Gate (alle 44 Konverter) | ✅ MF-263 |
| `.loss.json` Sidecar (LOSSY_DOCUMENTED Pfade) | ✅ MF-268 |
| Per-Track exakte Loss-Counts | v4.1.6 |
| Prinzip-7: `spec_status` für jedes Plugin | ✅ 80/80 |
| Prinzip-7: `features`-Matrix für jedes Plugin | ✅ 80/80 |

---

## Plattformen

| Plattform | Build | CI grün | Demo-getestet |
|---|---|---|---|
| Linux (Ubuntu 24.04, GCC 13, Qt 6.7.3 + 6.10.1) | qmake | ✅ | empfohlen (Phase C.1 Ziel-Plattform) |
| macOS (Clang, Qt) | qmake | ✅ | offen |
| Windows (MinGW-w64 g++ 13.1.0, Qt 6.10.1) | qmake | ✅ | offen |

Tests: 153/153 grün (ctest), Sanitizer (ASan+UBSan) grün, Coverage gemessen.

---

## Was UFT v4.1.5 NICHT kann

Bewusst weggelassen, dokumentiert in `RELEASE_NOTES_v4.1.5_DRAFT.md` §"What
this release does NOT ship":

- **Real-HW Verifikation** für SCP/XUM/Applesauce/ADFCopy (braucht Bench-Session)
- **UFI Windows + macOS Backends** (nur Linux SG_IO)
- **5-and-3 Apple GCR** (DOS 3.2 13-Sektor)
- **FM-Decoder Vollständigkeit** (`flux_decode_fm` als unvollständig markiert)
- **Per-Track exakte Loss-Counts** im Sidecar (aktuell category-level)
- **`uft-decode` CLI** (Scaffold existiert, Build-Wiring v4.1.6)
- **XUM1541 macOS .dylib loader**
- **Tier-3 HIL für nicht-GW-Controller**

Diese Punkte sind v4.1.6 Scope. Demo zeigt **was funktioniert**, nennt
das was nicht funktioniert ehrlich beim Namen.

---

## Verifikation dieser Matrix

Diese Tabelle wird beim Tag-Day gegen drei Quellen abgeglichen:

1. `releases/v4.1.5/hil_simulated.md` — Tier-2.5 SIMULATED-Liste muss passen
2. `audit_plugin_compliance.py` — Plugin-Compliance-Score muss passen
3. `src/hardwaretab.h:45-62` — V2-Provider-Liste muss passen

Wenn eine der drei Quellen mit dieser Matrix kollidiert: **CONSULT-Block
in der nächsten Session, Matrix korrigieren, nicht die Quelle ignorieren.**
