/**
 * @file test_d64_write_roundtrip.c
 * @brief D64 plugin write_track -> read_track round-trip (Commodore family).
 *
 * Links the real D64 plugin (src/formats/d64/uft_d64_plugin.c) and proves
 * writes persist through a read->modify->write->read cycle. D64 write_track
 * gates on uft_sector_t.data_len (`if (!data || len == 0) ... pad`), so this
 * also confirms the systemic data_len root fix (MF-321) works on a different
 * plugin family than IMD: read_track builds sectors via uft_format_add_sector,
 * which now sets data_len, so the write copies real data instead of padding.
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

#define D64_SIZE   174848u        /* 35-track image */

/* read_track fills a caller-owned (stack) track: free its heap sectors, not
 * the track struct (uft_track_free() would free(track), wrong for a stack). */
static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_d64_wr_%d.d64", dir, rand() % 100000);
}

/* Deterministic 174848-byte D64: byte i = (i*7 + 3) mod 251 so track-1
 * sector-0 (offset 0) has a known, non-uniform pattern. */
static int build_d64(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    for (uint32_t i = 0; i < D64_SIZE; i++)
        if (fputc((int)((i * 7u + 3u) % 251u), f) == EOF) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

TEST(write_track_persists_to_read) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_d64(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_d64.open(&disk, path, false) == UFT_OK);

    /* track 1 == cyl 0, 21 sectors */
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_d64.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 21);
    ASSERT(t.sectors[0].data != NULL);
    /* sector 0 first byte = synthesized byte at offset 0 = 3 */
    ASSERT(t.sectors[0].data[0] == (uint8_t)3);
    /* systemic fix: read-produced sector must carry a real length */
    ASSERT(t.sectors[0].data_len == 256);

    /* change sector 0 byte 5 and sector 3 byte 0, write back */
    t.sectors[0].data[5] = 0x99;
    t.sectors[3].data[0] = 0x77;
    ASSERT(uft_format_plugin_d64.write_track(&disk, 0, 0, &t) == UFT_OK);
    uint8_t sec2_b0 = t.sectors[2].data[0];   /* remember an untouched sector */
    free_track_sectors(&t);

    /* re-read: changes persisted, untouched sector unchanged */
    uft_track_t t2;
    memset(&t2, 0, sizeof(t2));
    /* MF-947: ERST SCHLIESSEN, DANN NEU OEFFNEN.
     *
     * Hier stand ein zweites read_track auf DEMSELBEN offenen Griff. Das
     * prueft den Speicherpuffer des Plugins, nicht die Datei — genau die
     * Konstruktion, mit der test_imd_write_roundtrip jahrelang gruen war,
     * waehrend IMD keine einzige Schreiboperation besass (MF-883). */
    if (uft_format_plugin_d64.close) uft_format_plugin_d64.close(&disk);

    uft_disk_t disk2;
    memset(&disk2, 0, sizeof(disk2));
    disk2.read_only = true;
    ASSERT(uft_format_plugin_d64.open(&disk2, path, true) == UFT_OK);

    ASSERT(uft_format_plugin_d64.read_track(&disk2, 0, 0, &t2) == UFT_OK);
    ASSERT(t2.sector_count == 21);
    ASSERT(t2.sectors[0].data[5] == 0x99);   /* THE POINT: write took effect */
    ASSERT(t2.sectors[3].data[0] == 0x77);
    ASSERT(t2.sectors[2].data[0] == sec2_b0); /* neighbour untouched */
    free_track_sectors(&t2);

    uft_format_plugin_d64.close(&disk2);
    remove(path);
}

int main(void) {
    printf("=== D64 write_track round-trip test ===\n");
    RUN(write_track_persists_to_read);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
