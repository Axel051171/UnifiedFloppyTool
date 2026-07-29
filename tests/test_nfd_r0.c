/**
 * @file test_nfd_r0.c
 * @brief NFD r0 (T98-Next PC-98) reader/writer rewrite (MF-358).
 *
 * The prior NFD reader was fabricated (164 per-TRACK entries + an invented
 * per-entry data offset at +12). The real r0 format is a fixed 163×26 per-SECTOR
 * table of 16-byte entries at 0x120, with sector data stored sequentially from
 * dwHeadSize (@0x110) in table order — no per-sector offset. Layout verified
 * against pc98.org/project/doc/nfdr0.html + tomari/d88split nfd2mhlt.pl.
 *
 * This builds a synthetic r0 image (full 163×26 table, four valid sectors on two
 * cylinders) and checks: geometry, per-(C,H) sector recovery with the real
 * record number as the sector id, DDAM->deleted, ST1-bit5->CRC-bad, in-place
 * write persistence, and that an r1 image is refused (not mis-read as r0).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_nfd;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-30s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define HDR       0x120u
#define TABLE     (163u * 26u * 16u)         /* 67808 */
#define DATA_OFF  (HDR + TABLE)              /* 68096 */
#define NCODE     1u                          /* 128<<1 = 256 */
#define SEC       256u
#define NSEC      4u
#define TOTAL     (DATA_OFF + NSEC * SEC)     /* 69120 */

static void wle32(uint8_t *p, uint32_t v) {
    p[0]=v&0xFF; p[1]=(v>>8)&0xFF; p[2]=(v>>16)&0xFF; p[3]=(v>>24)&0xFF;
}
static void wle16(uint8_t *p, uint16_t v) { p[0]=v&0xFF; p[1]=(v>>8)&0xFF; }
static void get_temp_path(char *path, size_t n) {
    const char *dir = getenv("TMPDIR");
    if (!dir||!dir[0]) dir = getenv("TMP");
    if (!dir||!dir[0]) dir = getenv("TEMP");
    if (!dir||!dir[0]) dir = ".";
    snprintf(path, n, "%s/uft_nfd_%d.nfd", dir, rand() % 100000);
}

/* Write one 16-byte r0 sector-ID entry at table slot `slot`. */
static void put_entry(uint8_t *f, int slot, uint8_t c, uint8_t h, uint8_t r,
                      uint8_t ddam, uint8_t st1) {
    uint8_t *e = f + HDR + (size_t)slot * 16;
    e[0]=c; e[1]=h; e[2]=r; e[3]=(uint8_t)NCODE; e[4]=1 /*MFM*/;
    e[5]=ddam; e[6]=0 /*status*/; e[7]=0; e[8]=st1; e[9]=0; e[10]=0x90 /*PDA*/;
}

/* Build a synthetic r0 image. sig_digit '0' => valid r0, '1' => r1. */
static size_t build_nfd(const char *path, char sig_digit) {
    uint8_t *f = calloc(1, TOTAL);
    if (!f) return 0;
    memcpy(f, "T98FDDIMAGE.R", 13);
    f[13] = (uint8_t)sig_digit;
    wle32(f + 0x110, DATA_OFF);   /* dwHeadSize = data-section start */
    f[0x115] = 1;                 /* number of heads */

    memset(f + HDR, 0xFF, TABLE); /* all slots ignored by default (C=0xFF) */
    /* (C=0,H=0): R=1 normal, R=2 deleted; (C=1,H=0): R=1 CRC-bad, R=2 normal */
    put_entry(f, 0, 0, 0, 1, 0, 0);
    put_entry(f, 1, 0, 0, 2, 1 /*DDAM*/, 0);
    put_entry(f, 2, 1, 0, 1, 0, 0x20 /*ST1 CRC*/);
    put_entry(f, 3, 1, 0, 2, 0, 0);
    /* Data in table order, each sector filled with a distinct marker. */
    for (unsigned s = 0; s < NSEC; s++)
        memset(f + DATA_OFF + s * SEC, (int)(0x11 + s), SEC);

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(f); return 0; }
    size_t w = fwrite(f, 1, TOTAL, fp);
    fclose(fp); free(f);
    return w == TOTAL ? TOTAL : 0;
}

