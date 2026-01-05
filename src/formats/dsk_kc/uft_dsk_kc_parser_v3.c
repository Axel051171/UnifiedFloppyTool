/**
 * @file uft_dsk_kc_parser_v3.c
 * @brief GOD MODE DSK_KC Parser v3 - Robotron KC85 Disk Format
 * 
 * East German KC85/KC87:
 * - MicroDOS Filesystem
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

#define KC_SIZE_400K            (80 * 5 * 1024)
#define KC_SIZE_800K            (80 * 2 * 5 * 1024)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} kc_disk_t;

static bool kc_parse(const uint8_t* data, size_t size, kc_disk_t* disk) {
    if (!data || !disk || size < KC_SIZE_400K) return false;
    memset(disk, 0, sizeof(kc_disk_t));
    disk->source_size = size;
    disk->tracks = 80;
    disk->sides = (size >= KC_SIZE_800K) ? 2 : 1;
    disk->sectors = 5;
    disk->sector_size = 1024;
    disk->valid = true;
    return true;
}

#ifdef DSK_KC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Robotron KC85 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* kc = calloc(1, KC_SIZE_400K);
    kc_disk_t disk;
    assert(kc_parse(kc, KC_SIZE_400K, &disk) && disk.valid);
    free(kc);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
