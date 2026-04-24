---
name: uft-filesystem
description: |
  Use when adding filesystem-layer support (AmigaDOS, FAT12/16, CBM DOS,
  Apple DOS 3.3, ProDOS, CP/M). Trigger phrases: "filesystem für X",
  "AmigaDOS hinzufügen", "FAT support", "file listing für X", "extract
  files from Y image", "directory traversal". Separate layer from format
  plugins — plugins read tracks, filesystems read files. DO NOT use for:
  adding new disk-image container (→ uft-format-plugin), conversions
  between containers (→ uft-format-converter), flux-level decoding.
---

# UFT Filesystem Layer

Use this skill when adding file/directory access on top of an existing
format plugin. The filesystem layer sits ABOVE plugins: plugins yield
tracks and sectors; filesystems interpret those sectors as directory
entries and files.

## When to use this skill

- Adding AmigaDOS OFS/FFS traversal
- Adding FAT12/16 file listing
- Adding CBM DOS (1541/1571/1581) directory
- Adding Apple DOS 3.3 or ProDOS navigation
- Adding CP/M file system

**Do NOT use this skill for:**
- Adding the underlying disk image container — use `uft-format-plugin`
- Converting between container formats — use `uft-format-converter`
- Flux-level work — that's `src/flux/`
- Filesystem-specific protection detection — use `uft-protection-scheme`

## The 4 touchpoints

| # | File | Purpose |
|---|------|---------|
| 1 | `src/filesystem/uft_fs_<n>.c` | Traversal, directory, file read |
| 2 | `include/uft/filesystem/uft_fs_<n>.h` | Public API |
| 3 | `src/filesystem/uft_fs_registry.c` | Auto-detect which FS for which format |
| 4 | `tests/test_fs_<n>.c` | Directory listing + file extraction |

## Filesystem → Format mapping

| Filesystem | Formats that use it |
|------------|---------------------|
| AmigaDOS OFS | ADF (OFS bootblock: `DOS\0` or `DOS\2`) |
| AmigaDOS FFS | ADF (FFS bootblock: `DOS\1` or `DOS\3`) |
| FAT12 | IMG 360K/720K/1.2M/1.44M, ST, MSA |
| CBM DOS 2.6 | D64, D71 (differs by disk variant) |
| CBM DOS 3.0 | D81 |
| Apple DOS 3.3 | DSK (no prodos marker), NIB |
| Apple ProDOS | DSK (with PO ordering), 2MG |
| CP/M | Many (TD0, IMD, generic IMG) |

The registry (`uft_fs_registry.c`) maps format ID → detector function.
One filesystem can match multiple formats.

## Workflow

### Step 1: Read the format's track layout

Filesystems read sectors, not files. Know which tracks/sectors hold the
directory and BAM/FAT for your target.

Examples:
- AmigaDOS: bootblock blocks 0-1, root block 880 (DD) / 1760 (HD)
- FAT12: boot sector, FATs, root dir at fixed offsets
- CBM DOS 2.6: BAM at track 18 sector 0, directory track 18 sector 1..

### Step 2: Implement the traversal API

Location: `src/filesystem/uft_fs_<n>.c`. Required entry points:

```c
/* Mount filesystem on an opened uft_disk_t */
uft_error_t uft_fs_<n>_mount(uft_disk_t *disk, uft_fs_ctx_t **out_ctx);

/* List entries in a directory */
uft_error_t uft_fs_<n>_list(uft_fs_ctx_t *ctx, const char *path,
                              uft_fs_entry_t **out, size_t *count);

/* Read a file's content */
uft_error_t uft_fs_<n>_read(uft_fs_ctx_t *ctx, const char *path,
                              uint8_t **out, size_t *size);

/* Unmount + free context */
void uft_fs_<n>_unmount(uft_fs_ctx_t *ctx);
```

Use `templates/fs.c.tmpl` for skeleton.

### Step 3: Register the detector

`src/filesystem/uft_fs_registry.c`:

```c
static const uft_fs_registration_t REGISTRATIONS[] = {
    { UFT_FORMAT_ADF,  uft_fs_amigados_detect, uft_fs_amigados_mount,
       uft_fs_amigados_list, uft_fs_amigados_read, uft_fs_amigados_unmount },
    { UFT_FORMAT_D64,  uft_fs_cbmdos_detect,   ... },
    /* NEW entry here */
    { UFT_FORMAT_UNKNOWN, NULL }
};
```

`detect` examines the bootblock/header to decide if THIS filesystem
applies (e.g. for ADF: is it OFS or FFS?).

### Step 4: Handle forensic edge cases

Real disks have corruption. Traversal must survive:

- Invalid block pointers (pointing past end of disk)
- Circular directory references
- Inconsistent BAM/FAT
- Mixed case (PETSCII vs ASCII, EBCDIC in CP/M)

Return `UFT_ERROR_FS_CORRUPTED` with audit entry, never segfault.

## Verification

```bash
# 1. Compile
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/filesystem/uft_fs_<n>.c

# 2. Mount + list on minimal fixture
./uft fs list tests/vectors/<format>/minimal_*.<ext>
# expect: at least "<root>" listed

# 3. Read a known file
./uft fs extract tests/vectors/<format>/sample.<ext> README
# expect: file contents on stdout or written

# 4. Run tests
cd tests && make test_fs_<n> && ./test_fs_<n>

# 5. Corruption resilience
./uft fs list tests/vectors/<format>/corrupted_root.<ext>
# expect: error message, not segfault
```

## Common pitfalls

### Confusing block size with sector size

- Amiga blocks = 512 bytes
- FAT12 sectors = 512 bytes; clusters vary (1–8 sectors)
- CBM DOS sectors = 256 bytes
- Apple DOS 3.3 sectors = 256 bytes, blocks (ProDOS) = 512 bytes

Always declare which you mean in comments.

### Encoding of filenames

- AmigaDOS: ISO-8859-1
- FAT12: CP850 (DOS) or CP1252 (Windows-style)
- CBM: PETSCII (case-reversed upper/lower)
- Apple: ASCII with high bit set
- CP/M: ASCII uppercase

Convert to UTF-8 at the API boundary, never earlier.

### Trusting the BAM/FAT

Don't rely on BAM/FAT for reachability when traversing. Follow the
chain from the root, then cross-check against BAM for orphans. Files
reachable from root but unmarked in BAM = potentially deleted content
(forensically valuable).

### Off-by-one in block numbering

Amiga blocks are 0-indexed from physical start. CBM tracks are 1-indexed.
Apple tracks are 0-indexed. Mixing = wrong data.

## Related

- `.claude/skills/uft-format-plugin/` — plugins provide the tracks
- `.claude/skills/uft-flux-fixtures/` — `gen_adf_fixture.py` makes test data
- `docs/FILESYSTEM_DESIGN.md` — layer architecture
- `src/formats/adf/` — ADF plugin (ADF is the container, AmigaDOS is the FS)
