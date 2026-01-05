/**
 * @file uft_dsk_eqx_parser_v3.c
 * @brief GOD MODE DSK_EQX Parser v3 - Epson QX-10/QX-16 Disk Format
 * 
 * Epson QX Series:
 * - TPM-III / Valdocs
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

#define EQX_SIZE_360K           (40 * 2 * 9 * 512)
#define EQX_SIZE_720K           (80 * 2 * 9 * 512)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} eqx_disk_t;

static bool eqx_parse(const uint8_t* data, size_t size, eqx_disk_t* disk) {
    if (!data || !disk || size < EQX_SIZE_360K) return false;
    memset(disk, 0, sizeof(eqx_disk_t));
    disk->source_size = size;
    disk->sectors = 9; disk->sector_size = 512; disk->sides = 2;
    disk->tracks = (size >= EQX_SIZE_720K) ? 80 : 40;
    disk->valid = true;
    return true;
}

#ifdef DSK_EQX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Epson QX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* eqx = calloc(1, EQX_SIZE_720K);
    eqx_disk_t disk;
    assert(eqx_parse(eqx, EQX_SIZE_720K, &disk) && disk.valid);
    free(eqx);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
