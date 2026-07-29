/**
 * @file test_fdi_spectrum.c
 * @brief ZX Spectrum FDI (Full Disk Image) reader/writer rewrite (MF-359).
 *
 * The prior FDI reader had the correct 14-byte header but a fabricated
 * track/sector model (a flat 4-byte-offset table + inline 5-byte sector
 * descriptors carrying the data). The real Spectrum FDI stores variable-length
 * track headers (7-byte FDI_TRACK + N×7-byte FDI_SECTOR) beginning at
 * 14+extra-length, with sector DATA held separately at
 * data_off + track_off + sector_off. Layout verified against SAMdisk's ReadFDI
 * (src/samdisk/fdi.cpp) + the WoS format reference.
 *
 * This builds a synthetic 2-cylinder image and checks geometry, per-sector
 * recovery via the sector-data offsets, R preserved as the id, deleted (flag
 * bit7) and CRC (flag bit N) representation, and in-place write persistence.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_fdi;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define FDI_HDR_SZ 14u
#define SEC      256u
#define DATA_OFF 56u                 /* header(14) + 2 track headers(21 each) */
#define TOTAL    (DATA_OFF + 4u * SEC)

static void wle16(uint8_t *p, uint16_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; }
static void wle32(uint8_t *p, uint32_t v) {
    p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF;
}
static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir||!dir[0]) dir = getenv("TMP");
    if (!dir||!dir[0]) dir = getenv("TEMP");
    if (!dir||!dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_fdi_%d.fdi", dir, rand() % 100000);
}

/* FDI_SECTOR (7 bytes) at f+off. */
static void put_sec(uint8_t *f, size_t off, uint8_t c, uint8_t h, uint8_t r,
                    uint8_t n, uint8_t flags, uint16_t soff) {
    f[off]=c; f[off+1]=h; f[off+2]=r; f[off+3]=n; f[off+4]=flags;
    wle16(f+off+5, soff);
}

static size_t build_fdi(const char *path) {
    uint8_t *f = calloc(1, TOTAL);
    if (!f) return 0;
    memcpy(f, "FDI", 3);
    f[3] = 0;                       /* write-protect off */
    wle16(f + 4, 2);                /* cylinders */
    wle16(f + 6, 1);                /* heads */
    wle16(f + 8, 0);                /* desc offset */
    wle16(f + 0x0A, DATA_OFF);      /* data offset */
    wle16(f + 0x0C, 0);             /* extra length -> track headers at 14 */

    /* Track 0 (cyl0,head0): FDI_TRACK @14 */
    wle32(f + 14, 0);               /* track data offset */
    f[14+6] = 2;                    /* sector count */
    put_sec(f, 21, 0,0,1, 1, 0x02, 0);        /* R1, CRC-ok (bit N=1) */
    put_sec(f, 28, 0,0,2, 1, 0x82, SEC);      /* R2, deleted + CRC-ok */
    /* Track 1 (cyl1,head0): FDI_TRACK @35, data offset 512 */
    wle32(f + 35, 512);
    f[35+6] = 2;
    put_sec(f, 42, 1,0,1, 1, 0x00, 0);        /* R1, no CRC-ok bit -> CRC bad */
    put_sec(f, 49, 1,0,2, 1, 0x02, SEC);      /* R2, CRC-ok */

    /* Data block: 4 sectors, each a distinct marker. */
    for (unsigned s = 0; s < 4; s++)
        memset(f + DATA_OFF + s * SEC, (int)(0x11 + s), SEC);

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(f); return 0; }
    size_t w = fwrite(f, 1, TOTAL, fp);
    fclose(fp); free(f);
    return w == TOTAL ? TOTAL : 0;
}

static int open_disk(const char *path, uft_disk_t *disk) {
    memset(disk, 0, sizeof(*disk));
    return uft_format_plugin_fdi.open(disk, path, false) == UFT_OK;
}
static void free_ts(uft_track_t *t) {
    for (size_t i=0;i<t->sector_count;i++) free(t->sectors[i].data);
    free(t->sectors); t->sectors=NULL; t->sector_count=0;
}

TEST(geometry_and_recovery) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_fdi(path));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));
    ASSERT(disk.geometry.cylinders == 2);
    ASSERT(disk.geometry.heads == 1);
    ASSERT(disk.geometry.sectors == 2);
    ASSERT(disk.geometry.sector_size == SEC);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_fdi.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 2);
    ASSERT(t.sectors[0].id.sector == 1 && t.sectors[1].id.sector == 2);
    ASSERT(t.sectors[0].data[0] == 0x11);        /* separate sector-data offset */
    ASSERT(t.sectors[1].data[0] == 0x12);
    ASSERT(t.sectors[1].deleted == true);        /* flag bit7 */
    ASSERT(t.sectors[0].crc_ok == true);
    free_ts(&t);

    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_fdi.read_track(&disk, 1, 0, &t2) == UFT_OK);
    ASSERT(t2.sector_count == 2);
    ASSERT(t2.sectors[0].data[0] == 0x13);       /* track_off 512 honoured */
    ASSERT(t2.sectors[0].crc_ok == false);       /* no CRC-ok flag bit */
    ASSERT(t2.sectors[1].crc_ok == true);
    free_ts(&t2);

    uft_format_plugin_fdi.close(&disk);
    remove(path);
}

TEST(write_persists) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_fdi(path));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_fdi.read_track(&disk, 1, 0, &t) == UFT_OK);
    memset(t.sectors[1].data, 0xC7, SEC);        /* modify cyl1 R2 */
    ASSERT(uft_format_plugin_fdi.write_track(&disk, 1, 0, &t) == UFT_OK);
    free_ts(&t);
    uft_format_plugin_fdi.close(&disk);

    uft_disk_t d2; ASSERT(open_disk(path, &d2));
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_fdi.read_track(&d2, 1, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[1].data[0] == 0xC7 && t2.sectors[1].data[SEC-1] == 0xC7);
    free_ts(&t2);
    uft_format_plugin_fdi.close(&d2);
    remove(path);
}

TEST(probe_valid) {
    uint8_t hdr[FDI_HDR_SZ]; memset(hdr, 0, sizeof(hdr));
    memcpy(hdr, "FDI", 3);
    wle16(hdr + 4, 40); wle16(hdr + 6, 2);
    int conf = 0;
    ASSERT(uft_format_plugin_fdi.probe(hdr, sizeof(hdr), sizeof(hdr), &conf) == true);
    ASSERT(conf > 0);
}

int main(void) {
    printf("=== ZX Spectrum FDI reader/writer rewrite (MF-359) ===\n");
    RUN(geometry_and_recovery);
    RUN(write_persists);
    RUN(probe_valid);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
