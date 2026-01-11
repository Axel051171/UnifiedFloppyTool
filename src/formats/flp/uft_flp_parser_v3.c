/**
 * @file uft_flp_parser_v3.c
 * @brief GOD MODE FLP Parser v3 - Fairlight CMI Disk Format
 * 
 * Fairlight CMI Series I/II/III:
 * - 8" Floppy
 * - Proprietary filesystem
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FLP_SIZE_256K           (77 * 26 * 128)
#define FLP_SIZE_1M             (77 * 26 * 512)

typedef struct {
    uint8_t tracks;
    uint8_t sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} flp_disk_t;

static bool flp_parse(const uint8_t* data, size_t size, flp_disk_t* disk) {
    if (!data || !disk || size < FLP_SIZE_256K) return false;
    memset(disk, 0, sizeof(flp_disk_t));
    disk->source_size = size;
    disk->tracks = 77;
    disk->sectors = 26;
    disk->sector_size = (size >= FLP_SIZE_1M) ? 512 : 128;
    disk->valid = true;
    return true;
}

#ifdef FLP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Fairlight CMI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* flp = calloc(1, FLP_SIZE_256K);
    flp_disk_t disk;
    assert(flp_parse(flp, FLP_SIZE_256K, &disk) && disk.valid);
    free(flp);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
