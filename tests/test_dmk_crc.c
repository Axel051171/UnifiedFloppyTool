/**
 * @file test_dmk_crc.c
 * @brief DMK per-sector ID/data CRC checking (MF-353).
 *
 * Links the real DMK plugin (src/formats/dmk/uft_dmk.c). DMK stores the raw
 * track bytes including the WD177x ID-field and data-field CRCs "just like a
 * real disk" (David Keil / openMSX spec). The plugin now computes CRC-CCITT-
 * FALSE (poly 0x1021, init 0xFFFF — the WD177x FDC CRC) over the address
 * mark + fields and compares to the stored CRC, marking id_crc_ok (header) and
 * crc_ok (data) separately.
 *
 * Verification without a corpus (liability-mode): the CRC algorithm is pinned
 * to the universal check value 0x29B1 for "123456789", and this test embeds a
 * spec-layout ID field whose CRC ([A1 A1 A1 FE 00 00 01 01]) is the fixed
 * reference 0xFA0C. A good sector must read crc_ok+id_crc_ok true; flipping one
 * stored CRC byte must flip exactly the corresponding flag.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_dmk;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define TRACK_LEN 6400u
#define SEC_SZ    256u

static uint16_t crc16(const uint8_t *d, size_t n) {
    uint16_t c = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t)d[i] << 8;
        for (int j = 0; j < 8; j++)
            c = (c & 0x8000) ? (uint16_t)((c << 1) ^ 0x1021) : (uint16_t)(c << 1);
    }
    return c;
}

static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_dmk_crc_%d.dmk", dir, rand() % 100000);
}
static void free_ts(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}

/* Build a 1-track DMK with one MFM sector; optionally corrupt the ID or data
   CRC by flipping one stored byte. */
static int build_dmk(const char *path, int corrupt_id, int corrupt_data) {
    uint8_t *file = calloc(1, 16 + TRACK_LEN);
    if (!file) return 0;
    /* DMK header */
    file[1] = 1;                          /* 1 track */
    file[2] = TRACK_LEN & 0xFF; file[3] = (TRACK_LEN >> 8) & 0xFF;
    file[4] = 0x00;                       /* double-sided, double-density (MFM) */

    uint8_t *t = file + 16;               /* track 0 */
    /* IDAM pointer table entry 0 -> FE at offset 0x93, DD flag (bit15). */
    uint16_t fe_off = 0x93;
    uint16_t ptr = fe_off | 0x8000;
    t[0] = ptr & 0xFF; t[1] = (ptr >> 8) & 0xFF;

    /* ID: A1 A1 A1 FE C H R N + CRC(BE) */
    t[0x90] = t[0x91] = t[0x92] = 0xA1;
    t[0x93] = 0xFE; t[0x94] = 0; t[0x95] = 0; t[0x96] = 1; t[0x97] = 1;
    uint16_t id_crc = crc16(&t[0x90], 8);         /* [A1 A1 A1 FE 00 00 01 01] */
    if (corrupt_id) id_crc ^= 0x0100;
    t[0x98] = id_crc >> 8; t[0x99] = id_crc & 0xFF;

    /* DATA: A1 A1 A1 FB data[256] + CRC(BE) */
    t[0x9C] = t[0x9D] = t[0x9E] = 0xA1;
    t[0x9F] = 0xFB;
    for (unsigned i = 0; i < SEC_SZ; i++) t[0xA0 + i] = 0xE5;
    uint8_t dbuf[3 + 1 + SEC_SZ];
    dbuf[0] = dbuf[1] = dbuf[2] = 0xA1; dbuf[3] = 0xFB;
    memset(dbuf + 4, 0xE5, SEC_SZ);
    uint16_t d_crc = crc16(dbuf, sizeof(dbuf));
    if (corrupt_data) d_crc ^= 0x0001;
    t[0xA0 + SEC_SZ] = d_crc >> 8; t[0xA0 + SEC_SZ + 1] = d_crc & 0xFF;

    FILE *f = fopen(path, "wb");
    if (!f) { free(file); return 0; }
    int ok = fwrite(file, 1, 16 + TRACK_LEN, f) == 16 + TRACK_LEN;
    fclose(f); free(file);
    return ok;
}

TEST(crc_algorithm_pinned) {
    /* universal CRC-CCITT-FALSE check + the spec ID reference. */
    const uint8_t chk[] = "123456789";
    ASSERT(crc16(chk, 9) == 0x29B1);
    const uint8_t id[] = { 0xA1, 0xA1, 0xA1, 0xFE, 0, 0, 1, 1 };
    ASSERT(crc16(id, 8) == 0xFA0C);
}

static void open_read(const char *path, uft_track_t *t) {
    uft_disk_t disk; memset(&disk, 0, sizeof(disk)); disk.read_only = true;
    if (uft_format_plugin_dmk.open(&disk, path, true) != UFT_OK) return;
    memset(t, 0, sizeof(*t));
    uft_format_plugin_dmk.read_track(&disk, 0, 0, t);
    if (uft_format_plugin_dmk.close) uft_format_plugin_dmk.close(&disk);
}

TEST(good_crc_reads_clean) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_dmk(path, 0, 0));
    uft_track_t t; open_read(path, &t);
    ASSERT(t.sector_count == 1);
    ASSERT(t.sectors[0].crc_ok == true);
    ASSERT(t.sectors[0].id_crc_ok == true);
    ASSERT(t.sectors[0].deleted == false);
    free_ts(&t); remove(path);
}

TEST(bad_data_crc_flagged) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_dmk(path, 0, 1));
    uft_track_t t; open_read(path, &t);
    ASSERT(t.sector_count == 1);
    ASSERT(t.sectors[0].crc_ok == false);      /* data CRC bad */
    ASSERT(t.sectors[0].id_crc_ok == true);    /* header intact */
    free_ts(&t); remove(path);
}

TEST(bad_id_crc_flagged) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_dmk(path, 1, 0));
    uft_track_t t; open_read(path, &t);
    ASSERT(t.sector_count == 1);
    ASSERT(t.sectors[0].id_crc_ok == false);   /* header CRC bad */
    ASSERT(t.sectors[0].crc_ok == true);       /* data intact */
    free_ts(&t); remove(path);
}

int main(void) {
    printf("=== DMK ID/data CRC checking (MF-353) ===\n");
    RUN(crc_algorithm_pinned);
    RUN(good_crc_reads_clean);
    RUN(bad_data_crc_flagged);
    RUN(bad_id_crc_flagged);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
