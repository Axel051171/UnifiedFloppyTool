# Differential Conformance Report — gw vs UFT (P3.2 / task #109)

Byte-exact differential of the **UFT flux decode engine** against
**Greaseweazle `gw` 1.23**, on a shared synthetic flux corpus.

- Harness: `tests/differential/test_gw_parity.py`
- Corpus: `tests/differential/corpus/` (Hybrid Layer 1 — synthetic,
  deterministic; `gen_corpus.py`)
- UFT side: `tests/differential/uft_flux_decode.c` (test fixture
  linking the UFT flux engine — UFT itself is GUI-only)
- Method: the SAME gw-produced `.scp` flux stream is decoded by both
  `gw convert` and the UFT engine; the two decoded sector images are
  compared byte-for-byte.

Last run: gw 1.23, UFT @ MF-225, MinGW gcc 13.1.0 — `pytest
tests/differential/` → **6 passed, 0 skipped, exit 0**.

## Per-format result

| Format | Disk class | gw `--format` | UFT encoding | Result |
|--------|-----------|---------------|--------------|--------|
| `ibm_dd`     | IBM 3.5" DD 720K   | `ibm.720`             | mfm       | ✅ **GLEICH** — byte-exact (737280 B, 1440 sectors) |
| `ibm_hd`     | IBM 3.5" HD 1.44M  | `ibm.1440`            | mfm       | ✅ **GLEICH** — byte-exact (1474560 B, 2880 sectors) |
| `atarist_dd` | Atari ST DD 720K   | `atarist.720`         | mfm       | ✅ **GLEICH** — byte-exact (737280 B, 1440 sectors) |
| `c64_gcr`    | Commodore 1541 GCR | `commodore.1541`      | gcr_c64   | ✅ **GLEICH** — byte-exact (196608 B, 768 sectors, 40-track .d64) |
| `apple2_525` | Apple II 5.25" GCR | `apple2.appledos.140` | gcr_apple | ✅ **GLEICH** — byte-exact (143360 B, 560 sectors, 35-track .do) |
| `amiga_dd`   | Amiga DD 880K      | `amiga.amigados`      | amiga     | ✅ **GLEICH** — byte-exact (901120 B, 1760 sectors, 80-cyl .adf) |

All six corpus disk classes reach byte-exact parity — no **DIVERGENT**
rows, and no skipped rows. If a format ever diverges, `test_gw_parity.py`
fails that parametrised case and prints the first differing offset with
32-byte hex context for both sides — that hex context is the per-format
diff-hex this report would carry.

## The 3 IBM-style MFM formats — real parity

`ibm_dd`, `ibm_hd`, `atarist_dd` all decode through UFT's
`flux_decode_mfm()`. They reach byte-exact parity with gw **only after
MF-218** — before that fix UFT's MFM decoder skipped two of the three
0xA1 sync words and decoded **zero** sectors from any standard IBM
flux. The differential harness surfaced that bug on its first real
file; `tests/test_flux_mfm_sync.c` is the regression guard.

Additional confidence: the round-trip `source image → gw convert →
.scp → UFT decode` is also byte-identical to the committed
`corpus/sources/*` for these three — UFT's decode is not just *equal to
gw*, it is *correct*.

## C64-GCR — real parity (MF-223)

`c64_gcr` decodes through `flux_decode_gcr_c64()`. The helper wires the
Commodore 1541 zone geometry: gw's `commodore.1541` diskdef is the
**40-track** .d64 (768 sectors, 196608 B — zone sector counts
21/19/18/17), not the 35-track .d64; `gen_corpus.py` generates the full
40-track source so the round-trip carries real data on every track.
`uft_flux_decode.c` places each decoded sector by `(track, sector)`
into the .d64 image via the cumulative zone table.

## Apple2-GCR — real parity, after a decoder fix (MF-224)

`apple2_525` decodes through `flux_decode_gcr_apple()`. Wiring it
surfaced a genuine decoder bug: the engine's 6-and-2 reassembly did not
undo Apple's bit-REVERSED 2-bit auxiliary groups, so every data byte's
low two bits came out swapped (`0x11`→`0x12`, `0x6e`→`0x6d`: high 6 bits
correct, low 2 reversed). Fixed in `decode_apple_gcr_sector()`. The
helper places each sector by `(track, sector)` with the DOS 3.3
physical→logical soft-skew table; that table is *verified* by the
byte-exact differential, not assumed.

## Corpus strengthened — placement is now actually tested (MF-224)

The original `synth_pattern` was `(i*31+17) & 0xFF` — period 256, so
**every 256-byte block was byte-identical**. The differential then only
proved per-sector decode correctness and sector COUNT; a sector
transposition bug would have passed silently. `synth_pattern` is now a
per-index avalanche hash → every 256- and 512-byte block on every
corpus disk is distinct (768/768, 560/560, 5760/5760, …). All five
wired formats were re-verified against this stronger corpus, so the
✅ rows now mean *placement* parity, not just count parity.

## Amiga-MFM — real parity, after writing the decoder (MF-225)

`amiga_dd` was the last gap: UFT had **no** Amiga-track flux decoder —
`FLUX_ENC_AMIGA` silently routed to `flux_decode_mfm`, which finds no
IBM IDAM/DAM in Amiga's whole-track MFM and decoded zero sectors.
`flux_decode_amiga()` was written from the AmigaDOS trackdisk format: 11
sectors/track, each `2× 0x4489` sync + an odd/even-split info long +
16-byte sector label + header/data checksums + a 512-byte odd/even-split
data block. The odd/even merge consumes the **raw** MFM cells directly
(`((odd & 0x55)<<1) | (even & 0x55)`) — there is no IBM-style
clock-strip step. `flux_decode_track` now routes `FLUX_ENC_AMIGA` to it,
ending the silent misroute.

`uft == gw == source` — all 901120 bytes, 1760 sectors, identical. gw
independently reports `1760 of 1760 (100%)`, confirming every Amiga
header + data checksum.

## Reproduce

```bash
# 1. install gw (not on PyPI): github.com/keirf/greaseweazle release
export GW=/path/to/gw            # or put gw on PATH
# 2. run — conftest.py generates the corpus + builds the helper
python -m pytest tests/differential/ -v
```
