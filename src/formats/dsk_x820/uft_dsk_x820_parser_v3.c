/**
 * @file uft_dsk_x820_parser_v3.c
 * @brief GOD MODE DSK_X820 Parser v3 - Xerox 820 Disk Format
 * 
 * Xerox 820:
 * - CP/M System
 * - SSSD und DSDD Varianten
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define X820_SIZE_SSSD          (40 * 18 * 128)   /* 90K */
#define X820_SIZE_DSDD          (40 * 2 * 18 * 256) /* 360K */

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} x820_disk_t;

static bool x820_parse(const uint8_t* data, size_t size, x820_disk_t* disk) {
    if (!data || !disk || size < X820_SIZE_SSSD) return false;
    memset(disk, 0, sizeof(x820_disk_t));
    disk->source_size = size;
    disk->tracks = 40;
    disk->sectors = 18;
    
    if (size >= X820_SIZE_DSDD) {
        disk->sides = 2;
        disk->sector_size = 256;
    } else {
        disk->sides = 1;
        disk->sector_size = 128;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_X820_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Xerox 820 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* x820 = calloc(1, X820_SIZE_SSSD);
    x820_disk_t disk;
    assert(x820_parse(x820, X820_SIZE_SSSD, &disk) && disk.valid);
    free(x820);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
