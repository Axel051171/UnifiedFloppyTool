# ADFCopy Emulator — Divergences from Real Hardware

Forensic-honesty ledger. `SPEC_STATUS: VendorDocumented (community)` —
the binary serial protocol is the in-tree SSOT in
`src/hardware_providers/adfcopy_serial_runners.h` (MF-252). ADFCopy is a
Teensy-based (PJRC VID:PID 0x16C0:0x0483) USB-CDC serial controller for
Amiga 3.5" drives. Binary framing: 1-byte command + payload, 1-byte
`'O'`/`'E'`/`'D'` response, a status bitmask, and a CMD_READ_FLUX 3-byte
header (status + LE16 length) + flux payload. Sample clock 40 MHz / 25 ns
(matches SCP).

Severity: **HIGH** could mask a bug / break on real hardware · **MEDIUM**
plausible but unverified · **LOW** cosmetic / tolerance.

| ID | Sev | Divergence |
|----|-----|------------|
| **ADFC-1** | **HIGH** | **Berichtigt MF-915 — von MEDIUM heraufgestuft, und die Formulierung war zu milde.** Hier stand, die Kommandotabelle sei „eine V1-audit reconstruction, not a byte-capture“ und die echte Firmware benutze *vielleicht* weitere Kommandos. Erstgeprüft an `Niteto/ADF-Drive-Firmware` (GPL-3.0, geklont nach `tools/uft-scout/work/ADF-Drive-Firmware/`) ist es **ein anderes Protokollmodell**: `getCommand()` liest bis `'\n'`, `doCommand()` vergleicht gegen **Zeichenketten** — gemessen **67** Vergleiche `cmd == "..."`, darunter `read`, `write`, `goto`, `init`, `index`; Antworten sind Text (`OK`, `NO DISK`). Der Emulator bildet damit **UFTs Annahme** nach, nicht die Hardware — und die Testkette bestätigt den Treiber gegen ein Modell seiner selbst. Nicht umgebaut: das wäre neuer ungeprüfter Code auf einem Pfad ohne Gerät (MF-310, EINFRIER-REGEL). Siehe P3-188. **Nachtrag MF-922: dieser Befund ist nicht mehr Prosa.** `tests/emulators/adfcopy/firmware_line_protocol.c` bildet die veröffentlichte Firmware nach — Zeilen bis `'\n'`, Zerlegung am **ersten** Leerzeichen, Textantworten —, und `tests/test_adfcopy_firmware_protokoll.c` misst daran: UFTs Byte `0x0B` erzeugt **0 ausgewertete Zeilen und 0 Byte Antwort**, das Byte bleibt im Puffer und wartet auf ein `'\n'`, das nie kommt. Mit Gegenprobe (derselbe Emulator antwortet auf `index\n`), damit das Schweigen am Protokoll hängt und nicht an einem toten Modell; **5 von 5** Mutationen fallen je an ihrer eigenen Stelle. Der Emulator daneben (`firmware_state_machine.c`) bleibt, was er ist — UFTs Annahme —, und ist jetzt als solcher **danebenstellbar**. |
| **ADFC-2** | HIGH | The CMD_READ_FLUX reply length field is 16-bit, so one reply carries at most 0xFFFF flux bytes. A full Amiga DD revolution has more transitions than that, so the generator stops at the 16-bit cap and a multi-rev read returns a bounded window. How the REAL firmware handles a track larger than 65535 flux bytes (chunked replies? coarser encoding? multiple reads?) is unverified — this is the most bench-critical unknown. |
| **ADFC-3** | MEDIUM | Flux byte encoding: one byte per interval, value = 25 ns ticks clamped to 255 (6.375 µs). This covers the DD 2/3/4 µs cell family but cannot represent long gaps (weak/killer tracks, index gaps) that exceed 6.375 µs — the real firmware's flux encoding for such gaps is unknown (an overflow/escape byte, or a wider field). |
| **ADFC-4** | MEDIUM | Cell timing is a deterministic 2/3/4 µs Amiga-MFM mix summed to a 200 ms (300 RPM) revolution. Real disks have continuous jitter and copy-protection structures (long tracks, Rob Northen, etc.) not modelled here — the generator is clean-DD-shaped plus the three defect flags. |
| **ADFC-5** | LOW | CMD_INIT is modelled as "spin motor + home head → 'O'". The real firmware may perform additional handshake (version query, calibration) before the first 'O'. Motor-off is a local flag with no command (matches the V1 audit: motor-off sets a flag only). |
| **ADFC-6** | LOW | Write is not modelled — ADF-Copy hardware refuses raw flux writes (V1 writeRawFlux always false); there is no write command frame. The emulator has no write path, matching the read-only design. This is faithful, not a divergence. |
| **ADFC-7** | LOW | `measureRPM` is not modelled: the V1 provider returns a constant 300.0 with no serial dialog. The generator carries a nominal 300 RPM for the same reason and does not pretend to measure it. |
| **ADFC-8** | LOW | SEEK addresses `track = cylinder*2 + head` in [0, 159] (80 cyl × 2 heads). The emulator validates this range; the real firmware's exact out-of-range behaviour (clamp vs 'E') is assumed to be 'E'. |

## What is NOT divergent (bench-relevant confidence)

- The binary framing round-trips: each command produces the documented
  response byte, and CMD_READ_FLUX returns a 3-byte header whose LE16
  length equals the flux byte count (group D).
- The status bitmask reflects real device state (disk/write-prot/motor/
  flux-capable) truthfully (group C).
- No-disk is visible: CMD_INIT answers `'D'`, CMD_READ_FLUX returns a
  3-byte no-disk header with zero flux — never a silent success (group E).
- Read-only: there is no write path; write-protect does not block reading
  (forensic capture never writes).
- The 40 MHz / 25 ns sample clock matches the SSOT.
- Determinism: same seed → identical flux bytes.

## Bench-verification gate (M3 ADFCopy)

Before the ADFCopy path is called "production", a bench session must
resolve ADFC-2 (how the firmware handles tracks larger than the 16-bit
flux reply) and ADFC-1 (the full command/status vocabulary vs the real
Teensy firmware). Until then: SIMULATED (FIRMWARE-REALISTIC), Tier-3 PASS
is bench-only.
