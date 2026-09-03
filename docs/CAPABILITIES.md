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
| XUM1541       | 🟡   | 🟡    | -         | mock-only    | libusb-direct   | libusb wired; Drahtprotokoll gegen die **OpenCBM-Quelle** geprüft (MF-301) — die alte Opcode-Tabelle war erfunden |
| Applesauce    | 🟡   | ⬜    | -         | byte-compile | USB-CDC serial  | `?vers` handshake wired; `?disk` read-state-machine **weiter offen** |
| ADF-Copy      | 🟡   | ⬜    | -         | SIMULATED    | USB-CDC serial  | QSerialPort verdrahtet, **Protokoll ungeprüft** (Opcodes aus eigenen Kommentaren, P3-31) |
| USB-Floppy    | Linux| Linux | -         | -            | SG_IO ioctl     | Linux-only via SG_IO; Win/Mac (DeviceIoControl/IOKit) **weiter offen** |

### Bench-Alter je Controller (MF-589)

Das Ziel dieses Releases verlangt, dass jede Aussage eine Quelle hat.
Für Hardware heißt das: **wann** wurde zuletzt an echtem Gerät geprüft?

| Controller | Letzter Tier-3-Bench | Quelle |
|---|---|---|
| Greaseweazle | **2026-05-15** (v4.1.4-rc1) | `RELEASE_NOTES.md` v4.1.5, Abschnitt „Hardware verification status"; der Produktionspfad ist seither byte-identisch |
| KryoFlux | **nie** | Subprocess-Wrapper um DTC; die Hardware-Verifikation liegt beim Fremdwerkzeug |
| FluxEngine | **nie** | dito, `fluxengine` CLI |
| FC5025 | **nie** | dito, `fcimage` CLI |
| SCP-Direct | **nie** | 22/22 Opcodes byte-exakt gegen samdisk — eine benannte Referenz, kein Gerät (UFT-008 offen) |
| XUM1541 | **nie** | Protokoll gegen OpenCBM-Quelle geprüft (MF-301), Emulator 56/56 — kein Gerät |
| Applesauce | **nie** | `?vers`-Handshake verdrahtet, byte-kompiliert — kein Gerät |
| ADF-Copy | **nie** | Transport verdrahtet, Teensy-Sonde — kein Gerät |
| USB-Floppy | **nie** | SG_IO nur unter Linux; UFI-Emulator treibt den Produktions-HAL |

**Ein einziger Bench, und der ist aus v4.1.4-rc1.** Das ✅ bei
Greaseweazle ruht darauf, dass sich der Code seither nicht geändert hat —
nicht auf einer neuen Messung.

> **Seit MF-848 ruht es nicht mehr allein darauf.** Der Bench sagt „am
> 15.05.2026 lief es an echtem Gerät"; die Frage danach war immer „und
> gilt das noch?". Beantwortet wurde sie durch *byte-identisch geblieben*
> — eine Aussage über den Code, nicht über sein Verhalten.
>
> `tests/test_gw_wire_conformance.c` fährt jetzt den **Produktions**-
> treiber (`src/hal/uft_greaseweazle_full.c`) gegen den
> **Firmware-Automaten** (`tests/emulators/greaseweazle/`), verbunden über
> die Byteebenen-Naht `uft_gw_open_stream()` (MF-686). Damit ist die
> beidseitige TRK0-Prüfung aus MF-799 — die Erkennung des
> **dekalibrierten Kopfes** — erstmals ohne Gerät prüfbar. Gemessen:
> nimmt man die zweite Richtung heraus, wird der Prüfstand rot
> (`seek(40) bei anliegendem /TRK0 gab 0, erwartet -7`).
>
> **Das ersetzt keinen Bench-Termin.** Der Prüfstand misst gegen ein
> Modell; wo Modell und Firmware auseinandergehen, geht er mit fehl —
> die bekannten Stellen stehen in
> `tests/emulators/greaseweazle/DIVERGENCES.md`. Was sich ändert, ist
> die **Reichweite einer Code-Änderung**: bisher entwertete jede
> Änderung am GW-Treiber den einzigen Bench des Projekts, weil
> „byte-identisch" die einzige Zusage war. Jetzt trägt eine Änderung
> einen eigenen Beweis, und der Bench muss nur noch das prüfen, was
> ein Modell nicht kann. Alles andere ist gegen **benannte
Referenzen** geprüft (samdisk, OpenCBM, Emulatoren), und das ist etwas
anderes als Silizium.

Es gibt kein Gerät hinter diesem Projekt (MF-310). Jeder Tier-3-Bench
muss von einem fremden Schreibtisch kommen — siehe den Aufruf im
`README.md`.

**TL;DR Hardware-Status v4.1.6 (unverändert gegenüber v4.1.5):**
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

**Status (Phase-0-Re-Verifikation 2026-07-05):** **84 Plugin-Structs**, 161
Format-IDs im Katalog (1 Plugin kann mehrere IDs bedienen, z.B. WOZ v1/v2/2.1;
einige IDs laufen über dedizierte Handler ohne Plugin-Struct, z.B. QDOS).

