/**
 * @file uft_st_parser_v3.c
 * @brief GOD MODE ST Parser v3 - Atari ST Disk Image
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    bool is_bootable;
    size_t source_size;
    bool valid;
} st_disk_t;

static bool st_parse(const uint8_t* data, size_t size, st_disk_t* st) {
    if (!data || !st || size < 512) return false;
    memset(st, 0, sizeof(st_disk_t));
    st->source_size = size;
    
    /* Check for 68000 BRA instruction or boot signature */
    if (data[0] == 0x60 || (data[0] == 0xEB && data[2] == 0x90)) {
        st->bytes_per_sector = data[11] | (data[12] << 8);
        st->sectors_per_cluster = data[13];
        st->reserved_sectors = data[14] | (data[15] << 8);
        st->fat_count = data[16];
        st->root_entries = data[17] | (data[18] << 8);
        st->total_sectors = data[19] | (data[20] << 8);
        st->media_descriptor = data[21];
        
        /* Validate */
        if (st->bytes_per_sector == 512 && st->fat_count >= 1) {
            st->valid = true;
        }
    }
    
    /* Accept standard ST sizes */
    if (size == 368640 || size == 737280 || size == 819200) {
        st->valid = true;
    }
    return true;
}

#ifdef ST_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari ST Disk Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t st[512] = {0x60, 0x00};  /* BRA */
    st[11] = 0x00; st[12] = 0x02;  /* 512 bytes/sector */
    st[16] = 2;  /* 2 FATs */
    st_disk_t disk;
    assert(st_parse(st, sizeof(st), &disk) && disk.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
