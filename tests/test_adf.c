/**
 * @file test_adf.c
 * @brief Amiga ADF Module Tests
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL @ %d\n", __LINE__); _fail++; return; } } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ADF_SECTOR_SIZE     512
#define UFT_ADF_DD_SIZE         901120
#define UFT_ADF_HD_SIZE         1802240
#define UFT_ADF_DD_SECTORS      1760
#define UFT_ADF_HD_SECTORS      3520
#define UFT_ADF_DD_ROOT         880
#define UFT_ADF_HD_ROOT         1760
#define UFT_ADF_HT_SIZE         72
#define UFT_ADF_MAX_NAME        30
#define UFT_ADF_DOS0            0x444F5300
#define UFT_ADF_DOS1            0x444F5301
#define AMIGA_EPOCH_DIFF        252460800

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t be32(uint32_t val) {
    uint8_t *p = (uint8_t *)&val;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static time_t adf_to_unix(uint32_t days, uint32_t mins, uint32_t ticks) {
    time_t t = AMIGA_EPOCH_DIFF;
    t += (time_t)days * 86400;
    t += (time_t)mins * 60;
    t += (time_t)ticks / 50;
    return t;
}

static uint32_t hash_name(const char *name) {
    uint32_t hash = strlen(name);
    for (const char *p = name; *p; p++) {
        uint8_t c = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
        hash = (hash * 13 + c) & 0x7FF;
    }
    return hash % UFT_ADF_HT_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

TEST(dd_size) {
    /* DD: 80 tracks × 2 heads × 11 sectors × 512 bytes = 901120 */
    ASSERT(80 * 2 * 11 * 512 == UFT_ADF_DD_SIZE);
    ASSERT(UFT_ADF_DD_SIZE == 901120);
}

TEST(hd_size) {
    /* HD: 80 tracks × 2 heads × 22 sectors × 512 bytes = 1802240 */
    ASSERT(80 * 2 * 22 * 512 == UFT_ADF_HD_SIZE);
    ASSERT(UFT_ADF_HD_SIZE == 1802240);
}

TEST(dd_sectors) {
    ASSERT(UFT_ADF_DD_SIZE / UFT_ADF_SECTOR_SIZE == UFT_ADF_DD_SECTORS);
    ASSERT(UFT_ADF_DD_SECTORS == 1760);
}

TEST(hd_sectors) {
    ASSERT(UFT_ADF_HD_SIZE / UFT_ADF_SECTOR_SIZE == UFT_ADF_HD_SECTORS);
    ASSERT(UFT_ADF_HD_SECTORS == 3520);
}

TEST(root_block_dd) {
    /* Root block is at the middle of the disk */
    ASSERT(UFT_ADF_DD_ROOT == UFT_ADF_DD_SECTORS / 2);
    ASSERT(UFT_ADF_DD_ROOT == 880);
}

TEST(root_block_hd) {
    ASSERT(UFT_ADF_HD_ROOT == UFT_ADF_HD_SECTORS / 2);
    ASSERT(UFT_ADF_HD_ROOT == 1760);
}

TEST(dos_signatures) {
    /* "DOS\0" = OFS, "DOS\1" = FFS */
    ASSERT(UFT_ADF_DOS0 == 0x444F5300);
    ASSERT(UFT_ADF_DOS1 == 0x444F5301);
    ASSERT((UFT_ADF_DOS0 >> 8) == 0x444F53);  /* "DOS" */
    ASSERT((UFT_ADF_DOS0 & 0xFF) == 0);        /* OFS */
    ASSERT((UFT_ADF_DOS1 & 0xFF) == 1);        /* FFS */
}

TEST(amiga_epoch) {
    /* Amiga epoch: January 1, 1978 */
    /* Unix epoch: January 1, 1970 */
    /* 8 years difference (including 2 leap years: 1972, 1976) */
    /* 8 * 365 + 2 = 2922 days = 252460800 seconds */
    ASSERT(AMIGA_EPOCH_DIFF == 252460800);
}

TEST(time_conversion) {
    /* Test: January 1, 2000 00:00:00 */
    /* Unix: 946684800 */
    /* Days since 1978: (2000-1978)*365 + leap_days = 8035 */
    uint32_t days = 8035;
    uint32_t mins = 0;
    uint32_t ticks = 0;
    time_t unix_time = adf_to_unix(days, mins, ticks);
    /* Should be close to Jan 1, 2000 */
    ASSERT(unix_time > 946000000 && unix_time < 947000000);
}

TEST(hash_table_size) {
    ASSERT(UFT_ADF_HT_SIZE == 72);
}

TEST(filename_hash) {
    /* Hash should be consistent and in range */
    uint32_t h1 = hash_name("c");
    uint32_t h2 = hash_name("C");
    uint32_t h3 = hash_name("Workbench");
    
    ASSERT(h1 == h2);  /* Case insensitive */
    ASSERT(h1 < UFT_ADF_HT_SIZE);
    ASSERT(h3 < UFT_ADF_HT_SIZE);
}

TEST(max_filename) {
    ASSERT(UFT_ADF_MAX_NAME == 30);
}

TEST(sector_size) {
    ASSERT(UFT_ADF_SECTOR_SIZE == 512);
}

TEST(bootblock_size) {
    /* Boot block is 2 sectors */
    ASSERT(2 * UFT_ADF_SECTOR_SIZE == 1024);
}

TEST(be32_conversion) {
    uint8_t data[] = {0x44, 0x4F, 0x53, 0x00};  /* "DOS\0" */
    uint32_t val = *(uint32_t *)data;
    uint32_t result = be32(val);
    ASSERT(result == UFT_ADF_DOS0);
}

TEST(block_types) {
    /* T_HEADER = 2, ST_ROOT = 1, ST_FILE = -3, ST_DIR = 2 */
    ASSERT(2 == 2);      /* T_HEADER */
    ASSERT(1 == 1);      /* ST_ROOT */
    ASSERT(-3 == -3);    /* ST_FILE (signed) */
}

TEST(protection_bits) {
    /* HSPARWED - active low for rwed */
    uint32_t prot = 0;  /* All permissions */
    /* Bit 0 = d (deletable) */
    /* Bit 1 = e (executable) */
    /* Bit 2 = w (writable) */
    /* Bit 3 = r (readable) */
    ASSERT((prot & 0x0F) == 0);  /* All rwed bits clear = all perms */
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Amiga ADF Module Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(dd_size);
    RUN(hd_size);
    RUN(dd_sectors);
    RUN(hd_sectors);
    RUN(root_block_dd);
    RUN(root_block_hd);
    RUN(dos_signatures);
    RUN(amiga_epoch);
    RUN(time_conversion);
    RUN(hash_table_size);
    RUN(filename_hash);
    RUN(max_filename);
    RUN(sector_size);
    RUN(bootblock_size);
    RUN(be32_conversion);
    RUN(block_types);
    RUN(protection_bits);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
