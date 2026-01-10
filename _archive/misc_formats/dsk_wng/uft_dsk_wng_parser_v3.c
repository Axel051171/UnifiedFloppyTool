/**
 * @file uft_dsk_wng_parser_v3.c
 * @brief GOD MODE DSK_WNG Parser v3 - Wang Professional Computer Disk Format
 * 
 * Wang Professional Computer (WPC):
 * - GCR Encoding
 * - 8" und 5.25" Formate
 * - CP/M-86 kompatibel
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define WNG_SIZE_256K           (77 * 26 * 128)   /* 8" SSSD */
#define WNG_SIZE_512K           (77 * 26 * 256)   /* 8" SSDD */
#define WNG_SIZE_1M             (77 * 26 * 2 * 256) /* 8" DSDD */

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} wng_disk_t;

static bool wng_parse(const uint8_t* data, size_t size, wng_disk_t* disk) {
    if (!data || !disk || size < WNG_SIZE_256K) return false;
    memset(disk, 0, sizeof(wng_disk_t));
    disk->source_size = size;
    disk->tracks = 77;
    disk->sectors = 26;
    
    if (size >= WNG_SIZE_1M) {
        disk->sides = 2;
        disk->sector_size = 256;
    } else if (size >= WNG_SIZE_512K) {
        disk->sides = 1;
        disk->sector_size = 256;
    } else {
        disk->sides = 1;
        disk->sector_size = 128;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_WNG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Wang Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* wng = calloc(1, WNG_SIZE_512K);
    wng_disk_t disk;
    assert(wng_parse(wng, WNG_SIZE_512K, &disk) && disk.valid);
    free(wng);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
