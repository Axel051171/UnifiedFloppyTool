# M3 — HAL-Stabilisierung & Release-Pipeline

**Stand:** 2026-04-24 (session-initiated)
**Ziel:** HAL-Schicht von „stubbed" → „production" für alle in CLAUDE.md
als supported genannten Controller. Emulator-CI-Pipeline live. Tag v4.3.0.

Dieser Plan ist der Master-Fahrplan für M3. Konkrete Implementations-
Tickets werden in den bestehenden TODO-Dokumenten weitergepflegt
(`docs/A8RAWCONV_INTEGRATION_TODO.md`, `docs/XCOPY_INTEGRATION_TODO.md`).

---

## Status-Update — 2026-05 (Reality-Check vor M3-Start)

Vor dem ersten M3-Transport-Commit wurde der Ist-Zustand gegen das
Hardware-Provider-Audit (`audit/`) und den lokalen Toolchain geprüft.
Drei Befunde, die den Plan unten relativieren:

1. **M3.1 SCP-Direct ist spec-blockiert.** Das Audit (`audit/scp/REPORT.md`,
   Finding SCP-D1-1, *high*) stuft die 6 SCP-Command-Bytes als
   `needs-source` ein — und vermutet sie sind **falsch**: das publizierte
   SCP SDK v1.7 nutzt einen gerahmten Command-Set (`[cmd][len][payload]
   [checksum]`) über FTDI-Serial, dessen Opcodes *nicht* im hier
   angenommenen `0x02-0x05`-Bereich liegen, und auch das Transport-Modell
   (USB-Bulk vs. FTDI-Serial) ist fraglich. Audit-Empfehlung wörtlich:
   **SCP SDK v1.7 vendoren BEVOR der Wiring-Commit.** M3.1 darf nicht
   gestartet werden, solange die Spec nicht vendored ist.

2. **M3.2 XUM1541 ist anders geformt als unten beschrieben.** Der Plan
   sagt „implementiere `src/hal/uft_xum1541.c` mit libusb". Aber der
   *Live-Pfad* — `xum1541_provider_v2.cpp`, was die GUI nutzt —
   delegiert an einen injizierten `Xum1541Runner`, der in Produktion die
   **OpenCBM-Dynamic-Library** lädt. `uft_xum1541.c` (libusb) wird nur
   vom Legacy-`uft_hal_unified.c` aufgerufen, *nicht* vom V2-Provider.
   M3.2 braucht daher zuerst eine Architektur-Entscheidung (welcher
   Pfad?) und dann die OpenCBM-Runner-Factory, nicht primär libusb-Code.
   Immerhin: die Protokoll-Konstanten sind spec-sauber (Audit
   `audit/xum1541/REPORT.md`: 82 PASS / 0 FAIL gegen OpenCBM).

3. **M3.3 Applesauce — Command-Vokabular `needs-source`.** Das Audit
   (`audit/applesauce/REPORT.md`, AS-D1-1) stuft die 18 Text-Command-
   Tokens als unvendored ein (wiki.applesaucefdc.com, nicht
   vendored). Milder als M3.1, aber der pure Text-Protokoll-Layer baut
   auf unverifiziertem Vokabular.

**Toolchain-Realität:** libusb-1.0, QtSerialPort und OpenCBM sind im
lokalen Build-Environment *nicht* installiert — alle drei M3.x-Transporte
sind Hardware- + Toolchain-gebundene Arbeit, nicht lokal verifizierbar.

**Konsequenz für die Reihenfolge:** M3.1 (im Plan P0) ist faktisch
*zuletzt* machbar (Spec-Blocker). M3.2 hat die sauberste Spec, braucht
aber die Pfad-Entscheidung. Die Milestone-Beschreibungen unten bleiben
als Ziel-Definition gültig, sind aber mit diesen Befunden zu lesen.

---

## Startzustand (Ende M2)

Aktiver HAL-Code (`src/hal/`):