static int open_disk(const char *path, uft_disk_t *disk) {
    memset(disk, 0, sizeof(*disk));
    disk->read_only = false;
    return uft_format_plugin_nfd.open(disk, path, false) == UFT_OK;
}
static void free_ts(uft_track_t *t) {
    for (size_t i=0;i<t->sector_count;i++) free(t->sectors[i].data);
    free(t->sectors); t->sectors=NULL; t->sector_count=0;
}

TEST(geometry_and_recovery) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_nfd(path, '0'));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));
    ASSERT(disk.geometry.cylinders == 2);
    ASSERT(disk.geometry.heads == 1);
    ASSERT(disk.geometry.sectors == 2);
    ASSERT(disk.geometry.sector_size == SEC);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_nfd.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 2);
    /* sector id is the real record number R (1,2), not a 0-based index */
    ASSERT(t.sectors[0].id.sector == 1);
    ASSERT(t.sectors[1].id.sector == 2);
    ASSERT(t.sectors[0].data[0] == 0x11);          /* first data block */
    ASSERT(t.sectors[1].data[0] == 0x12);
    ASSERT(t.sectors[1].deleted == true);          /* DDAM */
    free_ts(&t);

    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_nfd.read_track(&disk, 1, 0, &t2) == UFT_OK);
    ASSERT(t2.sector_count == 2);
    ASSERT(t2.sectors[0].data[0] == 0x13);
    ASSERT(t2.sectors[0].crc_ok == false);         /* ST1 bit5 */
    ASSERT(t2.sectors[1].crc_ok == true);
    free_ts(&t2);

    uft_format_plugin_nfd.close(&disk);
    remove(path);
}

TEST(write_persists) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_nfd(path, '0'));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_nfd.read_track(&disk, 0, 0, &t) == UFT_OK);
    memset(t.sectors[0].data, 0xC7, SEC);          /* modify R=1 */
    ASSERT(uft_format_plugin_nfd.write_track(&disk, 0, 0, &t) == UFT_OK);
    free_ts(&t);
    uft_format_plugin_nfd.close(&disk);

    /* reopen from disk -> change must be persisted */
    uft_disk_t d2; ASSERT(open_disk(path, &d2));
    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_nfd.read_track(&d2, 0, 0, &t2) == UFT_OK);
    ASSERT(t2.sectors[0].data[0] == 0xC7 && t2.sectors[0].data[SEC-1] == 0xC7);
    free_ts(&t2);
    uft_format_plugin_nfd.close(&d2);
    remove(path);
}

TEST(probe_valid) {
    uint8_t hdr[16]; memcpy(hdr, "T98FDDIMAGE.R0", 14);
    int conf = 0;
    ASSERT(uft_format_plugin_nfd.probe(hdr, sizeof(hdr), sizeof(hdr), &conf) == true);
    ASSERT(conf > 0);
}

/* ── r1 (MF-360): track-pointer table + retry/diagnostic skip accounting ── */

/* 16-byte NFD_SECT_ID1 entry. */
static void put_r1_sec(uint8_t *f, size_t off, uint8_t c, uint8_t h, uint8_t r,
                       uint8_t n, uint8_t ddam, uint8_t retry, uint8_t pda) {
    f[off]=c; f[off+1]=h; f[off+2]=r; f[off+3]=n; f[off+4]=1 /*MFM*/; f[off+5]=ddam;
    f[off+6]=0; f[off+7]=0; f[off+8]=0; f[off+9]=0; f[off+10]=retry; f[off+11]=pda;
}

/* Build a synthetic r1 image whose data section interleaves a retry copy and a
 * diagnostic block, so a correct reader must skip both to locate later sectors.
 * Returns 0 on failure. */
