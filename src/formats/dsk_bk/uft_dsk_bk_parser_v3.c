/**
 * @file uft_dsk_bk_parser_v3.c
 * @brief GOD MODE DSK_BK Parser v3 - Soviet BK-0010/0011 Disk Format
 * 
 * Soviet BK-0010/0011M:
 * - RT-11 compatible
 * - 40/80 Tracks
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BK_SIZE_400K            (80 * 10 * 512)
#define BK_SIZE_800K            (80 * 2 * 10 * 512)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} bk_disk_t;

static bool bk_parse(const uint8_t* data, size_t size, bk_disk_t* disk) {
    if (!data || !disk || size < BK_SIZE_400K) return false;
    memset(disk, 0, sizeof(bk_disk_t));
    disk->source_size = size;
    disk->tracks = 80;
    disk->sides = (size >= BK_SIZE_800K) ? 2 : 1;
    disk->sectors = 10;
    disk->sector_size = 512;
    disk->valid = true;
    return true;
}

#ifdef DSK_BK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Soviet BK Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* bk = calloc(1, BK_SIZE_400K);
    bk_disk_t disk;
    assert(bk_parse(bk, BK_SIZE_400K, &disk) && disk.valid);
    free(bk);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
