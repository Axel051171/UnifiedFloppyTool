/**
 * @file uft_dsk_flex_parser_v3.c
 * @brief GOD MODE DSK_FLEX Parser v3 - FLEX OS Disk Format
 * 
 * FLEX Filesystem:
 * - Technical Systems Consultants
 * - 6800/6809 Systeme
 * - SIR (System Information Record)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FLEX_SIR_TRACK          0
#define FLEX_SIR_SECTOR         3
#define FLEX_SECTOR_SIZE        256

typedef struct {
    char volume_name[12];
    uint16_t volume_number;
    uint8_t first_user_track;
    uint8_t first_user_sector;
    uint8_t last_user_track;
    uint8_t last_user_sector;
    uint16_t total_sectors;
    uint8_t month, day, year;
    uint8_t max_track;
    uint8_t max_sector;
    size_t source_size;
    bool valid;
} flex_disk_t;

static bool flex_parse(const uint8_t* data, size_t size, flex_disk_t* disk) {
    if (!data || !disk || size < 4 * 256) return false;
    memset(disk, 0, sizeof(flex_disk_t));
    disk->source_size = size;
    
    /* SIR at track 0, sector 3 (offset 2*256 = 512 for sectors 1,2) */
    const uint8_t* sir = data + (FLEX_SIR_SECTOR - 1) * FLEX_SECTOR_SIZE;
    
    memcpy(disk->volume_name, sir + 0x10, 11);
    disk->volume_name[11] = '\0';
    
    disk->volume_number = (sir[0x12] << 8) | sir[0x13];
    disk->first_user_track = sir[0x14];
    disk->first_user_sector = sir[0x15];
    disk->last_user_track = sir[0x16];
    disk->last_user_sector = sir[0x17];
    disk->total_sectors = (sir[0x18] << 8) | sir[0x19];
    disk->month = sir[0x1A];
    disk->day = sir[0x1B];
    disk->year = sir[0x1C];
    disk->max_track = sir[0x1D];
    disk->max_sector = sir[0x1E];
    
    disk->valid = (disk->max_track > 0 && disk->max_sector > 0);
    return true;
}

#ifdef DSK_FLEX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FLEX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* flex = calloc(1, 40 * 10 * 256);
    uint8_t* sir = flex + 2 * 256;
    memcpy(sir + 0x10, "TESTFLEX   ", 11);
    sir[0x1D] = 39;  /* max track */
    sir[0x1E] = 10;  /* max sector */
    
    flex_disk_t disk;
    assert(flex_parse(flex, 40 * 10 * 256, &disk) && disk.valid);
    free(flex);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
