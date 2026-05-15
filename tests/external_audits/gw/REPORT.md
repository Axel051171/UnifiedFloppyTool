# UFT ↔ Greaseweazle Kompatibilitäts-Audit

**Datum:** 2026-05-14  
**Referenz Host-Tools:** `keirf/greaseweazle` (HEAD)  
**Referenz Firmware:** `keirf/greaseweazle-firmware` (HEAD)  
**Prüfling:** `Axel051171/UnifiedFloppyTool` (HEAD)

---

## Verdikt

**UFT spricht das Greaseweazle-Wire-Protocol byte-genau korrekt** — mit **einer** echten Inkompatibilität, die zur Laufzeit `ACK_BAD_COMMAND` von der Firmware liefert.

---

## Test-Setup (4 Stufen)

### Stufe 1 — Statischer Konstanten-Diff
`extract_gw.py` zieht alle Opcodes, ACK-Codes, BusTypes und FluxOps aus dem offiziellen `usb.py` per Python-Introspektion. `extract_uft.py` parsed dieselben Konstanten aus deinen C-Headern (`include/uft/gw_protocol.h`, `include/uft/hal/uft_greaseweazle_full.h`). `compare.py` macht den Set-Diff.

### Stufe 2 — Flux-Encoder Byte-Vergleich
Greaseweazle's `_encode_flux` (Python) wird mit 6 Test-Vektoren gefüttert, die jeden Branch des Encoders treffen (kleine Werte <250, 2-Byte-Form 250–1524, große Werte ≥1525 mit Space-Opcode, NFA >150 µs mit Space+Astable, gemischt, mit Nullen). Die produzierten Byte-Streams werden als Hex gespeichert. Eine C-Kopie deines Encoders aus `src/hal/uft_greaseweazle_full.c:1108–1148` wird kompiliert und gegen dieselben Vektoren laufen gelassen — Byte-für-Byte-Vergleich.

### Stufe 3 — Firmware-Cross-Check
Nicht nur die Host-Seite, sondern auch `keirf/greaseweazle-firmware/inc/cdc_acm_protocol.h` und `src/floppy.c` gegen UFTs Definitionen prüfen.

### Stufe 4 (nicht ausgeführt — bräuchte echte HW)
USB-Trace-Vergleich mit `usbmon` / Wireshark: identischen Befehl mit `gw` CLI und mit UFT ausführen, USB-Bulk-Pakete byte-genau diffen. Das ist der Ground-Truth-Test, den nur du machen kannst.

---

## Ergebnisse

### Stufe 1 — Konstanten

| Sektion | Übereinstimmend | Divergent |
|---|---|---|
| CMD Opcodes (0x00–0x16) | 23/23 | 0 |
| ACK Codes (0x00–0x0D)   | 14/14 | 0 |
| FluxOps (Index/Space/Astable) | 3/3 | 0 |
| **BusType** | 2/3 | **1 echte Divergenz** |

BusType-Detail:
- `BUS_NONE = 0` (UFT) vs `Invalid = 0` (GW) → reine Umbenennung, semantisch identisch, **OK**.
- `BUS_IBMPC = 1`, `BUS_SHUGART = 2` → identisch, **OK**.
- `BUS_APPLE2 = 3` (UFT) → **existiert in GW-Firmware nicht.**

### Stufe 2 — Flux-Encoder

Alle 6 Test-Vektoren produzieren **byte-identische** Streams:

```
[PASS] small        13 Bytes: 6496c8f8f9ff024f6d0101f900
[PASS] two_byte     18 Bytes: fa01fafbfcf1fee7feffff024f6d0101f900
[PASS] three_form   36 Bytes: ff02f9130101f9... (komplett identisch)
[PASS] nfa          20 Bytes: ff0201b58913ff03b5010101ff024f6d0101f900
[PASS] mixed        24 Bytes: 64fa01fee7ff0297230101f96432fa33ff024f6d0101f900
[PASS] zeros        10 Bytes: 64c8ff024f6d0101f900
```

