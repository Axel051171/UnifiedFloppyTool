/**
 * @file test_imd_error_marks.c
 * @brief IMD disk-error marking: read, represent, preserve on write.
 *
 * Links the real IMD plugin (src/formats/imd/uft_imd_plugin.c). This is the
 * Phase-4 disk-error work package for an error-aware Klasse-3 format: IMD
 * encodes per-sector status in its data-record type byte (dtype 3/4=deleted,
 * 5/6=CRC error, 7/8=both). The requirement is "read the marks, represent
 * them internally, preserve them on write".
 *
 * Construction: one track with three raw 128-byte sectors —
 *   sector 1 dtype 1 (normal), sector 2 dtype 3 (deleted-address-mark),
 *   sector 3 dtype 5 (data CRC error).
 * We open, read the track, and assert the plugin surfaced deleted/crc_ok on
 * the right sectors (read + represent). Then we write the track back and read
 * again, asserting the marks survive (preserve on write — the IMD writer is an
 * in-place overwrite that keeps the dtype byte, so a captured error mark is
 * never silently downgraded to a clean sector).
 *
 * Documented limit: the in-place writer preserves the EXISTING dtype; it does
 * not re-encode a caller-flipped status into a new dtype (that would require
 * rewriting the record and is out of this format handler's scope). For the
 * forensic read->write->read preservation path this is exactly correct.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_imd;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-36s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 128u

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_imd_err_%d.imd", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* One track, three raw 128B sectors with dtypes normal/deleted/crc-error.
   Each sector's first data byte is a tag (0xA1/0xB2/0xC3) so we can match
   sectors to their expected status regardless of the id numbering scheme. */
static int build_imd_with_marks(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const char *comment = "IMD 1.18: uft error-mark test\r\n";
    fwrite(comment, 1, strlen(comment), f);
    fputc(0x1A, f);                 /* comment terminator */
    fputc(5, f);                    /* mode: MFM 250kbps */
    fputc(0, f);                    /* cylinder 0 */
    fputc(0, f);                    /* head 0, no cyl/head maps */
    fputc(3, f);                    /* 3 sectors */
    fputc(0, f);                    /* size code 0 -> 128 bytes */
    fputc(1, f); fputc(2, f); fputc(3, f);   /* sector numbering map */
    /* sector 1: dtype 1 (normal raw) */
    fputc(1, f);
    fputc(0xA1, f); for (unsigned i = 1; i < SS; i++) fputc(0x10, f);
    /* sector 2: dtype 3 (deleted-address-mark, raw) */
    fputc(3, f);
    fputc(0xB2, f); for (unsigned i = 1; i < SS; i++) fputc(0x20, f);
    /* sector 3: dtype 5 (data CRC error, raw) */
    fputc(5, f);
    fputc(0xC3, f); for (unsigned i = 1; i < SS; i++) fputc(0x30, f);
    fclose(f);
    return 1;
}

/* Locate a read-back sector by its first-byte tag. */
static const uft_sector_t *find_by_tag(const uft_track_t *t, uint8_t tag) {
    for (size_t i = 0; i < t->sector_count; i++)
        if (t->sectors[i].data && t->sectors[i].data[0] == tag)
            return &t->sectors[i];
    return NULL;
}

static void assert_marks(uft_track_t *t) {
    ASSERT(t->sector_count == 3);
    const uft_sector_t *n = find_by_tag(t, 0xA1);   /* normal */
    const uft_sector_t *d = find_by_tag(t, 0xB2);   /* deleted */
    const uft_sector_t *e = find_by_tag(t, 0xC3);   /* crc error */
    ASSERT(n && d && e);
    /* normal sector: no marks */
    ASSERT(n->crc_ok == true);
    ASSERT(n->deleted == false);
    /* deleted-address-mark sector */
    ASSERT(d->deleted == true);
    /* CRC-error sector: crc_ok false across all three alias fields (FMT-5) */
    ASSERT(e->crc_ok == false);
    ASSERT(e->crc_valid == false);
    ASSERT(e->data_crc_ok == false);
}

TEST(read_surfaces_error_marks) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_imd_with_marks(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_imd.open(&disk, path, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_imd.read_track(&disk, 0, 0, &t) == UFT_OK);
    assert_marks(&t);

    free_track_sectors(&t);
    if (uft_format_plugin_imd.close) uft_format_plugin_imd.close(&disk);
    remove(path);
}

TEST(marks_survive_write_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_imd_with_marks(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_imd.open(&disk, path, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_imd.read_track(&disk, 0, 0, &t) == UFT_OK);
    /* write the track back unchanged, then re-read */
    ASSERT(uft_format_plugin_imd.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_track_sectors(&t);

    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_imd.read_track(&disk, 0, 0, &t2) == UFT_OK);
    assert_marks(&t2);                 /* deleted + CRC-error still present */

    free_track_sectors(&t2);
    if (uft_format_plugin_imd.close) uft_format_plugin_imd.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== IMD disk-error marking (read / represent / preserve) ===\n");
    RUN(read_surfaces_error_marks);
    RUN(marks_survive_write_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
