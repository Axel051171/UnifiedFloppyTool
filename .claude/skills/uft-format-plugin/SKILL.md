---
name: uft-format-plugin
description: |
  Use when adding a new disk-image format plugin to UFT. Trigger phrases:
  "neuer format plugin", "füge plugin für X hinzu", "X format unterstützen",
  "add X plugin", "plugin für Y schreiben", "support Z disk image".
  Scaffolds the 6 touchpoints (plugin.c + enum + registry + .pro + test +
  CMakeLists). DO NOT use for: modifying existing plugins (→ structured-
  reviewer), conversion between formats (→ uft-format-converter),
  filesystem-layer work (→ uft-filesystem), flux decoder changes (→ flux/).
---

# UFT Format Plugin

Use this skill when adding a NEW format plugin. 80 plugins exist — the
pattern is stable. The skill provides a scaffold script that wires all
6 touchpoints in one pass.

## When to use this skill

- Adding support for a new disk image format (e.g. Roland `.roland`,
  Sinclair `.mdv`, TRS-80 `.jv1`)
- Promoting an existing parser to a full plugin
- Forking an existing plugin to handle a variant (D71 from D64)
- Updating `spec_status` after finding documentation

**Do NOT use this skill for:**
- Modifying an existing plugin's logic — use `structured-reviewer`
- Format-to-format conversion — use `uft-format-converter`
- Filesystem semantics (FAT, ProDOS, AmigaDOS) — use `uft-filesystem`
- Flux decoder tweaks — that's `src/flux/`, not `src/formats/`

## The 6 touchpoints

| # | File | Scaffolded? |
|---|------|-------------|
| 1 | `src/formats/<n>/uft_<n>_plugin.c` | yes, from `templates/plugin.c.tmpl` |
| 2 | `include/uft/uft_format_plugin.h` (enum) | yes, inserted before `UFT_FORMAT_UNKNOWN` |
| 3 | `src/formats/uft_format_registry.c` (entry) | yes, before sentinel |
| 4 | `UnifiedFloppyTool.pro` (SOURCES) | yes, appended to last `src/formats/` line |
| 5 | `tests/test_<n>_plugin.c` | yes, from `templates/test.c.tmpl` |
| 6 | `tests/CMakeLists.txt` | yes, `add_executable` + `add_test` |

## Workflow

### Step 1: Gather format facts

Before scaffolding, answer:

- Size bounds: `--min-size` / `--max-size` (bytes)
- Sector size: `--sector-size` (usually 256, 512, or 1024)
- Magic bytes: if any, at what offset?
- Writable? yes/no
- Spec status: `OFFICIAL_FULL` / `OFFICIAL_PARTIAL` /
  `REVERSE_ENGINEERED` / `DERIVED` / `UNKNOWN`
- Closest existing plugin (for reference while editing):

| Your format is… | Reference plugin |
|-----------------|------------------|
| Fixed sector dump, no header | `src/formats/d64/uft_d64_plugin.c` |
| Magic-byte header + sectors | `src/formats/atr/uft_atr_plugin.c` |
| Container wrapping DO/PO/NIB | `src/formats/2img/` |
| Zone-variable sectors/track | `src/formats/d64/` (uses `d64_spt[]`) |
| Amiga block format | `src/formats/adf/` |
| Bitstream (G64/HFE) | `src/formats/g64/` |
| Flux capture | `src/formats/scp/` |
| Compressed (zlib/LZSS) | `src/formats/td0/uft_td0.c` |

### Step 2: Run the scaffold

```bash
cd <uft-root>
python3 .claude/skills/uft-format-plugin/scripts/scaffold_plugin.py \
    --name trsdos \
    --description "TRS-80 LDOS/LS-DOS" \
    --ext jv1 \
    --min-size 102400 \
    --max-size 204800 \
    --sector-size 256 \
    --spec-status REVERSE_ENGINEERED
```

The script creates/modifies all 6 touchpoints and is **idempotent** — re-run
with `--dry-run` first to preview.

### Step 3: Fill in the scaffolded plugin

The scaffold produces a compiling-but-incomplete `uft_<n>_plugin.c`. Edit:

