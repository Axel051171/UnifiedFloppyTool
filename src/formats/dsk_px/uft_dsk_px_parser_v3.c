/**
 * @file uft_dsk_px_parser_v3.c
 * @brief GOD MODE DSK_PX Parser v3 - Epson PX-8/PX-4 Portable Disk Format
 * 
 * Epson PX-8 Geneva / PX-4:
 * - Microcassette und Microfloppy
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

#define PX_SIZE_180K            (40 * 9 * 512)
#define PX_SIZE_360K            (40 * 2 * 9 * 512)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} px_disk_t;

static bool px_parse(const uint8_t* data, size_t size, px_disk_t* disk) {
    if (!data || !disk || size < PX_SIZE_180K) return false;
    memset(disk, 0, sizeof(px_disk_t));
    disk->source_size = size;
    disk->tracks = 40; disk->sectors = 9; disk->sector_size = 512;
    disk->sides = (size >= PX_SIZE_360K) ? 2 : 1;
    disk->valid = true;
    return true;
}

#ifdef DSK_PX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Epson PX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* px = calloc(1, PX_SIZE_180K);
    px_disk_t disk;
    assert(px_parse(px, PX_SIZE_180K, &disk) && disk.valid);
    free(px);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
