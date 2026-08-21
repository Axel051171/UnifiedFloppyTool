/**
 * @file test_cqm_layout.c
 * @brief CopyQM header layout and RLE polarity (MF-461).
 *
 * Links the real CQM plugin (src/formats/cqm/uft_cqm.c) and feeds it an image
 * built byte for byte to the published layout:
 *
 *   [1] "CopyQM Format (*.cqm) — Disk image layout", RPN, 2023-03-31,
 *       https://rio.early8bitz.de/cqm/cqm-format.pdf (from LibDsk drvqm.c)
 *   [2] SAMdisk 4.0, src/samdisk/cqm.cpp:10-41
 *
 * Why this test exists. Before MF-461 the plugin read a layout that appears in
 * neither source: sector size as a 128<<n code from byte 0x03, sectors per
 * track from 0x08, heads from 0x09, cylinders from 0x0F, comment length from
 * 0x10, data starting at offset 18 instead of 133 — and the RLE counts with
 * inverted signs. Against a spec-conformant file that reader takes byte 0x08
 * (the FAT-copy count, 2 in a DOS image) for the sector count and byte 0x0F
 * (high byte of sectors-per-FAT, 0) for the cylinder count, so it produces an
 * empty disk. Every assertion below fails on it.
 *
 * The file built here exercises both RLE branches: each sector is written as a
 * short literal run (its tag) followed by a repeat run for the remainder.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_cqm;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define HDR_SIZE   133
#define SEC_SIZE   512
#define SPT        9
#define HEADS      2
#define USED_CYLS  3
#define TOTAL_CYLS 40
#define COMMENT    "TEST"
#define TAG_LEN    4

static void get_temp_path(char *path, size_t size, const char *tag) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_cqm_%s_%d.cqm", dir, tag, rand() % 100000);
}

static void put_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/* CopyQM data CRC, written from [1]: reflected CRC-32 (0xEDB88320), start 0,
 * and only the low six bits of each byte reach the table. Kept separate from
 * the plugin's copy on purpose — a test that calls the code under test to
 * compute its own expectation proves nothing. */
static uint32_t ref_crc(const uint8_t *buf, size_t len) {
    uint32_t table[0x40];
    for (uint32_t i = 0; i < 0x40; i++) {
        uint32_t e = i;
        for (int j = 0; j < 8; j++) e = (e >> 1) ^ ((e & 1u) ? 0xEDB88320u : 0u);
        table[i] = e;
    }
    uint32_t crc = 0;
    while (len--) crc = table[(crc ^ *buf++) & 0x3fu] ^ (crc >> 8);
    return crc;
}

/* Tag written into the first TAG_LEN bytes of every sector, so a wrong offset
 * anywhere (comment length, header size, track stride) shows up as a mismatch
 * rather than as plausible-looking data. */
static void sector_tag(uint8_t *out, int cyl, int head, int sec) {
    out[0] = 0xC0; out[1] = (uint8_t)cyl; out[2] = (uint8_t)head; out[3] = (uint8_t)sec;
}

/**
 * Build a CQM image. @p break_header_sum and @p break_data_crc deliberately
 * corrupt one integrity field each.
 */
