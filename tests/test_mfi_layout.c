/**
 * @file test_mfi_layout.c
 * @brief MFI header layout — signature width and entry offset (MF-614).
 *
 * Links the real MFI plugin (src/formats/mfi/uft_mfi.c) and feeds it an image
 * built byte for byte to the authoritative layout:
 *
 *   [1] MAME, src/lib/formats/mfi_dsk.h — struct header / struct entry,
 *       RESOLUTION_SHIFT = 30, CYLINDER_MASK = 0x3fffffff
 *   [2] MAME, src/lib/formats/mfi_dsk.cpp:81-82 —
 *       sign[16]     = "MAMEFLOPPYIMAGE"  (includes the final \0)
 *       sign_old[16] = "MESSFLOPPYIMAGE"
 *       identify() compares all 16 bytes; load() reads the entry array at
 *       sizeof(header), i.e. offset 0x20.
 *
 * Both files are BSD-3-Clause (per-file SPDX headers), so quoting the layout
 * here is unproblematic. No MAME code is copied.
 *
 * ── Why this test exists ────────────────────────────────────────────────
 *
 * Before MF-614 the plugin checked EIGHT bytes ("MAMEFLOP") — a prefix of the
 * real signature. It therefore did not reject genuine MAME files, it ACCEPTED
 * them, and then read the track entries at offset 0x10 (its own comment said
 * so). At 0x10 sit cyl_count and head_count; at 0x18/0x1C form_factor and
 * variant. It also took form_factor from offset 0x08, which is inside the
 * signature.
 *
 * A parser that rejects a file is an annoyance. One that accepts it and reads
 * it wrong is the fabrication class this project has been burned by five times
 * (FMT-2/3/10/11/12). Every assertion below fails on the old reader.
 *
 * ── What this test does NOT prove ───────────────────────────────────────
 *
 * Nothing about the compressed track payload: the fixture carries a header and
 * a well-formed entry array, not real MFI cell data. Payload decoding needs a
 * real file (MFI is T3, no corpus entry — see docs/OPEN_ITEMS.md).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_mfi;

static int _fail = 0;
static int _t_fail = 0;
#define ASSERT(c) do { if (!(c)) { \
    printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; _t_fail = 1; return; } } while (0)

#define CYLS   3
#define HEADS  2
#define NENT   (CYLS * HEADS)
#define HDRSZ  32
#define ENTSZ  16

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Byte-exact per [1]/[2]. resolution = 0, so cyl_count carries no high bits. */
static size_t build(uint8_t **out)
{
    size_t payload = NENT * 8;                  /* 8 dummy bytes per track */
    size_t total = HDRSZ + NENT * ENTSZ + payload;
    uint8_t *b = calloc(1, total);
    if (!b) return 0;

    memcpy(b, "MAMEFLOPPYIMAGE", 16);           /* 15 chars + the final \0 */
    put32(b + 0x10, CYLS);                      /* cyl_count  (resolution 0) */
    put32(b + 0x14, HEADS);                     /* head_count */
    put32(b + 0x18, 0x0000ABCD);                /* form_factor */
    put32(b + 0x1C, 0x00001234);                /* variant */

    size_t off = HDRSZ + NENT * ENTSZ;
    for (int i = 0; i < NENT; i++) {
        uint8_t *e = b + HDRSZ + i * ENTSZ;
        put32(e + 0,  (uint32_t)off);           /* offset */
        put32(e + 4,  8);                       /* compressed_size */
        put32(e + 8,  16);                      /* uncompressed_size */
        put32(e + 12, 0);                       /* write_splice */
        off += 8;
    }
    *out = b;
    return total;
}

static const char *tmp_path(void)
{
    static char p[512];
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, sizeof(p), "%s/uft_mfi_layout_%d.mfi", d, (int)(size_t)p);
    return p;
}

static void test_full_signature_is_required(void)
{
    uint8_t *b = NULL;
    size_t n = build(&b);
    ASSERT(n > 0);

    int conf = 0;
    ASSERT(uft_format_plugin_mfi.probe(b, n, n, &conf));

    /* A prefix must NOT be enough: corrupt byte 8, which lies inside the
     * signature but outside the old 8-byte check. [2] identify() compares
     * all 16. */
    b[8] = 'X';
    conf = 0;
    ASSERT(!uft_format_plugin_mfi.probe(b, n, n, &conf));

    free(b);
}

static void test_geometry_comes_from_the_header(void)
{
    uint8_t *b = NULL;
    size_t n = build(&b);
    ASSERT(n > 0);

    const char *path = tmp_path();
    FILE *f = fopen(path, "wb");
    ASSERT(f != NULL);
    ASSERT(fwrite(b, 1, n, f) == n);
    fclose(f);
    free(b);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_mfi.open(&disk, path, true) == UFT_OK);

    /* cyl_count at 0x10, head_count at 0x14 — not derived from file size. */
    ASSERT(disk.geometry.cylinders == CYLS);
    ASSERT(disk.geometry.heads == HEADS);

    uft_format_plugin_mfi.close(&disk);
    remove(path);
}

int main(void)
{
    printf("=== MFI header layout (MF-614) ===\n");
    struct { const char *n; void (*f)(void); } tests[] = {
        { "full_signature_is_required",   test_full_signature_is_required },
        { "geometry_comes_from_header",   test_geometry_comes_from_the_header },
    };
    int run = 0, passed = 0;
    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        printf("  %-32s ", tests[i].n);
        run++; _t_fail = 0;
        tests[i].f();
        if (!_t_fail) { passed++; printf("PASSED\n"); }
    }
    printf("=== %d/%d ===\n", passed, run);
    return passed == run ? 0 : 1;
}
