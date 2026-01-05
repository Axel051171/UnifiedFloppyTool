/**
 * @file uft_dsk_alp_parser_v3.c
 * @brief GOD MODE DSK_ALP Parser v3 - Triumph-Adler Alphatronic PC Disk Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ALP_SIZE_320K           (40 * 2 * 16 * 256)
#define ALP_SIZE_640K           (80 * 2 * 16 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} alp_disk_t;

static bool alp_parse(const uint8_t* data, size_t size, alp_disk_t* disk) {
    if (!data || !disk || size < ALP_SIZE_320K) return false;
    memset(disk, 0, sizeof(alp_disk_t));
    disk->source_size = size;
    disk->sectors = 16; disk->sector_size = 256; disk->sides = 2;
    disk->tracks = (size >= ALP_SIZE_640K) ? 80 : 40;
    disk->valid = true;
    return true;
}

#ifdef DSK_ALP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Alphatronic Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* alp = calloc(1, ALP_SIZE_320K);
    alp_disk_t disk;
    assert(alp_parse(alp, ALP_SIZE_320K, &disk) && disk.valid);
    free(alp);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
