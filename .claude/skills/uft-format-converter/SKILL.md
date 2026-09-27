---
name: uft-format-converter
description: |
  Use when adding a NEW conversion path between two format plugins (e.g.
  D64→G64, SCP→ADF, IMG→HFE). Trigger phrases: "konvertierung X nach Y",
  "convert X to Y", "neuer conversion path", "X zu Y converter", "format
  bridge Y→X". 44 paths exist — pattern is stable. DO NOT use for: adding
  a new source/target format (→ uft-format-plugin), filesystem traversal
  (→ uft-filesystem), flux-to-flux timing preservation (→ uft-deepread-module).
---

# UFT Format Converter

Use this skill when adding a new conversion path between two existing format
plugins. UFT has 44 such paths, all following the same pattern: read via
source plugin → intermediate representation → write via target plugin.

## When to use this skill

- Adding a path like `D64→G64`, `SCP→ADF`, `IMG→HFE`, `TD0→IMG`
- Bridging sector↔bitstream, bitstream↔flux, or flux↔flux
- Adding a lossy conversion with explicit fidelity warnings
- Adding round-trip verification for an existing path

**Do NOT use this skill for:**
- Adding a new source or target format — use `uft-format-plugin` first
- Parsing or walking a filesystem inside the image — `uft-filesystem`
- Flux-to-flux where timing matters (preservation) — `uft-deepread-module`
- Pure re-encoding inside the same family (D64 with/without error info)

## Conversion taxonomy

| Class | Example | Fidelity risk |
|-------|---------|---------------|
| **Sector↔Sector** | D64↔IMG, IMD↔IMG | None if geometries match |
| **Sector→Bitstream** | D64→G64, ADF→HFE | Bitstream synthesized; no real timing |
| **Bitstream→Sector** | G64→D64, HFE→IMG | Lossy: protection/weak bits discarded |
| **Flux→Sector** | SCP→D64, HFE→IMG | Lossy: timing discarded |
| **Flux→Bitstream** | SCP→HFE, SCP→G64 | Keeps bit timing, loses sub-bit flux |
| **Flux→Flux** | KryoFlux→SCP | Format conversion; full fidelity |

Classify the path BEFORE writing the converter. Fidelity class drives
warnings and audit entries.

## The 4 touchpoints

| # | File | Purpose |
|---|------|---------|
| 1 | `src/convert/uft_convert_<src>_to_<dst>.c` | Conversion function |
| 2 | `include/uft/uft_format_convert.h` | Public API entry |
| 3 | `src/convert/uft_convert_registry.c` | Path registration |
| 4 | `tests/test_convert_<src>_to_<dst>.c` | Round-trip test |

## Workflow

### Step 1: Identify source and target plugin APIs

Both plugins must exist and be functional. Check:

```bash
./uft list-formats | grep -iE "^(src|dst)"
```

Read both plugin files end-to-end. Know exactly:

- Source: geometry fields populated after `open()`
- Target: geometry fields required for `write_track()`
- Any format-specific metadata that doesn't survive (protection, timing,
  weak bits, labels beyond track/sector)

### Step 2: Choose the intermediate representation

Pick the lowest common denominator that preserves what you need:

| Source class → Target class | Intermediate |
|---|---|
| Sector → Sector | `uft_track_t` (sector list) |
| Sector → Bitstream | `uft_bitstream_t` (synthesized MFM/GCR) |
| Bitstream → Sector | `uft_track_t` (decoded sectors) |
| Flux → Sector | flux → bitstream → `uft_track_t` |
| Flux → Bitstream | `uft_bitstream_t` (PLL-clocked) |
| Flux → Flux | raw `uft_flux_t` |

### Step 3: Write the converter

Location: `src/convert/uft_convert_<src>_to_<dst>.c`

Use `templates/converter.c.tmpl`. Entry point signature:

```c
uft_error_t uft_convert_<src>_to_<dst>(const char *input_path,
                                         const char *output_path,
                                         const uft_convert_opts_t *opts);
```

### Step 4: Register the path

`src/convert/uft_convert_registry.c`:

```c
{
    .src_format = UFT_FORMAT_<SRC>,
    .dst_format = UFT_FORMAT_<DST>,
    .convert    = uft_convert_<src>_to_<dst>,
    .fidelity   = UFT_FIDELITY_LOSSY,   /* LOSSLESS / LOSSY / SYNTHETIC */
    .description = "<src> → <dst> (sector-level)",
},
```

### Step 5: Write the round-trip test

Round-trip tests verify that `A → B → A` produces identical output within
the fidelity class. Template: `templates/test.c.tmpl`.

For LOSSY conversions, the test asserts what IS preserved, not that
everything round-trips.

## Verification

```bash
# 1. Compile isolated
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/convert/uft_convert_<src>_to_<dst>.c

# 2. Full build
qmake && make -j$(nproc)

# 3. Test the conversion on a fixture
./uft convert \
    --from tests/vectors/<src>/sample.<src-ext> \
    --to /tmp/out.<dst-ext>

# 4. Verify the output is valid in the target format
./uft info /tmp/out.<dst-ext>

# 5. Round-trip test
./uft convert --from /tmp/out.<dst-ext> --to /tmp/roundtrip.<src-ext>
diff tests/vectors/<src>/sample.<src-ext> /tmp/roundtrip.<src-ext>
# expected: empty for LOSSLESS, documented delta for LOSSY

# 6. Run the new test
cd tests && make test_convert_<src>_to_<dst> && ./test_convert_<src>_to_<dst>

# 7. Full test suite regression
ctest --output-on-failure
```

## Common pitfalls

### Silent data loss is worse than loud failure

If the source has protection, weak bits, or timing that the target can't
represent, the converter MUST emit an audit entry:

```c
if (src_has_protection) {
    uft_audit_warn(ctx, "Protection lost in %s→%s conversion: %s",
                    "D64", "IMG", protection_name);
}
```

Never silently drop data. The audit chain is how forensic users trust
UFT's output.

### Geometry mismatch on write

If source has 40 tracks and target format expects 35, either:

- Truncate with explicit warning, OR
- Fail with `UFT_ERROR_GEOMETRY_MISMATCH`

Don't pad with zeros and pretend.

### Intermediate CRC recomputation

When going Sector → Bitstream, you MUST regenerate CRCs — the bitstream
encoding includes them. Don't pass through old CRCs as data.

See `uft-crc-engine` for the CRC family per format.

### Endianness in multi-byte fields

Disk formats disagree on endianness. Amiga is big-endian, most PC formats
little-endian, Apple mixed. Always use `uft_read_le16`/`uft_read_be16` etc.,
not raw pointer casts.

## Related

- `.claude/skills/uft-format-plugin/` — add source/target plugin first
- `.claude/skills/uft-crc-engine/` — CRC regeneration for bitstream conversions
- `.claude/skills/uft-filesystem/` — for filesystem-aware conversions
- `.claude/skills/uft-flux-fixtures/` — generate test inputs
- `.claude/agents/forensic-integrity.md` — review lossy conversions
- `docs/DESIGN_PRINCIPLES.md` Principle 1 (no silent data loss)
- `src/convert/uft_convert_d64_to_g64.c` — canonical sector→bitstream reference
- `src/convert/uft_convert_scp_to_adf.c` — canonical flux→sector reference
