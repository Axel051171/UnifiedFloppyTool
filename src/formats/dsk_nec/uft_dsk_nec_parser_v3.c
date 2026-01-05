/**
 * @file uft_dsk_nec_parser_v3.c
 * @brief GOD MODE DSK_NEC Parser v3 - NEC PC-6001/PC-8001/PC-8801 Disk Format
 * 
 * NEC PC Series (before PC-98):
 * - N-BASIC / N88-BASIC
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

#define NEC_SIZE_320K           (40 * 2 * 16 * 256)
#define NEC_SIZE_640K           (80 * 2 * 16 * 256)
#define NEC_SIZE_1M             (80 * 2 * 26 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} nec_disk_t;

static bool nec_parse(const uint8_t* data, size_t size, nec_disk_t* disk) {
    if (!data || !disk || size < NEC_SIZE_320K) return false;
    memset(disk, 0, sizeof(nec_disk_t));
    disk->source_size = size;
    disk->sides = 2; disk->sector_size = 256;
    
    if (size >= NEC_SIZE_1M) {
        disk->tracks = 80; disk->sectors = 26;
    } else if (size >= NEC_SIZE_640K) {
        disk->tracks = 80; disk->sectors = 16;
    } else {
        disk->tracks = 40; disk->sectors = 16;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_NEC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== NEC PC Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* nec = calloc(1, NEC_SIZE_640K);
    nec_disk_t disk;
    assert(nec_parse(nec, NEC_SIZE_640K, &disk) && disk.tracks == 80);
    free(nec);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
