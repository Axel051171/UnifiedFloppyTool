/**
 * @file uft_dsk_mtx_parser_v3.c
 * @brief GOD MODE DSK_MTX Parser v3 - Memotech MTX Disk Format
 * 
 * Memotech MTX500/512 Format:
 * - 40/80 Tracks
 * - CP/M kompatibel
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MTX_SIZE_SS             (40 * 16 * 256)
#define MTX_SIZE_DS             (40 * 2 * 16 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} mtx_disk_t;

static bool mtx_parse(const uint8_t* data, size_t size, mtx_disk_t* disk) {
    if (!data || !disk || size < MTX_SIZE_SS) return false;
    memset(disk, 0, sizeof(mtx_disk_t));
    disk->source_size = size;
    disk->tracks = 40;
    disk->sides = (size >= MTX_SIZE_DS) ? 2 : 1;
    disk->sectors = 16;
    disk->sector_size = 256;
    disk->valid = true;
    return true;
}

#ifdef DSK_MTX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Memotech MTX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* mtx = calloc(1, MTX_SIZE_DS);
    mtx_disk_t disk;
    assert(mtx_parse(mtx, MTX_SIZE_DS, &disk) && disk.valid);
    free(mtx);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