static int build_cqm(const char *path, bool break_header_sum, bool break_data_crc) {
    const size_t plain_size = (size_t)USED_CYLS * HEADS * SPT * SEC_SIZE;
    uint8_t *plain = calloc(1, plain_size);
    if (!plain) return 0;

    size_t off = 0;
    for (int c = 0; c < USED_CYLS; c++)
        for (int h = 0; h < HEADS; h++)
            for (int s = 0; s < SPT; s++, off += SEC_SIZE) {
                sector_tag(&plain[off], c, h, s);
                memset(&plain[off + TAG_LEN], 0xE5, SEC_SIZE - TAG_LEN);
            }

    uint8_t hdr[HDR_SIZE];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'C'; hdr[1] = 'Q'; hdr[2] = 0x14;
    put_le16(&hdr[0x03], SEC_SIZE);     /* bytes per sector      */
    hdr[0x05] = 2;                      /* sectors per cluster   */
    put_le16(&hdr[0x06], 1);            /* reserved sectors      */
    hdr[0x08] = 2;                      /* FAT copies            */
    put_le16(&hdr[0x09], 112);          /* root dir entries      */
    put_le16(&hdr[0x0B], (uint16_t)(USED_CYLS * HEADS * SPT));
    hdr[0x0D] = 0xFD;                   /* media descriptor      */
    put_le16(&hdr[0x0E], 2);            /* sectors per FAT       */
    put_le16(&hdr[0x10], SPT);          /* sectors per track     */
    put_le16(&hdr[0x12], HEADS);        /* heads                 */
    memcpy(&hdr[0x1C], "UFT synthetic CQM", 17);
    hdr[0x58] = 0;                      /* blind = 0 (DOS)       */
    hdr[0x59] = 0;                      /* density = DD          */
    hdr[0x5A] = USED_CYLS;
    hdr[0x5B] = TOTAL_CYLS;
    put_le32(&hdr[0x5C], ref_crc(plain, plain_size) ^ (break_data_crc ? 0xFFFFFFFFu : 0u));
    memcpy(&hdr[0x60], "** NONE **", 10);
    put_le16(&hdr[0x6F], (uint16_t)strlen(COMMENT));
    hdr[0x71] = 0;                      /* sector base: first sector is 1 */
    hdr[0x74] = 1;                      /* interleave            */
    hdr[0x76] = 2;                      /* 5.25" 1.2MB           */

    unsigned sum = 0;
    for (int i = 0; i < HDR_SIZE - 1; i++) sum += hdr[i];
    hdr[0x84] = (uint8_t)((0u - sum) & 0xffu);
    if (break_header_sum) hdr[0x84] ^= 0x01;

    FILE *f = fopen(path, "wb");
    if (!f) { free(plain); return 0; }
    fwrite(hdr, 1, sizeof(hdr), f);
    fwrite(COMMENT, 1, strlen(COMMENT), f);

    /* RLE per [1]: positive count = that many literal bytes,
     *              negative count = next byte repeated -count times. */
    for (size_t p = 0; p < plain_size; p += SEC_SIZE) {
        uint8_t cnt[2];
        put_le16(cnt, TAG_LEN);
        fwrite(cnt, 1, 2, f);
        fwrite(&plain[p], 1, TAG_LEN, f);

        put_le16(cnt, (uint16_t)(int16_t)(-(SEC_SIZE - TAG_LEN)));
        fwrite(cnt, 1, 2, f);
        fputc(0xE5, f);
    }
    fclose(f);
    free(plain);
    return 1;
}

static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

TEST(probe_grades_full_header_above_bare_marker) {
    uint8_t buf[HDR_SIZE];
    int conf_marker = -1, conf_full = -1, conf_bad = -1;

    memset(buf, 0, sizeof(buf));
    buf[0] = 'C'; buf[1] = 'Q'; buf[2] = 0x14;

    /* three bytes and nothing else — the marker is real, the rest unseen */
    ASSERT(uft_format_plugin_cqm.probe(buf, 3, 3, &conf_marker));
    ASSERT(conf_marker == 60);

    /* a full, self-consistent header must outrank it */
    char path[512];
    get_temp_path(path, sizeof(path), "probe");
    ASSERT(build_cqm(path, false, false));
    FILE *f = fopen(path, "rb");
    ASSERT(f != NULL);
    ASSERT(fread(buf, 1, sizeof(buf), f) == sizeof(buf));
    fclose(f);

    ASSERT(uft_format_plugin_cqm.probe(buf, sizeof(buf), sizeof(buf), &conf_full));
    ASSERT(conf_full == 95);

    /* header checksum broken: still a CQM by marker, but not a confident one */
    buf[0x84] ^= 0x01;
    ASSERT(uft_format_plugin_cqm.probe(buf, sizeof(buf), sizeof(buf), &conf_bad));
    ASSERT(conf_bad == 60);

    remove(path);
}

