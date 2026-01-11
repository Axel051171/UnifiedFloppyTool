/**
 * @file uft_dsk_fm7_parser_v3.c
 * @brief GOD MODE DSK_FM7 Parser v3 - Fujitsu FM-7/FM-77 Disk Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FM7_SIZE_320K           (40 * 2 * 16 * 256)
#define FM7_SIZE_640K           (80 * 2 * 16 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} fm7_disk_t;

static bool fm7_parse(const uint8_t* data, size_t size, fm7_disk_t* disk) {
    if (!data || !disk || size < FM7_SIZE_320K) return false;
    memset(disk, 0, sizeof(fm7_disk_t));
    disk->source_size = size;
    disk->sides = 2; disk->sectors = 16; disk->sector_size = 256;
    disk->tracks = (size >= FM7_SIZE_640K) ? 80 : 40;
    disk->valid = true;
    return true;
}

#ifdef DSK_FM7_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Fujitsu FM-7 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* fm7 = calloc(1, FM7_SIZE_320K);
    fm7_disk_t disk;
    assert(fm7_parse(fm7, FM7_SIZE_320K, &disk) && disk.valid);
    free(fm7);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
