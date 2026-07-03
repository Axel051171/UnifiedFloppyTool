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
| **ADFC-1** | MEDIUM | The command/response vocabulary (INIT 0x01, SEEK 0x02, READ_FLUX 0x06, GET_STATUS 0x0B; `'O'`/`'E'`/`'D'` responses; status bits DISK/WPROT/MOTOR/FLUX) is taken from the runner SSOT header, which is itself a V1-audit reconstruction of the Teensy firmware, not a byte-capture. The values are internally consistent and drive the provider, but the real firmware may use additional commands or sub-status the emulator does not model. |
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