TEST(geometry_comes_from_the_documented_offsets) {
    char path[512];
    get_temp_path(path, sizeof(path), "geom");
    ASSERT(build_cqm(path, false, false));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_cqm.open(&disk, path, true) == UFT_OK);

    ASSERT(disk.geometry.sector_size == SEC_SIZE);   /* 0x03,0x04 — bytes, not a size code */
    ASSERT(disk.geometry.sectors     == SPT);        /* 0x10,0x11 — not 0x08 (FAT copies)  */
    ASSERT(disk.geometry.heads       == HEADS);      /* 0x12,0x13 — not 0x09 (dir entries) */

    /* u-cyl, not t-cyl: only the cylinders the image actually carries are
     * reported. Claiming 40 would hand out 37 cylinders that are not there. */
    ASSERT(disk.geometry.cylinders   == USED_CYLS);
    ASSERT(disk.geometry.total_sectors == (uint32_t)USED_CYLS * HEADS * SPT);

    uft_format_plugin_cqm.close(&disk);
    remove(path);
}

TEST(sector_data_lands_at_the_right_offset) {
    char path[512];
    get_temp_path(path, sizeof(path), "data");
    ASSERT(build_cqm(path, false, false));

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_cqm.open(&disk, path, true) == UFT_OK);

    /* Last track of the image — the furthest point from every offset error:
     * header size, comment length, track stride and RLE polarity all have to
     * be right for this one to match. */
    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_cqm.read_track(&disk, USED_CYLS - 1, HEADS - 1, &tr) == UFT_OK);
    ASSERT(tr.sector_count == SPT);

    for (size_t s = 0; s < tr.sector_count; s++) {
        uint8_t want[TAG_LEN];
        sector_tag(want, USED_CYLS - 1, HEADS - 1, (int)s);
        ASSERT(tr.sectors[s].data != NULL);
        ASSERT(tr.sectors[s].data_len == SEC_SIZE);
        ASSERT(memcmp(tr.sectors[s].data, want, TAG_LEN) == 0);
        /* remainder came from the repeat run */
        ASSERT(tr.sectors[s].data[TAG_LEN] == 0xE5);
        ASSERT(tr.sectors[s].data[SEC_SIZE - 1] == 0xE5);
        /* sector base 0 means the first sector is numbered 1 */
        ASSERT(tr.sectors[s].id.sector == (uint8_t)(s + 1));
    }
    free_track_sectors(&tr);

    /* Cylinder 0 must not be a copy of cylinder 2 — catches a stride of zero. */
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_cqm.read_track(&disk, 0, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == SPT);
    uint8_t want0[TAG_LEN];
    sector_tag(want0, 0, 0, 0);
    ASSERT(memcmp(tr.sectors[0].data, want0, TAG_LEN) == 0);
    free_track_sectors(&tr);

    uft_format_plugin_cqm.close(&disk);
    remove(path);
}

TEST(broken_header_checksum_is_refused) {
    char path[512];
    get_temp_path(path, sizeof(path), "hdrsum");
    ASSERT(build_cqm(path, true, false));

    /* [1]: the sum over the whole header must be zero. If it is not, every
     * geometry field below it is a guess — the reader says so instead of
     * inventing a disk. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_cqm.open(&disk, path, true) == UFT_ERR_CRC);
    ASSERT(disk.plugin_data == NULL);

    remove(path);
}

TEST(broken_data_crc_still_yields_the_data) {
    char path[512];
    get_temp_path(path, sizeof(path), "datacrc");
    ASSERT(build_cqm(path, false, true));

    /* Deliberate policy, opposite to the header checksum: the data CRC covers
     * the whole image, so a mismatch cannot be attributed to any one sector.
     * Refusing would throw away a damaged image that is still mostly readable.
     * The reader warns and hands the bytes over unchanged. */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_cqm.open(&disk, path, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof(tr));
    ASSERT(uft_format_plugin_cqm.read_track(&disk, 1, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == SPT);
    uint8_t want[TAG_LEN];
    sector_tag(want, 1, 0, 0);
    ASSERT(memcmp(tr.sectors[0].data, want, TAG_LEN) == 0);
    free_track_sectors(&tr);

    uft_format_plugin_cqm.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== CQM header layout and RLE polarity (MF-461) ===\n");
    RUN(probe_grades_full_header_above_bare_marker);
    RUN(geometry_comes_from_the_documented_offsets);
    RUN(sector_data_lands_at_the_right_offset);
    RUN(broken_header_checksum_is_refused);
    RUN(broken_data_crc_still_yields_the_data);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
