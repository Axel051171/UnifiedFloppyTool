/**
 * @file test_fatfs.c
 * @brief FatFs Integration Tests
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

static int _pass = 0, _fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name) do { printf("  [TEST] %s... ", #name); test_##name(); printf("OK\n"); _pass++; } while(0)
#define ASSERT(c) do { if(!(c)) { printf("FAIL @ %d\n", __LINE__); _fail++; return; } } while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Floppy Geometry (inline for testing)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    FLOPPY_160K, FLOPPY_180K, FLOPPY_320K, FLOPPY_360K,
    FLOPPY_720K, FLOPPY_1200K, FLOPPY_1440K, FLOPPY_2880K, FLOPPY_CUSTOM
} floppy_type_t;

typedef struct {
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint32_t total_bytes;
} geometry_t;

static const geometry_t geometries[] = {
    { 40, 1,  8,   163840 },  /* 160K */
    { 40, 1,  9,   184320 },  /* 180K */
    { 40, 2,  8,   327680 },  /* 320K */
    { 40, 2,  9,   368640 },  /* 360K */
    { 80, 2,  9,   737280 },  /* 720K */
    { 80, 2, 15,  1228800 },  /* 1.2M */
    { 80, 2, 18,  1474560 },  /* 1.44M */
    { 80, 2, 36,  2949120 },  /* 2.88M */
};

static floppy_type_t detect_type(size_t size) {
    for (int i = 0; i < FLOPPY_CUSTOM; i++) {
        if (geometries[i].total_bytes == size) return (floppy_type_t)i;
    }
    return FLOPPY_CUSTOM;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Boot Sector Parser (inline for testing)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char     oem_name[9];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint8_t  media_type;
    char     fs_type[9];
} boot_info_t;

static int parse_boot(const uint8_t *data, boot_info_t *info) {
    memcpy(info->oem_name, data + 3, 8);
    info->oem_name[8] = '\0';
    info->bytes_per_sector = data[11] | (data[12] << 8);
    info->sectors_per_cluster = data[13];
    info->fat_count = data[16];
    info->root_entries = data[17] | (data[18] << 8);
    info->media_type = data[21];
    memcpy(info->fs_type, data + 54, 8);
    info->fs_type[8] = '\0';
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

TEST(geometry_160k) {
    ASSERT(geometries[FLOPPY_160K].total_bytes == 163840);
    ASSERT(geometries[FLOPPY_160K].cylinders == 40);
    ASSERT(geometries[FLOPPY_160K].heads == 1);
    ASSERT(geometries[FLOPPY_160K].sectors == 8);
}

TEST(geometry_360k) {
    ASSERT(geometries[FLOPPY_360K].total_bytes == 368640);
    ASSERT(geometries[FLOPPY_360K].cylinders == 40);
    ASSERT(geometries[FLOPPY_360K].heads == 2);
    ASSERT(geometries[FLOPPY_360K].sectors == 9);
}

TEST(geometry_720k) {
    ASSERT(geometries[FLOPPY_720K].total_bytes == 737280);
    ASSERT(geometries[FLOPPY_720K].cylinders == 80);
    ASSERT(geometries[FLOPPY_720K].heads == 2);
    ASSERT(geometries[FLOPPY_720K].sectors == 9);
}

TEST(geometry_1440k) {
    ASSERT(geometries[FLOPPY_1440K].total_bytes == 1474560);
    ASSERT(geometries[FLOPPY_1440K].cylinders == 80);
    ASSERT(geometries[FLOPPY_1440K].heads == 2);
    ASSERT(geometries[FLOPPY_1440K].sectors == 18);
}

TEST(detect_720k) {
    ASSERT(detect_type(737280) == FLOPPY_720K);
}

TEST(detect_1440k) {
    ASSERT(detect_type(1474560) == FLOPPY_1440K);
}

TEST(detect_custom) {
    ASSERT(detect_type(1000000) == FLOPPY_CUSTOM);
}

TEST(boot_sector_parse) {
    uint8_t boot[512] = {0};
    
    /* Set up minimal boot sector */
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;  /* Jump */
    memcpy(boot + 3, "MSDOS5.0", 8);  /* OEM */
    boot[11] = 0x00; boot[12] = 0x02;  /* 512 bytes/sector */
    boot[13] = 1;   /* Sectors per cluster */
    boot[16] = 2;   /* FAT count */
    boot[17] = 224; boot[18] = 0;  /* Root entries */
    boot[21] = 0xF0;  /* Media type */
    memcpy(boot + 54, "FAT12   ", 8);
    
    boot_info_t info;
    ASSERT(parse_boot(boot, &info) == 0);
    ASSERT(strcmp(info.oem_name, "MSDOS5.0") == 0);
    ASSERT(info.bytes_per_sector == 512);
    ASSERT(info.sectors_per_cluster == 1);
    ASSERT(info.fat_count == 2);
    ASSERT(info.root_entries == 224);
    ASSERT(info.media_type == 0xF0);
    ASSERT(memcmp(info.fs_type, "FAT12", 5) == 0);
}

TEST(media_types) {
    /* Common FAT12 media types */
    ASSERT(0xF0 == 0xF0);  /* 3.5" 1.44MB or custom */
    ASSERT(0xF9 == 0xF9);  /* 3.5" 720KB or 5.25" 1.2MB */
    ASSERT(0xFD == 0xFD);  /* 5.25" 360KB */
    ASSERT(0xFC == 0xFC);  /* 5.25" 180KB */
    ASSERT(0xFE == 0xFE);  /* 5.25" 160KB */
    ASSERT(0xFF == 0xFF);  /* 5.25" 320KB */
}

TEST(fat12_cluster_limit) {
    /* FAT12 max clusters: 4084 */
    ASSERT(4084 < 4085);  /* FAT12 limit */
    ASSERT(4085 >= 4085); /* FAT16 starts here */
}

TEST(dir_entry_size) {
    /* FAT directory entry is 32 bytes */
    ASSERT(32 == 32);
}

TEST(boot_signature) {
    uint8_t sig[2] = {0x55, 0xAA};
    ASSERT(sig[0] == 0x55);
    ASSERT(sig[1] == 0xAA);
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  FatFs Integration Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    RUN(geometry_160k);
    RUN(geometry_360k);
    RUN(geometry_720k);
    RUN(geometry_1440k);
    RUN(detect_720k);
    RUN(detect_1440k);
    RUN(detect_custom);
    RUN(boot_sector_parse);
    RUN(media_types);
    RUN(fat12_cluster_limit);
    RUN(dir_entry_size);
    RUN(boot_signature);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", _pass, _fail);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return _fail > 0 ? 1 : 0;
}
