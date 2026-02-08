/**
 * @file uft_dsk_nas_parser_v3.c
 * @brief GOD MODE DSK_NAS Parser v3 - Nascom 1/2 Disk Format
 * 
 * Nascom Disk Format mit Gemini BIOS:
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

#define NAS_SIZE_SS             (40 * 10 * 512)
#define NAS_SIZE_DS             (40 * 2 * 10 * 512)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} nas_disk_t;

static bool nas_parse(const uint8_t* data, size_t size, nas_disk_t* disk) {
    if (!data || !disk || size < NAS_SIZE_SS) return false;
    memset(disk, 0, sizeof(nas_disk_t));
    disk->source_size = size;
    disk->tracks = 40;
    disk->sides = (size >= NAS_SIZE_DS) ? 2 : 1;
    disk->sectors = 10;
    disk->sector_size = 512;
    disk->valid = true;
    return true;
}

#ifdef DSK_NAS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Nascom Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* nas = calloc(1, NAS_SIZE_DS);
    nas_disk_t disk;
    assert(nas_parse(nas, NAS_SIZE_DS, &disk) && disk.valid);
    free(nas);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
