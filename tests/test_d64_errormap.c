/**
 * @file test_d64_errormap.c
 * @brief D64 error-info block: read + represent the 1541 error codes (MF-333).
 *
 * Links the real D64 plugin (src/formats/d64/uft_d64_plugin.c). A .d64 with a
 * trailing error-info block (175531 bytes = 174848 data + 683 error bytes)
 * stores one 1541 controller error code per sector. Before MF-333 the plugin
 * read only the sector data and dropped that block silently. This test builds
 * such an image with one sector flagged 0x05 (data checksum error) and asserts
 * the plugin now surfaces it as a CRC-bad sector, while a plain 174848-byte
 * .d64 (no error block) reports every sector clean.
 *
 * Scope note: this is the read + represent half. Preserving the error block on
 * write is a container-level concern (whole-file trailer, not per-track) and
 * remains open (KNOWN_ISSUES FMT-9).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d64;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D64_DATA_35  174848u
#define D64_SECS_35  683u                 /* error bytes for a 35-track image */
#define ERR_SECTOR   5                    /* track 1 sector 5 -> linear index 5 */

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_d64_err_%d.d64", dir, rand() % 100000);
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* Write a 35-track .d64. If with_errors, append a 683-byte error block with
   every sector 0x01 (OK) except linear index ERR_SECTOR set to 0x05. */
static int build_d64(const char *path, int with_errors) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    uint8_t *data = malloc(D64_DATA_35);
    if (!data) { fclose(f); return 0; }
    memset(data, 0xAA, D64_DATA_35);
    int ok = fwrite(data, 1, D64_DATA_35, f) == D64_DATA_35;
    free(data);
    if (with_errors && ok) {
        uint8_t *err = malloc(D64_SECS_35);
        if (!err) { fclose(f); return 0; }
        memset(err, 0x01, D64_SECS_35);       /* all OK */
        err[ERR_SECTOR] = 0x05;               /* data checksum error */
        ok = fwrite(err, 1, D64_SECS_35, f) == D64_SECS_35;
        free(err);
    }
    fclose(f);
    return ok;
}

TEST(errormap_surfaces_crc_bad_sector) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path, 1));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, path, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 21);           /* track 1 has 21 sectors */

    /* sectors are added in order 0..20; index ERR_SECTOR must be CRC-bad,
       every other sector clean. */
    for (size_t s = 0; s < t.sector_count; s++) {
        if ((int)s == ERR_SECTOR)
            ASSERT(t.sectors[s].crc_ok == false);
        else
            ASSERT(t.sectors[s].crc_ok == true);
    }

    free_track_sectors(&t);
    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);
    remove(path);
}

TEST(no_errormap_all_clean) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path, 0));             /* plain 174848-byte .d64 */

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk, path, true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 21);
    for (size_t s = 0; s < t.sector_count; s++)
        ASSERT(t.sectors[s].crc_ok == true);

    free_track_sectors(&t);
    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== D64 error-info block read/represent ===\n");
    RUN(errormap_surfaces_crc_bad_sector);
    RUN(no_errormap_all_clean);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
