/**
 * @file test_fatfs.c
 * @brief FAT detection, tested against the SHIPPED implementation (MF-396).
 *
 * This file used to define detect_type() and parse_boot() locally and assert
 * against those copies; uft_fat12_detect() in src/fs/uft_fat12.c was never
 * called. A drift between the two would have been invisible.
 *
 * Every expectation is derived from the implementation (src/fs/uft_fat12.c:106)
 * and the FAT specification's cluster thresholds, not from re-running the code
 * and writing down whatever it happened to produce:
 *   < 4085 clusters   FAT12
 *   < 65525 clusters  FAT16
 *   otherwise         FAT32
 *   boot signature    0xAA55 at offset 0x1FE
 *   Atari variant     first byte 0x60 (branch instruction) instead of 0xEB/0xE9
 */

#include "uft/fs/uft_fat12.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8); }

/** Build a 1.44 MB FAT12 boot sector, the most common PC floppy shape. */
static void build_fat12_boot(uint8_t *s)
{
    memset(s, 0, 512);
    s[0] = 0xEB; s[1] = 0x3C; s[2] = 0x90;      /* JMP short + NOP */
    memcpy(s + 3, "MSDOS5.0", 8);
    put16(s + 0x0B, 512);                        /* bytes per sector */
    s[0x0D] = 1;                                 /* sectors per cluster */
    put16(s + 0x0E, 1);                          /* reserved sectors */
    s[0x10] = 2;                                 /* number of FATs */
    put16(s + 0x11, 224);                        /* root entries */
    put16(s + 0x13, 2880);                       /* total sectors (1.44 MB) */
    s[0x15] = 0xF0;                              /* media descriptor */
    put16(s + 0x16, 9);                          /* sectors per FAT */
    put16(s + 0x1FE, 0xAA55);                    /* boot signature */
}

TEST(recognises_a_standard_144mb_fat12_floppy) {
    uint8_t s[512];
    build_fat12_boot(s);

    uft_fat_detect_t r;
    ASSERT(uft_fat12_detect(s, sizeof(s), &r) == 0);
    ASSERT(r.valid);
    ASSERT(r.type == UFT_FAT_TYPE_FAT12);
    ASSERT(!r.boot_sig_missing);
    ASSERT(!r.bpb_inconsistent);
    ASSERT(r.confidence > 50);
    ASSERT(strstr(r.description, "FAT12") != NULL);
}

TEST(missing_boot_signature_is_reported_not_ignored) {
    uint8_t s[512];
    build_fat12_boot(s);
    put16(s + 0x1FE, 0x0000);                    /* signature removed */

    uft_fat_detect_t r;
    ASSERT(uft_fat12_detect(s, sizeof(s), &r) == 0);
    ASSERT(r.boot_sig_missing);
    ASSERT(!r.valid);                            /* must not claim validity */
    ASSERT(r.confidence < 50);                   /* and must say so quietly */
}

TEST(implausible_bpb_values_are_rejected) {
    uint8_t s[512];

    /* 1024 bytes per sector is not a FAT floppy this reader supports */
    build_fat12_boot(s);
    put16(s + 0x0B, 1024);
    uft_fat_detect_t r;
    ASSERT(uft_fat12_detect(s, sizeof(s), &r) == -1);
    ASSERT(r.bpb_inconsistent);

    /* zero sectors per cluster would divide by zero downstream */
    build_fat12_boot(s);
    s[0x0D] = 0;
    ASSERT(uft_fat12_detect(s, sizeof(s), &r) == -1);
    ASSERT(r.bpb_inconsistent);

    /* a filesystem without FATs is not a filesystem */
    build_fat12_boot(s);
    s[0x10] = 0;
    ASSERT(uft_fat12_detect(s, sizeof(s), &r) == -1);
    ASSERT(r.bpb_inconsistent);
}

TEST(cluster_count_decides_between_fat12_and_fat16) {
    /* The 4085-cluster threshold is the spec's, not this implementation's
     * invention. Push the volume past it and the type must change. */
    uint8_t s[512];
    build_fat12_boot(s);
    put16(s + 0x13, 0);                          /* clear 16-bit total */
    /* 32 MB volume, one sector per cluster -> far beyond 4085 clusters */
    s[0x20] = 0x00; s[0x21] = 0x00; s[0x22] = 0x01; s[0x23] = 0x00;

    uft_fat_detect_t r;
    ASSERT(uft_fat12_detect(s, sizeof(s), &r) == 0);
    ASSERT(r.type != UFT_FAT_TYPE_FAT12);
}

TEST(atari_boot_sector_is_distinguished_from_pc) {
    uint8_t s[512];

    build_fat12_boot(s);
    uft_fat_detect_t pc;
    ASSERT(uft_fat12_detect(s, sizeof(s), &pc) == 0);
    ASSERT(pc.platform == UFT_FAT_PLATFORM_PC);

    /* Atari ST boot sectors start with 0x60 (BRA.S) instead of 0xEB/0xE9 */
    s[0] = 0x60;
    uft_fat_detect_t atari;
    ASSERT(uft_fat12_detect(s, sizeof(s), &atari) == 0);
    ASSERT(atari.platform == UFT_FAT_PLATFORM_ATARI);
}

TEST(short_and_null_input_is_refused) {
    uint8_t s[512];
    build_fat12_boot(s);
    uft_fat_detect_t r;

    ASSERT(uft_fat12_detect(NULL, 512, &r) == -1);
    ASSERT(uft_fat12_detect(s, 511, &r) == -1);   /* below one sector */
    ASSERT(uft_fat12_detect(s, 0, &r) == -1);
}

int main(void) {
    printf("=== FAT detection, real implementation (MF-396) ===\n");
    RUN(recognises_a_standard_144mb_fat12_floppy);
    RUN(missing_boot_signature_is_reported_not_ignored);
    RUN(implausible_bpb_values_are_rejected);
    RUN(cluster_count_decides_between_fat12_and_fat16);
    RUN(atari_boot_sector_is_distinguished_from_pc);
    RUN(short_and_null_input_is_refused);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
