/**
 * @file test_moof_roundtrip.c
 * @brief MOOF writer — save -> read semantic round-trip test.
 *
 * Links the real module (src/formats/apple/uft_moof_parser.c) and proves
 * the new uft_moof_save() produces a MOOF that the production reader loads
 * back with identical flux content: build a disk struct with known TMAP +
 * per-track bitstream, save, reopen, and compare TMAP, bit_count and the
 * bitstream bytes transition-for-transition.
 *
 * MOOF INFO is 60 bytes on disk but uft_moof_info_t models 56, so whole-file
 * byte-identity is not the contract; flux fidelity (TMAP + bitstreams +
 * bit_count) is, and that is what preservation needs. If the writer got the
 * TRK table, block placement or CRC wrong, the reader would read back wrong
 * bytes or fail, and this test would catch it.
 */

#include "uft/formats/apple/uft_moof.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_moof_rt_%d.moof", dir, rand() % 100000);
}

/* ── A. TMAP + two tracks' bitstreams round-trip through save/open ──── */

TEST(save_read_roundtrip) {
    uft_moof_disk_t d1;
    memset(&d1, 0, sizeof(d1));
    d1.info.info_version = 1;
    d1.info.disk_type = 2;            /* 3.5" */
    d1.info.optimal_bit_timing = 16;
    memset(d1.tmap, UFT_MOOF_TMAP_UNUSED, sizeof(d1.tmap));
    d1.tmap[0] = 0;                   /* quarter-track 0 -> TRK 0 */
    d1.tmap[4] = 1;                   /* quarter-track 4 -> TRK 1 */

    /* Track 0: 1 block at block 3; Track 1: 1 block at block 4. */
    uint8_t bits0[UFT_MOOF_BLOCK_SIZE], bits1[UFT_MOOF_BLOCK_SIZE];
    for (int i = 0; i < UFT_MOOF_BLOCK_SIZE; i++) {
        bits0[i] = (uint8_t)((i * 7 + 3) & 0xFF);
        bits1[i] = (uint8_t)((i * 13 + 71) & 0xFF);
    }
    d1.tracks[0].bitstream = malloc(UFT_MOOF_BLOCK_SIZE);
    memcpy(d1.tracks[0].bitstream, bits0, UFT_MOOF_BLOCK_SIZE);
    d1.tracks[0].bitstream_size = UFT_MOOF_BLOCK_SIZE;
    d1.tracks[0].bit_count = 4000;
    d1.tracks[0].start_block = 3;
    d1.tracks[0].block_count = 1;
    d1.tracks[0].valid = true;

    d1.tracks[1].bitstream = malloc(UFT_MOOF_BLOCK_SIZE);
    memcpy(d1.tracks[1].bitstream, bits1, UFT_MOOF_BLOCK_SIZE);
    d1.tracks[1].bitstream_size = UFT_MOOF_BLOCK_SIZE;
    d1.tracks[1].bit_count = 4096;
    d1.tracks[1].start_block = 4;
    d1.tracks[1].block_count = 1;
    d1.tracks[1].valid = true;
    d1.track_count = 2;

    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(uft_moof_save(&d1, path) == UFT_MOOF_OK);

    uft_moof_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    ASSERT(uft_moof_open(path, &d2) == UFT_MOOF_OK);

    ASSERT(memcmp(d2.tmap, d1.tmap, sizeof(d1.tmap)) == 0);
    ASSERT(d2.info.disk_type == 2);

    ASSERT(d2.tracks[0].valid);
    ASSERT(d2.tracks[0].bit_count == 4000);
    ASSERT(d2.tracks[0].block_count == 1);
    ASSERT(d2.tracks[0].bitstream_size == UFT_MOOF_BLOCK_SIZE);
    ASSERT(memcmp(d2.tracks[0].bitstream, bits0, UFT_MOOF_BLOCK_SIZE) == 0);

    ASSERT(d2.tracks[1].valid);
    ASSERT(d2.tracks[1].bit_count == 4096);
    ASSERT(memcmp(d2.tracks[1].bitstream, bits1, UFT_MOOF_BLOCK_SIZE) == 0);

    /* the two tracks address distinct blocks -> distinct data */
    ASSERT(memcmp(d2.tracks[0].bitstream, d2.tracks[1].bitstream, UFT_MOOF_BLOCK_SIZE) != 0);

    free(d1.tracks[0].bitstream);
    free(d1.tracks[1].bitstream);
    uft_moof_close(&d2);
    remove(path);
}

/* ── B. written MOOF has a valid CRC and header ────────────────────── */

TEST(written_moof_crc_valid) {
    uft_moof_disk_t d1;
    memset(&d1, 0, sizeof(d1));
    d1.info.info_version = 1;
    d1.info.disk_type = 2;
    memset(d1.tmap, UFT_MOOF_TMAP_UNUSED, sizeof(d1.tmap));
    d1.tmap[0] = 0;
    uint8_t blk[UFT_MOOF_BLOCK_SIZE];
    for (int i = 0; i < UFT_MOOF_BLOCK_SIZE; i++) blk[i] = (uint8_t)(0xA5 ^ i);
    d1.tracks[0].bitstream = malloc(UFT_MOOF_BLOCK_SIZE);
    memcpy(d1.tracks[0].bitstream, blk, UFT_MOOF_BLOCK_SIZE);
    d1.tracks[0].bitstream_size = UFT_MOOF_BLOCK_SIZE;
    d1.tracks[0].bit_count = 3000;
    d1.tracks[0].start_block = 3;
    d1.tracks[0].block_count = 1;
    d1.tracks[0].valid = true;

    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(uft_moof_save(&d1, path) == UFT_MOOF_OK);

    uft_moof_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    ASSERT(uft_moof_open(path, &d2) == UFT_MOOF_OK);
    ASSERT(d2.crc_valid == true);    /* writer computed a correct CRC32 */

    free(d1.tracks[0].bitstream);
    uft_moof_close(&d2);
    remove(path);
}

/* ── C. error paths ────────────────────────────────────────────────── */

TEST(save_rejects_null) {
    uft_moof_disk_t d;
    memset(&d, 0, sizeof(d));
    ASSERT(uft_moof_save(NULL, "x.moof") != UFT_MOOF_OK);
    ASSERT(uft_moof_save(&d, NULL) != UFT_MOOF_OK);
}

int main(void) {
    printf("=== MOOF writer round-trip tests ===\n");
    RUN(save_read_roundtrip);
    RUN(written_moof_crc_valid);
    RUN(save_rejects_null);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