1. `<n>_plugin_probe` — add magic-byte or structural checks beyond size
2. `<n>_plugin_open` — set real geometry (heads, sectors per track)
3. `<n>_plugin_read_track` — implement sector enumeration
4. `<n>_plugin_write_track` (delete if read-only)

See the template comments for each section — they flag where to edit.

### Step 4: Fill in the test

`tests/test_<n>_plugin.c` has 5 test cases scaffolded. Add at least:

- One real-sample positive test (if you have a fixture in `tests/vectors/`)
- One false-positive test against a neighbour format (e.g. D64 vs D71)

### Step 5: Register the registry probe function

The registry entry calls `uft_<n>_probe()` (note: underscore, not plugin's
static probe). This public symbol lives in `uft_format_registry.c` or in a
separate probe file. For simple plugins:

```c
/* In src/formats/uft_format_registry.c, near the other probes: */
int uft_<n>_probe(const void *data, size_t size, int *confidence) {
    /* delegate to the plugin's static probe */
    extern const uft_format_plugin_t uft_format_plugin_<n>;
    return uft_format_plugin_<n>.probe(data, size, size, confidence);
}
```

For complex formats with large probe logic, create `src/formats/<n>/uft_<n>_probe.c`.

## Verification

```bash
# 1. Build the plugin isolated (syntax check)
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/formats/<n>/uft_<n>_plugin.c

# 2. Full qmake build — must produce no new warnings
qmake && make -j$(nproc) 2>&1 | grep -E "error:|warning:" \
    | grep -i "<n>" && echo "NEW WARNINGS" || echo "clean"

# 3. Run the new test
cd tests && cmake . -DUFT_BUILD_TESTS=ON && \
    make test_<n>_plugin && ./test_<n>_plugin
# expect: "N passed, 0 failed"

# 4. Regression check — total test count should be old+1
ctest --output-on-failure 2>&1 | tail -3
# expect: "Total Test time ... X% (N/N tests passed)"

# 5. Verify it appears in the format list
./uft list-formats | grep -i "<n>"
# expect: one line with your format name

# 6. Compliance audit
python3 scripts/audit_plugin_compliance.py | grep "<n>"
# expect: no ERROR entries for your plugin
```

## Common pitfalls

### Probe returns non-zero on partial match

```c
/* WRONG */
if (memcmp(data, MAGIC, 2) == 0) *confidence = 50;
return true;  /* true even when magic didn't match */

/* RIGHT */
if (size < MIN_SIZE) return false;
if (memcmp(data, MAGIC, MAGIC_LEN) != 0) return false;
*confidence = 90;
return true;
```

### Forgotten cleanup on error paths

Every `fopen` needs `fclose` on every error path:

```c
FILE *f = fopen(path, "rb");
if (!f) return UFT_ERROR_FILE_OPEN;
if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);            /* MUST close */
    return UFT_ERROR_IO;
}
```

### Track indexing off-by-one

- D64 tracks are 1-indexed (1..35)
- UFT `cyl` parameter is 0-indexed (0..34)
- Always: `int d64_track = cyl + 1;`

### `spec_status = UNKNOWN` is almost never right

Default to `DERIVED` (de-facto from emulator) unless you really have no
reference at all. `UNKNOWN` makes the forensic report say "we don't know
if this plugin is correct".

### Basename collision in `UnifiedFloppyTool.pro`

UFT has `CONFIG += object_parallel_to_source` because basename collisions
exist (35+ files share names across dirs). Check before adding:

```bash
grep -c "uft_<n>_plugin\.c" UnifiedFloppyTool.pro
# expect: 0 before, 1 after
```

If a plugin file named `uft_<n>_plugin.c` already exists in another
directory, rename yours to `uft_<n>_<family>_plugin.c`.

## Related

- `.claude/skills/uft-crc-engine/` — CRC families (if format uses CRC)
- `.claude/skills/uft-flux-fixtures/` — synthesize test data
- `.claude/skills/uft-filesystem/` — if format needs a filesystem layer
- `.claude/agents/structured-reviewer.md` — review the final plugin
- `docs/DESIGN_PRINCIPLES.md` Principle 7 (honesty matrix)
- `src/formats/d64/uft_d64_plugin.c` — canonical D-series reference
- `src/formats/atr/uft_atr_plugin.c` — canonical magic-byte reference
- `tests/test_atr_plugin.c` — canonical probe-test reference