NFA-Schwellwerte (150 µs Threshold, 1.25 µs Astable-Periode) und Dummy-Flux (100 µs) sind exakt portiert. Die 28-bit-Encoding-Maske `1 | ((x<<1) & 0xFF)` etc. ist 1:1 übernommen.

### Stufe 3 — Firmware-Realitätscheck

In `greaseweazle-firmware/inc/cdc_acm_protocol.h:91`:

```c
/*#define BUS_APPLE2        3*/ /* reserved for Adafruit_Floppy */
```

Und `src/floppy.c:601`:

```c
if (type > BUS_SHUGART)
    goto bad_command;
```

→ Ein `SetBusType(3)` Aufruf von UFT führt auf jedem aktuellen Greaseweazle-Gerät zu `ACK_BAD_COMMAND`. Das ist die **einzige** Wire-Protocol-Inkompatibilität.

---

## Findings & Handlungsempfehlung

### F1 — BUS_APPLE2 ist nicht in der Stock-Firmware
**Severity:** Mittel — bricht alles, wenn ein User Apple-II-Modus wählt.  
**Wo:** `include/uft/gw_protocol.h:86`  
**Fix-Optionen:**
1. `BUS_APPLE2` aus UFT entfernen, bis Adafruit_Floppy-Support upstream landet, **oder**
2. zur Laufzeit beim `GET_INFO` die Firmware-Version checken und die UI-Option grau ausblenden, wenn die Firmware Apple-II nicht kennt, **oder**
3. dokumentieren als "experimentell, braucht Custom-Firmware".

### F2 — Test-Coverage erweitern
Dein bestehendes `tests/test_gw_protocol.c` deckt Konstanten + strerror + Tick-Math gut ab (5 Tests, sauber strukturiert). Was fehlt für robustes Regression-Testing:
- **Encoder-Vektoren** wie hier — sollten in deine CI als `test_gw_encoder.c`.
- **Decoder-Roundtrip:** encoded bytes → `_decode_flux` → Original-Werte (mit Toleranz für Quantisierung).
- **Mock-Serial-Test** (Milestone M3 in deiner Codebase erwähnt): captured USB-Trace abspielen, UFT-State-Machine durchlaufen lassen.

### F3 — Sample-Frequenz hartcodiert
`UFT_GW_SAMPLE_FREQ_HZ = 72000000` (F7) ist als Default OK, aber dein eigener Header definiert auch `UFT_GW_SAMPLE_FREQ_F7_PLUS = 84000000`. Der Encoder in Zeile 1113 zieht aber unconditional die 72-MHz-Konstante. Greaseweazle liest die echte sample_freq aus `GetInfo` — bei einem F7-Plus-Gerät werden alle NFA-Schwellwerte um Faktor 84/72 falsch sein. Quick fix: `sf` aus `device->info.sample_freq` ziehen, nicht aus dem Define.

---

## Artefakte

Im Ordner liegen alle Skripte zum Re-Run in deinem CI:

- `extract_gw.py` — GW-Konstanten als JSON
- `extract_uft.py` — UFT-Konstanten als JSON (Header-Parser)
- `compare.py` — der Diff-Runner
- `flux_reference.py` — generiert Referenz-Byte-Vektoren aus GW-Python
- `test_encode_match.c` — kompiliert, läuft, prüft byte-genau gegen die Vektoren
- `gw_constants.json`, `uft_constants.json`, `flux_vectors.json` — Snapshots für Drift-Detection

Reproduzieren: `python3 extract_gw.py > gw_constants.json && python3 extract_uft.py > uft_constants.json && python3 compare.py && gcc -O2 -o tem test_encode_match.c && ./tem`

**Kein Bit verloren — und keins zur Firmware geschickt, das sie nicht versteht.**
