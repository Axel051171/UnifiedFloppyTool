# FC5025 Emulator — Divergences from Real Hardware

Forensic-honesty ledger. `SPEC_STATUS: VendorDocumented` — the FC5025
Command Set Specification v1309 (Device Side Data, Inc.,
deviceside.com/fc5025.html) documents the USB CBW/CSW protocol and
opcodes; the framing is standard USB Bulk-Only Transport. FC5025 is a
**sector-only, read-only** device (CLAUDE.md hard rule: "FC5025 kann
keinen Flux lesen — nur Sektordaten"). This is the richest-documented
controller of the seven so far, so the divergences are narrower.

Severity: **HIGH** could mask a bug / break on real hardware · **MEDIUM**
plausible but unverified · **LOW** cosmetic / tolerance.

| ID | Sev | Divergence |
|----|-----|------------|
| **FC-1** | MEDIUM | CSW status byte values (`FC_CSW_*`: OK=0, FAILED=1, CRC_ERROR=2, NO_SECTOR=3, NO_DISK=4, WRITE_DENY=5) are modelled on the standard MSC CSW convention plus the v1309 sense semantics; the exact vendor sense codes the real FC5025 returns for CRC vs missing-address-mark are documented in v1309 but not byte-verified against a device here. The distinctions the emulator makes (CRC vs NO_SECTOR) are the forensically important ones. |
| **FC-2** | MEDIUM | The `CMD_READ_FLEXIBLE` CDB layout in `fc_build_read_cbw` (opcode 0xE5 + track/side/sector/size) is a plausible v1309 flexible-read block, but the exact field order and any mode bytes are from the spec's general shape, not a captured CBW. The emulator's own parser round-trips it; a real device may order the CDB differently. |
| **FC-3** | MEDIUM | Divergent-read modelling: a CRC-error sector is made to diverge by XOR-perturbing ~4 bytes per re-read attempt. Real marginal sectors diverge in weak-bit regions with a physical distribution; the emulator's perturbation is deterministic and uniform. What is faithful is the INVARIANT (Rule F-3): distinct copies are preserved (>= 2), status is never silently cleared. The exact divergence pattern is not physical. |
| **FC-4** | MEDIUM | The device "cannot auto-detect geometry" (matches the provider comment: FC5025 firmware does not report drive geometry; the user-selected format determines it). `fc_detect` reports presence only. This is faithful to the provider, but a real fcimage run may print format hints the emulator does not model. |
| **FC-5** | LOW | Only four formats carry geometry (Apple DOS 3.3 / 3.2, IBM 3740 FM 8", TRS-80 SSSD). The real FC5025 supports ~15 formats (Kaypro, Osborne, North Star, TI-99, Atari FM/MFM, …); the emulator models the geometry contract for a representative subset, not all. |
| **FC-6** | LOW | Motor / seek / recalibrate are not modelled — the provider scopes FC5025ProviderV2 to ReadsSectors + DetectsDrive, and in CLI mode those primitives are implicit (fcimage handles them). Matches the provider's honest mixin omission. |
| **FC-7** | LOW | Write is always refused (`FC_CSW_WRITE_DENY`). This is not a divergence but a faithful model: there is NO write opcode in the FC5025 command table — the device is read-only by design. |
| **FC-8** | LOW | `measureRPM` is not modelled: the V1 provider returns a hardcoded constant (300 for 5.25", 360 for 8") derived from the format table, not a measurement. The emulator carries the nominal RPM in the geometry table for the same reason and does not pretend to measure it. |

## What is NOT divergent (bench-relevant confidence)

- **Rule F-3 (divergent-read preservation)** is the core forensic
  property and is faithfully modelled and tested: a CRC-error sector,
  re-read up to the retry limit, keeps its DISTINCT copies (>= 2) and its
  CRC status is never silently cleared to OK (group E).
- Read-only is enforced structurally: `fc_write_sector` always returns
  `FC_CSW_WRITE_DENY` — no silent no-op.
- CBW/CSW framing round-trips through the emulator's own build/parse
  (standard "USBC"/"USBS" signatures, tag, residue, status).
- Per-format geometry matches the documented sector counts (Apple DOS
  3.3 = 16 sec, 3.2 = 13 sec, IBM 3740 = 26×128 B).
- Determinism: same seed → identical sector payloads.

## Bench-verification gate (M3 FC5025)

Before the FC5025 path is called "production", a bench session should
byte-verify the CSW sense codes (FC-1) and the CMD_READ_FLEXIBLE CDB
layout (FC-2) against a real device + the v1309 spec. Until then:
SIMULATED (FIRMWARE-REALISTIC), Tier-3 PASS is bench-only.