| Backend         | C-HAL       | Qt-Provider | Status lt. CLAUDE.md  |
|-----------------|-------------|-------------|----------------------|
| Greaseweazle    | real        | real        | production           |
| KryoFlux        | DTC-subprocess | real     | read-only via CLI    |
| FC5025          | CLI-subprocess | real     | read-only via CLI    |
| SuperCard Pro   | **stubbed** | real        | HAL stubbed          |
| XUM1541         | **stubbed** | real        | HAL stubbed          |
| Applesauce      | **stubbed** | real        | HAL stubbed          |
| UFI/Cowork      | **planned** | —           | own firmware         |

CLAUDE.md-Eintrag für "stubbed" muss am Ende von M3 auf einen der
folgenden Werte umgestellt werden: `production`, `read-only`, oder
`not planned`. Kein `stubbed` mehr.

---

## Milestones innerhalb M3

### M3.1 — SCP-Direct HAL (P0)

Bezug: `docs/A8RAWCONV_INTEGRATION_TODO.md` TA4.

**Muss:**
- `src/hal/uft_scp_direct.c` (neu) mit USB-Bulk-Transfer-Protokoll
  direkt gegen SuperCard Pro (ohne SCP.exe subprocess).
- Protokoll-Doku: SCP USB commands 0x02 (revs), 0x03 (track select),
  0x04 (read flux), 0x05 (write flux). Referenz: a8rawconv/scp.cpp.
- Capability-Flags: `can_read_flux=true`, `can_write_flux=true`,
  `can_read_sector=false`.
- Test: `tests/test_scp_direct_backend.c` — ohne Hardware ein Mock-USB
  (fake file descriptor) der die erwarteten Commandbyte-Sequenzen
  zurückgibt.

**Abschluss-Kriterium:** SCP über USB direkt, ohne SCP.exe.

### M3.2 — XUM1541 HAL (P1)

Bezug: XUM1541-Protokoll + ZoomFloppy-Firmware.

**Muss:**
- `src/hal/uft_xum1541.c` — libusb-basierte Kommunikation mit XUM1541
  (Commodore IEC-bus via USB).
- Bus-Commands: READ_TRACK, WRITE_TRACK, STEP, RESET.
- Capability-Flags: `can_read_flux=false` (IEC-Bus liefert Sektor-
  daten), `can_read_sector=true` (D64/G64 read), `can_write_sector=true`.
- Integration mit dem uft_cbm_drive-Abstrahierungslayer.

**Abschluss-Kriterium:** Commodore-Disks via ZoomFloppy-Hardware lesen
und schreiben ohne Python-Subprocess-Umweg.

### M3.3 — Applesauce HAL (P1)

Bezug: Applesauce text-protokoll spec.

**Muss:**
- `src/hal/uft_applesauce.c` — serielles Protokoll (115200 8N1) mit
  textbasierten Befehlen ("track 00 h0 capture 3 revolutions").
- Output-Parser für Applesauce's WOZ/MOOF/A2R-Ausgabe.
- Capability-Flags: `can_read_flux=true` (Applesauce ist flux-native),
  `can_write_flux=true`, `format_native=WOZ`.

**Abschluss-Kriterium:** Applesauce in einem Qt6-Tab ohne Python-
Brücke.

### M3.4 — UFI/Cowork-Backend (P2)

Bezug: eigene UFI-Firmware (STM32H723ZGT6).

**Muss:**
- `src/hal/uft_ufi.c` — nur das Desktop-Ende (Firmware ist separates
  Repo).
- Capability-Flags per Handshake mit Firmware (dynamisch, statt
  hardcoded — UFI kann sich erweitern).
- `uft-stm32-portability`-Skill aktivieren für alle geteilten Header.

**Abschluss-Kriterium:** Desktop spricht mit UFI über USB, Firmware
entscheidet über Capabilities.

### M3.5 — Emulator-CI-Pipeline (P1)

