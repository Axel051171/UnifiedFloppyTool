# Gutachten: davidgiven/fluxengine — USB-Drahtprotokoll

**Zyklus:** 2026-09-04 · **Auftrag:** Eigentümer-Entscheidung P3-98
(nativer FluxEngine-Treiber; Reihenfolge Firmware-Automat → Treiber)
**Geklonter Stand:** `tools/uft-scout/work/fluxengine`, HEAD
`909fac7282bae38494886c8605a72375dc6822dc` (2026-06-18, flacher Klon).
**Messdatei:** `tools/uft-scout/work/fluxengine.messung.json` (834 Dateien,
automatische Zone PRUEFEN — Begründung siehe §6, die Automatik stolpert
über `dep/`-Mehrfachlizenzen, nicht über den Protokollteil).

**Neubesuchs-Anlass (AGENT.md Regel 6):** `data/known_negatives.json`
führt `davidgiven/fluxengine` als `bewertet` mit Grund „Umsetzungsplan
liegt vor (VFS, textpb-Ernte, VCD)". Die Drahtprotokoll-Frage ist darin
nicht enthalten; sie ist neu seit der Eigentümer-Entscheidung in
`docs/OPEN_ITEMS.md` P3-98 (Zeile 282) und dem Greaseweazle-Präzedenzfall
MF-848.

---

## Pflichtfelder

| Feld | Wert |
|---|---|
| Kategorie | **Daten/Spec** (Schnittstellenfakten für einen Drahtautomaten), sekundär Oracle-Grundlage |
| Lizenzzone | Hauptbaum **GPL-2.0-only** → Matrix-Zeile GRÜN (`playbook/lizenzmatrix.md:8`); Urteil beim Eigentümer (MF-679). `dep/`-Unterverzeichnisse abweichend, für das Protokoll irrelevant (§6) |
| Attribution | Verhaltens-Spec nach `protocol.h`, `lib/usb/fluxengineusb.cc`, `FluxEngine.cydsn/` aus davidgiven/fluxengine (GPL-2.0-only), Commit 909fac72 — als Referenz im Header zu führen, nicht als Port |
| Bewegte Kennzahl | Keine der vier direkt — dieselbe Lage, die P3-98 bereits festhält: ein Drahtautomat senkt das Bench-Alter nicht, „verkleinert aber, was ein Bench fangen muss" (MF-848-Muster) |
| Inventar | siehe unten, zitiert |
| Einhängepunkt | `docs/OPEN_ITEMS.md` P3-98 (mit `git grep P3-98` auffindbar); Bauform-Vorbild `tests/emulators/greaseweazle/gw_wire_bridge.h` + `uft_gw_stream_ops_t` (MF-848) |
| Oracle-Kandidat | der zu bauende Firmware-Automat selbst; externe Referenz = diese Quelltexte. **Eine zweite unabhängige Protokoll-Umsetzung existiert nicht** (§7) |
| Beschaffung | optional: USB-Mitschnitt (pcap) einer echten `fluxengine read`-Sitzung aus der Community — im Korpus (40 Abbilder, `inv["korpus"]`) liegt nichts dergleichen; Abbilder helfen hier nicht, es geht um die Leitung |
| Aufwandsklasse | Automat: **M** (Kommandoebene klein, 12 Kommandos; Fluxstrom-Erzeugung ist der Kern); Treiber danach: M |
| Differenzlauf | entfällt — keine „besser als unseres"-Behauptung |

**Inventar-Abfragen (wörtlich):**

```
"fluxengine": { "vorhanden": true, "abgedeckt": true,
  "treffer": ["flux", "fluxengine"], "schwache_treffer": [],
  "plugin_liste_vollstaendig": true }

"usb wire protocol state machine": { "vorhanden": false, "abgedeckt": false,
  "hinweis": "INVENTAR DECKT DAS NICHT AB … keine Faehigkeiten …" }
```

