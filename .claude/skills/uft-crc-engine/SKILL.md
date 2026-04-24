---
name: uft-crc-engine
description: |
  Use when implementing, reviewing, or debugging a CRC/checksum
  computation for MFM/FM/Amiga/Apple/DEC/etc. formats. Trigger phrases:
  "CRC16 für X", "MFM CRC falsch", "Amiga-Checksum", "Apple checksum",
  "reveng integration", "CRC polynomial X", "verify CRC against
  reveng". Covers the six CRC families UFT supports with canonical
  polynomials, init values, and cross-checks via reveng.
---

# UFT CRC Engine Reference

Use this skill when touching any CRC/checksum code. UFT supports six
distinct CRC families — they differ in polynomial, init value, reflected
input/output, and XOR-out. Getting one wrong is a silent forensic bug:
the decoded sector "looks fine" but isn't.

## CRC family cheat-sheet

| Family | Use | Poly | Init | RefIn | RefOut | XorOut |
|---|---|---|---|---|---|---|
| CRC-16/IBM-3740 (MFM, FM) | IBM PC, Amstrad, Atari ST | 0x1021 | 0xFFFF | false | false | 0x0000 |
| CRC-16 preset for A1 A1 A1 | After MFM address mark | 0x1021 | **0xCDB4** | false | false | 0x0000 |
| CRC-16 preset for C2 C2 C2 | After MFM index mark | 0x1021 | **0x82C4** | false | false | 0x0000 |
| Amiga MFM checksum | `uft_amigados` | XOR sum | 0 | n/a | n/a | 0 |
| Apple GCR checksum | DOS 3.3, ProDOS | byte XOR + rotate | 0 | n/a | n/a | 0 |
| DEC RX02 | RX02 FM tracks | 0x1021 | 0x0000 | false | false | 0x0000 |

The preset values (0xCDB4, 0x82C4) are the CRC state AFTER processing
`A1 A1 A1` or `C2 C2 C2` with init 0xFFFF — use them to skip the
sync-byte processing in hot loops (see PERFORMANCE_REVIEW finding #4).

## Step 1: Locate the canonical impl

Do NOT write a new CRC routine if one exists:

```bash
cd src/crc/ && ls *.c
# uft_crc16.c                  — CRC-16/IBM-3740
# uft_crc_unified.c            — dispatcher (picks family by format id)
# uft_amiga_checksum.c         — Amiga MFM
# uft_apple_gcr_checksum.c     — Apple
```

Reuse and wrap. Creating a seventh "for my format" CRC is almost
always wrong.

## Step 2: If you really do need a new engine

Template: `src/crc/uft_crc16.c`. New file conventions:

```c
#include "uft/crc/uft_crc_unified.h"

/*
 * CRC-16 variant: CRC-16/YOUR-NAME
 * Polynomial:     0x1021
 * Initial value:  0xFFFF
 * Input reflected: false
 * Output reflected: false
 * XOR out:        0x0000
 *
 * Verified against reveng: TODO after table-drop
 */

uint16_t uft_crc16_yourname(const uint8_t *data, size_t len, uint16_t init)
{
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
        }
    }
    return crc;
}
```

Prefer **byte-at-a-time** (no table) for cold paths, **256-entry LUT**
for hot paths. AVX2 `_mm_crc32_u64` is desktop-only — do NOT use on
firmware path (violates portability; see `uft-stm32-portability` skill).

## Step 3: Reveng cross-check (MANDATORY for new families)

[reveng](https://reveng.sourceforge.io/) is the canonical CRC parameter
finder. Every new CRC variant must be verified against it:

```bash
# 1. Collect known good-data + expected-CRC pairs from real disks
echo "123456789" | xxd -r -p > check.bin
EXPECTED_HEX="29B1"   # from the real format's specification

# 2. Verify parameters with reveng
reveng -c -m CRC-16/IBM-3740 -s $(cat check.bin.hex)
# reveng prints the CRC — must match EXPECTED_HEX

# 3. If parameters differ, brute-force search:
reveng -s -w 16 $(known_crc_input_1) $(known_crc_output_1) \
              $(known_crc_input_2) $(known_crc_output_2)
```

Commit the reveng command and expected output as a comment in the
implementation file — future maintainers will need it.

## Step 4: Unit test pattern

`tests/test_crc16.c` is the reference. For each variant, supply at
least THREE vectors from the format's public reference:

```c
struct test_vector {
    const char *desc;
    const uint8_t *data;
    size_t len;
    uint16_t init;
    uint16_t expected;
};

static const struct test_vector VECTORS[] = {
    /* CRC-16/IBM-3740: "123456789" → 0x29B1 */
    { "reveng std",     (const uint8_t *)"123456789", 9, 0xFFFF, 0x29B1 },
    /* Amiga MFM: A1 A1 preset */
    { "A1 A1 preset",   (const uint8_t[]){0xA1,0xA1}, 2, 0xFFFF, 0xCDB4 },
    { "A1 A1 A1 preset",(const uint8_t[]){0xA1,0xA1,0xA1}, 3, 0xFFFF, 0xB230 },
    /* Edge cases */
    { "empty",          NULL, 0, 0xFFFF, 0xFFFF },
    { "single 0x00",    (const uint8_t[]){0}, 1, 0xFFFF, 0xE1F0 },
};
```

## Step 5: Performance hot-loops

If the CRC is on a hotpath (every flux decode), these optimisations are
in scope per `PERFORMANCE_REVIEW.md` finding #4:

- **Preset skip**: start at `0xCDB4` for A1 A1 A1, save 24 ops/sector.
- **Table of 256**: `uint16_t T[256]` → one XOR + shift per byte.
- **Slicing-by-8** (optional): 8 tables, process 8 bytes per iteration.
  Firmware-safe with `-Os`.
- **Never** use 16-entry table — the speedup vs 256-entry is minimal,
  and a 256-entry table is only 512 bytes (fits in M7 cache trivially).

## Step 6: Integration with uft_crc_unified

`src/crc/uft_crc_unified.c` is the dispatcher. Add your new variant:

```c
uint16_t uft_crc_compute(uft_crc_family_t family, const uint8_t *data,
                          size_t len, uint16_t init) {
    switch (family) {
    case UFT_CRC_IBM_3740:  return uft_crc16(data, len, init);
    case UFT_CRC_YOUR_NEW:  return uft_crc16_yourname(data, len, init);
    /* ... */
    }
}
```

Never call the specific impl directly from format plugins — always go
through the dispatcher. This is a SSOT requirement (MF-003).

## Anti-patterns

- **Don't copy-paste a CRC loop from Stack Overflow** — reveng-verify
  every polynomial you import from the web.
- **Don't use float/double arithmetic** — CRC is pure integer.
- **Don't XOR the sync bytes (`A1 A1 A1`) into every CRC call** —
  use the `0xCDB4` preset.
- **Don't assume init=0** — IBM-3740 is 0xFFFF, DEC RX02 is 0x0000,
  XModem is 0x0000. Document what you pick.
- **Don't return a signed type** — CRC-16 result is uint16_t always.

## Related

- `src/crc/uft_crc_unified.c` — dispatcher
- `tests/test_crc16.c` — vector pattern
- `docs/PERFORMANCE_REVIEW.md` #4 (CRC allocation bug)
- reveng: https://reveng.sourceforge.io/
- Koopman's CRC zoo: https://users.ece.cmu.edu/~koopman/crc/
