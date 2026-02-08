/**
 * @file uft_dsk_vt_parser_v3.c
 * @brief GOD MODE DSK_VT Parser v3 - VTech Laser 128/200/500 Disk Format
 * 
 * VTech Laser Series:
 * - Apple II kompatibel (Laser 128)
 * - CP/M (Laser 200/500)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VT_SIZE_140K            (35 * 16 * 256)   /* Apple II compat */
#define VT_SIZE_200K            (40 * 10 * 512)   /* CP/M */

typedef struct {
    uint8_t tracks, sectors;
    uint16_t sector_size;
    bool apple_compat;
    size_t source_size;
    bool valid;
} vt_disk_t;

static bool vt_parse(const uint8_t* data, size_t size, vt_disk_t* disk) {
    if (!data || !disk || size < VT_SIZE_140K) return false;
    memset(disk, 0, sizeof(vt_disk_t));
    disk->source_size = size;
    
    if (size == VT_SIZE_140K) {
        disk->tracks = 35;
        disk->sectors = 16;
        disk->sector_size = 256;
        disk->apple_compat = true;
    } else {
        disk->tracks = 40;
        disk->sectors = 10;
        disk->sector_size = 512;
        disk->apple_compat = false;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_VT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== VTech Laser Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* vt = calloc(1, VT_SIZE_140K);
    vt_disk_t disk;
    assert(vt_parse(vt, VT_SIZE_140K, &disk) && disk.apple_compat);
    free(vt);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