Der starke Treffer `fluxengine` meint den Qt-Provider und den
CLI-Emulator — von Hand nachgesehen: einen Drahtautomaten oder ein
libusb-Backend gibt es im Baum nicht. `src/hal/` hat kein
FluxEngine-Backend (CLAUDE.md: „FluxEngine + UFI/USB-Floppy exist as Qt
providers but not as HAL backends"); `tests/emulators/fluxengine/DIVERGENCES.md`
FE-3 sagt selbst, dass er die CLI modelliert, nicht die Leitung. Kein
Dubletten-Risiko.

---

## 1. `protocol.h` — die Konstanten (alle: `protocol.h`, Stand 909fac72)

| Konstante | Wert | Fundstelle |
|---|---|---|
| `FLUXENGINE_PROTOCOL_VERSION` | **17** | protocol.h:6 |
| `FLUXENGINE_VID` | 0x1209 | protocol.h:8 |
| `FLUXENGINE_PID` | 0x6e00 | protocol.h:9 |
| `FLUXENGINE_DATA_OUT_EP` | 0x01 | protocol.h:13 |
| `FLUXENGINE_DATA_IN_EP` | 0x82 | protocol.h:14 |
| `FLUXENGINE_CMD_OUT_EP` | 0x03 | protocol.h:15 |
| `FLUXENGINE_CMD_IN_EP` | 0x84 | protocol.h:16 |
| `*_EP_NUM` (Firmware-Seite) | jeweils `EP & 0x0f` | protocol.h:19–22 |
| `FRAME_SIZE` | 64 | protocol.h:32 |
| `TICK_FREQUENCY` | 12 000 000 (12 MHz) | protocol.h:33 |
| `PRECOMPENSATION_THRESHOLD_TICKS` | `(int)(2.25 * 12)` = 27 | protocol.h:37 |

Dazu Bitfelder: `SIDE_SIDEA/B` = Bit 0 (protocol.h:24–25),
`DRIVE_0/1` = Bit 0, `DRIVE_DD/HD` = Bit 1 (protocol.h:27–30).
Fehlercodes `F_ERROR_NONE/BAD_COMMAND/UNDERRUN/INVALID_VALUE/INTERNAL`
= 0..4 (protocol.h:72–79). Indexmodi `F_INDEX_REAL/300/360` = 0..2
(protocol.h:81–86).

Historische Stabilität: letzte Änderung an `protocol.h` am **2022-03-26**
(„Bump the protocol version to ensure people upgrade"), davor 2022-03-06
(Multi-Read-Refactor) — seitdem über vier Jahre unverändert
(GitHub-Commitliste zu `protocol.h`, Web-Abruf 2026-09-04; im flachen
Klon nicht messbar). Version 17 ist also seit 2022 der Stand.

## 2. Rahmen-Typen und Byte-Größen

24 Rahmentypen `F_FRAME_ERROR` = 0 bis `F_FRAME_MEASURE_VOLTAGES_REPLY`
= 23, fortlaufend nummeriert (protocol.h:44–70). Zuordnung Typ → Struct
steht als Kommentar an jedem Enum-Wert.

**Packung: es gibt keine.** `#pragma pack` und `__attribute__((packed))`
kommen im gesamten Repo nicht vor (grep über `*.h`, `*.c`, `*.cc`,
0 Treffer). Das Protokoll verlässt sich auf **natürliche Ausrichtung**
— beide Seiten kompilieren denselben Header. Ein Automat darf die
Größen deshalb **nicht als gepackt rechnen**, sondern muss sie messen.
Gemessen (MinGW gcc 13.1.0, x86-64, Testprogramm im Scratchpad; die
Member-Typen sind ≤ 4 Byte, ARM-EABI der PSoC-Seite richtet identisch
aus — als Annahme markiert, nicht als Messung der Cortex-M3-Seite):

| Struct | sizeof | Bemerkung | Definition |
|---|---|---|---|
| `frame_header` | 2 | `type`, `size` je uint8 | protocol.h:96–100 |
| `any_frame` | 2 | | protocol.h:102–105 |
| `error_frame` | 3 | | protocol.h:107–111 |
| `debug_frame` | 62 | `char payload[60]` | protocol.h:113–117 |
| `version_frame` | 3 | | protocol.h:119–123 |
| `seek_frame` | 3 | | protocol.h:125–129 |
| `measurespeed_frame` | 3 | | protocol.h:131–135 |
| `speed_frame` | 4 | `period_ms` uint16 @2 | protocol.h:137–141 |
| `read_frame` | **8** | `milliseconds` uint16 **@4** (1 Füllbyte @7) | protocol.h:143–150 |
| `write_frame` | **12** | `bytes_to_write` uint32 **@4** (Füllbyte @3, drei @9–11) | protocol.h:152–158 |
| `erase_frame` | 4 | | protocol.h:160–165 |
| `set_drive_frame` | 5 | | protocol.h:167–173 |
| `voltages` | 4 | 2 × uint16 | protocol.h:175–179 |
| `voltages_frame` | 42 | header + 10 × `voltages` | protocol.h:181–194 |

Die Offsets der Mehrbytefelder (`milliseconds` @4, `bytes_to_write` @4)
sind mitgemessen. Füllbytes gehen mit über die Leitung; der Client
initialisiert per Designated-Initializer, also Null.

**Byteordnung:** little-endian. Der Client serialisiert `milliseconds`
und `bytes_to_write` von Hand LE (fluxengineusb.cc:254–256, 281–284)
und konvertiert die Spannungswerte beim Empfang
(`read_short_from_usb`, fluxengineusb.cc:11–16 — Kommentar: „the board
always operates in little-endian mode"). Inkonsequenz im Client:
`speed_frame.period_ms` wird **ohne** Konvertierung gelesen
(fluxengineusb.cc:164) — auf LE-Hosts folgenlos, auf BE-Hosts ein
Client-Fehler. Für den Automaten gilt: Leitung ist LE.

Rahmenlänge auf der Leitung: `send_reply()` sendet `f->f.size` Bytes
(main.c:191–196, per `DECLARE_REPLY_FRAME` = sizeof, main.c:47–48);
der Client liest immer `FRAME_SIZE` = 64 (fluxengineusb.cc:21, 111).
Die Firmware **validiert beim Empfang weder Länge noch `size`-Feld**
(`handle_command`, main.c:820–875: liest bis zu 64 Byte, verzweigt nur
über `f->f.type`).

## 3. Antwort-Ablauf

`await_reply<T>(desired)` (fluxengineusb.cc:106–123):

1. blockierend 64 Byte von CMD_IN lesen;
2. `F_FRAME_DEBUG` → Payload auf stdout, **weiterlesen** (Schleife);
3. Typ ≠ erwartet → `bad_reply()` (fluxengineusb.cc:88–104):
   - Typ auch ≠ `F_FRAME_ERROR` → `error("bad USB reply 0x…")`;
   - `F_FRAME_ERROR` → Fehlercode auflösen; benannt sind nur
     `BAD_COMMAND` und `UNDERRUN`, `INVALID_VALUE` (3) und `INTERNAL`
     (4) fallen in `default:` „unknown device error".
   `error()` ist `[[noreturn]]` und wirft `ErrorException`
   (lib/core/globals.h:52–54) — jeder Protokollverstoß ist ein harter
   Abbruch, keine Wiederaufnahme.

**Zeitlimit: keines.** Im gesamten `lib/usb/`-FluxEngine-Pfad gibt es
keinen `set_timeout`-Aufruf (grep; einziger Treffer ist `serial.cc`
für die Applesauce-Seite). libusbp wartet ohne `set_timeout`
unbegrenzt („If this function is not called, the default behavior of
the handle is to have no timeout" — libusbp.h, pololu/libusbp master;
die exakte von fluxengine gepinnte Revision liegt unter `dep/r/` und
ist im Klon nicht enthalten → UNGEKLÄRT #2). Eine stumme Firmware
hängt den Client für immer.

**Firmware sendet nie `F_FRAME_DEBUG`.** `print()` geht auf UART
(main.c:127–138); grep über das Repo findet `F_FRAME_DEBUG` nur in
protocol.h:47 und im Client (fluxengineusb.cc:113). Die
DEBUG-Behandlung des Clients ist defensiver Altbestand. Ein Automat,
der DEBUG-Rahmen einstreut, prüft den Client-Pfad, bildet aber kein
belegtes Firmware-Verhalten ab — gehört in sein `DIVERGENCES.md`.

Unbekanntes Kommando: Firmware antwortet `F_FRAME_ERROR` mit
`F_ERROR_BAD_COMMAND` (main.c:873–874).

## 4. Flux-Kodierung der READ-Antwort

Die Kodierung entsteht in **Hardware** (PSoC-UDB), nicht in
Firmware-C: `FluxEngine.cydsn/Sampler/Sampler.v`.

- 6-Bit-Zähler `counter`, getaktet mit `sampleclock` (Sampler.v:24,47);
  die Tick-Einheit ist `TICK_FREQUENCY` = 12 MHz (protocol.h:33).
- Bei Flanke auf `rdata` oder `index` **oder** bei `counter == 0x3f`
  wird ein Byte emittiert: `opcode <= { rdata_edge, index_edge,
  counter }` — Bit 7 = `F_BIT_PULSE` (0x80), Bit 6 = `F_BIT_INDEX`
  (0x40), Bits 5:0 = Tickzahl. Danach `counter <= 1` („remember to
  count this tick") (Sampler.v:70–76).

**Fortsetzung, präzisiert gegen das Vorwissen:** die Marke ist
tatsächlich das Byte `0x3F` (beide Event-Bits leer), aber die Schwelle
liegt bei **63, nicht 62**: das Byte wird emittiert, wenn der Zähler
0x3F **erreicht**, und trägt den Wert 63. Das Intervall ist die
**Summe der unteren 6 Bits** aller Bytes bis einschließlich des Bytes
mit gesetztem Event-Bit; der Leser rechnet exakt so
(`ticks += b & 0x3f`, fluxmapreader.cc:28). Die 62 aus dem Vorwissen
stammt von der **Gegenrichtung**: der Client-Encoder
(`Fluxmap::appendInterval`, fluxmap.cc:40–48) spaltet mit
`while (ticks >= 0x3f)` ab, sein abschließendes Event-Byte trägt also
0–62 Ticks — die Hardware kann dagegen ein Event-Byte mit 63 senden
(Flanke exakt bei `counter == 0x3f` → z. B. 0xBF). Ein Automat, der
0xBF/0x7F als illegal behandelt, wäre falsch. Und: `0x3F` ist **kein
reserviertes Zeichen**, nur ein Wert ohne Event-Bits.

- `F_DESYNC` = 0x00 (protocol.h:92, „obsolete"): der Leser beendet die
  Akkumulation bei einem Nullbyte mit Ereignis 0
  (fluxmapreader.cc:29–32, Bedingung `!b`); der Client-Encoder erzeugt
  es nur noch über `appendDesync()` (fluxmap.cc:63–67) zur Trennung
  mehrerer Umdrehungen in Dateien. `F_EOF` = 0x100 ist rein
  bibliotheksintern („synthetic, only produced by library",
  protocol.h:93), existiert auf der Leitung nicht.
- Schreibrichtung (`Sequencer.v:59–86`): `opcode[7]` = Puls am Ende
  des Intervalls, `opcode[5:0]` = Ticks (mit `-1`-Kompensation für den
  Automaten-Takt, Null gesondert behandelt), **`opcode[6]` wird beim
  Schreiben ignoriert**. Bei Datenmangel füllt die Firmware mit
  `0x3f`-Bytes auf (main.c:606) — pulslose Verzögerung.

**Transport der READ-Daten:** DATA_IN in 64-Byte-Paketen
(`BUFFER_SIZE` 64, main.c:36; DMA-Ringpuffer 64 × 64, main.c:35–38),
abgeschlossen mit einem **Zero-Length-Paket** (main.c:513,
`USBFS_LoadInEP(NULL, 0)`). Der Client liest in 32-KiB-Blöcken
(`MAX_TRANSFER`, fluxengineusb.cc:9) und endet beim kurzen Transfer
(fluxengineusb.cc:60–61). Erst **nach** dem Datenstrom kommt
`F_FRAME_READ_REPLY` bzw. bei Abbruch `F_ERROR_UNDERRUN` auf CMD_IN
(main.c:516–531). Der Client deckelt eine READ-Antwort hart bei
**1 MiB** (fluxengineusb.cc:261) — läuft der Strom darüber hinaus,
hört der Client auf zu lesen, während die Firmware in
`wait_until_writeable` auf den DATA-Endpunkt wartet (main.c:179–183,
505): aus dem Quelltext gelesen eine wechselseitige Blockade ohne
Zeitlimit (Watchdog-Verhalten der Firmware dabei UNGEKLÄRT #3).

`read_frame.milliseconds` ist die Messdauer (Firmware bricht die
DMA-Kette nach Ablauf ab, main.c:478–490); `synced` = 1 wartet vor
Beginn auf den Indeximpuls (main.c:440–447); `hardsec_threshold_ms`
steuert die Hartsektor-Indexlogik (ISR main.c:74–110).

## 5. Versionsabgleich beim Verbindungsaufbau

Im Konstruktor des Clients: `getVersion()` sendet
`F_FRAME_GET_VERSION_CMD`, erwartet `version_frame`; weicht
`r->version` von 17 ab, wird `error("your FluxEngine firmware is at
version {} but the client is for version {}; please upgrade")`
geworfen — Verbindungsaufbau schlägt fehl, keine Aushandlung, kein
Rückfall (fluxengineusb.cc:72–79). Die Firmware antwortet stets mit
ihrer Kompilierzeit-Konstante (main.c:221–226). Dass der Fall real
vorkommt, belegt Issue #767 („firmware is at version 15 but the client
is for version 17"). Für den Automaten heißt das: **exakte Gleichheit
oder harter Abbruch** — dieselbe Prüfschärfe wie die
Mindest-Firmware-Zusage aus MF-849 auf der GW-Seite, nur strenger
(Gleichheit statt Minimum).

## 6. Lizenzurteil (Vorlage, kein Urteil — MF-679)

- **`COPYING.md` (Wurzel, 350 Zeilen): „The FluxEngine code as a whole
  is GPL 2.0-licensed … Note: FluxEngine is GPL 2.0, not GPL
  2.0-or-later."** Danach folgt der vollständige GPLv2-Text.
- `protocol.h` trägt **keinen eigenen** Copyright-/SPDX-Kopf
  (0 Treffer für „Copyright"/„SPDX" in der Datei) → fällt unter die
  Repo-Lizenz GPL-2.0-only. Gleiches gilt für `lib/usb/` und
  `FluxEngine.cydsn/main.c` samt Verilog.
- Vendoring geprüft: eigene Lizenzdateien nur `dep/adflib/COPYING`
  (GPL-2.0-Text; die Messdatei markiert „GPL-2.0 + LGPL (mehrdeutig)")
  und `dep/hfsutils/COPYING` (GPL-2.0). Das README behauptet für
  `dep/emu` und `dep/agg` BSD-3 (README.md:230–237) — README-Angabe,
  keine Lizenzdatei bei maxdepth 2; für die Protokollfrage ohne
  Belang, da kein `dep/`-Code berührt wird. Deshalb steht die
  automatische Gesamtzone der Messdatei auf PRUEFEN.
- **Das Vorwissen „Public-Domain-artig" ist widerlegt**: das README
  selbst sagt „licensed under the GPL 2.0-only open source license
  (not any later version)" (README.md:226–228).
- Einordnung nach Matrix: GPL-2.0(-only) steht in Zone **GRÜN**
  (`playbook/lizenzmatrix.md:8` — Port mit Attribution zulässig,
  samdisk-Muster). Für den beschlossenen Weg wird ohnehin **kein
  Code portiert**: der Automat ist ein Neuschrieb gegen die hier
  destillierten Schnittstellenfakten; Kommandonummern, Rahmenlayouts
  und Tickraten sind Schnittstellenfakten (dieselbe Abgrenzung, die
  P3-107/ADF-Copy für GPLv3 zieht — hier ist die Lage milder, weil
  selbst der Port matrix-grün wäre). **Entscheidung liegt beim
  Eigentümer.**
- Rand-Notiz: die pid.codes-Registrierung von 1209:6E00 führt als
  Lizenzfeld „MIT" — das ist ein Metadatum der Registrierung, kein
  Beleg über den Quellcode; maßgeblich bleibt `COPYING.md`.

## 7. Zweite Hand: **nicht gefunden**

Gesucht (Web, zweifach; GitHub-Code-Suche war mangels Zugangsdaten
nicht verfügbar): eine unabhängige Client- oder Firmware-Umsetzung des
FluxEngine-Board-Protokolls existiert nach diesen Suchen **nicht**.
Die Interoperabilität läuft in Gegenrichtung: der FluxEngine-**Client**
spricht Greaseweazle- und Applesauce-Protokolle
(lib/usb/usbfinder.cc:12, greaseweazleusb.cc, applesauceusb.cc), aber
kein fremdes Werkzeug spricht das Board-Protokoll. Einzige unabhängige
Bestätigung ist die pid.codes-Registrierung — und die belegt nur
VID/PID.

**Konsequenz, ausdrücklich:** der Automat ruht auf **einer** Quelle
(Client und Firmware stammen vom selben Autor im selben Repo und sind
füreinander geschrieben — das sind zwei Seiten, keine zwei Hände).
Das gehört als Erstzeile in das `DIVERGENCES.md` des künftigen
Automaten, zusammen mit: DEBUG-Rahmen nie firmwareseitig belegt (§3),
ARM-Ausrichtung angenommen statt gemessen (§2), Watchdog-Verhalten
ungeklärt (§4). Mildernd: `protocol.h` ist seit 2022-03 unverändert,
und Client↔Firmware desselben Standes sind per Versionsgleichheit
(§5) aneinander gebunden.

## 8. Die Naht in UFT — Beobachtung, und eine Stelle, an der der Plan präziser werden muss

`FluxEngineProviderV2` nimmt im Konstruktor einen `FluxEngineRunner`
(fluxengine_provider_v2.h:237, gehalten als `m_runner` :279):

```
using FluxEngineRunner = std::function<FluxEngineRunResult(
    const std::vector<std::string>& /*argv*/,
    const std::string&              /*stdin_data*/)>;   // :200–202
```

Das ist eine **argv-Naht**: sie transportiert CLI-Argumente. Ein
„zweiter, USB-basierter Runner" hinter dieser Signatur müsste
fluxengine-Kommandozeilen **parsen** und in Rahmen übersetzen — er
erbte damit exakt das CLI-Dialekt-Risiko, das
`tests/emulators/fluxengine/DIVERGENCES.md` FE-3 als HIGH führt
(Flag-Drift zwischen fluxengine-Versionen). **Messung schlägt Plan:**
die tragfähige Naht für einen nativen Treiber liegt eine Ebene tiefer,
nach dem Vorbild von MF-848 — ein HAL-Backend mit Byteebenen-Naht
(`uft_gw_stream_ops_t` + `uft_gw_open_stream`,
include/uft/hal/uft_greaseweazle_full.h:232–255), gegen die der
Automat als eingespeiste Leitung läuft
(tests/emulators/greaseweazle/gw_wire_bridge.h). Der Provider bekäme
dann — wie beim GW-Provider — ein natives Backend **neben** dem
QProcess-Weg, statt USB hinter die argv-Signatur zu zwängen. Die
Runner-Injektion bleibt wertvoll für das, was sie ist: die Naht des
CLI-Wegs.

Was der Automat aus diesem Gutachten unmittelbar bekommt: 12
Kommandos mit Rahmen und gemessenen Größen, die Antwortdisziplin
(§3), die Fluxstrom-Grammatik aus dem Verilog (§4), die
Versionssemantik (§5), die Transportrahmung (64-Byte-Pakete + ZLP,
§4) — und vier benannte Divergenzen für sein `DIVERGENCES.md` (§7).

---

## OPEN_ITEMS-Vorschlag (1 von max. 5 — Fortschreibung von P3-98, kein neuer Punkt)

> | P3-98a | **FluxEngine-Drahtprotokoll vermessen — der Automat kann
> gegen eine benannte, stabile, aber EINHÄNDIGE Quelle gebaut werden.**
> Gutachten `tools/uft-scout/out/fluxengine.gutachten.md`: Version 17
> (seit 2022-03 unverändert), 24 Rahmentypen mit gemessenen Größen
> (natürliche Ausrichtung, kein pack im Repo — Größen messen, nicht
> rechnen), Fluxstrom-Grammatik aus dem Sampler-Verilog (Fortsetzung
> 0x3F bei Zählerstand 63, nicht 62; 0xBF/0x7F gültig), harte
> Versionsgleichheit, kein Timeout im Referenz-Client. Lizenz
> GPL-2.0-only (COPYING.md, Matrix GRÜN) — Eigentümer-Urteil nach
> MF-679 ausstehend. **Zwei Planpräzisierungen aus der Messung:** (1)
> die `FluxEngineRunner`-argv-Naht ist für USB die falsche Ebene, das
> native Backend gehört als Byteebenen-Naht in den HAL (MF-848-Muster
> `uft_gw_stream_ops_t`); (2) das `DIVERGENCES.md` des Automaten muss
> die Einquelligkeit als Erstzeile führen — es gibt keine unabhängige
> zweite Umsetzung des Protokolls. Kennzahl: wie P3-98 — keine der
> vier direkt; verkleinert, was ein Bench fangen muss |

Weitere Vorschläge: keine. Die übrigen Befunde (BE-Host-Fehler im
Referenz-Client §2, 1-MiB-Deckel/Blockade §4) betreffen das
Fremdprojekt bzw. gehören in die Automat-Spezifikation, nicht auf die
OPEN_ITEMS-Liste.

## UNGEKLÄRT

1. **Exakte ARM-Struct-Layouts:** Größen auf x86-64 gemessen; für
   Cortex-M3 (ARM EABI) aus den Ausrichtungsregeln abgeleitet, nicht
   gemessen. Ein Automat sollte die Layouts aus demselben Header
   kompilieren und `static_assert`-artig prüfen statt Zahlen zu
   übernehmen.
2. **Gepinnte libusbp-Revision:** `dep/r/libusbp` wird beim Build
   geholt und liegt nicht im Klon; „kein Timeout als Default" ist
   gegen pololu/libusbp master belegt, nicht gegen die gepinnte
   Fassung.
3. **Watchdog-Verhalten der Firmware** bei blockiertem DATA-Endpunkt
   (§4): `CyWdtClear()` steht in der Leseschleife (main.c:470), nicht
   in `wait_until_writeable` — ob der Watchdog dort auslöst und mit
   welchem Zeithorizont, steht nicht im Quelltext (PSoC-Konfiguration).
4. **`dep/emu`/`dep/agg`-Lizenzen** nur per README behauptet; für die
   Protokollfrage ohne Belang, für jede andere Nutzung des Repos vorab
   zu klären.
5. **`sampleclock`-Herkunft im Verilog:** dass `sampleclock` exakt
   `TICK_FREQUENCY` = 12 MHz ist, folgt aus protocol.h und der
   Tick-Arithmetik des Clients (NS_PER_TICK, protocol.h:40); die
   Takt-Verdrahtung selbst liegt im PSoC-Schaltplan (.cysch, binär)
   und wurde nicht gelesen.
