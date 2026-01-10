/**
 * @file uft_dsk_sv_parser_v3.c
 * @brief GOD MODE DSK_SV Parser v3 - Spectravideo SVI-328/728 Disk Format
 * 
 * Spectravideo Disk Format:
 * - MSX-kompatibel
 * - 40 Tracks × 9 Sektoren × 512 Bytes
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SV_SIZE_SS              (40 * 9 * 512)
#define SV_SIZE_DS              (40 * 2 * 9 * 512)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} sv_disk_t;

static bool sv_parse(const uint8_t* data, size_t size, sv_disk_t* disk) {
    if (!data || !disk || size < SV_SIZE_SS) return false;
    memset(disk, 0, sizeof(sv_disk_t));
    disk->source_size = size;
    disk->tracks = 40;
    disk->sides = (size >= SV_SIZE_DS) ? 2 : 1;
    disk->sectors = 9;
    disk->sector_size = 512;
    disk->valid = true;
    return true;
}

#ifdef DSK_SV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Spectravideo Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* sv = calloc(1, SV_SIZE_DS);
    sv_disk_t disk;
    assert(sv_parse(sv, SV_SIZE_DS, &disk) && disk.valid);
    free(sv);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
