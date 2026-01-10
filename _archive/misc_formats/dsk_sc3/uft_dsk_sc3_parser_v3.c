/**
 * @file uft_dsk_sc3_parser_v3.c
 * @brief GOD MODE DSK_SC3 Parser v3 - Sega SC-3000 Disk Format
 * 
 * Sega SC-3000/SF-7000:
 * - MSX-ähnlich
 * - 40 Tracks × 16 Sektoren
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SC3_SIZE_SS             (40 * 16 * 256)
#define SC3_SIZE_DS             (40 * 2 * 16 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} sc3_disk_t;

static bool sc3_parse(const uint8_t* data, size_t size, sc3_disk_t* disk) {
    if (!data || !disk || size < SC3_SIZE_SS) return false;
    memset(disk, 0, sizeof(sc3_disk_t));
    disk->source_size = size;
    disk->tracks = 40;
    disk->sides = (size >= SC3_SIZE_DS) ? 2 : 1;
    disk->sectors = 16;
    disk->sector_size = 256;
    disk->valid = true;
    return true;
}

#ifdef DSK_SC3_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sega SC-3000 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* sc3 = calloc(1, SC3_SIZE_DS);
    sc3_disk_t disk;
    assert(sc3_parse(sc3, SC3_SIZE_DS, &disk) && disk.valid);
    free(sc3);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
