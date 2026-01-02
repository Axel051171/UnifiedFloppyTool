/**
 * @file uft_dsk_xm_parser_v3.c
 * @brief GOD MODE DSK_XM Parser v3 - Sharp X1/X68000 Disk Format
 * 
 * Sharp X1 / X68000:
 * - X1: 40/80 Tracks
 * - X68000: 80 Tracks 2HD
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XM_SIZE_320K            (40 * 2 * 16 * 256)
#define XM_SIZE_640K            (80 * 2 * 16 * 256)
#define XM_SIZE_1232K           (77 * 2 * 8 * 1024)  /* X68000 2HD */

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    bool is_x68k;
    size_t source_size;
    bool valid;
} xm_disk_t;

static bool xm_parse(const uint8_t* data, size_t size, xm_disk_t* disk) {
    if (!data || !disk || size < XM_SIZE_320K) return false;
    memset(disk, 0, sizeof(xm_disk_t));
    disk->source_size = size;
    
    if (size >= XM_SIZE_1232K) {
        disk->is_x68k = true;
        disk->tracks = 77; disk->sides = 2;
        disk->sectors = 8; disk->sector_size = 1024;
    } else {
        disk->is_x68k = false;
        disk->tracks = (size >= XM_SIZE_640K) ? 80 : 40;
        disk->sides = 2; disk->sectors = 16; disk->sector_size = 256;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_XM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sharp X1/X68000 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* xm = calloc(1, XM_SIZE_640K);
    xm_disk_t disk;
    assert(xm_parse(xm, XM_SIZE_640K, &disk) && !disk.is_x68k);
    free(xm);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
