/**
 * @file uft_alt_parser_v3.c
 * @brief GOD MODE ALT Parser v3 - Altair/IMSAI 8" Disk Format
 * 
 * Altair 8800 / IMSAI 8080:
 * - 8" SSSD/SSDD
 * - CP/M, Altair DOS
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ALT_SIZE_SSSD           (77 * 26 * 128)   /* 256K */
#define ALT_SIZE_SSDD           (77 * 26 * 256)   /* 512K */

typedef struct {
    uint8_t tracks;
    uint8_t sectors;
    uint16_t sector_size;
    bool is_dd;
    size_t source_size;
    bool valid;
} alt_disk_t;

static bool alt_parse(const uint8_t* data, size_t size, alt_disk_t* disk) {
    if (!data || !disk || size < ALT_SIZE_SSSD) return false;
    memset(disk, 0, sizeof(alt_disk_t));
    disk->source_size = size;
    disk->tracks = 77;
    disk->sectors = 26;
    disk->is_dd = (size >= ALT_SIZE_SSDD);
    disk->sector_size = disk->is_dd ? 256 : 128;
    disk->valid = true;
    return true;
}

#ifdef ALT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Altair/IMSAI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* alt = calloc(1, ALT_SIZE_SSSD);
    alt_disk_t disk;
    assert(alt_parse(alt, ALT_SIZE_SSSD, &disk) && disk.valid);
    free(alt);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