Bezug: `docs/KNOWN_ISSUES.md` §6.1.

**Muss:**
- `.github/workflows/emulator.yml` — läuft bei PRs die `src/flux/`,
  `src/formats/adf/`, `src/formats/d64/` berühren.
- WinUAE im Docker-Container: lädt UFT-decodiertes ADF, prüft ob es
  bootet; passt Decode-Regressionen ab.
- VICE-x64 für D64: bootet ADF, `dir$` vergleichen vs. erwarteter
  Ausgabe.
- Ergebnis pro Format als CI-Gate.

**Abschluss-Kriterium:** ADF/D64 decode-Regression wird automatisch
erkannt bevor Code auf main landet.

### M3.6 — CLAUDE.md HAL-Update (P2)

**Muss:**
- Alle `stubbed`-Einträge in CLAUDE.md entfernen (entweder auf
  `production` / `read-only` umstellen, oder in `not planned`-Sektion
  migrieren).
- HAL-Kompatibilitäts-Matrix aktualisieren pro Controller × Format.

**Abschluss-Kriterium:** CLAUDE.md spiegelt die reale HAL-Situation
wider.

### M3.7 — Tag v4.3.0 (P0)

**Muss:**
- Alle M3-Items oben abgeschlossen.
- CI grün auf allen Plattformen.
- CHANGELOG.md aktualisiert (jede M3-Milestone als eigener Eintrag).
- Release-Notes mit BAMCOPY/CHECKDISK/Salvage als User-facing
  Highlights.

---

## Abhängigkeits-Graph

```
M3.1 SCP-Direct ──┐
                   ├── M3.5 Emulator-CI (braucht stable HAL-Back-ends)
M3.2 XUM1541  ────┤
                   │
M3.3 Applesauce ──┤
                   │
M3.4 UFI      ────┤
                   │
                   └── M3.6 CLAUDE.md-Update ─── M3.7 Tag v4.3.0
```

M3.1-M3.4 sind unabhängig und können parallelisiert werden. M3.5
braucht ≥ 1 fertigen Backend-Pfad zum Start.

---

## Nicht-Ziele für M3

- Kein neuer Format-Parser (M1/M2-Thema).
- Kein neues Protection-Scheme (M1/M2-Thema).
- Kein GUI-Redesign (separat, später).
- Keine Dark-Mode / Accessibility-Arbeit (separat).
- **Keine Firmware-Portierung** (UFI-Firmware hat eigenes Repo, hier
  nur Desktop-Seite).

---

## Zeitabschätzung

- M3.1 SCP-Direct: 1 Woche (Protokoll-Port, Mock-USB-Test)
- M3.2 XUM1541:     1 Woche (libusb-Integration, Bus-Semantik)
- M3.3 Applesauce:  1 Woche (text-Protokoll, Output-Parser)
- M3.4 UFI:         2 Wochen (Handshake-Design, dynamische Caps)
- M3.5 Emulator-CI: 1 Woche (Docker + WinUAE/VICE)
- M3.6 + M3.7:      parallel, 2 Tage

**Gesamt: 4-6 Wochen.** Entspricht der MASTER_PLAN-Schätzung.

---

## Verwandte Dokumente

- `docs/MASTER_PLAN.md` — Master-Fahrplan
- `docs/A8RAWCONV_INTEGRATION_TODO.md` — TA4 SCP-Direct Detail
- `docs/XCOPY_INTEGRATION_TODO.md` — T1-T8 Amiga Detail (M2-Bezug)
- `docs/DESIGN_PRINCIPLES.md` — Prinzip 6 Emulator-Kompatibilität
- `docs/KNOWN_ISSUES.md` §6 — Emulator-Pipeline-Anforderungen
- `.claude/skills/uft-hal-backend/SKILL.md` — wie neue HAL-Backends
  strukturell aussehen sollen
- `.claude/skills/uft-stm32-portability/SKILL.md` — wichtig für M3.4
