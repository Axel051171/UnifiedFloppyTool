/**
 * @file uft_dsk_cro_parser_v3.c
 * @brief GOD MODE DSK_CRO Parser v3 - Cromemco Disk Format
 * 
 * Cromemco CDOS:
 * - 5.25" und 8" Formate
 * - Hard-Sector und Soft-Sector
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CRO_SIZE_5_SSSD         (40 * 10 * 256)   /* 100K */
#define CRO_SIZE_5_DSDD         (40 * 2 * 18 * 256) /* 360K */
#define CRO_SIZE_8_SSSD         (77 * 26 * 128)   /* 250K */

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    bool is_8_inch;
    size_t source_size;
    bool valid;
} cro_disk_t;

static bool cro_parse(const uint8_t* data, size_t size, cro_disk_t* disk) {
    if (!data || !disk || size < CRO_SIZE_5_SSSD) return false;
    memset(disk, 0, sizeof(cro_disk_t));
    disk->source_size = size;
    
    if (size >= CRO_SIZE_8_SSSD && size % (77 * 26) == 0) {
        disk->is_8_inch = true;
        disk->tracks = 77;
        disk->sectors = 26;
        disk->sector_size = size / (77 * 26);
        disk->sides = 1;
    } else {
        disk->is_8_inch = false;
        disk->tracks = 40;
        disk->sides = (size >= CRO_SIZE_5_DSDD) ? 2 : 1;
        disk->sectors = disk->sides == 2 ? 18 : 10;
        disk->sector_size = 256;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_CRO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Cromemco Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* cro = calloc(1, CRO_SIZE_5_SSSD);
    cro_disk_t disk;
    assert(cro_parse(cro, CRO_SIZE_5_SSSD, &disk) && disk.valid);
    free(cro);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
