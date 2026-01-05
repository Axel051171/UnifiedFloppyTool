/**
 * @file uft_tan_parser_v3.c
 * @brief GOD MODE TAN Parser v3 - Tandy TRS-80 Model I/III/4
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TAN_SIZE_SSSD           (35 * 10 * 256)   /* 87.5K */
#define TAN_SIZE_SSDD           (40 * 18 * 256)   /* 180K */
#define TAN_SIZE_DSDD           (80 * 18 * 256)   /* 360K */

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint16_t sector_size;
    bool is_trsdos;
    size_t source_size;
    bool valid;
} tan_disk_t;

static bool tan_parse(const uint8_t* data, size_t size, tan_disk_t* disk) {
    if (!data || !disk || size < TAN_SIZE_SSSD) return false;
    memset(disk, 0, sizeof(tan_disk_t));
    disk->source_size = size;
    disk->sector_size = 256;
    
    if (size <= TAN_SIZE_SSSD + 1000) {
        disk->tracks = 35; disk->sides = 1; disk->sectors = 10;
    } else if (size <= TAN_SIZE_SSDD + 1000) {
        disk->tracks = 40; disk->sides = 1; disk->sectors = 18;
    } else {
        disk->tracks = 80; disk->sides = 2; disk->sectors = 18;
    }
    
    /* Check for TRSDOS signature */
    disk->is_trsdos = (data[0] == 0x00 && data[1] == 0xFE);
    disk->valid = true;
    return true;
}

#ifdef TAN_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Tandy TRS-80 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* tan = calloc(1, TAN_SIZE_SSSD);
    tan_disk_t disk;
    assert(tan_parse(tan, TAN_SIZE_SSSD, &disk) && disk.valid);
    free(tan);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
