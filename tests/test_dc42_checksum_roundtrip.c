/**
 * @file test_dc42_checksum_roundtrip.c
 * @brief DC42 (DiskCopy 4.2) write_track must keep the data checksum valid.
 *
 * Links the real DC42 plugin (src/formats/dc42/uft_dc42.c). DC42 stores a
 * data checksum (BE32 at 0x48) over the whole data fork; write_track modifies
 * sector data, so if it does not recompute the checksum the written image is
 * spec-invalid (real DiskCopy / emulators flag it as corrupt) even though the
 * tool's own reader — which does not validate it — reads back fine.
 *
 * The test builds a DC42 with a deliberately-wrong (zero) checksum, opens it,
 * modifies a sector, write_tracks, then INDEPENDENTLY recomputes the DiskCopy
 * checksum over the final data fork and asserts the header now matches — i.e.
 * write_track fixed it. Reference algorithm: add each big-endian 16-bit word
 * to a 32-bit accumulator, then rotate right 1 (DiscFerret / Mini vMac).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_dc42;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define DC42_HDR      84u
#define DC42_DATA     409600u          /* 400K GCR: 80 cyl x 1 head x 10 spt x 512 */
#define DC42_FILE     (DC42_HDR + DC42_DATA)

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
    snprintf(path, size, "%s/uft_dc42_%d.image", dir, rand() % 100000);
}

/* Independent reference implementation of the DiskCopy 4.2 data checksum. */
static uint32_t ref_checksum(const uint8_t *d, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2) {
        uint16_t w = (uint16_t)(((uint16_t)d[i] << 8) | d[i + 1]);
        sum += w;
        sum = (sum >> 1) | (sum << 31);
    }
    return sum;
}

/* Build a DC42 (fmt 0x00, 400K) with a WRONG (zero) data checksum. */
static int build_dc42(const char *path) {
    uint8_t *buf = calloc(1, DC42_FILE);
    if (!buf) return 0;
    /* header */
    buf[0x40] = (uint8_t)(DC42_DATA >> 24); buf[0x41] = (uint8_t)(DC42_DATA >> 16);
    buf[0x42] = (uint8_t)(DC42_DATA >> 8);  buf[0x43] = (uint8_t)DC42_DATA;   /* data size */
    /* tag size (0x44) = 0; data checksum (0x48) deliberately left 0 (wrong) */
    buf[0x50] = 0x00;                 /* format: 400K GCR */
    buf[82] = 0x01; buf[83] = 0x00;   /* magic 0x0100 */
    /* deterministic data fork */
    for (uint32_t i = 0; i < DC42_DATA; i++)
        buf[DC42_HDR + i] = (uint8_t)((i * 5u + 1u) & 0xFF);
    FILE *f = fopen(path, "wb");
    if (!f) { free(buf); return 0; }
    size_t w = fwrite(buf, 1, DC42_FILE, f);
    fclose(f); free(buf);
    return w == DC42_FILE;
}

TEST(write_updates_data_checksum) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_dc42(path));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_dc42.open(&disk, path, false) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_dc42.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 10);
    ASSERT(t.sectors[0].data != NULL);

    /* modify a sector and write it back (must recompute the checksum) */
    t.sectors[0].data[7] = 0xC5;
    ASSERT(uft_format_plugin_dc42.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_track_sectors(&t);
    uft_format_plugin_dc42.close(&disk);

    /* read the whole file back and verify the header checksum matches the
     * data fork (i.e. write_track kept it valid) */
    FILE *f = fopen(path, "rb");
    ASSERT(f != NULL);
    uint8_t *buf = malloc(DC42_FILE);
    ASSERT(buf != NULL);
    ASSERT(fread(buf, 1, DC42_FILE, f) == DC42_FILE);
    fclose(f);

    uint32_t stored = ((uint32_t)buf[0x48] << 24) | ((uint32_t)buf[0x49] << 16) |
                      ((uint32_t)buf[0x4A] << 8) | buf[0x4B];
    uint32_t expect = ref_checksum(buf + DC42_HDR, DC42_DATA);
    ASSERT(stored != 0);          /* was 0 before — proves it was updated */
    ASSERT(stored == expect);     /* THE POINT: checksum is valid for the data */
    /* and the modified byte really landed */
    ASSERT(buf[DC42_HDR + 7] == 0xC5);

    free(buf);
    remove(path);
}

int main(void) {
    printf("=== DC42 write checksum round-trip test ===\n");
    RUN(write_updates_data_checksum);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