> ⚠ **MF-620: die beiden Zahlen haben verschiedene Quellen, und eine
> Zuschreibung hier war falsch.** Der Satz endete mit „Code-abgeleitet
> via `scripts/gen_format_list.py`" — dieses Skript liefert **88**
> Plugins (137 mit dem DSK-Makro), nie 161. Die 161 stammen aus einer Tabelle **ohne Aufrufer**, die bis MF-624 in
> `src/formats/uft_format_registry_v2.c` stand und seither als
> [`FORMAT_CATALOG.md`](FORMAT_CATALOG.md) gefuehrt wird — dort mit einer
> gemessenen Spalte: 95 der 162 Eintraege haben ein registriertes Plugin,
> 67 nicht. Sie beschreiben einen Katalog, nicht das
> Verhalten des Werkzeugs.

| Metric | Wert | Quelle |
|---|---|---|
| Plugin-Structs (`uft_format_plugin_t`) | 84 | `scripts/gen_format_list.py` / `audit_plugin_compliance.py` |
| davon auto-registriert (Makro) | 80 | `UFT_REGISTER_FORMAT_PLUGIN` |
| davon manuell registriert | 4 | g64, hfe, img, scp |
| Format-IDs im Katalog | 161 ⚠ | [`FORMAT_CATALOG.md`](FORMAT_CATALOG.md) — **eine Namensliste, kein Verhalten** (MF-619/624; die C-Datei dahinter hatte keinen Aufrufer und ist geloescht; 95 der 162 Eintraege haben ein Plugin, 67 nicht). Die Zahl beschreibt eine Tabelle, nicht das Verhalten des Werkzeugs. Registriert wird ueber `src/formats/format_registry/uft_format_registry.c:434`, gerufen von `src/main.cpp:43` |
| `spec_status` populiert | **84/84 (100%)** | MF-262 / audit |
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

## Nicht unterstützt: DDR- und Ostblock-Formate (MF-812)

Robotron/KC (MicroDOS, SCP, CP/A), Meritum und Pravetz werden **nicht**
unterstützt. Bis MF-812 lagen dafür drei Module im Baum — 114, 108 und
112 Zeilen unter `src/formats/eastblock/` —, die gebaut wurden und
**keinen einzigen Aufrufer** hatten: nicht in der Registry, nicht in
einem Test, in keinem Dokument.

Sie sind entfernt, und der Grund ist nicht die fehlende Verdrahtung,
sondern **was sie getan hätten, wäre sie da gewesen**. Alle drei
erkannten ausschließlich über die Dateigröße, und **5 ihrer 11 Größen
gehören Formaten, die dieser Baum auf T1b belegt hat:**

| Größe | behauptet | ist in Wahrheit |
|---|---|---|
| 737 280 | von **allen drei** (`KC MicroDOS 720KB`, `Meritum DS/DD 80T`, `Pravetz 8D DS/DD`) | `gw_img.img`, `gw_msx_2dd.img`, `mtools_fat12_720k.img` |
| 819 200 | `KC 85/4 DS 800KB` | `gw_sam.img`, `vice_c1541_80trk.d81` |
| 143 360 | `Pravetz 82 (Apple II)` | `gw_po.img` |
| 102 400 | `Meritum SS/SD 100KB` | `gw_ssd.img` |
| 89 600 | `TNS SS/SD 87KB` | `gw_northstar.img` |

Das ist „Größengleichheit ist keine Geometriegleichheit" (MF-784) in
Reinform, dreifach. Dazu waren die Angaben selbst erfunden: „KC 85/87"
gibt es nicht als Familie — der KC 85/2-4 kam aus Mühlhausen, der
KC 85/1 und KC 87 aus Dresden, zwei nicht kompatible Linien —, und
MicroDOS kennt kein 9×512-Format; das ist die IBM-PC-Geometrie unter
falscher Flagge.

**Wenn diese Formate aufgenommen werden, dann als CP/M-Diskdefs.** Sie
*sind* CP/M-Formate: `src/formats/cpm/uft_cpm_diskdefs.c` führt Spuren,
Sektoren, Sektorgröße, Blockgröße, Verzeichniseinträge, Skew und
**Bootspuren** bereits für 55 andere Systeme. Die Bootspur ist dabei der
Punkt, an dem eine Größenprüfung prinzipiell scheitern muss: CP/K legt
das Betriebssystem in die Systemspuren, MicroDOS ausdrücklich nicht
(`MICRODOS.SYS`) — zwei Abbilder desselben physischen Formats haben
damit verschiedene Länge.

> **Namenskollision, vor dem ersten Korpus-Eintrag zu klären:** in der
> DDR-Welt heißt **SCP** *Singlecomputer Control Program* und ist der
> Oberbegriff für die CP/M-kompatiblen DDR-Betriebssysteme (SCP1526,
> SCP1715, SCP/M). In diesem Baum ist SCP das SuperCard-Pro-Flussformat.
> Auf Dateiebene kollidiert das nicht — die Erkennung geht über die
> Kennung im Kopf —, auf Korpus- und Beschriftungsebene sehr wohl.

> **Und eine stillschweigende Annahme, die dort nicht trägt:** das
> Laufwerk K5601 formatiert bis 800 KB, zwei Seiten, 80 Spuren, **FM
> oder MFM**. Eine DDR-Diskette kann FM-kodiert sein.
