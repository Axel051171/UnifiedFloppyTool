/**
 * @file test_adf_ext_plugin.c
 * @brief Extended ADF (UAE-1ADF) reader — container parse + track preservation (MF-352).
 *
 * Links the real Extended-ADF plugin. Builds a synthetic UAE-1ADF (big-endian,
 * per the WinUAE read_header_ext2 layout) with 2 tracks — one AmigaDOS track
 * (11x512 sectors) and one raw-MFM track — and asserts:
 *   - the "UAE-1ADF" magic probes,
 *   - the header + 12-byte track table parse correctly,
 *   - the AmigaDOS track yields 11 sectors with the right data,
 *   - the raw-MFM track is preserved verbatim in raw_data (no bit dropped).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_adf_ext;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define AMIGADOS_LEN (11u * 512u)   /* 5632 */
#define RAWMFM_LEN   1024u

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_adfext_%d.adf", dir, rand() % 100000);
}
static void free_ts(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
    free(tr->raw_data); tr->raw_data = NULL; tr->raw_size = tr->raw_len = 0;
}
static void put_be16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xFF; }
/* 24-bit big-endian value in a 4-byte field (high byte 0), per WinUAE. */
static void put_be24_in32(uint8_t *p, uint32_t v) {
    p[0] = 0; p[1] = (v >> 16) & 0xFF; p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF;
}

/* Build a 2-track UAE-1ADF: track0 AmigaDOS (11x512), track1 raw MFM. */
static int build_adfext(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = 1;
    /* header: magic + reserved(0) + num_tracks(2) */
    ok &= fwrite("UAE-1ADF", 1, 8, f) == 8;
    uint8_t hw[4]; put_be16(&hw[0], 0); put_be16(&hw[2], 2);
    ok &= fwrite(hw, 1, 4, f) == 4;
    /* track table: 2 x 12 bytes */
    uint8_t td[12];
    memset(td, 0, sizeof(td));
    td[3] = 0; put_be24_in32(&td[4], AMIGADOS_LEN); put_be24_in32(&td[8], 0);
    ok &= fwrite(td, 1, 12, f) == 12;               /* track0: AmigaDOS */
    memset(td, 0, sizeof(td));
    td[3] = 1; put_be24_in32(&td[4], RAWMFM_LEN); put_be24_in32(&td[8], RAWMFM_LEN * 8);
    ok &= fwrite(td, 1, 12, f) == 12;               /* track1: raw MFM */
    /* track0 data: 11x512, sector s first byte = 0xA0+s */
    for (int s = 0; s < 11; s++) {
        uint8_t sec[512]; memset(sec, 0x11, sizeof(sec)); sec[0] = (uint8_t)(0xA0 + s);
        ok &= fwrite(sec, 1, 512, f) == 512;
    }
    /* track1 data: raw MFM, byte i = i & 0xFF */
    uint8_t *raw = malloc(RAWMFM_LEN);
    for (unsigned i = 0; i < RAWMFM_LEN; i++) raw[i] = (uint8_t)(i & 0xFF);
    ok &= fwrite(raw, 1, RAWMFM_LEN, f) == RAWMFM_LEN;
    free(raw);
    fclose(f);
    return ok;
}

TEST(probe_and_geometry) {
    int c = 0;
    ASSERT(uft_format_plugin_adf_ext.probe((const uint8_t*)"UAE-1ADF\0\0\0\2", 12, 0, &c) == true);
    ASSERT(c > 0);
    ASSERT(uft_format_plugin_adf_ext.probe((const uint8_t*)"NOTMAGIC1234", 12, 0, &c) == false);

    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_adfext(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_adf_ext.open(&disk, path, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 1);   /* 2 tracks / 2 heads */
    ASSERT(disk.geometry.heads == 2);
    ASSERT(disk.geometry.sectors == 11);
    if (uft_format_plugin_adf_ext.close) uft_format_plugin_adf_ext.close(&disk);
    remove(path);
}

TEST(amigados_track_decodes_sectors) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_adfext(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_adf_ext.open(&disk, path, true) == UFT_OK);
    /* track 0 = cyl 0 head 0 (AmigaDOS) */
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_adf_ext.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 11);
    ASSERT(t.sectors[0].data[0] == 0xA0);
    ASSERT(t.sectors[10].data[0] == 0xAA);
    ASSERT(t.raw_data == NULL);              /* AmigaDOS track is sectors, not raw */
    free_ts(&t);
    if (uft_format_plugin_adf_ext.close) uft_format_plugin_adf_ext.close(&disk);
    remove(path);
}

TEST(raw_mfm_track_preserved) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_adfext(path));
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    ASSERT(uft_format_plugin_adf_ext.open(&disk, path, true) == UFT_OK);
    /* track 1 = cyl 0 head 1 (raw MFM) */
    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_adf_ext.read_track(&disk, 0, 1, &t) == UFT_OK);
    ASSERT(t.sector_count == 0);             /* raw track, no decoded sectors */
    ASSERT(t.raw_data != NULL);
    ASSERT(t.raw_size == RAWMFM_LEN);
    /* verbatim preservation: byte i == i & 0xFF (no bit dropped) */
    for (unsigned i = 0; i < RAWMFM_LEN; i++)
        ASSERT(t.raw_data[i] == (uint8_t)(i & 0xFF));
    free_ts(&t);
    if (uft_format_plugin_adf_ext.close) uft_format_plugin_adf_ext.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== Extended ADF (UAE-1ADF) reader (Phase-3 new format) ===\n");
    RUN(probe_and_geometry);
    RUN(amigados_track_decodes_sectors);
    RUN(raw_mfm_track_preserved);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
