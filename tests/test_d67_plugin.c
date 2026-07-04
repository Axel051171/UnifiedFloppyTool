/**
 * @file test_d67_plugin.c
 * @brief Commodore 2040/3040 (.D67, DOS 1) format — real end-to-end tests.
 *
 * Links the REAL module (src/formats/commodore/d67.c) and drives its
 * production open/read/write/close FloppyDevice API against a synthesized
 * image. Also the regression guard for FMT-1: the module used to reject a
 * valid .D67 because its size gate hardcoded the FREE-block count (670)
 * instead of the total block count (690 = 176640 B, VICE reference).
 *
 * D67: 35 tracks, 1 head, 256-byte sectors, 690 sectors total. The 2040
 * DOS1 layout differs from the 1541 only in tracks 18-24 (20 sectors vs
 * 19), giving 690 vs 683 total. Sectors 0-based; LBA = track_offset(t)+s.
 * Correctness is proven table-independently via track_offset(1)==0.
 */

#include "uft/floppy/uft_floppy_device.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── production API from src/formats/commodore/d67.c ────────────────── */
int uft_cbm_d67_open(FloppyDevice *dev, const char *path);
int uft_cbm_d67_close(FloppyDevice *dev);
int uft_cbm_d67_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_cbm_d67_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define D67_BPS        256u
#define D67_SECTORS    690u           /* 2040 DOS1 total (VICE reference) */
#define D67_SIZE       (D67_SECTORS * D67_BPS)   /* 176640 */
#define D67_FREE_SIZE  (670u * D67_BPS)          /* the OLD buggy gate value */
#define TMP_IMG        "uft_d67_plugin_tmp.img"

static void fill_block(uint8_t *b, uint32_t lba) {
    b[0] = (uint8_t)(lba & 0xFF);
    b[1] = (uint8_t)((lba >> 8) & 0xFF);
    b[2] = (uint8_t)((lba >> 16) & 0xFF);
    b[3] = (uint8_t)((lba >> 24) & 0xFF);
    for (uint32_t i = 4; i < D67_BPS; i++)
        b[i] = (uint8_t)((lba * 131u + i * 17u) & 0xFF);
}

