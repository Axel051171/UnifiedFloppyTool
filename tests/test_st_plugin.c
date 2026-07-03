/**
 * @file test_st_plugin.c
 * @brief Atari ST (.ST) raw-sector format — real end-to-end tests.
 *
 * Unlike the probe-replica plugin tests, this links the REAL module
 * (src/formats/atari/st.c) and drives its actual open/read/write/close
 * FloppyDevice API against a synthesized on-disk image. It therefore
 * tests the production code, not a mirror of it:
 *
 *   synthesize 737280-byte DD image (deterministic per-LBA payload)
 *     -> uft_ata_st_open  (geometry inference 80/2/9/512)
 *     -> uft_ata_st_read_sector  (LBA math + byte-exact readback)
 *     -> uft_ata_st_write_sector (round-trip: write, reopen, read back)
 *     -> bounds + bad-size + null-arg error paths
 *
 * The module (st.h) declares nothing public, so the API is forward-
 * declared here matching st.c exactly.
 */

#include "uft/floppy/uft_floppy_device.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── production API from src/formats/atari/st.c (no public header) ──── */
int uft_ata_st_open(FloppyDevice *dev, const char *path);
int uft_ata_st_close(FloppyDevice *dev);
int uft_ata_st_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_ata_st_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define ST_DD_SIZE   737280u          /* 80 trk × 2 hd × 9 spt × 512 B */
#define ST_TRACKS    80u
#define ST_HEADS     2u
#define ST_SPT       9u
#define ST_BPS       512u
#define TMP_IMG      "uft_st_plugin_tmp.img"

/* Deterministic per-LBA payload — byte 0/1 encode the LBA, rest a mix. */
static void fill_sector(uint8_t *buf, uint32_t lba) {
    buf[0] = (uint8_t)(lba & 0xFF);
    buf[1] = (uint8_t)((lba >> 8) & 0xFF);
    for (uint32_t i = 2; i < ST_BPS; i++)
        buf[i] = (uint8_t)((lba * 31u + i * 7u) & 0xFF);
}

static uint32_t chs_to_lba(uint32_t t, uint32_t h, uint32_t s) {
    return (t * ST_HEADS + h) * ST_SPT + (s - 1u);
}

/* Write a full deterministic DD image to TMP_IMG. Returns 1 on success. */
static int synth_image(uint32_t total_bytes) {
    FILE *fp = fopen(TMP_IMG, "wb");
    if (!fp) return 0;
    uint8_t sec[ST_BPS];
    uint32_t written = 0;
    uint32_t lba = 0;
    while (written + ST_BPS <= total_bytes) {
        fill_sector(sec, lba++);
        if (fwrite(sec, 1, ST_BPS, fp) != ST_BPS) { fclose(fp); return 0; }
        written += ST_BPS;
    }
    /* tail padding for non-sector-multiple sizes (bad-size test) */
    while (written < total_bytes) { fputc(0, fp); written++; }
    fclose(fp);
    return 1;
}

/* ── A. open + geometry inference ──────────────────────────────────── */

TEST(open_dd_infers_geometry) {
    ASSERT(synth_image(ST_DD_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_ata_st_open(&dev, TMP_IMG) == UFT_OK);
    ASSERT(dev.tracks == ST_TRACKS);
    ASSERT(dev.heads == ST_HEADS);
    ASSERT(dev.sectors == ST_SPT);
    ASSERT(dev.sectorSize == ST_BPS);
    ASSERT(dev.flux_supported == false);
    uft_ata_st_close(&dev);
    remove(TMP_IMG);
}

/* ── B. read is byte-exact and LBA-correct ─────────────────────────── */

TEST(read_sector_byte_exact) {
    ASSERT(synth_image(ST_DD_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_ata_st_open(&dev, TMP_IMG) == UFT_OK);

    /* a spread of CHS positions, including head-1 and last track */
    struct { uint32_t t, h, s; } probes[] = {
        {0,0,1}, {0,1,9}, {1,0,1}, {40,1,5}, {79,1,9}
    };
    for (size_t i = 0; i < sizeof(probes)/sizeof(probes[0]); i++) {
        uint8_t got[ST_BPS], want[ST_BPS];
        uint32_t lba = chs_to_lba(probes[i].t, probes[i].h, probes[i].s);
        fill_sector(want, lba);
        ASSERT(uft_ata_st_read_sector(&dev, probes[i].t, probes[i].h, probes[i].s, got) == UFT_OK);
        ASSERT(memcmp(got, want, ST_BPS) == 0);
    }
    uft_ata_st_close(&dev);
    remove(TMP_IMG);
}

/* ── C. write round-trip preserves bytes ───────────────────────────── */

TEST(write_read_roundtrip) {
    ASSERT(synth_image(ST_DD_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_ata_st_open(&dev, TMP_IMG) == UFT_OK);

    uint8_t wbuf[ST_BPS];
    for (uint32_t i = 0; i < ST_BPS; i++) wbuf[i] = (uint8_t)(0xA5 ^ i);
    ASSERT(uft_ata_st_write_sector(&dev, 10, 1, 4, wbuf) == UFT_OK);
    uft_ata_st_close(&dev);

    /* reopen and confirm the write persisted */
    memset(&dev, 0, sizeof(dev));
    ASSERT(uft_ata_st_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t rbuf[ST_BPS];
    ASSERT(uft_ata_st_read_sector(&dev, 10, 1, 4, rbuf) == UFT_OK);
    ASSERT(memcmp(rbuf, wbuf, ST_BPS) == 0);
    uft_ata_st_close(&dev);
    remove(TMP_IMG);
}

/* ── D. bounds + error paths ───────────────────────────────────────── */

TEST(read_out_of_bounds_rejected) {
    ASSERT(synth_image(ST_DD_SIZE));
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_ata_st_open(&dev, TMP_IMG) == UFT_OK);
    uint8_t buf[ST_BPS];
    ASSERT(uft_ata_st_read_sector(&dev, 0, 0, 0, buf) == UFT_EBOUNDS);     /* sector 0 invalid (1-based) */
    ASSERT(uft_ata_st_read_sector(&dev, 0, 0, ST_SPT + 1, buf) == UFT_EBOUNDS);
    ASSERT(uft_ata_st_read_sector(&dev, ST_TRACKS, 0, 1, buf) == UFT_EBOUNDS);
    ASSERT(uft_ata_st_read_sector(&dev, 0, ST_HEADS, 1, buf) == UFT_EBOUNDS);
    uft_ata_st_close(&dev);
    remove(TMP_IMG);
}

TEST(open_bad_size_rejected) {
    ASSERT(synth_image(12345u));          /* not a known ST geometry */
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_ata_st_open(&dev, TMP_IMG) == UFT_EINVAL);
    remove(TMP_IMG);
}

TEST(open_null_and_missing) {
    FloppyDevice dev; memset(&dev, 0, sizeof(dev));
    ASSERT(uft_ata_st_open(NULL, TMP_IMG) == UFT_EINVAL);
    ASSERT(uft_ata_st_open(&dev, NULL) == UFT_EINVAL);
    ASSERT(uft_ata_st_open(&dev, "definitely_not_here_9f8e7d.img") == UFT_ENOENT);
}

int main(void) {
    printf("=== Atari ST (.ST) plugin — real open/read/write tests ===\n");
    RUN(open_dd_infers_geometry);
    RUN(read_sector_byte_exact);
    RUN(write_read_roundtrip);
    RUN(read_out_of_bounds_rejected);
    RUN(open_bad_size_rejected);
    RUN(open_null_and_missing);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
