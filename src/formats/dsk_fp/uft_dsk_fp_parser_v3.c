/**
 * @file uft_dsk_fp_parser_v3.c
 * @brief GOD MODE DSK_FP Parser v3 - Casio FP-1100 Disk Format
 * 
 * Casio FP-1100:
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

#define FP_SIZE_2D              (40 * 2 * 16 * 256)
#define FP_SIZE_2DD             (80 * 2 * 16 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} fp_disk_t;

static bool fp_parse(const uint8_t* data, size_t size, fp_disk_t* disk) {
    if (!data || !disk || size < FP_SIZE_2D) return false;
    memset(disk, 0, sizeof(fp_disk_t));
    disk->source_size = size;
    disk->tracks = (size >= FP_SIZE_2DD) ? 80 : 40;
    disk->sides = 2;
    disk->sectors = 16;
    disk->sector_size = 256;
    disk->valid = true;
    return true;
}

#ifdef DSK_FP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Casio FP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* fp = calloc(1, FP_SIZE_2D);
    fp_disk_t disk;
    assert(fp_parse(fp, FP_SIZE_2D, &disk) && disk.valid);
    free(fp);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