static uint32_t decode_lba(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static int synth_image(uint32_t total_bytes) {
    FILE *fp = fopen(TMP_IMG, "wb");
    if (!fp) return 0;
    uint8_t blk[D67_BPS];
    uint32_t written = 0, lba = 0;
    while (written + D67_BPS <= total_bytes) {
        fill_block(blk, lba++);
        if (fwrite(blk, 1, D67_BPS, fp) != D67_BPS) { fclose(fp); return 0; }
        written += D67_BPS;
    }
    while (written < total_bytes) { fputc(0, fp); written++; }
    fclose(fp);
    return 1;
}

/* ── A. open accepts the correct 690-block / 176640 B image ─────────── */

TEST(open_accepts_valid_690_block_image) {
    ASSERT(synth_image(D67_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d67_open(&dev, TMP_IMG) == UFT_OK);   /* FMT-1 regression */
    ASSERT(dev.tracks == 35);
    ASSERT(dev.heads == 1);
    ASSERT(dev.sectorSize == D67_BPS);
    uft_cbm_d67_close(&dev);
    remove(TMP_IMG);
}

/* ── B. the OLD free-block-count size (670) is NOT a valid image ─────── */

TEST(open_rejects_wrong_670_block_size) {
    ASSERT(synth_image(D67_FREE_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    /* 670*256 is the free-block count, not an image size — must be rejected. */
    ASSERT(uft_cbm_d67_open(&dev, TMP_IMG) == UFT_EINVAL);
    remove(TMP_IMG);
}

/* ── C. read maps track 1 sector s -> LBA s, byte-exact ─────────────── */

TEST(read_track1_maps_lba_directly) {
    ASSERT(synth_image(D67_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d67_open(&dev, TMP_IMG) == UFT_OK);
    for (uint32_t s = 0; s < 5; s++) {
        uint8_t got[D67_BPS], want[D67_BPS];
        ASSERT(uft_cbm_d67_read_sector(&dev, 1, 0, s, got) == UFT_OK);
        ASSERT(decode_lba(got) == s);
        fill_block(want, s);
        ASSERT(memcmp(got, want, D67_BPS) == 0);
    }
    uft_cbm_d67_close(&dev);
    remove(TMP_IMG);
}

/* ── D. the last track is reachable (was unreadable under the 670 gate) ─ */

TEST(last_track_is_reachable) {
    ASSERT(synth_image(D67_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d67_open(&dev, TMP_IMG) == UFT_OK);
    /* track 35 sector 0 must read a valid block below the total. Under the
     * old 670*256 gate an accepted file was too short and this returned
     * UFT_EBOUNDS. */
    uint8_t got[D67_BPS], want[D67_BPS];
    ASSERT(uft_cbm_d67_read_sector(&dev, 35, 0, 0, got) == UFT_OK);
    uint32_t lba = decode_lba(got);
    ASSERT(lba < D67_SECTORS);
    ASSERT(lba == D67_SECTORS - 17u);   /* track 35 sector 0 = total - spt[34](=17) */
    fill_block(want, lba);
    ASSERT(memcmp(got, want, D67_BPS) == 0);
    uft_cbm_d67_close(&dev);
    remove(TMP_IMG);
}

/* ── E. write round-trip ───────────────────────────────────────────── */

TEST(write_read_roundtrip) {
    ASSERT(synth_image(D67_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d67_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t wbuf[D67_BPS];
    for (uint32_t i = 0; i < D67_BPS; i++) wbuf[i] = (uint8_t)(0xC3 ^ i);
    ASSERT(uft_cbm_d67_write_sector(&dev, 1, 0, 4, wbuf) == UFT_OK);
    uft_cbm_d67_close(&dev);

    memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d67_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t rbuf[D67_BPS];
    ASSERT(uft_cbm_d67_read_sector(&dev, 1, 0, 4, rbuf) == UFT_OK);
    ASSERT(memcmp(rbuf, wbuf, D67_BPS) == 0);
    uft_cbm_d67_close(&dev);
    remove(TMP_IMG);
}

/* ── F. bounds + error paths ───────────────────────────────────────── */

TEST(bounds_and_null) {
    ASSERT(synth_image(D67_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_cbm_d67_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t buf[D67_BPS];
    ASSERT(uft_cbm_d67_read_sector(&dev, 0, 0, 0, buf) == UFT_EBOUNDS);   /* track < 1 */
    ASSERT(uft_cbm_d67_read_sector(&dev, 36, 0, 0, buf) == UFT_EBOUNDS);  /* track > 35 */
    ASSERT(uft_cbm_d67_read_sector(&dev, 1, 1, 0, buf) == UFT_EBOUNDS);   /* head != 0 */
    ASSERT(uft_cbm_d67_read_sector(&dev, 1, 0, 99, buf) == UFT_EBOUNDS);  /* sector >= spt */
    uft_cbm_d67_close(&dev);
    remove(TMP_IMG);
    ASSERT(uft_cbm_d67_open(NULL, TMP_IMG) == UFT_EINVAL);
    ASSERT(uft_cbm_d67_open(&dev, NULL) == UFT_EINVAL);
    ASSERT(uft_cbm_d67_open(&dev, "definitely_absent_d67_2a5c.img") == UFT_ENOENT);
}

int main(void) {
    printf("=== Commodore 2040 (.D67) plugin — real open/read/write tests ===\n");
    RUN(open_accepts_valid_690_block_image);
    RUN(open_rejects_wrong_670_block_size);
    RUN(read_track1_maps_lba_directly);
    RUN(last_track_is_reachable);
    RUN(write_read_roundtrip);
    RUN(bounds_and_null);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