static size_t build_nfd_r1(const char *path) {
    const size_t TP   = 0x120;                 /* track-pointer table */
    const size_t TH   = 0x120 + 164u * 4u;     /* track headers (944) */
    const size_t T0   = TH;                     /* track0 header */
    const size_t T1   = TH + 64u;              /* track1 header (16+2*16+16) */
    const size_t DATA = T1 + 48u;              /* dwHeadSize (16+2*16) -> 1056 */
    const size_t d_s0   = DATA;                 /* sec0 primary */
    const size_t d_s0r  = d_s0 + SEC;          /* sec0 retry copy (skipped) */
    const size_t d_s1   = d_s0r + SEC;         /* sec1 primary */
    const size_t d_diag = d_s1 + SEC;          /* diagnostic data (skipped) */
    const size_t d_t1s0 = d_diag + 128u;       /* track1 sec0 */
    const size_t d_t1s1 = d_t1s0 + SEC;        /* track1 sec1 */
    const size_t total  = d_t1s1 + SEC;

    uint8_t *f = calloc(1, total);
    if (!f) return 0;
    memcpy(f, "T98FDDIMAGE.R1", 14);
    wle32(f + 0x110, (uint32_t)DATA);          /* dwHeadSize */
    f[0x115] = 1;                              /* heads */

    wle32(f + TP + 0u * 4u, (uint32_t)T0);
    wle32(f + TP + 1u * 4u, (uint32_t)T1);

    /* track0: 2 sectors, 1 diagnostic */
    wle16(f + T0, 2); wle16(f + T0 + 2, 1);
    put_r1_sec(f, T0 + 16,      0,0,1, 1, 0, 1 /*retry copy*/, 0x90);
    put_r1_sec(f, T0 + 16 + 16, 0,0,2, 1, 1 /*DDAM*/, 0, 0x90);
    { size_t dg = T0 + 16 + 32; f[dg+9] = 0 /*retry*/; wle32(f + dg + 10, 128); }

    /* track1: 2 sectors, 0 diagnostics */
    wle16(f + T1, 2); wle16(f + T1 + 2, 0);
    put_r1_sec(f, T1 + 16,      1,0,1, 1, 0, 0, 0x90);
    put_r1_sec(f, T1 + 16 + 16, 1,0,2, 1, 0, 0, 0x90);

    memset(f + d_s0,   0x11, SEC);
    memset(f + d_s0r,  0x99, SEC);             /* retry copy — must be skipped */
    memset(f + d_s1,   0x12, SEC);
    memset(f + d_diag, 0x88, 128);             /* diagnostic — must be skipped */
    memset(f + d_t1s0, 0x13, SEC);
    memset(f + d_t1s1, 0x14, SEC);

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(f); return 0; }
    size_t w = fwrite(f, 1, total, fp);
    fclose(fp); free(f);
    return w == total ? total : 0;
}

TEST(r1_recovery_with_retry_and_diag) {
    char path[300]; get_temp_path(path, sizeof(path));
    ASSERT(build_nfd_r1(path));
    uft_disk_t disk; ASSERT(open_disk(path, &disk));
    ASSERT(disk.geometry.cylinders == 2);
    ASSERT(disk.geometry.heads == 1);
    ASSERT(disk.geometry.sectors == 2);

    uft_track_t t; memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_nfd.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 2);
    ASSERT(t.sectors[0].id.sector == 1 && t.sectors[1].id.sector == 2);
    ASSERT(t.sectors[0].data[0] == 0x11);
    /* sector1 == 0x12 only if sector0's retry copy (0x99) was skipped correctly */
    ASSERT(t.sectors[1].data[0] == 0x12);
    ASSERT(t.sectors[1].deleted == true);
    free_ts(&t);

    uft_track_t t2; memset(&t2, 0, sizeof(t2));
    ASSERT(uft_format_plugin_nfd.read_track(&disk, 1, 0, &t2) == UFT_OK);
    ASSERT(t2.sector_count == 2);
    /* track1 == 0x13/0x14 only if track0's diagnostic block (0x88) was skipped */
    ASSERT(t2.sectors[0].data[0] == 0x13);
    ASSERT(t2.sectors[1].data[0] == 0x14);
    free_ts(&t2);

    uft_format_plugin_nfd.close(&disk);
    remove(path);
}

int main(void) {
    printf("=== NFD r0/r1 reader/writer (MF-358/360) ===\n");
    RUN(geometry_and_recovery);
    RUN(write_persists);
    RUN(probe_valid);
    RUN(r1_recovery_with_retry_and_diag);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
