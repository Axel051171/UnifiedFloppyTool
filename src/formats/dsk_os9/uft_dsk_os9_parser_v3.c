/**
 * @file uft_dsk_os9_parser_v3.c
 * @brief GOD MODE DSK_OS9 Parser v3 - OS-9 Disk Format
 * 
 * OS-9 Filesystem:
 * - Microware OS-9
 * - CoCo, 6809, 68K Systeme
 * - RBF (Random Block File) Manager
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define OS9_LSN0_OFFSET         0
#define OS9_DD_TOT              0x00    /* Total sectors (3 bytes) */
#define OS9_DD_TKS              0x03    /* Track size in sectors */
#define OS9_DD_MAP              0x04    /* Allocation map length (2 bytes) */
#define OS9_DD_BIT              0x06    /* Sectors per cluster */
#define OS9_DD_DIR              0x08    /* Root directory LSN (3 bytes) */
#define OS9_DD_OWN              0x0B    /* Owner ID (2 bytes) */
#define OS9_DD_ATT              0x0D    /* Disk attributes */
#define OS9_DD_DSK              0x0E    /* Disk ID (2 bytes) */
#define OS9_DD_FMT              0x10    /* Format byte */
#define OS9_DD_SPT              0x11    /* Sectors per track (2 bytes) */
#define OS9_DD_RES              0x13    /* Reserved (2 bytes) */
#define OS9_DD_BT               0x15    /* Bootstrap LSN (3 bytes) */
#define OS9_DD_BSZ              0x18    /* Bootstrap size (2 bytes) */
#define OS9_DD_DAT              0x1A    /* Creation date (5 bytes) */
#define OS9_DD_NAM              0x1F    /* Volume name (32 bytes) */

typedef struct {
    char volume_name[33];
    uint32_t total_sectors;
    uint16_t sectors_per_track;
    uint8_t sectors_per_cluster;
    uint32_t root_dir_lsn;
    uint8_t format_byte;
    uint8_t tracks;
    uint8_t sides;
    bool is_os9;
    size_t source_size;
    bool valid;
} os9_disk_t;

static uint32_t os9_read_be24(const uint8_t* p) {
    return ((uint32_t)p[0] << 16) | (p[1] << 8) | p[2];
}
static uint16_t os9_read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static bool os9_parse(const uint8_t* data, size_t size, os9_disk_t* disk) {
    if (!data || !disk || size < 256) return false;
    memset(disk, 0, sizeof(os9_disk_t));
    disk->source_size = size;
    
    /* Parse LSN 0 */
    disk->total_sectors = os9_read_be24(data + OS9_DD_TOT);
    disk->sectors_per_track = os9_read_be16(data + OS9_DD_SPT);
    disk->sectors_per_cluster = data[OS9_DD_BIT];
    disk->root_dir_lsn = os9_read_be24(data + OS9_DD_DIR);
    disk->format_byte = data[OS9_DD_FMT];
    
    /* Volume name */
    memcpy(disk->volume_name, data + OS9_DD_NAM, 32);
    disk->volume_name[32] = '\0';
    /* Strip high bit from OS-9 name encoding */
    for (int i = 0; i < 32; i++) {
        disk->volume_name[i] &= 0x7F;
        if (disk->volume_name[i] == 0) break;
    }
    
    /* Derive geometry */
    disk->sides = (disk->format_byte & 0x01) ? 2 : 1;
    if (disk->total_sectors > 0 && disk->sectors_per_track > 0) {
        disk->tracks = disk->total_sectors / (disk->sectors_per_track * disk->sides);
    }
    
    disk->is_os9 = (disk->total_sectors > 0 && disk->sectors_per_track > 0);
    disk->valid = disk->is_os9;
    return true;
}

#ifdef DSK_OS9_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== OS-9 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t os9[512] = {0};
    os9[0] = 0x00; os9[1] = 0x02; os9[2] = 0xD0;  /* 720 sectors */
    os9[0x11] = 0x00; os9[0x12] = 0x12;  /* 18 sectors/track */
    os9[0x10] = 0x01;  /* DS */
    memcpy(os9 + OS9_DD_NAM, "TESTDISK", 8);
    
    os9_disk_t disk;
    assert(os9_parse(os9, sizeof(os9), &disk) && disk.is_os9);
    free(NULL);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
