/**
 * @file uft_dsk_hk_parser_v3.c
 * @brief GOD MODE DSK_HK Parser v3 - Heathkit H17/H37 Disk Format
 * 
 * Heathkit/Zenith Disk Formate:
 * - H17: 5.25" Hard-Sector
 * - H37: 5.25" Soft-Sector
 * - HDOS Filesystem
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define HK_H17_SIZE             (40 * 10 * 256)   /* 102400 */
#define HK_H37_SIZE             (40 * 16 * 256)   /* 163840 */
#define HK_H37_DD_SIZE          (80 * 16 * 512)   /* 655360 */

typedef enum { HK_TYPE_H17 = 17, HK_TYPE_H37 = 37 } hk_type_t;

typedef struct {
    hk_type_t type;
    uint8_t tracks;
    uint8_t sectors;
    uint16_t sector_size;
    bool is_dd;
    size_t source_size;
    bool valid;
} hk_disk_t;

static bool hk_parse(const uint8_t* data, size_t size, hk_disk_t* disk) {
    if (!data || !disk || size < HK_H17_SIZE) return false;
    memset(disk, 0, sizeof(hk_disk_t));
    disk->source_size = size;
    
    if (size == HK_H17_SIZE) {
        disk->type = HK_TYPE_H17;
        disk->tracks = 40; disk->sectors = 10; disk->sector_size = 256;
    } else if (size == HK_H37_SIZE) {
        disk->type = HK_TYPE_H37;
        disk->tracks = 40; disk->sectors = 16; disk->sector_size = 256;
    } else {
        disk->type = HK_TYPE_H37;
        disk->tracks = 80; disk->sectors = 16; disk->sector_size = 512;
        disk->is_dd = true;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_HK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Heathkit Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* hk = calloc(1, HK_H17_SIZE);
    hk_disk_t disk;
    assert(hk_parse(hk, HK_H17_SIZE, &disk) && disk.type == HK_TYPE_H17);
    free(hk);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
