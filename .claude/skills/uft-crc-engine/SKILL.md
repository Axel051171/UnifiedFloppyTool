---
name: uft-crc-engine
description: |
  Use when implementing, reviewing, or debugging CRC/checksum code for UFT
  formats (MFM/FM/Amiga/Apple/DEC). Trigger phrases: "CRC16 für X",
  "MFM CRC falsch", "Amiga-Checksum", "Apple checksum", "CRC polynomial X",
  "verify CRC against reference", "neue CRC variante". Includes Python
  reference impl for cross-check without requiring reveng installation.
  DO NOT use for: network CRCs (different domain), file-format checksums
  that aren't CRC (MD5/SHA → use hash-engine), flux-level bit errors.
---

# UFT CRC Engine

Use this skill when touching any CRC/checksum computation. UFT supports six
families — they differ in polynomial, init value, reflection, and xor-out.
Getting one wrong produces silent forensic bugs (decoded sector "looks
fine" but isn't).

## When to use this skill

- Implementing CRC for a new format
- Debugging a format where CRC fails
- Reviewing a PR touching `src/crc/`
- Cross-checking a CRC against a spec value

**Do NOT use this skill for:**
- Network protocol CRCs (CRC-32 Ethernet, etc.) — different domain
- Cryptographic hashes (MD5, SHA) — use hash-engine reference
- Flux-level bit errors — that's `src/flux/`, CRC assumes clean bitstream
- CRC computation for build/file integrity — unrelated concern

## CRC family cheat-sheet (authoritative)

| Family | Format | Poly | Init | RefIn | RefOut | XorOut |
|--------|--------|------|------|-------|--------|--------|
| CRC-16/IBM-3740 | MFM, FM (PC/ST) | 0x1021 | 0xFFFF | false | false | 0x0000 |
| Preset after A1 A1 A1 | MFM address mark | 0x1021 | **0xCDB4** | false | false | 0x0000 |
| Preset after C2 C2 C2 | MFM index mark | 0x1021 | **0x82C4** | false | false | 0x0000 |
| Amiga MFM | AmigaDOS | XOR-sum | 0 | — | — | 0 |
| Apple DOS 3.3 | ProDOS | byte XOR + rotate | 0 | — | — | 0 |
| DEC RX02 | RX02 FM | 0x1021 | 0x0000 | false | false | 0x0000 |

The preset values (0xCDB4, 0x82C4) are the CRC state after processing the
sync bytes with init 0xFFFF — use them to skip sync-byte processing in
hotloops. See `PERFORMANCE_REVIEW.md` finding #4.

## Workflow

### Step 1: Don't write a new one

Check if it exists:

```bash
cd src/crc/ && ls *.c
```

Reuse. Wrap. Creating a seventh "CRC for my format" is almost always wrong.
The dispatcher in `uft_crc_unified.c` is the right entry point.

### Step 2: If you truly need a new variant

Use `templates/crc_variant.c.tmpl`. Every new variant requires:

1. Polynomial + init + reflection parameters documented in a comment
2. At least 3 verification vectors from the format's public spec
3. Cross-check against the Python reference impl (see Step 4)

### Step 3: Wire it into the dispatcher

`src/crc/uft_crc_unified.c`:

```c
uint16_t uft_crc_compute(uft_crc_family_t family, const uint8_t *data,
                          size_t len, uint16_t init) {
    switch (family) {
    case UFT_CRC_IBM_3740:   return uft_crc16(data, len, init);
    case UFT_CRC_NEW_VARIANT: return uft_crc16_newvariant(data, len, init);
    /* ... */
    }
}
```

Never call the specific impl directly from format plugins — always
via the dispatcher. SSOT requirement (MF-003).

### Step 4: Cross-check without reveng

reveng is the gold standard for CRC verification, but we don't always
have it. This skill includes `scripts/reference/crc_reference.py` — a
pure-Python reference implementation that matches all 6 families.

```bash
python3 .claude/skills/uft-crc-engine/scripts/reference/crc_reference.py \
    --family ibm-3740 \
    --init 0xFFFF \
    --input 313233343536373839
# expect: 0x29B1
```

If reveng IS installed:
```bash
reveng -c -m CRC-16/IBM-3740 313233343536373839
# expect: 0x29B1
```

Both should agree. If they don't, one of them is wrong (usually your
parameter translation).

## Verification

```bash
# 1. Compile
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/crc/uft_crc16_<variant>.c

# 2. Unit test vectors pass
cd tests && make test_crc16 && ./test_crc16
# expect: all vectors pass

# 3. Cross-verify 3+ format-spec vectors
python3 .claude/skills/uft-crc-engine/scripts/reference/crc_reference.py \
    --family <variant> --input <hex> --expected <hex>

# 4. If applicable: reveng check
which reveng && reveng -c -m CRC-16/<name> <hex>

# 5. Integration test on a real fixture
./uft info tests/vectors/<format>/minimal_*.<ext> | grep -i crc
```

## Performance optimizations (per PERFORMANCE_REVIEW.md #4)

If the CRC is on a hotpath:

- **Preset skip**: start at `0xCDB4` for A1 A1 A1 → save 24 ops per sector
- **256-entry LUT**: `uint16_t T[256]` → 1 XOR + shift per byte
- **Slicing-by-8**: 8 tables, 8 bytes per iteration (firmware-safe with -Os)

Never use a 16-entry table — the speedup vs 256-entry is marginal, and
256-entry is only 512 bytes (fits trivially in any cache).

## Common pitfalls

### Copy-pasted Stack Overflow CRC

Every polynomial imported from the web must be reveng-verified. Common
mistakes: wrong reflection, wrong init, inverted output, wrong polynomial
(0x1021 vs 0x8408).

### XOR the sync bytes into every call

```c
/* SLOW */
uft_crc16(data_with_sync_prepended, data_len + 3, 0xFFFF);

/* FAST */
uft_crc16(data, data_len, MFM_CRC_SYNC_STATE);  /* = 0xCDB4 */
```

### Using float arithmetic

CRC is pure integer. `float` in a CRC implementation is a sign of confusion.

### Signed return type

CRC-16 result is `uint16_t` always. Returning `int` risks sign-extension
bugs.

## Related

- `.claude/skills/uft-format-plugin/` — plugins use CRC via dispatcher
- `docs/PERFORMANCE_REVIEW.md` #4 — CRC allocation bug
- `src/crc/uft_crc_unified.c` — dispatcher
- `tests/test_crc16.c` — vector pattern reference
- reveng: https://reveng.sourceforge.io/
- Koopman's CRC zoo: https://users.ece.cmu.edu/~koopman/crc/
